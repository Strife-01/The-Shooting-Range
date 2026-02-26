/// @desc Handle responses from API calls
var req        = async_load[? "id"];
var net_status = async_load[? "status"];        // 0 means transport OK
var http_code  = async_load[? "http_status"];   // real HTTP status (needs modern runtime)
var result     = async_load[? "result"];

// Safely get result body if exists
var body = ds_map_exists(async_load, "result") ? async_load[? "result"] : "";


show_debug_message("[API] HTTP Async received. req=" + string(req) +
                   " net_status=" + string(net_status) +
                   " http_status=" + string(http_code));

// Create Player
if (req == global.last_api_request) {
    if (net_status == 0 && http_code == 201) {
        show_debug_message("[API] Player created successfully (201).");
        show_debug_message("[API] Body: " + result);

        var uname = string(global.reg_username);
        if (uname != "") {
            user_store_add(uname);
            show_debug_message("[API] Username '" + uname + "' saved locally.");
            global.needs_username_refresh = true;
        }
    } else {
        show_debug_message("[API] Create player failed. http=" + string(http_code) + " body=" + result);
    }
    global.last_api_request = -1;
    exit;
}

// Login Response
if (req == global.last_login_request) {
    if (http_code == 200 && net_status == 0) {
        // parse {"token": "..."}
        var token = "";
        if (is_string(body)) {
            var jj;
            try { jj = json_parse(body); } catch (_) { jj = undefined; }
            if (is_struct(jj) && variable_struct_exists(jj, "token")) {
                token = string(jj.token);
            }
        }

        if (token != "") {
            global.jwt_token = token;

            // Extract user_id from JWT payload 
            var uid = jwt_extract_user_id(token);
            global.jwt_user_id = uid;
            show_debug_message("[LOGIN] user_id from token = " + string(uid));

            if (is_string(global.login_username) && global.login_username != "") {
			    // Make absolutely sure function_exists is the built-in
			    if (is_real(script_get_name)) {
			        // Try a manual function check instead of function_exists
			        var has_fn = false;
			        try {
			            has_fn = script_exists(user_store_mark_last);
			        } catch (_) {
			            has_fn = false;
			        }

			        if (has_fn) {
			            var ok = user_store_mark_last(global.login_username);
			            show_debug_message("[USER STORE] Marked last username '" + string(global.login_username) + "' (write ok=" + string(ok) + ").");
			        } else {
			            show_debug_message("[USER STORE] WARNING: user_store_mark_last() not found.");
			        }
			    } else {
			        // absolute fallback
			        var ok = user_store_mark_last(global.login_username);
			        show_debug_message("[USER STORE] Marked last username '" + string(global.login_username) + "' (no check).");
			    }
			}
			
			// Clear cached score because user changed and then fetch
			global.highscore       = 0;
			global.highscore_owner = "";
			api_fetch_highscore_current();
            // move to your main room after successful login
            room_goto(rm_main);
        } else {
            show_debug_message("[LOGIN] ERROR: token missing in body=" + string(body));
        }
    } else {
        show_debug_message("[LOGIN] FAILED. http=" + string(http_code) + " body=" + string(body));
    }
}

// GET CURRENT USER HIGHSCORE RESPONSE
if (req == global.last_highscore_fetch_request) {
    if (net_status == 0 && http_code == 200) {
        // reuse the existing body variable already declared earlier
        var obj;
        try { obj = json_parse(body); } catch (_) { obj = undefined; }

        if (is_struct(obj) && variable_struct_exists(obj, "highscore")) {
            global.highscore       = real(obj.highscore);
            global.highscore_owner = string(global.jwt_user_id);
            // Clear any manual display override when a real score is received
            global.highscore_display_override = "";
            show_debug_message("[API] User highscore fetched: " + string(global.highscore));
        } else {
            show_debug_message("[API] Highscore parse error. Body=" + string(body));
        }
    } else {
        var b2 = ds_map_exists(async_load, "result") ? async_load[? "result"] : "";
        show_debug_message("[API] Highscore fetch failed. http=" + string(http_code) + " body=" + string(b2));
    }
    global.last_highscore_fetch_request = -1;
}


// HIGHSCORE RESPONSE
if (req == global.last_highscore_request) {
    if (http_code == 200 && net_status == 0) {
        show_debug_message("[API] Highscore update OK (200).");
        // refresh cached value for this user
        api_fetch_highscore_current();
    } else {
        var b = (ds_map_exists(async_load,"result") ? async_load[? "result"] : "no body");
        show_debug_message("[API] Highscore update FAILED. http=" + string(http_code) + " body=" + b);
    }
}






