/// @func api_login(username, fp_hash)
/// @desc POST /api/login with username + fingerprintScannerHashCode
function api_login(username, fp_hash) {
    var url = global.api_base + "/login";
    var body = json_stringify({
        username: username,
        fingerprint_scanner_hash_code: fp_hash
    });

    var headers = ds_map_create();
    ds_map_add(headers, "Content-Type", "application/json");

    var req = http_request(url, "POST", headers, body);

    ds_map_destroy(headers);

    global.last_login_request = req;
    global.pending_login_username = username;

    show_debug_message("[API] Sent POST /login for user '" + string(username) + "' (req_id=" + string(req) + ")");
    return req;
}
