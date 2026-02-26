{
  "$GMRoom": "v1",
  "%Name": "rm_main_menu",
  "creationCodeFile": "rooms/rm_main_menu/RoomCreationCode.gml",
  "inheritCode": false,
  "inheritCreationOrder": false,
  "inheritLayers": false,
  "instanceCreationOrder": [],
  "isDnd": false,
  "layers": [
    {
      "$GMRInstanceLayer": "",
      "%Name": "Background",
      "depth": 0,
      "effectEnabled": true,
      "effectType": null,
      "gridX": 32,
      "gridY": 32,
      "hierarchyFrozen": false,
      "inheritLayerDepth": false,
      "inheritLayerSettings": false,
      "inheritSubLayers": true,
      "inheritVisibility": true,
      "instances": [
        {
          "$GMRInstance": "v4",
          "%Name": "inst_background",
          "colour": 4294967295,
          "frozen": false,
          "hasCreationCode": false,
          "ignore": false,
          "imageIndex": 0,
          "imageSpeed": 1.0,
          "inheritCode": false,
          "inheritedItemId": null,
          "inheritItemSettings": false,
          "isDnd": false,
          "name": "inst_background",
          "objectId": {
            "name": "Spr_BG",
            "path": "sprites/Spr_BG/Spr_BG.yy"
          },
          "properties": [],
          "resourceType": "GMRInstance",
          "resourceVersion": "2.0",
          "rotation": 0.0,
          "scaleX": 1.0,
          "scaleY": 1.0,
          "x": 0,
          "y": 0
        }
      ],
      "layers": [],
      "name": "Background",
      "properties": [],
      "resourceType": "GMRInstanceLayer",
      "resourceVersion": "2.0",
      "userdefinedDepth": false,
      "visible": true
    },
    {
      "$GMRInstanceLayer": "",
      "%Name": "Signboard Layer",
      "depth": 100,
      "effectEnabled": true,
      "effectType": null,
      "gridX": 32,
      "gridY": 32,
      "hierarchyFrozen": false,
      "inheritLayerDepth": true,
      "inheritLayerSettings": false,
      "inheritSubLayers": true,
      "inheritVisibility": true,
      "instances": [
        {
          "$GMRInstance": "v4",
          "%Name": "inst_signboard",
          "colour": 4294967295,
          "frozen": false,
          "hasCreationCode": false,
          "ignore": false,
          "imageIndex": 0,
          "imageSpeed": 1.0,
          "inheritCode": false,
          "inheritedItemId": null,
          "inheritItemSettings": false,
          "isDnd": false,
          "name": "inst_signboard",
          "objectId": {
            "name": "obj_signboard",
            "path": "objects/obj_signboard/obj_signboard.yy"
          },
          "properties": [],
          "resourceType": "GMRInstance",
          "resourceVersion": "2.0",
          "rotation": 0.0,
          "scaleX": 2.0,
          "scaleY": 1.5,
          "x": 960,
          "y": 540
        }
      ],
      "layers": [],
      "name": "Signboard Layer",
      "properties": [],
      "resourceType": "GMRInstanceLayer",
      "resourceVersion": "2.0",
      "userdefinedDepth": false,
      "visible": true
    }
  ],
  "name": "rm_main_menu",
  "parent": {
    "name": "Rooms",
    "path": "folders/Rooms.yy"
  },
  "parentRoom": null,
  "physicsSettings": {
    "inheritPhysicsSettings": false,
    "PhysicsWorld": false,
    "PhysicsWorldGravityX": 0.0,
    "PhysicsWorldGravityY": 10.0,
    "PhysicsWorldPixToMetres": 0.1
  },
  "resourceType": "GMRoom",
  "resourceVersion": "2.0",
  "roomSettings": {
    "Height": 1080,
    "inheritRoomSettings": false,
    "persistent": false,
    "Width": 1920
  },
  "sequenceId": null,
  "views": [],
  "viewSettings": {
    "clearDisplayBuffer": true,
    "clearViewBackground": false,
    "enableViews": false,
    "inheritViewSettings": false
  },
  "volume": 1.0
}