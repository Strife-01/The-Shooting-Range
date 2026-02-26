// stop if out of ammo; set reload flag so UI can prompt
if (global.shots >= global.max_shots) {
    global.need_reload = true;
    exit;
}


var target = instance_nearest(mouse_x, mouse_y, Obj_Target)

if (instance_exists(target) && !target.fading_out) {

    // If scoring is disabled (between rounds), ignore clicks on targets
    if (variable_global_exists("round_clicks_disabled") && global.round_clicks_disabled) {
        exit;
    }

    // Only count as a hit if the click actually landed on the target's mask/sprite
    if (collision_point(mouse_x, mouse_y, target, true, false)) {

        var dist = point_distance(mouse_x, mouse_y, target.x, target.y);
        var max_dist = 150;
        dist = clamp(dist, 0, max_dist);
        var points = round(100 * (1 - dist / max_dist));

        // time factor scaled to the target's time_to_live (1 at spawn, 0 at expiry)
        var alive_time = (current_time - target.spawn_time) / 1000; // in seconds
        var ttl = (variable_instance_exists(target, "time_to_live") ? target.time_to_live : 5);
        var time_factor = clamp(1 - (alive_time / ttl), 0, 1);

        var total_points = round(points * time_factor);

        global.score += total_points;
        // Play gunshot sound before decrementing/incrementing the shot counter so the
        // final bullet sound always plays regardless of event ordering.
        if (!(variable_global_exists("need_reload") && global.need_reload) &&
            (variable_global_exists("shots") && variable_global_exists("max_shots") ? global.shots < global.max_shots : true)) {
            audio_play_sound(Snd_Gunshot, 1, false);
            global.shot_sound_played = true;
        }
        global.shots += 1;

        var popup = instance_create_layer(target.x, target.y, "Instances", Obj_Score_Popup);
        popup.score_value = total_points;

        if (surface_exists(target.surf)) {
            var local_x = mouse_x - (target.x - target.sprite_width * 0.5);
            var local_y = mouse_y - (target.y - target.sprite_height * 0.5);

            surface_set_target(target.surf);
            gpu_set_blendmode(bm_normal);
            draw_sprite_ext(
                spr_bullet_hole,
                irandom(image_number - 1),
                local_x,
                local_y,
                random_range(0.4, 0.6),
                random_range(0.4, 0.6),
                irandom(360),
                c_white,
                1
            );
            gpu_set_blendmode(bm_normal);
            surface_reset_target();
        }

        // mark the target as shot and freeze its timer bar at current pct
        var pct = time_factor; // fraction 1..0
        with (target) {
            shot = true;
            shot_pct = pct;
            fading_out = true;
            fade_timer = 120;
        }

        if (global.shots >= global.max_shots) {
            global.need_reload = true;
            show_debug_message("[WEAPON] Out of ammo - press the button on the back of the gun to reload.");
        }

    } else {
        // Missed the target (click not on target). Count as a shot but do not remove the target.
        // Missed the target: play sound before counting the shot
        if (!(variable_global_exists("need_reload") && global.need_reload) &&
            (variable_global_exists("shots") && variable_global_exists("max_shots") ? global.shots < global.max_shots : true)) {
            audio_play_sound(Snd_Gunshot, 1, false);
            global.shot_sound_played = true;
        }
        global.shots += 1;
        if (global.shots >= global.max_shots) {
            global.need_reload = true;
            show_debug_message("[WEAPON] Out of ammo - press the button on the back of the gun to reload.");
        }
    }

} else {
    // No target found under the cursor: regular miss
    // No target found under the cursor: regular miss. Play sound first, then count the shot.
    if (!(variable_global_exists("need_reload") && global.need_reload) &&
        (variable_global_exists("shots") && variable_global_exists("max_shots") ? global.shots < global.max_shots : true)) {
        audio_play_sound(Snd_Gunshot, 1, false);
        global.shot_sound_played = true;
    }
    global.shots += 1;
    if (global.shots >= global.max_shots) {
        global.need_reload = true;
        show_debug_message("[WEAPON] Out of ammo - press the button on the back of the gun to reload.");
    }
}