/// @desc Initialize API globals
global.api_base = "https://the_shooting_range.strife01.me/api";

// Ensure globals exist
global.last_api_request = -1;
global.last_login_request = -1;   // POST /login
global.last_highscore_request  = -1; // highscores PUT

global.reg_create_sent  = false;
global.reg_username     = "";
global.reg_password     = "";

global.auth_token      = ""; 
global.current_user_id = ""; 

global.pending_login_username = "";

// login overlay text & timer
global.login_prompt_text  = "";
global.login_prompt_timer = 0;



// Username store boot
user_store_init();
if (!variable_global_exists("needs_username_refresh")) {
    global.needs_username_refresh = false;
}

// cache for highscore + who it belongs to
if (!variable_global_exists("highscore"))        global.highscore = 0;
if (!variable_global_exists("highscore_owner"))  global.highscore_owner = ""; // user_id whose score is loaded


global.last_highscore_fetch_request = -1; 

show_debug_message("[API] Initialized global variables.");
