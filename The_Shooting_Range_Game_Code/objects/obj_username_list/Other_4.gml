/// @desc Reload usernames on room entry

// Clear any leftover IPC message safely
if (variable_global_exists("ipc_path")) {
    if (file_exists(global.ipc_path)) {
        var del_ok = file_delete(global.ipc_path);
        if (del_ok) show_debug_message("[IPC] Cleared IPC file at Start: " + string(del_ok));
    }
}

// Force a refresh on first Step; the Step event should listen for this flag
global.needs_username_refresh = true;
