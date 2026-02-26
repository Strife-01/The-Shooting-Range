/// @desc Keyboard/mouse selection + refresh
var count = array_length(usernames);

// Allow right-click passthrough (e.g., to go register)
if (mouse_check_button_pressed(mb_right)) {
    event_user(0);
}

// Refresh from store if requested
if (variable_global_exists("needs_username_refresh") && global.needs_username_refresh) {
    global.needs_username_refresh = false;
    var store = user_store_read();
    if (is_struct(store)) usernames = store.usernames;
    selected_ix = clamp(selected_ix, 0, max(0, array_length(usernames) - 1));
    show_debug_message("[USERNAME LIST] Live refresh done. Count: " + string(array_length(usernames)));
}

// Login bypass for testing: press Q to skip login and enter as guest
if (keyboard_check_pressed(ord("Q"))) {
    global.signin_username = "guest";
    global.login_username  = "guest";
    // Ensure no JWT/user id is present for guest
    global.jwt_user_id = "";

    // Keep numeric highscore as 0 to avoid breaking comparisons, but
    // provide a display override so UI shows '-' for guest.
    global.highscore = 0;
    global.highscore_owner = "";
    global.highscore_display_override = "-";

    global.login_prompt_text = "Login skipped (guest)";

    // Go to main room immediately as if logged-in as guest
    room_goto(rm_main);
}

// Keyboard nav
if (count > 0) {
    if (keyboard_check_pressed(vk_up))   selected_ix = max(0, selected_ix - 1);
    if (keyboard_check_pressed(vk_down)) selected_ix = min(count - 1, selected_ix + 1);

    if (selected_ix < scroll) scroll = selected_ix;
    if (selected_ix > scroll + max_rows_on_screen - 1)
        scroll = selected_ix - (max_rows_on_screen - 1);

    global.signin_username = string(usernames[selected_ix]);
}

// Mouse selection (use SAME INSTANCE VARS used for drawing)
var mx = device_mouse_x_to_gui(0);
var my = device_mouse_y_to_gui(0);

// list rect matches Draw GUI: list starts at panel_y + 20, height = rows * row_h
var list_x1 = panel_x + 20;
var list_y1 = panel_y + 20;
var list_x2 = panel_x + panel_w - 20;
var list_y2 = list_y1 + max_rows_on_screen * row_h;

if (mouse_check_button_pressed(mb_left)) {
    if (mx >= list_x1 && mx <= list_x2 && my >= list_y1 && my <= list_y2) {
        var rel = my - list_y1;
        var ix  = scroll + floor(rel / row_h);
        if (ix >= 0 && ix < count) {
            selected_ix = ix;
            global.signin_username = string(usernames[selected_ix]);
            global.login_username  = global.signin_username;

            // Reset per-user highscore display (so we fetch fresh)
            global.highscore       = 0;
            global.highscore_owner = "";
            // clear any manual display override so fetch/display logic behaves
            global.highscore_display_override = "";

            // Send CMD3 to scanner
            var sent = login_send_cmd3(global.login_username);
            if (sent) {
                global.login_prompt_text  = "Place your finger on the scanner...";
                global.login_prompt_timer = current_time + 5000;
                show_debug_message("[LOGIN] CMD3 sent for user: " + global.login_username);
            }
        }
    }
}

// Scroll wheel
var wheel = mouse_wheel_up() - mouse_wheel_down();
if (wheel != 0) {
    scroll = clamp(scroll - wheel, 0, max(0, count - max_rows_on_screen));
}
