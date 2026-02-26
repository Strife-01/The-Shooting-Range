/// register_init_paths.gml
function register_init_paths() {
    show_debug_message("[Init] Setting up IPC + QR environment...");

    // 1) Ensure globals exist (fallbacks)
    if (!variable_global_exists("gm_outbox_path") || global.gm_outbox_path == "") {
        global.gm_outbox_path = "/home/badr/gm_outbox"; // keep trailing slash
    }
    if (!variable_global_exists("scanner_cmd_path") || global.scanner_cmd_path == "") {
        global.scanner_cmd_path ="/home/badr/command.json";
    }

    // 2) Create directories safely using filename_dir()
    var gm_dir  = filename_dir(global.gm_outbox_path);
    if (gm_dir == "") gm_dir = global.gm_outbox_path; // if the path is already a folder
    if (!directory_exists(gm_dir)) directory_create(gm_dir);

    var cmd_dir = filename_dir(global.scanner_cmd_path);
    if (cmd_dir == "") cmd_dir = "/home/badr/";
    if (!directory_exists(cmd_dir)) directory_create(cmd_dir);

    // 3) Ask obj_register_creds to spawn the QR renderer on first Step
    global.qr_spawn_pending = true;

    show_debug_message("[Init] IPC paths ready:\n  gm_outbox: " + global.gm_outbox_path + "\n  scanner_cmd: " + global.scanner_cmd_path);
}
