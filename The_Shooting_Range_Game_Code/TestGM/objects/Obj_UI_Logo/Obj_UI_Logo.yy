{
  "$GMObject":"v1",
  "%Name":"Obj_UI_Logo",
  "spriteId":{"name":"Spr_UI_Elements","path":"sprites/Spr_UI_Elements/Spr_UI_Elements.yy"},
  "properties":[
    {"name":"width","value":800},
    {"name":"height","value":200},
    {"name":"positionX","value":560},
    {"name":"positionY","value":50},
    {"name":"text","value":"Welcome to the Game! Please Log In."},
    {"name":"fontId","value":{"name":"Fnt_UI","path":"fonts/Fnt_UI.yy"}},
    {"name":"textColor","value":4294967295},
    {"name":"backgroundColor","value":4278190080},
    {"name":"borderColor","value":4278190080},
    {"name":"borderThickness","value":5}
  ],
  "resourceType":"GMObject",
  "resourceVersion":"2.0",
  "events":[
    {
      "eventType":"Create",
      "code":"// Initialization code for the UI Logo\n"
    },
    {
      "eventType":"Draw",
      "code":"draw_set_color(backgroundColor);\n" +
               "draw_rectangle(positionX, positionY, positionX + width, positionY + height, false);\n" +
               "draw_set_color(borderColor);\n" +
               "draw_rectangle(positionX, positionY, positionX + width, positionY + height, true);\n" +
               "draw_set_color(textColor);\n" +
               "draw_set_font(fontId);\n" +
               "draw_text(positionX + 10, positionY + 10, text);\n"
    }
  ]
}