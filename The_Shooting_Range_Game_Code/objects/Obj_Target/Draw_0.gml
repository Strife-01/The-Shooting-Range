// make a surface the same size as the sprite the first time we draw
if (surf == -1) {
    surf = surface_create(sprite_width, sprite_height);
    surface_set_target(surf);
    draw_clear_alpha(c_black, 0); 
    surface_reset_target();
}


// draw the target normally
draw_self()

// draw its surface of bullet holes over it
if (surface_exists(surf)) {
    draw_surface_ext(
        surf,
        x - sprite_width * 0.5,
        y - sprite_height * 0.5,
        image_xscale,
        image_yscale,
        image_angle,
        c_white,
        image_alpha
    );
}

// draw a small timer bar above the target showing remaining time
var ttl = (variable_instance_exists(id, "time_to_live") ? time_to_live : 5);
var alive = (current_time - spawn_time) / 1000;
var remaining = max(0, ttl - alive);
var pct = clamp(remaining / ttl, 0, 1);

// If the target has been shot, freeze pct at the moment it was shot
if (variable_instance_exists(id, "shot") && shot) {
    if (variable_instance_exists(id, "shot_pct")) {
        pct = shot_pct;
    }
}
var bar_w = sprite_width;
var bar_h = 8;
var bx1 = x - bar_w * 0.5;
var by1 = y - sprite_height * 0.5 - 12 - bar_h;
var bx2 = bx1 + bar_w;
var by2 = by1 + bar_h;

// background
draw_set_color(c_black);
draw_rectangle(bx1 - 1, by1 - 1, bx2 + 1, by2 + 1, false);
draw_set_color(c_grey);
draw_rectangle(bx1, by1, bx2, by2, false);

// fill according to pct (left->right) and color-shift from green->red as it approaches 0
var gcol_r1 = 76; var gcol_g1 = 175; var gcol_b1 = 80;   // green
var gcol_r2 = 244; var gcol_g2 = 67;  var gcol_b2 = 54;   // red
var mix = 1 - pct; // 0 => green, 1 => red
var cr = round(gcol_r1 + (gcol_r2 - gcol_r1) * mix);
var cg = round(gcol_g1 + (gcol_g2 - gcol_g1) * mix);
var cb = round(gcol_b1 + (gcol_b2 - gcol_b1) * mix);
draw_set_color(make_color_rgb(cr, cg, cb));
draw_rectangle(bx1, by1, bx1 + bar_w * pct, by2, false);
