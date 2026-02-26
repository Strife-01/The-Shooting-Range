
// Spawn manager: supports multiple simultaneous targets, prevents overlap,
// and gradually speeds up spawn rate and reduces target TTL.
spawn_timer++;

// Round state machine:
// - round_pending == true: banner is being shown until round_banner_timer
// - round_active == true: spawning is enabled
// Start a banner if neither pending nor active
if (!variable_global_exists("round_pending")) global.round_pending = false;
if (!variable_global_exists("round_active")) global.round_active = false;

if (!global.round_active && !global.round_pending) {
    // show banner for the upcoming round
    var rstart = (variable_global_exists("round_number") ? global.round_number : 1);
    global.round_pending = true;
    global.round_banner_timer = current_time + (variable_global_exists("round_banner_ms") ? global.round_banner_ms : 2000);
    show_debug_message("[ROUND] Showing banner for round " + string(rstart));
}

// If banner finished, start the round
if (global.round_pending && current_time >= global.round_banner_timer) {
    global.round_pending = false;
    global.round_active = true;
    global.round_start_time = current_time;
    var r = (variable_global_exists("round_number") ? global.round_number : 1);
    // initialize per-round spawn & ttl
    global.spawn_interval_seconds = max(0.4, global.base_spawn_interval_seconds * power(0.9, r - 1));
    global.target_ttl_seconds = max(0.6, global.base_target_ttl_seconds * power(0.95, r - 1));
    global.spawn_decay = max(0.9, global.spawn_decay * (1 - 0.02 * (r - 1)));
    global.ttl_decay   = max(0.9, global.ttl_decay   * (1 - 0.02 * (r - 1)));
    // allow scoring again
    global.round_clicks_disabled = false;
    show_debug_message("[ROUND] Starting round " + string(r) + ", spawn_interval=" + string(global.spawn_interval_seconds) + "s, ttl=" + string(global.target_ttl_seconds) + "s");
}

// Rounds are no longer time-limited — they continue until the player loses a life.
// Round end on life loss is handled in Obj_Target, which will set round_pending
// and destroy existing targets when a life is lost.

// Only spawn when a round is active
if (global.round_active && spawn_timer >= spawn_delay) {
    // only spawn if we have fewer than max targets
    var curr = instance_number(Obj_Target);
    if (curr < global.max_targets) {
        var attempts = 0;
        var spawned = false;
        var min_dist = 120; // minimum distance between targets
        while (attempts < 12 && !spawned) {
            var spawn_x = random_range(160, 1504);
            var spawn_y = random_range(192, 736);

            // check overlap against all existing targets (robust non-overlap)
            var too_close = false;
            var existing = instance_number(Obj_Target);
            for (var ii = 0; ii < existing; ii++) {
                var inst = instance_find(Obj_Target, ii);
                if (inst != noone) {
                    if (point_distance(spawn_x, spawn_y, inst.x, inst.y) < min_dist) {
                        too_close = true;
                        break;
                    }
                }
            }

            if (!too_close) {
                var t = instance_create_layer(spawn_x, spawn_y, "Effects", Obj_Target);
                // give the target the current TTL (seconds)
                if (variable_global_exists("target_ttl_seconds")) {
                    t.time_to_live = global.target_ttl_seconds;
                } else {
                    t.time_to_live = 5;
                }
                // Log TTL for this spawn
                show_debug_message("[SPAWN] Target spawned at (" + string(spawn_x) + "," + string(spawn_y) + ") TTL=" + string(t.time_to_live) + "s (round " + string(global.round_number) + ")");
                spawned = true;
            }

            attempts += 1;
        }
    }

    // reset spawn timer and apply small randomization around the base interval
    spawn_timer = 0;
    var base_frames = (is_undefined(room_speed) ? 60 : room_speed);
    // decay spawn interval and ttl so the game gradually speeds up
    // use slightly lower minimums to allow faster pacing
    global.spawn_interval_seconds = max(0.4, global.spawn_interval_seconds * global.spawn_decay);
    global.target_ttl_seconds = max(0.6, global.target_ttl_seconds * global.ttl_decay);
    spawn_delay = irandom_range(round(global.spawn_interval_seconds * base_frames * 0.8), round(global.spawn_interval_seconds * base_frames * 1.2));
}
