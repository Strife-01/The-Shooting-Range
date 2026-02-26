if (rise_delay > 0) {
    // Wait before starting the pop-up
    rise_delay -= 1;
}
else if (!drop_done) {

    y += velocity;

    if (!bounced && y <= y_target - 30) {  
        velocity = 4;      
        bounced = true;
    }
    if (bounced && y >= y_target) {
        y = y_target;
        velocity = 0;
        drop_done = true;
        anim_time = 0;
    }

    if (!drop_done) {
        y += velocity;
    }

} else {
    // Slow down the base motion
    anim_time += 0.03;  // ↓ was 0.08

    // Bigger base motion
    var sway_y   = sin(anim_time * 0.8) * 6;  // ↑ amplitude from 3 to 6, slower freq
    var sway_x   = sin(anim_time * 0.6) * 5;  // ↑ amplitude from 2 to 5, slower freq
    var sway_rot = sin(anim_time * 0.5) * 4;  // ↑ amplitude from 2 to 4, slower freq

    // Pick new random targets less frequently (every ~1 second)
    if (anim_time mod 60 == 0) {
        sway_x_target   = random_range(-8, 8);  // ↑ wider random range
        sway_y_target   = random_range(-8, 8);
        sway_rot_target = random_range(-5, 5);
    }

    // Smoothly drift toward random targets (slightly slower lerp)
    sway_x = lerp(sway_x, sway_x_target, 0.03);
    sway_y = lerp(sway_y, sway_y_target, 0.03);
    sway_rot = lerp(sway_rot, sway_rot_target, 0.03);

    // Apply combined motion
    x = x_target + sway_x;
    y = y_target + sway_y;
    image_angle = sway_rot;
}
