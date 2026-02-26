/// @desc Handle delete button click (GUI-safe)
var mx = device_mouse_x_to_gui(0);
var my = device_mouse_y_to_gui(0);

if (point_in_rectangle(mx, my, button_x, button_y, button_x + button_w, button_y + button_h)) {

    if (!is_string(global.fp_user_id) || string_length(global.fp_user_id) == 0) {
        show_debug_message("[DELETE] No scanner user_id available � cannot delete.");
        exit;
    }

    var user_id = string(global.fp_user_id);

    if (variable_global_exists("scanner_cmd_path")) {
        var cmd = "CMD5 " + user_id;
        var f = file_text_open_write(global.scanner_cmd_path);
        file_text_write_string(f, cmd);
        file_text_close(f);
        show_debug_message("[DELETE] Sent " + cmd + " to scanner.");
    } else {
        show_debug_message("[DELETE] scanner_cmd_path not set.");
    }
}
