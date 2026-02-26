fading_out = false;
fade_timer = 0;
surf = -1
spawn_time = current_time; // time in milliseconds since game start
// per-target time-to-live in seconds (manager sets global.target_ttl_seconds)
if (variable_global_exists("target_ttl_seconds")) {
	time_to_live = global.target_ttl_seconds;
} else {
	time_to_live = 5; // default 5s
}