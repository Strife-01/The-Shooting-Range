/// @desc Prepare username list panel safely and support live refresh

// Ensure store exists
user_store_init();

// IPC setup 
global.ipc_rel_dir  = "fpscan/";
global.ipc_filename = "data.json";
global.ipc_path     = "/home/badr/data.json";

// Globals
if (!variable_global_exists("needs_username_refresh")) global.needs_username_refresh = false;
if (!variable_global_exists("signin_username"))        global.signin_username        = "";

// Load usernames
var store = user_store_read();
if (is_undefined(store) || !is_struct(store)) store = { usernames: [], last: "" };

usernames   = store.usernames;
selected_ix = 0;

if (array_length(usernames) > 0) {
    var last = string(store.last);
    if (last != "") {
        var found = -1;
        for (var i = 0; i < array_length(usernames); i++) {
            if (string(usernames[i]) == last) { found = i; break; }
        }
        if (found >= 0) selected_ix = found;
    }
    global.signin_username = string(usernames[selected_ix]);
} else {
    global.signin_username = "";
}

// Visual + layout (INSTANCE VARS; used by Draw & Step)
row_h   = 50;
pad     = 14;
max_rows_on_screen = 6;

panel_w = 400;
panel_h = 60 + max_rows_on_screen * row_h + pad * 2;

// Use GUI dimensions to position. Compute ONCE here
var gui_w = display_get_gui_width();
var gui_h = display_get_gui_height() / 0.58; //sclaing the height

panel_x = (gui_w - panel_w) * 0.5;
panel_y = (gui_h - panel_h) * 0.3;

scroll  = 0;

show_debug_message("[USERNAME LIST] Initialized with " + string(array_length(usernames)) + " usernames.");
