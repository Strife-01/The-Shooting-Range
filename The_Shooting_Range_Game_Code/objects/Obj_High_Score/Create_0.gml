/// @desc Initialize highscore display for the current logged-in user
if (!variable_global_exists("next_hs_fetch_ms")) global.next_hs_fetch_ms = 0;
draw_set_font(font_main);
global.score     = 0;
global.highscore = 0;
global.shots     = 0;
global.max_shots = 5;
global.need_reload = false;

// Fetch only the current user's highscore if logged in
if (variable_global_exists("jwt_user_id") && string_length(global.jwt_user_id) > 0) {
    api_fetch_highscore_current();
}
