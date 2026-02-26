/// @desc Send CMD3 login request to scanner with the selected username.
/// @param username

function login_send_cmd3(_username) {
    if (_username == "") {
        show_debug_message("[LOGIN] Username empty, cannot send CMD3.");
        return false;
    }

    // Ensure paths exist
    if (!variable_global_exists("scanner_cmd_path")) {
        show_debug_message("[LOGIN] scanner_cmd_path not set.");
        return false;
    }

    var cmd_path = global.scanner_cmd_path;
    var tmp = cmd_path + ".tmp";

    // Build payload
    var payload = { username: _username };
    var ver = current_time;

    var cmd_struct = {
        version: ver,
        command: "CMD3",
        payload: payload
    };

    var cmd_json = json_stringify(cmd_struct);

    // Write atomically
    if (file_exists(tmp)) file_delete(tmp);
    var fh = file_text_open_write(tmp);
    if (fh >= 0) {
        file_text_write_string(fh, cmd_json);
        file_text_close(fh);
        if (file_exists(cmd_path)) file_delete(cmd_path);
        file_rename(tmp, cmd_path);
        show_debug_message("[LOGIN] CMD3 written to: " + cmd_path);
        return true;
    } else {
        show_debug_message("[LOGIN] Failed to open " + tmp + " for write.");
        return false;
    }
}
