/// @desc Atomically write username store JSON. Returns bool.
function user_store_write(store_struct) {
    if (!variable_global_exists("user_store_file")) user_store_init();

    var tmp = string(global.user_store_file) + ".tmp";
    if (file_exists(tmp)) file_delete(tmp);

    var fh = file_text_open_write(tmp);
    if (fh < 0) return false;

    var json_txt = json_stringify(store_struct);
    file_text_write_string(fh, json_txt);
    file_text_close(fh);

    if (file_exists(global.user_store_file)) file_delete(global.user_store_file);
    file_rename(tmp, global.user_store_file);
    return true;
}
