register_init_paths();
global.gm_outbox_path = "/home/badr/gm_outbox";
global.gm_outbox_last_token = "";
global.gm_outbox_last_ts    = -1;

global.scanner_cmd_path = "/home/badr/command.json";
// guard so we don’t re-send for the same QR token
global.register_last_token = "";
global.register_seq = 0;

// polling throttle
global.gm_outbox_poll_ms   = 500;          // poll every 0.5s (tune as you like)
global.gm_outbox_next_poll = current_time; // start immediately
