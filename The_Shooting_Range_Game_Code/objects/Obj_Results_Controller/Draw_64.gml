var cx = display_get_gui_width() / 2;
var cy = display_get_gui_height() / 2;

draw_set_color(c_black);

draw_set_halign(fa_center);
draw_set_valign(fa_middle);

draw_text(cx, cy - 100, "GAME OVER");
draw_text(cx, cy - 40, "Score: " + string(global.score));
draw_text(cx, cy, "High Score: " + string(global.highscore));

draw_set_color(make_color_rgb(171, 113, 12));
draw_text(cx, cy + 80, "Click to return to the main menu");
