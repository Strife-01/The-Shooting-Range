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
    }

    if (!drop_done) {
        y += velocity;
    }

} 