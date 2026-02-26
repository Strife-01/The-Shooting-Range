/// @desc Mark the last-used username in the usernames store
function user_store_mark_last(username) {
    if (string(username) == "") return;

    // ensure store exists
    user_store_init();

    // read, modify, write
    var store = user_store_read();
    store.last = string(username);
    var ok = user_store_write(store);

    show_debug_message("[USER STORE] Marked last username '" + string(username) +
                       "' (write ok=" + string(ok) + ").");
}
