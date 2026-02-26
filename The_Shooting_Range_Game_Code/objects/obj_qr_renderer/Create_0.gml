global.qr_sprite  = -1;
global.qr_token   = "";
global.qr_req_id  = -1;

alpha = 1;

// Note to Andrei: Server connection values (I'll change it later when we swtich to https)
var protocolType = "http://";
var ip_address = "145.126.74.107";
var port = ":9443";
var serverRequest = "/pair_request";

global.qr_url = protocolType + ip_address + port + serverRequest;

// Kick off the HTTP GET
var headers = ds_map_create();
ds_map_add(headers, "Accept", "application/json");
global.qr_req_id = http_request(global.qr_url, "GET", headers, "");
ds_map_destroy(headers);

show_debug_message("[QR] GET " + string(global.qr_url) + " -> id=" + string(global.qr_req_id));


// Uses a text file now to simulate the server payloa.
/*
global.qr_sprite = qr_sprite_from_file("qr_test.txt");

if (global.qr_sprite != -1) {
    qr_center_origin(global.qr_sprite);
    show_debug_message("[QR] Sprite created from Included File.");
} else {
    show_debug_message("[QR] Failed to create sprite from Included File.");
}
*/