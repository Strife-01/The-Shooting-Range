/// @function user_store_add(username)
/// @desc Add username to persistent store (dedup), set as last, save.
/// @return bool

function user_store_add(username) {
    username = string(username);
    if (username == "") return false;

    // --- Ensure store initialized (just call directly)
    user_store_init();

    var store = user_store_read();
    var arr   = store.usernames;

    // Deduplicate
    var found = false;
    for (var i = 0; i < array_length(arr); i++) {
        if (string(arr[i]) == username) { found = true; break; }
    }
    if (!found) {
        var n = array_length(arr);
        array_resize(arr, n + 1);
        arr[n] = username;
    }

    store.usernames = arr;
    store.last = username;

    var ok = user_store_write(store);
    if (ok) {
        show_debug_message("[USER STORE] Added username '" + username + "' and saved file.");
        global.needs_username_refresh = true; // for UI refresh later
    }
    return ok;
}
