/// @desc IPC file reader + fingerprint event bridge

// Poll throttle: only proceed when our next poll time has arrived
if (current_time < ipx__next_poll_time) exit;
ipx__next_poll_time = current_time + global.ipc_poll_ms;

// Ensure the file exists
if (!file_exists(global.ipc_path)) exit;

// Read the file
var fh = file_text_open_read(global.ipc_path);
if (fh < 0) exit;

var txt = "";
while (!file_text_eof(fh)) {
    txt += file_text_read_string(fh);
    file_text_readln(fh);
}
file_text_close(fh);

// Parse JSON safely
var obj;
try {
    obj = json_parse(txt);
} catch (e) {
    show_debug_message("[IPC] JSON parse error: " + string(e.message));
    exit;
}
if (!is_struct(obj)) exit;

// Get version + event
var ver = (variable_struct_exists(obj, "version")) ? obj.version : 0;
if (!is_real(ver)) ver = real(string(ver));
var evt = "";
if (variable_struct_exists(obj, "event")) evt = string(obj.event);

// Ignore repeats: same version or same event = skip
if (ver <= global.ipc_last_ver && evt == global.ipc_last_evt) exit;

// Update tracking
global.ipc_last_ver = ver;
global.ipc_last_evt = evt;

// Show new data
show_debug_message("[IPC] NEW ver=" + string(ver) + " event=" + evt);
if (variable_struct_exists(obj, "payload")) {
    global.fp_payload = obj.payload;
    try {
        show_debug_message("[IPC] Payload: " + json_stringify(global.fp_payload));
    } catch (_) {
        show_debug_message("[IPC] Payload parse error.");
    }
} else {
    global.fp_payload = {};
}

