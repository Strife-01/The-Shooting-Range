// If another handler already played the shot sound this frame, skip here to avoid double-play.
if (variable_global_exists("shot_sound_played") && global.shot_sound_played) {
	// already played by click handler this frame
} else {
	// Only play gunshot sound if not currently in a reload-needed state and bullets remain
	if (!(variable_global_exists("need_reload") && global.need_reload) &&
		(variable_global_exists("shots") && variable_global_exists("max_shots") ? global.shots < global.max_shots : true)) {
		audio_play_sound(Snd_Gunshot, 1, false);
	}
}