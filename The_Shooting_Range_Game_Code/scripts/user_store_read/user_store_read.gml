/// @desc Read username store -> {usernames:Array, last:String}
function user_store_read() {
    var out = { usernames: [], last: "" };

    if (!variable_global_exists("user_store_file")) user_store_init();
    if (!file_exists(global.user_store_file)) return out;

    var fh = file_text_open_read(global.user_store_file);
    if (fh < 0) return out;

    var txt = "";
    while (!file_text_eof(fh)) {
        txt += file_text_read_string(fh);
        file_text_readln(fh);
    }
    file_text_close(fh);

    var obj;
    try { obj = json_parse(txt); } catch (_) { obj = undefined; }

    if (is_struct(obj)) {
        if (variable_struct_exists(obj, "usernames") && is_array(obj.usernames)) {
            out.usernames = obj.usernames;
        }
        if (variable_struct_exists(obj, "last")) {
            out.last = string(obj.last);
        }
    }
    return out;
}
