// Remember the final resting position
x_target = x;
y_target = y;

// Idle sway timer
anim_time = 0;

sway_x_target = 0;
sway_y_target = 0;
sway_rot_target = 0;

if (room_previous == rm_game) {
	drop_done = true;
	bounced = true;
	rise_delay = 0
} else {
	// Start below the target so it can pop up
	y = y_target + 700;

	// Start with upward velocity
	velocity = -15;

	// State control
	rise_delay = 60;     // 60 steps = 1 second at 60fps
	drop_done = false;
	bounced   = false;
}

show_debug_message(room_previous)