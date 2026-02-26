{
  "$GMObject":"v1",
  "%Name":"obj_Login_prompt",
  "resourceType":"GMObject",
  "resourceVersion":"2.0",
  "spriteId":{"name":"Spr_SignBoard","path":"sprites/Spr_SignBoard/Spr_SignBoard.yy"},
  "properties":[
    {"name":"width","value":800},
    {"name":"height","value":600},
    {"name":"positionX","value":200},
    {"name":"positionY","value":200},
    {"name":"textColor","value":4294967295},
    {"name":"backgroundColor","value":4278190080},
    {"name":"borderColor","value":4294901760},
    {"name":"borderThickness","value":5},
    {"name":"fontId","value":{"name":"Fnt_UI","path":"fonts/Fnt_UI.yy"}}
  ],
  "events":[
    {
      "eventType":"Create",
      "code":"// Initialization code for the login prompt\n"
    },
    {
      "eventType":"Draw",
      "code":"draw_set_color(backgroundColor);\n"
            + "draw_rectangle(positionX, positionY, positionX + width, positionY + height, false);\n"
            + "draw_set_color(borderColor);\n"
            + "draw_rectangle(positionX, positionY, positionX + width, positionY + height, true);\n"
            + "draw_set_color(textColor);\n"
            + "draw_set_font(fontId);\n"
            + "draw_text(positionX + 20, positionY + 20, \"Login Prompt\");\n"
    }
  ]
}