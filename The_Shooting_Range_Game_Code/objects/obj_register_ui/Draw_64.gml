/// @desc Draw registration progress and messages
if(!global.qrCodeUsed){
	exit;
}
draw_set_font(font_main);
draw_set_halign(fa_center);
draw_set_valign(fa_middle);
draw_set_colour(c_black);

var cx = display_get_gui_width() / 2;
var cy = display_get_gui_height() / 2 + 360;

draw_text(display_get_gui_width()/2, display_get_gui_height() - 40, "Right-click to cancel registration");


// --- Hand sprite (animated) ---
if (sprite_exists(fingerprint_scanning)) {
    draw_sprite_ext(
        fingerprint_scanning,
        0,
        1024,
        192,
        pulse,
        pulse,
        -53.169,
        c_white,
        1
    );
}
// Title
draw_text_transformed(cx, cy - 120, "Fingerprint Registration", 1.4, 1.4, 0);

// Step info
if (variable_global_exists("enroll_step") && variable_global_exists("enroll_total")) {
    var st = global.enroll_step;
    var tot = global.enroll_total;
    var prog_txt = "Step " + string(st) + " / " + string(tot);
    draw_text_transformed(cx, cy - 50, prog_txt, 1.2, 1.2, 0);
}

// Main scanner message
if (variable_global_exists("enroll_message")) {
    draw_text_transformed(cx, cy + 20, string(global.enroll_message), 1.2, 1.2, 0);
}

// Optional progress bar
if (variable_global_exists("enroll_step") && variable_global_exists("enroll_total")) {
    var pct = global.enroll_step / max(1, global.enroll_total);
    var bar_w = 280;
    var bar_h = 14;
    draw_rectangle_colour(
	    cx - bar_w/2, cy + 100,
	    cx - bar_w/2 + bar_w * pct, cy + 60 + bar_h,
	    c_lime, c_lime, c_lime, c_lime,
	    false
	);
}