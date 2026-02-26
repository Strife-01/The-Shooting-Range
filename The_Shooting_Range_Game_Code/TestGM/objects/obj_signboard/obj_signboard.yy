{
  "$GMObject":"v1",
  "%Name":"obj_signboard",
  "spriteId":{"name":"Spr_SignBoard","path":"sprites/Spr_SignBoard/Spr_SignBoard.yy"},
  "resourceType":"GMObject",
  "resourceVersion":"2.0",
  "properties":{
    "depth":0,
    "visible":true,
    "persistent":false,
    "collisionEnabled":true,
    "solid":false,
    "inheritCode":false,
    "inheritSettings":false
  },
  "events":[
    {
      "eventType":"Create",
      "code":"// Initialization code for the signboard\n\n// Set the size of the signboard\nwidth = 800;\nheight = 600;\n\n// Position the signboard\nx = (room_width - width) / 2;\ny = (room_height - height) / 2;\n\n// Additional properties can be set here"
    },
    {
      "eventType":"Draw",
      "code":"// Draw the signboard\n\n// Draw the sprite\ndraw_sprite(spriteId, 0, x, y);\n\n// Draw additional text or information on the signboard\nvar text = \"Welcome to the Sign-In!\";\ndraw_set_font(Fnt_UI);\ndraw_set_color(c_black);\ndraw_text(x + 20, y + 20, text);"
    }
  ]
}