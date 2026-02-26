var rid = async_load[? "id"];
if (rid != global.qr_req_id) exit;

var net_status = async_load[? "status"];      // 0 = success
var http_code  = async_load[? "http_status"]; // 200..299 = OK
var body       = async_load[? "result"];      // JSON text

if (net_status == 0 && http_code >= 200 && http_code < 300) {
    if (qr_apply_from_json(body)) {
        show_debug_message("[QR] JSON applied. Token=" + string(global.qr_token));
    } else {
        // Fallback
        show_debug_message("[QR] JSON parse/apply failed; showing server_down sprite.");
        if (variable_global_exists("qr_sprite") && is_real(global.qr_sprite) && global.qr_sprite != -1)
            sprite_delete(global.qr_sprite);
        global.qr_sprite = server_down;
    }
} else {
	// Connection failed or server offline
    show_debug_message("[QR] HTTP failed. net=" + string(net_status) + " http=" + string(http_code));
    // Fallback: Included File
    if (variable_global_exists("qr_sprite") && is_real(global.qr_sprite) && global.qr_sprite != -1)
        sprite_delete(global.qr_sprite);
}

// set sprite position offset (draw slightly above center)
global.qr_draw_x = display_get_gui_width() / 2.8;
global.qr_draw_y = display_get_gui_height() / 2 - 350; // move up a bit

// clear outstanding id
global.qr_req_id = -1;
