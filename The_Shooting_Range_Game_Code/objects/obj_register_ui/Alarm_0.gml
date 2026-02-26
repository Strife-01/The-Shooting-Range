/// @desc Fade and optionally return to main menu
ui_alpha -= 0.02;
if (ui_alpha <= 0) {
    room_goto_previous(); // or your main menu room
}
