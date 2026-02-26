/// @func api_fetch_highscore_current()
function api_fetch_highscore_current() {
    if (!is_string(global.jwt_user_id) || string_length(global.jwt_user_id) == 0) {
        show_debug_message("[API] Skip fetch highscore: no jwt_user_id yet.");
        return -1;
    }

    var url = global.api_base + "/highscores/" + string(global.jwt_user_id);

    var headers = ds_map_create();
    ds_map_add(headers, "Accept", "application/json");
    ds_map_add(headers, "Authorization", "Bearer " + string(global.jwt_token)); // ok if endpoint is public
    var req_id = http_request(url, "GET", headers, "");
    ds_map_destroy(headers);

    global.last_highscore_fetch_request = req_id;
    show_debug_message("[API] GET highscore for current user -> " + url + " (req_id=" + string(req_id) + ")");
    return req_id;
}
