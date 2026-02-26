/// @desc Delayed deletion to avoid race with writer
if (file_exists(global.ipc_path)) {
    var del_ok = file_delete(global.ipc_path);
    if (del_ok) show_debug_message("[IPC] Cleared IPC file: " + string(del_ok));
}
