// obj_Results_Controller - wherever you finalize results
if (is_real(global.score)) {
    // If player is a guest (bypass), don't send highscore to the API
    var uname = "";
    if (variable_global_exists("login_username")) uname = string(global.login_username);
    if (!(uname == "guest" || uname == "Guest" || uname == "GUEST")) {
        api_send_highscore(global.score);
    } else {
        show_debug_message("[RESULTS] Guest play detected - skipping highscore upload.");
    }
}

if (global.score > global.highscore) {
    global.highscore = global.score;

    ini_open("save.ini");
    ini_write_real("Scores", "HighScore", global.highscore);
    ini_close();
}

global.shots = 0;
