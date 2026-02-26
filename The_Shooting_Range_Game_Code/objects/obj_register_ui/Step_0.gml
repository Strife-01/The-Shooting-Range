/// @desc Update registration progress and UI text from scanner events

// Don't run if IPC controller hasnt created the globals
if (!variable_global_exists("fp_event")) exit;

// Pulse animation (gentle breathing)
pulse = 0.95 + 0.05 * sin(current_time / 200);

// --- Handle scanner events ---
switch (global.fp_event)
{
    case "enroll_progress":
        if (is_struct(global.fp_payload)) {
            if (variable_struct_exists(global.fp_payload, "step"))
                ui_step = global.fp_payload.step;
            if (variable_struct_exists(global.fp_payload, "total"))
                ui_total = global.fp_payload.total;
            if (variable_struct_exists(global.fp_payload, "message"))
                ui_message = global.fp_payload.message;
        }
    break;

    case "enrolled":
        ui_done = true;
        ui_message = "Registration complete!";
        global.fp_event = ""; // reset after handling
    break;

    case "error":
        ui_message = "? " + string(global.fp_payload.message);
        ui_done = false;
        global.fp_event = ""; // reset after showing error
    break;
}
