// RoomCreationCode.gml
// This code initializes the sign-in room and creates the signboard for the login UI.

var signboard_width = 800; // Width of the signboard
var signboard_height = 600; // Height of the signboard
var signboard_x = (room_width - signboard_width) / 2; // Center the signboard horizontally
var signboard_y = (room_height - signboard_height) / 2; // Center the signboard vertically

// Create the signboard instance
var signboard_instance = instance_create_layer(signboard_x, signboard_y, "Instances_1", obj_signboard);
signboard_instance.width = signboard_width;
signboard_instance.height = signboard_height;

// Set the signboard sprite
signboard_instance.sprite_index = Spr_SignBoard;

// Additional initialization code for the sign-in UI can be added here.