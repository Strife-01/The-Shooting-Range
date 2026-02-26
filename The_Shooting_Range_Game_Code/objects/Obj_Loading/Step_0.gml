timer--;

if (timer <= 0 && !loaded) {
    room_goto(rm_main); 
	show_debug_message("room loaded")
	loaded = true;
}
