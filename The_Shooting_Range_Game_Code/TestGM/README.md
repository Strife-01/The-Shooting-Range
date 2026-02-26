# TestGM Project

## Overview
TestGM is a game project developed using GameMaker Studio. The project includes various rooms, objects, sprites, scripts, fonts, shaders, and audio assets to create an engaging user experience.

## Project Structure
- **rooms/**: Contains the different rooms of the game.
  - **rm_sign_in/**: The sign-in room where users can log in.
    - `rm_sign_in.yy`: Defines the layout and instances of UI elements for the sign-in process.
    - `RoomCreationCode.gml`: Contains the GML code for initializing the sign-in room.
  - **rm_gameplay/**: The gameplay room where the main game mechanics occur.
    - `rm_gameplay.yy`: Defines the gameplay room.
  - **rm_main_menu/**: The main menu room, serving as the entry point for the game.
    - `rm_main_menu.yy`: Defines the main menu layout.

- **objects/**: Contains the game objects that define behaviors and interactions.
  - `obj_signin_controller.yy`: Handles the sign-in logic and interactions.
  - `obj_Login_prompt.yy`: Displays the input fields for user credentials.
  - `obj_username_list.yy`: Manages the list of usernames.
  - `obj_signboard.yy`: Displays messages or information in the game.
  - `obj_ipc_controller.yy`: Handles inter-process communication.
  - `Obj_UI_Logo.yy`: Displays the UI logo.
  - `Obj_UI_Exit_Game.yy`: Allows users to exit the game.

- **sprites/**: Contains the graphical assets used in the game.
  - `Spr_BG.yy`: Background sprite for the rooms.
  - `Spr_SignBoard.yy`: Sprite for the signboard used in the login UI.
  - `Spr_UI_Elements.yy`: Various UI element sprites.

- **scripts/**: Contains scripts for various functionalities.
  - `scr_create_signboard.gml`: Code for creating the signboard object.
  - `scr_draw_signboard.gml`: Code for drawing the signboard on the screen.
  - `scr_ui_scaling.gml`: Code for scaling UI elements based on screen size.

- **fonts/**: Contains font definitions for UI elements.
  - `Fnt_UI.yy`: Font used for UI elements.

- **shaders/**: Contains shaders for enhancing visual effects.
  - `sh_ui_effects.yy`: Shaders used for UI effects.

- **assets/**: Contains audio assets for the game.
  - **audio/**: Audio files for UI sound effects.
    - `ui_sfx.yy`: UI sound effects.

- **project.yy**: Overall project settings and configurations.

## Features
- User-friendly sign-in interface.
- Dynamic gameplay room for engaging user experience.
- Main menu for easy navigation.
- Customizable UI elements and scaling for different resolutions.
- Audio feedback for user interactions.

## Setup Instructions
1. Open the project in GameMaker Studio.
2. Navigate to the desired room or object to modify or test.
3. Run the project to see the game in action.

## Future Improvements
- Expand the gameplay mechanics in the gameplay room.
- Enhance the sign-in UI with additional features.
- Implement more audio effects for a richer experience.