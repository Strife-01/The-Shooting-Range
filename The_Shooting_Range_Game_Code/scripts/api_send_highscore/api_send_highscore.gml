/// @func api_send_highscore(score)
/// @desc Sends highscore using JWT + user_id from login. Returns request id or -1.

function api_send_highscore(score) {
    if (!is_real(score)) score = real(score);

    if (!is_string(global.jwt_token) || string_length(global.jwt_token) == 0) {
        show_debug_message("[API] Highscore send aborted: no JWT token.");
        return -1;
    }
    if (!is_string(global.jwt_user_id) || string_length(global.jwt_user_id) == 0) {
        show_debug_message("[API] Highscore send aborted: no jwt_user_id.");
        return -1;
    }

    var url = global.api_base + "/highscores/" + string(global.jwt_user_id);

    // integer JSON (no .0)
    var hs = round(score); // <- was iround(score)
    var body_str = "{\"highscore\":" + string(hs) + "}";

    var headers = ds_map_create();
    ds_map_add(headers, "Content-Type",  "application/json");
    ds_map_add(headers, "Authorization", "Bearer " + string(global.jwt_token));
    ds_map_add(headers, "Accept",        "application/json");

    show_debug_message("[API] HS JSON body: " + body_str);

    var req_id = http_request(url, "PUT", headers, body_str);
    ds_map_destroy(headers);

    global.last_highscore_request = req_id;
    show_debug_message("[API] Sent PUT highscore=" + string(hs) + " to " + url + " (req_id=" + string(req_id) + ")");
    return req_id;
}
