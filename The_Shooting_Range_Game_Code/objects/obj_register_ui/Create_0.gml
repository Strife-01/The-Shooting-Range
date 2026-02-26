/// @desc Initialize registration UI

// Current registration state
ui_step    = 0;
ui_total   = 3;
ui_message = "Waiting for scanner...";
ui_done    = false;

// Pulse animation setup
pulse = 1;

// Make sure global event vars exist so we don�t crash
if (!variable_global_exists("fp_event"))   global.fp_event   = "";
if (!variable_global_exists("fp_payload")) global.fp_payload = {};
