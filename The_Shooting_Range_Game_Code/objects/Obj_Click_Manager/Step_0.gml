// Allow reload anytime with right mouse button
if (mouse_check_button_pressed(mb_right)) {
    global.shots = 0;
    global.need_reload = false;
    show_debug_message("[WEAPON] Reloaded via right-click.");
}

// Reset per-frame shot-sound flag so subsequent frames can play sounds again
global.shot_sound_played = false;
