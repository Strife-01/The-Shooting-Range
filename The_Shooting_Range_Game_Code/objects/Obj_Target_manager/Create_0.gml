
// Gameplay parameters
global.max_targets = 3;                // don't have more than this many targets on screen
global.lives = 3;                      // player lives

// Spawn/timing progression (seconds)
global.base_spawn_interval_seconds = 2.5;   // base spawn interval (seconds)
global.base_target_ttl_seconds   = 4.0;     // base per-target time-to-live (seconds)
// Current per-round spawn/ttl (will be set at round start)
global.spawn_interval_seconds = global.base_spawn_interval_seconds;
global.target_ttl_seconds     = global.base_target_ttl_seconds;
// Speed up progression: higher decay (smaller multiplier) speeds the game faster per spawn
global.spawn_decay = 0.98;             // multiplicative decay per spawn (speeds up faster)
global.ttl_decay = 0.98;               // multiplicative decay per spawn (reduces time-to-live faster)

// Round system
global.round_number = 1;
global.round_duration_seconds = 30; // seconds per round (assumption — can be tuned)
global.round_banner_ms = 2000;      // show "ROUND N" banner for 2s
global.round_active = false;
global.round_start_time = 0;
global.round_banner_timer = 0;

spawn_timer = 0;
// spawn_delay measured in frames (based on room_speed)
var base_frames = (is_undefined(room_speed) ? 60 : room_speed);
spawn_delay = irandom_range(round(global.spawn_interval_seconds * base_frames * 0.8), round(global.spawn_interval_seconds * base_frames * 1.2));
