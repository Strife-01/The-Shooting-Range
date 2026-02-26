/// @desc Draw delete button
draw_set_font(font_main);
draw_set_halign(fa_center);
draw_set_valign(fa_middle);

// background (red)
draw_set_alpha(0.8);
draw_set_color(make_color_rgb(150, 40, 40));
draw_roundrect(button_x, button_y, button_x + button_w, button_y + button_h, false);
draw_set_alpha(1);

// border
draw_set_color(make_color_rgb(60, 0, 0));
draw_roundrect(button_x, button_y, button_x + button_w, button_y + button_h, true);

// text
draw_set_color(c_white);
draw_text(button_x + button_w / 2, button_y + button_h / 2, button_text);
