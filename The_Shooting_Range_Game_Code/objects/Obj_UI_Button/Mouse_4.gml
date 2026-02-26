global.highscore = 0;

ini_open("save.ini");
ini_write_real("Scores", "HighScore", 0);
ini_close();

show_debug_message("High score reset!");
