// Remember the final resting position
x_target = x;
y_target = y;

// Start below the target so it can pop up
y = y_target - 256;

// Start with upward velocity
velocity = 15;

// State control
rise_delay = 0;     // 60 steps = 1 second at 60fps
drop_done = true;
bounced   = true;

// Idle sway timer
anim_time = 0;

sway_x_target = 0;
sway_y_target = 0;
sway_rot_target = 0;

