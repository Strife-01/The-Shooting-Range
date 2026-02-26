show_debug_message("[SIGNIN] Right-click detected going to registration room.");

// set flag so QR will be created in next room
global.want_register_qr = true;

// go to registration room
room_goto(rm_register);