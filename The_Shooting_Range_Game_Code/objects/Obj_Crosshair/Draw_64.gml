// Draw GUI event: always render crosshair on top of everything (GUI layer)
var gx = device_mouse_x_to_gui(0);
var gy = device_mouse_y_to_gui(0);

if (visible) {
    // draw sprite centered at GUI mouse coords
    draw_sprite_ext(sprite_index, image_index, gx, gy, image_xscale, image_yscale, image_angle, c_white, image_alpha);
}
