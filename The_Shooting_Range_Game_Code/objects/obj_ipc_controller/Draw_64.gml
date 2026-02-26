draw_set_halign(fa_center);
draw_set_valign(fa_middle);
draw_set_color(c_black);
draw_set_font(font_main);

var cx = display_get_gui_width() / 2;
var cy = display_get_gui_height() / 2;

if (scan_state == 0) {
    draw_text(cx, cy, message_wait);
} else if (scan_state == 1){
    draw_text(cx, cy - 12, "Welcome agent");
    draw_text(cx, cy + 12, user_id);
} else if (scan_state == 2) {
	draw_text(cx, cy - 12, "registering");
}

// reset alignment if other draw code relies on defaults
draw_set_halign(fa_left);
draw_set_valign(fa_top);