/// @function jwt_extract_user_id(token) : string
/// @desc Extracts `user_id` from a JWT (no verification, just base64url decode)
function jwt_extract_user_id(token) {
    if (is_undefined(token)) return "";
    
    var dot1 = string_pos(".", token);
    if (dot1 <= 0) return "";
    
    // Get the part after the first '.'
    var rest = string_copy(token, dot1 + 1, 999999);  // big enough length
    
    var dot2 = string_pos(".", rest);
    if (dot2 <= 0) return "";
    
    // Extract payload (between 1st and 2nd dot)
    var payload = string_copy(rest, 1, dot2 - 1);

    // base64url � base64
    payload = string_replace_all(payload, "-", "+");
    payload = string_replace_all(payload, "_", "/");
    var pad = 4 - (string_length(payload) mod 4);
    if (pad < 4) payload += string_repeat("=", pad);

    // Decode payload
    var json = "";
    try { json = base64_decode(payload); } catch (_) { return ""; }

    var obj;
    try { obj = json_parse(json); } catch (_) { return ""; }

    if (is_struct(obj) && variable_struct_exists(obj, "user_id")) {
        return string(obj.user_id);
    }

    return "";
}
