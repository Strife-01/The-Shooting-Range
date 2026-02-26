/// @desc Send fingerprint-based login to the database.
/// @param username
/// @param fingerprint_hash

function api_login_with_fingerprint(_username, _finger_hash) {
    if (_username == "" || _finger_hash == "") {
        show_debug_message("[API] Missing username or fingerprint hash, cannot login.");
        return false;
    }

    var url = global.api_base + "/login";
    var payload = {
        username: _username,
        fingerprint_scanner_hash_code: _finger_hash
    };
    var json_body = json_stringify(payload);

    var headers = ds_map_create();
    ds_map_add(headers, "Content-Type", "application/json");

    var req_id = http_request(url, "POST", headers, json_body);
    global.last_api_request = req_id;

    ds_map_destroy(headers);

    show_debug_message("[API] Sent POST /login for user '" + string(_username) + "' (req_id=" + string(req_id) + ")");
    return true;
}
