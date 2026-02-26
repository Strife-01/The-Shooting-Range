{
  "$GMRObject":"v1",
  "%Name":"obj_username_list",
  "resourceType":"GMRObject",
  "resourceVersion":"2.0",
  "spriteId":{
    "name":"Spr_SignBoard",
    "path":"sprites/Spr_SignBoard/Spr_SignBoard.yy"
  },
  "properties":[],
  "events":[
    {
      "eventType":"Create",
      "eventCode":"// Initialize the username list\nusername_list = [];\nmax_usernames = 10;\n",
      "eventVersion":"2.0"
    },
    {
      "eventType":"Draw",
      "eventCode":"// Draw the signboard background\nvar sign_x = x;\nvar sign_y = y;\nvar sign_width = 800;\nvar sign_height = 600;\n\n// Draw the signboard sprite\ndraw_sprite(spriteId.name, 0, sign_x, sign_y);\n\n// Draw the usernames\nfor (var i = 0; i < array_length(username_list); i++) {\n    var username = username_list[i];\n    draw_text(sign_x + 20, sign_y + 20 + (i * 30), username);\n}\n",
      "eventVersion":"2.0"
    }
  ],
  "name":"obj_username_list",
  "parent":null
}