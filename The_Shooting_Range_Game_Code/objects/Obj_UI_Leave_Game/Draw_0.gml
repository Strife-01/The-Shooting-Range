// Draw simple sliders for BGM and SFX when in the settings room
if (room == rm_settings) {
    // labels
    draw_set_halign(fa_left);
    draw_set_valign(fa_middle);
    draw_set_color(c_white);
    draw_set_font(font_main);
    draw_text(slider_x, slider_y_bgm - 28, "Music Volume");
    draw_text(slider_x, slider_y_sfx - 28, "SFX Volume");

    // bar background
    draw_set_color(make_color_rgb(60, 60, 60));
    draw_rectangle(slider_x, slider_y_bgm - slider_h/2, slider_x + slider_w, slider_y_bgm + slider_h/2, false);
    draw_rectangle(slider_x, slider_y_sfx - slider_h/2, slider_x + slider_w, slider_y_sfx + slider_h/2, false);

    // bar fill
    draw_set_color(make_color_rgb(120, 180, 255));
    var fill_bgm_w = clamp(knob_bgm_x - slider_x, 0, slider_w);
    var fill_sfx_w = clamp(knob_sfx_x - slider_x, 0, slider_w);
    draw_rectangle(slider_x, slider_y_bgm - slider_h/2, slider_x + fill_bgm_w, slider_y_bgm + slider_h/2, false);
    draw_rectangle(slider_x, slider_y_sfx - slider_h/2, slider_x + fill_sfx_w, slider_y_sfx + slider_h/2, false);

    // knobs
    draw_set_color(make_color_rgb(240,240,240));
    draw_circle(knob_bgm_x, slider_y_bgm, knob_r, false);
    draw_circle(knob_sfx_x, slider_y_sfx, knob_r, false);

    // values
    draw_set_color(c_white);
    var bgm_pct = string_format(global.bgm_volume * 100, 0, 0) + "%";
    var sfx_pct = string_format(global.sfx_volume * 100, 0, 0) + "%";
    draw_text(slider_x + slider_w + 16, slider_y_bgm, bgm_pct);
    draw_text(slider_x + slider_w + 16, slider_y_sfx, sfx_pct);
}
