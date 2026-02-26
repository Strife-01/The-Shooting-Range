if (global.login_prompt_text != "" && current_time < global.login_prompt_timer) {
    var xx = display_get_gui_width() * 0.5;
    var yy = display_get_gui_height() * 0.9;

    var txt = global.login_prompt_text;
    var w = string_width(txt) + 40;
    var h = string_height(txt) + 20;

    draw_set_alpha(0.5);
    draw_set_color(c_black);
    draw_rectangle(xx - w/2, yy - h/2, xx + w/2, yy + h/2, false);
    draw_set_alpha(1);
    draw_set_color(c_white);
    draw_set_font(font_main);
    draw_set_halign(fa_center);
    draw_set_valign(fa_middle);
    draw_text(xx, yy, txt);
}
