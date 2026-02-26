/// @desc Read the latest credentials JSON from QR outbox and send CMD2 to scanner.
/// @return struct { ok, username, password, token }

function register_read_latest() {
    // --- CONFIG ---
    var folder = global.gm_outbox_path;
    var cmd_path = global.scanner_cmd_path;
    if (!directory_exists(folder)) return { ok: false };

    // --- Find the first (or latest-looking) .json file ---
    var newest_file = "";
    var pattern = folder + "/*.json";
    var f = file_find_first(pattern, fa_readonly);

    if (f != "") {
        // If multiple exist, take the last found (token names increase)
        newest_file = folder + "/" + f;
        f = file_find_next();
        while (f != "") {
            newest_file = folder + "/" + f;
            f = file_find_next();
        }
    }
    file_find_close();

    // If no new file, stay silent
    if (newest_file == "") return { ok: false };

    // --- Read the file ---
    var raw = "";
    var fh = file_text_open_read(newest_file);
    if (fh < 0) return { ok: false };
    while (!file_text_eof(fh)) raw += file_text_readln(fh);
    file_text_close(fh);

    // --- Parse JSON ---
    var j;
    try { j = json_parse(raw); } catch (e) {
        show_debug_message("[register_read_latest] JSON parse error: " + string(e.message));
        return { ok: false };
    }

    if (!is_struct(j)) return { ok: false };

    var username = "";
    var password = "";
    var token    = "";
    if (variable_struct_exists(j, "username")) username = j.username;
    if (variable_struct_exists(j, "password")) password = j.password;
    if (variable_struct_exists(j, "token"))    token    = j.token;

    // --- Delete file after reading ---
    var deleted = file_delete(newest_file);
    show_debug_message("[register_read_latest] Deleted " + newest_file + ": " + string(deleted));

    // --- Immediately send CMD2 to scanner ---
    var payload_struct = {
        username: username,
        password: password,
        token: token
    };

    var ver = current_time;
    var cmd_json = json_stringify({
        version: ver,
        command: "CMD2",
        payload: payload_struct
    });

    var tmp = cmd_path + ".tmp";
    if (file_exists(tmp)) file_delete(tmp);
    var fh2 = file_text_open_write(tmp);
    if (fh2 >= 0) {
        file_text_write_string(fh2, cmd_json);
        file_text_close(fh2);
        if (file_exists(cmd_path)) file_delete(cmd_path);
        file_rename(tmp, cmd_path);
        show_debug_message("[register_read_latest] CMD2 written to: " + cmd_path);
    }

    return { ok: true, username: username, password: password, token: token };
}
