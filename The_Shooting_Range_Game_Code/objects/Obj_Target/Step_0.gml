// auto-destroy after time_to_live seconds if not shot; lose a life when this happens
if (!fading_out) {
    var alive_time = (current_time - spawn_time) / 1000; // seconds
    var ttl = (variable_instance_exists(id, "time_to_live") ? time_to_live : 5);
    if (alive_time >= ttl) {
        // too slow, show +0 popup and deduct a life
        var popup = instance_create_layer(x, y, "Instances", Obj_Score_Popup);
        popup.score_value = 0;

        // decrement player lives (initialize if needed)
        if (!variable_global_exists("lives")) global.lives = 3;
        global.lives -= 1;
        show_debug_message("[TARGET] Missed target - lives left: " + string(global.lives));

        // trigger fade/destruction
        fading_out = true;
        fade_timer = 120; // fade for 2 seconds

        // if out of lives, end game
        if (global.lives <= 0) {
            room_goto(rm_results);
        } else {
            // End the round immediately due to a life loss: remove all existing targets
            // and prevent scoring until the next round's banner has finished.
            if (variable_global_exists("round_active") && global.round_active) {
                global.round_active = false;
                global.round_clicks_disabled = true;
                // schedule next round banner
                global.round_banner_timer = current_time + (variable_global_exists("round_banner_ms") ? global.round_banner_ms : 2000);
                global.round_pending = true;
                // advance round number
                global.round_number = (variable_global_exists("round_number") ? global.round_number + 1 : 2);
                show_debug_message("[ROUND] Ending round due to life loss. Preparing round " + string(global.round_number));

                // destroy all existing targets without awarding points
                with (Obj_Target) {
                    // ensure they don't trigger popups (they're just removed)
                    instance_destroy();
                }
            }
        }
    }
}


if (fading_out) {
    fade_timer--;
    image_alpha = fade_timer / 120;

    if (fade_timer <= 0) {
        instance_destroy();
    }
}
