
draw_set_halign(fa_left)
draw_set_valign(fa_bottom)
draw_set_font(Font6);
draw_set_color(c_black);
draw_text(20, 50, "Score: " + string(global.score));


var bullets_left = global.max_shots - global.shots
draw_set_color(c_black)
draw_set_font(Font7);
draw_text(20, 1060, "Bullets: " + string(bullets_left))
// Draw player lives
draw_set_font(Font6);
if (variable_global_exists("lives")) {
	draw_text(20, 95, "Lives: " + string(global.lives));
}

// If out of bullets, show reload prompt with boxed background and specific fonts
if (variable_global_exists("need_reload") && global.need_reload) {
	var box_w = 600;
	var box_h = 70;
	var bx = room_width / 2 - box_w / 2;
	var by = room_height - 48 - box_h / 2;

	// draw translucent black box
	draw_set_color(c_black);
	draw_set_alpha(0.3);
	draw_rectangle(bx, by, bx + box_w, by + box_h, false);
	draw_set_alpha(1);

	// draw text in white: big header with Font5, smaller subtext with Font6
	draw_set_halign(fa_center);
	draw_set_valign(fa_middle);
	draw_set_color(c_white);
	if (!is_undefined(Font5)) draw_set_font(Font6); else draw_set_font(font_main);
	draw_text(room_width / 2, by + box_h * 0.35, "OUT OF BULLETS");
	if (!is_undefined(Font6)) draw_set_font(Font5); else draw_set_font(font_main);
	draw_text(room_width / 2, by + box_h * 0.75, "press the button on the back of the gun to reload");

	// restore defaults
	draw_set_font(font_main);
	draw_set_halign(fa_left);
	draw_set_valign(fa_bottom);
}

// Round banner (centered) — show at round start for a short time
if (variable_global_exists("round_banner_timer") && current_time < global.round_banner_timer) {
	var rn = (variable_global_exists("round_number") ? global.round_number : 1);
	draw_set_halign(fa_center);
	draw_set_valign(fa_middle);
	// use Font4 for the round banner
	if (!is_undefined(Font4)) draw_set_font(Font4); else draw_set_font(font_main);
	draw_set_color(c_black);
	draw_text(room_width / 2, room_height / 2, "ROUND " + string(rn));
	// restore defaults
	draw_set_font(font_main);
	draw_set_halign(fa_left);
	draw_set_valign(fa_bottom);
}
