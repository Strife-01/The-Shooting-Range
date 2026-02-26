// initialize all required paths
register_init_paths();

// check if QR should be created
if (variable_global_exists("want_register_qr") && global.want_register_qr) {
    show_debug_message("[REGISTER] Preparing to create QR renderer...");
    global.want_register_qr = false; // reset the flag

    // delay creation to first frame
    global.qr_spawn_pending = true;
}