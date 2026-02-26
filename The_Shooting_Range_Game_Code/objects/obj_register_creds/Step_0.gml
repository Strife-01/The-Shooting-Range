// simple throttle
// if (current_time < global.gm_outbox_next_poll) exit;
// global.gm_outbox_next_poll = current_time + global.gm_outbox_poll_ms;
/// @desc Create QR renderer on room enter
if (global.qr_spawn_pending) {
    global.qr_spawn_pending = false;

    if (!instance_exists(obj_qr_renderer)) {
        var cx = room_width / 2;
        var cy = room_height / 2;

        var qr = instance_create_layer(cx, cy, "Instances_2", obj_qr_renderer);
        if (variable_instance_exists(qr, "gui_x")) qr.gui_x = display_get_gui_width() / 2;
        if (variable_instance_exists(qr, "gui_y")) qr.gui_y = display_get_gui_height() / 2;

        show_debug_message("[REGISTER] ? QR Renderer created in rm_register.");
    }
}

var info = register_read_latest();  // our existing parser; returns {ok, username, password, token}
if (is_struct(info) && info.ok) {
	
    // Prevent duplicate sends for the same QR token
    if (global.register_last_token != info.token) {
		global.reg_username = info.username;
        global.reg_password = info.password;
        var sent = register_send_cmd2(info.username, info.password, info.token);
        if (sent) {
			global.qrCodeUsed = true;
            global.register_last_token = info.token;
            show_debug_message("[REGISTER] CMD2 sent. Waiting for scanner events (enroll_progress / enrolled)...");
			
			global.reg_username = info.username;
			global.reg_password = info.password;
			// room_goto(rm_register);
        } else {
            show_debug_message("[REGISTER] Failed to write command file.");
        }
    }
}

// After scanner finishes enrollment successfully:
if (global.enroll_done && !global.reg_create_sent) {
    global.reg_create_sent = true;

    // ?? This is the function that sends to our actual Go server
    api_create_player(
        global.reg_username,
        global.reg_password,
        string(global.fp_user_id), // use fingerprint ID as hash code
        "player"
    );
}

