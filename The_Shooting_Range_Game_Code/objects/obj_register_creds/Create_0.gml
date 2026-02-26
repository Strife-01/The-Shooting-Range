if (!variable_global_exists("fp_user_id")) global.fp_user_id = "";
alarm[0] = 1; // delay a frame so everything is initialized

// Variables for the parsed login data
global.login_username = "";
global.login_password = "";
global.login_token    = "";
global.login_ready    = false;

global.qrCodeUsed = false;

