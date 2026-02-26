if (variable_global_exists("qr_sprite") && global.qr_sprite != -1) {
    gpu_set_texfilter(false); // no blur
    draw_sprite_ext(global.qr_sprite, 0, global.qr_draw_x, global.qr_draw_y, 2, 2, 0, c_white, 1);
	draw_set_alpha(alpha);
	draw_self();
	draw_set_alpha(1)
} else {
    draw_text(display_get_gui_width()/2, display_get_gui_height()/2, "No QR available");
}



/*
// shows token below the QR
if (is_string(global.qr_token) && string_length(global.qr_token) > 0) {
    var gui_w = display_get_gui_width();
    var gui_h = display_get_gui_height();
    draw_set_halign(fa_center); draw_set_valign(fa_top);
    draw_set_color(c_black);
    draw_text(gui_w div 2, (gui_h * 0.8), "Token: " + string(global.qr_token));
}
*/