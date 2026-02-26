/// @desc Ensure username store file exists (JSON) in persistent storage

function user_store_init() {
    // Use GameMaker's persistent sandbox path
    global.user_store_file = "user://usernames.json";

    if (!file_exists(global.user_store_file)) {
        var fh = file_text_open_write(global.user_store_file);
        if (fh >= 0) {
            // schema: { "usernames":[], "last":"" }
            file_text_write_string(fh, "{\"usernames\":[],\"last\":\"\"}");
            file_text_close(fh);
        }
    }
}
