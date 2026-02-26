
if (!variable_instance_exists(self, "color")) color = c_white;

var alpha = lifetime / 30;
draw_set_alpha(alpha);
draw_set_color(color);
draw_set_font(font_popup);
draw_set_halign(fa_center);
draw_text(x, y-50, "+" + string(score_value));
draw_set_alpha(1);
