y -= y_speed;
lifetime--;
if (lifetime <= 0) instance_destroy();

if (!variable_instance_exists(self, "score_value")) score_value = 0;


if (score_value >= 100) {
    color = make_color_rgb(200, 0, 255); // Purple
} else if (score_value >= 80) {
    color = make_color_rgb(255,191,0); // Gold
} else if (score_value >= 40) {
    color = make_color_rgb(192, 192, 192); // Silver
} else if (score_value >= 10) {
    color = make_color_rgb(205, 127, 50); // Bronze
} else {
    color = make_color_rgb(150,150,150); // gray for 0 points
}