// Handle event types
switch (evt)
{
    case "ready": {
        show_debug_message("[FP] READY ping.");
    } break;

    case "enroll_progress": {
	    var st = -1, tot = 3;
	    var msg = "";

	    if (is_struct(global.fp_payload)) {
	        if (variable_struct_exists(global.fp_payload, "step")) st = global.fp_payload.step;
	        if (variable_struct_exists(global.fp_payload, "total")) tot = global.fp_payload.total;
	        if (variable_struct_exists(global.fp_payload, "message")) msg = string(global.fp_payload.message);
	    }

	    // Save globally so UI can draw it
	    global.enroll_step = st;
	    global.enroll_total = tot;
	    global.enroll_message = msg;

	    show_debug_message("[FP] ENROLL PROGRESS " + string(st) + "/" + string(tot));
	    show_debug_message("[FP] MSG: " + msg);

	    scan_state = 2;

	    // remove the IPC file so it's not reprocessed
	    if (file_exists(global.ipc_path)) {
	        var del_ok = file_delete(global.ipc_path);
	        show_debug_message("[IPC] Cleared IPC file: " + string(del_ok));
	    }
	} break;


    case "match": {
	    var uid_hash = "";
	    var name = "";
		global.login_prompt_text  = "";
		global.login_prompt_timer = 0;
	    if (is_struct(global.fp_payload)) {
	        if (variable_struct_exists(global.fp_payload, "user_id")) uid_hash = string(global.fp_payload.user_id);
	        if (variable_struct_exists(global.fp_payload, "name"))    name = string(global.fp_payload.name);
	    }

	    // Assume you stored the clicked username into global.signin_username
	    var uname = string(global.signin_username);

	    show_debug_message("[FP] LOGIN MATCH user_id=" + uid_hash + " for username=" + uname);

	    // Kick off POST /login
	    api_login(uname, uid_hash);

	    // clear IPC file, UI, etc...
	    if (file_exists(global.ipc_path)) file_delete(global.ipc_path);
	} break;

    case "unknown": {
	    // Log payload safely
	    show_debug_message("[FP] UNKNOWN fingerprint");
	    if (is_struct(global.fp_payload)) {
	        try {
	            show_debug_message("[IPC] Payload: " + json_stringify(global.fp_payload));
	        } catch (_) {
	            show_debug_message("[IPC] Payload (non-json-stringifiable).");
	        }
	    } else {
	        show_debug_message("[IPC] No payload struct for 'unknown'.");
	    }

	    // Default message
	    var hint_msg = "Unknown fingerprint detected.";

	    // If the payload includes a hint, make it clearer
	    if (is_struct(global.fp_payload) && variable_struct_exists(global.fp_payload, "hint")) {
	        var hint = string(global.fp_payload.hint);
	        if (hint == "not_enrolled") {
	            hint_msg = "This fingerprint is not registered.";
	        } else if (hint == "no_match") {
	            hint_msg = "Fingerprint not recognized � try again.";
	        } else {
	            hint_msg = "Unknown fingerprint � " + hint;
	        }
	    }

	    // Show on-screen prompt
	    global.login_prompt_text  = hint_msg;
	    global.login_prompt_timer = current_time + 4000; // ~4 seconds

	    // Clear the IPC file so this event doesn't spam
	    if (file_exists(global.ipc_path)) {
	        var del_ok = file_delete(global.ipc_path);
	        show_debug_message("[IPC] Cleared IPC file after 'unknown': " + string(del_ok));
	    }
	} break;


    case "error": {
        var msg = "";
        if (is_struct(global.fp_payload) && variable_struct_exists(global.fp_payload, "message"))
            msg = string(global.fp_payload.message);
        show_debug_message("[FP] ERROR: " + msg);
    } break;

    case "enrolled": {
	    // Scanner payload carries the fingerprint hash (string/hex)
	    var uid = -1;
	    if (is_struct(global.fp_payload) && variable_struct_exists(global.fp_payload, "user_id")) {
	        uid = global.fp_payload.user_id; // scanner hash / ID
	    }

	    show_debug_message("[FP] ENROLLED user_id=" + string(uid));

	    // Avoid duplicate API calls if the event repeats
	    if (!variable_global_exists("reg_create_sent") || !global.reg_create_sent) {
	        global.reg_create_sent = true;

	        var access = "player";
	        // Kick API create; api_create_player should set global.last_api_request internally,
	        // or you can capture its return if it returns an id.
	        var _reqid = api_create_player(global.reg_username, global.reg_password, string(uid), access);
	        show_debug_message("[API] POST /api/players sent for " + string(global.reg_username));
	    }

	    // Mark some UI flags if you need them
	    scan_state = 1;
	    user_id = string(uid);
	    global.enroll_done = true;

	    // Clear IPC so we don't reprocess this event
	    if (file_exists(global.ipc_path)) {
	        var del_ok = file_delete(global.ipc_path);
	        show_debug_message("[IPC] Cleared IPC file: " + string(del_ok));
	    }
	} break;

    case "cleared": {
        show_debug_message("[FP] CLEARED all users");
    } break;
	
	case "userAddFailed": {
	    show_debug_message("[FP] Registration failed. Returning to sign-in menu...");

	    // delete the IPC file so it doesn't reprocess
	    if (file_exists(global.ipc_path)) {
	        var del_ok = file_delete(global.ipc_path);
	        show_debug_message("[IPC] Cleared IPC file after 'error': " + string(del_ok));
	    }

	    // reset globals
	    global.qrCodeScanned = false;
	    global.register_last_token = "";

	    // Fade out or instantly go back to rm_sign_in
		show_message_async("Registration failed. Returning to main menu...");
	    room_goto(rm_sign_in);
	} break;
	
	case "user_deleted": {
		var deleted_id = payload.user_id;
	    show_debug_message("[FP] USER DELETED id=" + deleted_id);

	    // Compare with locally stored users to find username
	    if (function_exists("user_store_remove_by_id")) {
	        var ok = user_store_remove_by_id(deleted_id);
	        show_debug_message("[USER STORE] Removed user with id " + deleted_id + " (ok=" + string(ok) + ")");
	    } else if (function_exists("user_store_remove")) {
	        // fallback: if only username is known
	        if (variable_global_exists("login_username")) {
	            user_store_remove(global.login_username);
	            show_debug_message("[USER STORE] Removed username " + string(global.login_username));
	        }
	    }

	    global.needs_username_refresh = true;
	} break;

    default: {
        show_debug_message("[FP] EVENT '" + evt + "' (no handler).");
    } break;
}

// Delete file after successful read (and small delay for sync safety)
alarm[1] = 2; // let Alarm[1] do deletion shortly
