{
  "$GMShader":"v1",
  "%Name":"sh_ui_effects",
  "resourceType":"GMShader",
  "resourceVersion":"2.0",
  "vertexShader":"// Vertex shader code for UI effects\n\nvoid main() {\n    gl_Position = gm_MVP * gm_Position;\n    gm_Color = gm_Color;\n}",
  "fragmentShader":"// Fragment shader code for UI effects\n\nvoid main() {\n    vec4 color = gm_Color;\n    // Apply some effects like brightness or contrast\n    color.rgb *= 1.2; // Example: increase brightness\n    gl_FragColor = color;\n}"
}