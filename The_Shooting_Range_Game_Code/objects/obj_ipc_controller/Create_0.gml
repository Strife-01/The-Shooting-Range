// Keep IPC inside the game's working directory so the runner/debugger can access it.
// External writer should write into this folder (working_directory + ipc_rel_dir).
global.ipc_rel_dir  = "fpscan/"; // folder under working_directory
global.ipc_filename = "data.json"; // IPC file name
global.ipc_poll_ms  = 200; // poll every 200ms
global.ipc_last_ver = -1; // last processed version

// Ensure all global state variables exist
global.enroll_done     = false;
global.reg_create_sent = false;
global.want_register_qr = false;
global.register_last_token = "";
global.reg_username    = "";
global.reg_password    = "";


// working_directory points to: /home/badr/GameMakerStudio2/vm/TestGM/assets/
global.ipc_path = "/home/badr/" + global.ipc_filename

// Throttle timer
ipx__next_poll_time = current_time; // ms since game start

show_debug_message("[IPC] File-polling mode active. Path: " + global.ipc_path);

scan_state = 0;        // 0 = waiting, 1 = success
user_id = "";
message_wait = "Please scan finger";