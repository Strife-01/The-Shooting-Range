/// @desc Handle right-click cancel on QR registration screen

if (mouse_check_button_pressed(mb_right)) {
    show_debug_message("[REGISTER] Right-click detected returning to sign-in...");

    // Optional cleanup of IPC/QR state
    if (file_exists(global.ipc_path)) {
        var del_ok = file_delete(global.ipc_path);
        show_debug_message("[REGISTER] IPC file cleared on cancel: " + string(del_ok));
    }

    global.qrCodeScanned = false;
    global.register_last_token = "";
    global.reg_create_sent = false;

    room_goto(rm_sign_in);
}
