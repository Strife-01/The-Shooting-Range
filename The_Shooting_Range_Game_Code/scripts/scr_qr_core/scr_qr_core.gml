// Cleans/pads a Base64 string (handles BOM, whitespace, data-URI prefix etc.)
// Note to AndreiÇ Check if there's smmth I'm missing 
function qr_clean_b64(b64) {
    b64 = string_replace_all(b64, chr(65279), ""); // strip UTF-8 BOM if present
    b64 = string_replace_all(b64, " ", "");
    b64 = string_replace_all(b64, "\n", "");
    b64 = string_replace_all(b64, "\r", "");
    b64 = string_replace_all(b64, "\t", "");
    var k = string_pos("base64,", b64);
    if (k > 0) b64 = string_delete(b64, 1, k);
    var rem = string_length(b64) mod 4;
    if (rem != 0) b64 += string_repeat("=", 4 - rem);
    return b64;
}

// Builds a sprite from a Base64 PNG string. Returns sprite id or -1.
function qr_sprite_from_b64(b64) {
    b64 = qr_clean_b64(b64);

    var buf = buffer_base64_decode(b64);
    if (buf == -1) {
        show_debug_message("[QR] Base64 decode failed.");
        return -1;
    }
	
	// PNG signature already checked. Now check IHDR for bit depth
	var bitdepth  = buffer_peek(buf, 24, buffer_u8); // IHDR: bit depth
	var colortype = buffer_peek(buf, 25, buffer_u8); // IHDR: color type

	// Ask server for 8-bit.
	if (bitdepth < 8) {
	    show_debug_message("[QR] PNG bit depth " + string(bitdepth) +
	        " detected. Please send 8-bit PNG (convert to 'L' or 'RGBA' server-side).");
	    buffer_delete(buf);
	    return -1;
	}

    // Quick PNG signature sanity 
    if (!(buffer_peek(buf,0,buffer_u8)==$89 && buffer_peek(buf,1,buffer_u8)==$50 && buffer_peek(buf,2,buffer_u8)==$4E)) {
        buffer_delete(buf);
        show_debug_message("[QR] Not a PNG signature.");
        return -1;
    }

    var fname = "qr_tmp.png"; // inside sandbox
    if (file_exists(fname)) file_delete(fname);
    buffer_save(buf, fname);
    buffer_delete(buf);

    var spr = sprite_add(fname, 1, false, false, 0, 0);
    if (spr == -1) {
        show_debug_message("[QR] sprite_add failed.");
        return -1;
    }
    return spr;
}

// Reads an Included File (txt) with Base64 and returns a sprite id or -1.
// I USED THIS FOR MANUAL TESTING, CAN BE REMOVED
function qr_sprite_from_file(filename) {
    if (!file_exists(filename)) {
        show_debug_message("[QR] File not found in working_directory: " + filename);
        return -1;
    }
    var f = file_text_open_read(filename);
    if (f < 0) {
        show_debug_message("[QR] Could not open " + filename);
        return -1;
    }
    var s = "";
    while (!file_text_eof(f)) s += file_text_readln(f);
    file_text_close(f);

    return qr_sprite_from_b64(s);
}

/// Centers a sprite’s origin.
function qr_center_origin(spr) {
    if (spr == -1) return;
    sprite_set_offset(spr, sprite_get_width(spr) div 2, sprite_get_height(spr) div 2);
}

/// Draws the QR sprite
function qr_draw_centered(spr, percent_of_gui_min) {
    if (spr == -1) {
        draw_set_halign(fa_center); 
		draw_set_valign(fa_middle);
        draw_text(display_get_gui_width() div 2, display_get_gui_height() div 2, "QR not ready");
        return;
    }

    var gui_w = display_get_gui_width();
    var gui_h = display_get_gui_height();

    var sw = sprite_get_width(spr);
    var sh = sprite_get_height(spr);

    var target_px = floor(min(gui_w, gui_h) * clamp(percent_of_gui_min, 0.1, 1));
    var scale = max(1, floor(target_px / max(sw, sh))); // integer scale = sharp modules

    var cx = gui_w div 2 // X horizontal
    var cy = gui_h div 2; // y vertical

    gpu_set_texfilter(false); // nearest-neighbor = no blur

    // Optional white backdrop with padding
	/*
    var dw = sw * scale, dh = sh * scale, pad = 8;
    draw_rectangle_colour(cx - dw div 2 - pad, cy - dh div 2 - pad,
                          cx + dw div 2 + pad, cy + dh div 2 + pad,
                          c_white, c_white, c_white, c_white, false);
	*/

    draw_sprite_ext(spr, 0, cx, cy, scale, scale, 0, c_white, 1);
}

/// apply qr_b64 + token to globals (build sprite)
function _qr_apply_from_fields(qr_b64, token) {
    if (!is_string(qr_b64) || string_length(qr_b64) <= 0) {
        show_debug_message("[QR] Empty qr_png_b64.");
        return false;
    }

    if (variable_global_exists("qr_sprite") && is_real(global.qr_sprite) && global.qr_sprite != -1) {
        sprite_delete(global.qr_sprite);
        global.qr_sprite = -1;
    }

    global.qr_sprite = qr_sprite_from_b64(qr_b64);
    if (global.qr_sprite != -1) {
        // qr_center_origin(global.qr_sprite);
        global.qr_token = string(token);
        return true;
    } else {
        show_debug_message("[QR] Failed to build sprite from Base64.");
        return false;
    }
}

// Parses the server JSON and updates global.qr_sprite + global.qr_token. Returns true on success
// Accepts either struct JSON (json_parse) or DS map JSON (json_decode fallback)
function qr_apply_from_json(json_text) {
    // Preferred: struct JSON
    var data = json_parse(json_text);

    if (is_struct(data)) {
        var has_qr = variable_struct_exists(data, "qr_png_b64");
        var has_tk = variable_struct_exists(data, "token");

        var qr_b64 = has_qr ? data.qr_png_b64 : "";
        var token  = has_tk ? data.token      : "";

        return _qr_apply_from_fields(qr_b64, token);
    }

    // try DS map decode if parse didn't yield a struct
    var m = json_decode(json_text);
    if (is_real(m) && ds_exists(m, ds_type_map)) {
        var qr_b64_f = ds_map_exists(m, "qr_png_b64") ? m[? "qr_png_b64"] : "";
        var token_f  = ds_map_exists(m, "token")      ? m[? "token"]      : "";
        var ok = _qr_apply_from_fields(qr_b64_f, token_f);
        ds_map_destroy(m);
        return ok;
    }

    show_debug_message("[QR] JSON was neither struct nor ds_map.");
    return false;
}
