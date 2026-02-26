var uname = (is_string(global.login_username) && string_length(global.login_username) > 0)
    ? global.login_username : "Guest";

draw_set_color(c_black);
draw_set_font(font_main);
draw_set_halign(fa_left);
draw_set_valign(fa_bottom);

if (room == rm_settings || room == rm_main) {
    draw_text(20, 32, "User: " + uname);

    var current_uid = is_string(global.jwt_user_id) ? global.jwt_user_id : "";
    if (current_uid != "" && global.highscore_owner == current_uid) {
        draw_text(20, 64, "High Score: " + string(global.highscore));
    } else {
        // Allow an optional override (used for guest/bypass) to display a custom string
        if (variable_global_exists("highscore_display_override") && string(global.highscore_display_override) != "") {
            draw_text(20, 64, "High Score: " + string(global.highscore_display_override));
        } else {
            draw_text(20, 64, "High Score: �");
        }

        // auto-fetch if we have a user_id and not up-to-date yet
        if (current_uid != "" && current_time >= global.next_hs_fetch_ms) {
            api_fetch_highscore_current();
            global.next_hs_fetch_ms = current_time + 1500; // 1.5s throttle
        }
    }
}
