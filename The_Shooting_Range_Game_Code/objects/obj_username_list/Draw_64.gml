/// @desc Draw the username picker panel (styled like login prompt)

var count = array_length(usernames);

// allow instance overrides for the select-box position/size; default to panel_* if not set
if (!variable_instance_exists(id, "select_box_x")) select_box_x = 664;
if (!variable_instance_exists(id, "select_box_y")) select_box_y = 444;
if (!variable_instance_exists(id, "select_box_w")) select_box_w = 610;
if (!variable_instance_exists(id, "select_box_h")) select_box_h = 550;

// Background box (uses instance panel_* from Create) — now use select_box_*
draw_set_alpha(0.1);
draw_set_color(c_black);
draw_rectangle(select_box_x, select_box_y, select_box_x + select_box_w, select_box_y + select_box_h, false);
draw_set_alpha(1);

// Title removed (no "Choose Your User")

draw_set_font(Font6);
draw_set_halign(fa_center);
draw_set_valign(fa_middle);
if (count == 0) {
    draw_text(select_box_x + select_box_w / 2, select_box_y + select_box_h / 2, "No local usernames yet.\nRight-click to Register.");
    exit;
}

draw_set_font(Font5);
// List
var lx = select_box_x + 20;
var ly = select_box_y + 20;

for (var r = 0; r < max_rows_on_screen; r++) {
    var ix = scroll + r;
    if (ix >= count) break;

    var yy    = ly + r * row_h + row_h / 2;
    var uname = string(usernames[ix]);

    // Highlight
    if (ix == selected_ix) {
        draw_set_alpha(0.6);
        draw_set_color(make_color_rgb(40, 80, 160));
        draw_rectangle(select_box_x + 10, yy - row_h / 2, select_box_x + select_box_w - 10, yy + row_h / 2, false);
        draw_set_alpha(1);
        draw_set_color(c_white);
    } else {
        draw_set_color(c_white);
    }

    draw_set_font(font_main);
    draw_set_halign(fa_center);
    draw_set_valign(fa_middle);
    draw_text(select_box_x + select_box_w / 2, yy, uname);
}

// Footer hint
draw_set_font(font_main);
draw_set_color(make_color_rgb(220, 220, 220));
draw_set_halign(fa_center);
draw_set_valign(fa_top);
draw_text(select_box_x + select_box_w / 2, select_box_y + select_box_h + 16,
          "Click a name to log in  |  Right-click to register");
