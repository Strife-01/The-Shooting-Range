/// @desc Send CMD2 command to scanner with username/password/token.
function register_send_cmd2(_username, _password, _token) {
    if (!variable_global_exists("scanner_cmd_path")) {
        show_debug_message("[register_send_cmd2] ERROR: global.scanner_cmd_path not set!");
        return false;
    }

    var cmd_path = global.scanner_cmd_path;
    var dir = "";
    
    // Extract directory manually using string functions (works for both Windows and Linux)
    var last_slash = max(string_last_pos("/", cmd_path), string_last_pos("\\", cmd_path));
    if (last_slash > 0) {
        dir = string_copy(cmd_path, 1, last_slash);
    } else {
        dir = working_directory;
    }

    // Ensure directory exists
    if (!directory_exists(dir)) {
        directory_create(dir);
        show_debug_message("[register_send_cmd2] Created directory: " + dir);
    }

    // Prepare payload
    var payload_struct = {
        username: _username,
        password: _password,
        token: _token
    };

    var ver = register__version_ms();
    var cmd_json = json_stringify({
        version: ver,
        command: "CMD2",
        payload: payload_struct
    });

    // Write command atomically
    var tmp = cmd_path + ".tmp";
    if (file_exists(tmp)) file_delete(tmp);

    var fh = file_text_open_write(tmp);
    if (fh < 0) {
        show_debug_message("[register_send_cmd2] Failed to open " + tmp + " for write.");
        return false;
    }
    file_text_write_string(fh, cmd_json);
    file_text_close(fh);

    // Replace old file safely
    if (file_exists(cmd_path)) file_delete(cmd_path);
    file_rename(tmp, cmd_path);

    show_debug_message("[register_send_cmd2] CMD2 written to: " + cmd_path);
    return true;
}
