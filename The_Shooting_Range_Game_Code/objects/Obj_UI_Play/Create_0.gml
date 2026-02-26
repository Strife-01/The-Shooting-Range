// Remember the final resting position
x_target = x;
y_target = y;

// Start below the target so it can pop up
y = y_target + 400;

// Start with upward velocity
velocity = -15;

// State control
rise_delay = 10;     // 60 steps = 1 second at 60fps
drop_done = false;
bounced   = false;
