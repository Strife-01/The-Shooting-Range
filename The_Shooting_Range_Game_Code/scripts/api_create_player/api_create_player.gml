/// @desc Send create player request (registration)
function api_create_player(_username, _password, _finger_hash, _access) {
    var url = global.api_base + "/players";

    // build the JSON body
    var payload = json_stringify({
        "username": _username,
        "password": _password,
        "fingerprint_scanner_hash_code": _finger_hash,
        "fingerprint_scanner_access_level": _access
    });

    // set headers
    var headers = ds_map_create();
    ds_map_add(headers, "Content-Type", "application/json");

    // send HTTP POST request
    var req_id = http_request(url, "POST", headers, payload);

    // cleanup
    ds_map_destroy(headers);

    // store globally so Async HTTP can match responses
    global.last_api_request = req_id;
    global.reg_create_sent = true;

    show_debug_message("[API] Sent POST /players for user '" + _username + "' (req_id=" + string(req_id) + ")");
	room_goto(rm_sign_in)
}
