# The Shooting Range System Integration Guide

This document explains how to connect **the GameMaker game**, **the fingerprint scanner**, and the **QR pairing server**.  
It focuses on all configurable **JSON file paths**, where they’re declared, and what to change when adapting the system to your own environment.

---

## Overview of Shared Files

All three components communicate using shared JSON files:

| Purpose | Direction | Typical Path | Description |
|----------|------------|---------------|--------------|
| **Scanner Command** | Game -> Scanner | `.../command.json` | Game tells the scanner to register or log in. |
| **Scanner Events** | Scanner -> Game | `.../data.json` | Scanner reports events back to the game. |
| **QR Inbox** | QR Server -> Game | `.../gm_outbox/<token>.json` | QR server tells credentials to the game after successful pairing. |
| **Local User Store** | Game only | `.../userdata/usernames.json` | Stores cached usernames and last login info locally. |

> These paths are **user configurable**. You can rename or relocate them as long as all components agree on the same locations.

---

## Game (GameMaker)

### Paths Declared in Game

| Variable | Purpose | Declared In | Example |
|-----------|----------|--------------|----------|
| `global.ipc_path` | Path where the **game reads** scanner events. | `obj_ipc_controller(Create) & obj_username_list(Create)` | `global.ipc_path = "<YOUR_PATH>/data.json";` |
| `global.scanner_cmd_path` | Path where the **game writes** commands for the scanner. | `register_init_paths() & obj_register_creds(Game Starts)` | `global.scanner_cmd_path = "<YOUR_PATH>/command.json";` |
| `global.gm_outbox_path` | Folder where the **QR server** drops `<token>.json` credentials. | `register_init_paths() & obj_register_creds(Game Starts` | `global.gm_outbox_path = "<YOUR_PATH>/gm_outbox/";` |
| `global.user_store_dir & global.user_store_file` | Local cache for usernames and “last used” info. | `user_store_init()` | `global.user_store_dir = "<YOUR_PATH>/userdata/";`<br>`global.user_store_file = global.user_store_dir + "usernames.json";` |
| `ip_address & port` | To establish connection with QR server to load QR. | `obj_qr_renderer` | `var ip_address = "<ip_address>"; `<br>`var port = ":<port>";` |

---

### How the Game Uses These Paths

- **Reads scanner events:**  
  `obj_ipc_controller -> Step` reads from `global.ipc_path` and deletes the file after use.

- **Receives QR credentials:**  
  `obj_register_creds` uses `register_read_latest()` to scan `global.gm_outbox_path` for the newest `<token>.json`, then deletes it after using it.

- **Sends scanner commands:**  
  - Registration -> `register_send_cmd2()` -> writes CMD2 to `global.scanner_cmd_path`  
  - Login -> `login_send_cmd3()` -> writes CMD3 to `global.scanner_cmd_path`  

- **Local usernames:**  
  `user_store_*` functions read/write `global.user_store_file`.

---

## Fingerprint Scanner (Python)

The scanner **listens for commands** and **emits events** using JSON files.

### Required Paths
```python
COMMAND_FILE = "<YOUR_PATH>/command.json"
DATA_FILE = "<YOUR_PATH>/data.json"  
```
The scanner reads CMD_JSON and executes commands like:  
**CMD2:** register fingerprint  
**CMD3:** login attempt

It writes DATA_JSON with events such as:  
{ "event": "match", "payload": { "user_id": "abcd1234", "name": null } }  
{ "event": "unknown", "payload": { "hint": "not_enrolled" } }  
{ "event": "enrolled", "payload": { "user_id": "abcd1234" } }  
{ "event": "user_deleted", "payload": { "user_id": "abcd1234" } }

## Running the Fingerprint Scanner
### 1. Set your paths
Edit the script and set:
```python
COMMAND_FILE = "<YOUR_PATH>/command.json"  
DATA_FILE = "<YOUR_PATH>/data.json" 
```
### 2. Start the scanner
```bash
python3 scanner.py
```
### 3. How it works
The scanner watches command.json for new commands
When an action is processed, it writes a response to data.json

## QR Pairing Server 

The **QR Pairing Server** handles the login handshake between a player’s phone and the game.  
It generates a unique **token**, encrypts credentials via RSA, and drops them into a JSON file that the game later reads.

---

### Configuration Variables

| Variable | Description | Default | Example |
|-----------|--------------|----------|----------|
| `OUTBOX_DIR` | Directory where the server saves `<token>.json` files for the game to read. Must match the game’s `global.gm_outbox_path`. | `./gm_outbox` | `OUTBOX_DIR=".../gm_outbox"` |
| `PORT` | Port the Flask server listens on. | `9443` | `PORT=9443` |
| `PAIR_TTL_SECONDS` | How long a token stays valid before expiring. | `60` | `PAIR_TTL_SECONDS=60` |

---

### Example Output Files

When a player scans a QR code and submits their credentials, the server writes a JSON file like:  
- gm_outbox/123456.json  

**Contents:**
```json
{
  "token": "123456",
  "username": "PlayerOne",
  "password": "hunter2",
  "ts": 1762106420
}
```
The game automatically detects and deletes this file once processed. 

## Running the Server

### 1. Install dependencies
```bash
pip install flask cryptography qrcode[pil]
```
### 2. Set your environment variables
```python
export OUTBOX_DIR=".../gm_outbox"  
export PORT=9443
```
### 3. Start the server
```bash
python3 qr_server.py
```
### 4. Access the pairing page

The game requests: GET /pair_request that generates QR code  
Player scans the QR, browser opens: `/pair?token=XXXXXX`


## Troubleshooting

These are the list of the most common issues we've faced when integrating the **Game**, **Scanner**, and **QR Server**.
| Issue | Likely Cause | Fix |
|-------|--------------|-----|
| Game never receives scanner events | `global.ipc_path` does **not** match `DATA_FILE` | Make sure both point to the same `data.json` file |
| Scanner does nothing when pressing Register/Login | Game is **not writing** `command.json` | Check that `register_send_cmd2()` / `login_send_cmd3()` are being called |
| `command.json` stays forever and is not deleted | Scanner script never processed the command | Ensure scanner is running and watching the correct path |
| QR register never appears in game | `OUTBOX_DIR` (server) ≠ `global.gm_outbox_path` (game) | Both must reference the *same* folder |
| `<token>.json` stays in folder and is not removed | Game never detected or processed the QR result | Check `register_read_latest()` logic in GameMaker |
| QR page loads in browser but shows "invalid token" | Game requested a token but timer expired (`PAIR_TTL_SECONDS`) | Increase TTL or scan QR faster |
| Game crashes on startup due to missing files | Required folders not created | Ensure `userdata/`, `gm_outbox/`, ipc paths exist before running |
| Scanner reports `"unknown"` even for enrolled users | Fingerprint DB wiped or mismatched | Re-enroll user or check scanner storage path |
| Game sees wrong username after login | `usernames.json` cached old data | Delete file or call `user_store_reset()` |
| IP/Port error when loading QR in GameMaker | `ip_address` or `port` variables incorrect | Match QR server host + port exactly |
| QR server starts but never writes `.json` files | `OUTBOX_DIR` not writable or missing | Create folder and ensure write permissions |

### 🙏 When All Else Fails... 

If the game won’t talk to the scanner,  
and the QR server refuses to cooperate,  
and JSON files are not appearing like ghosts...  
**…then at this point, just close your eyes and pray to GOD (or Stackoverflow users)** 

Because sometimes even sudo can’t save you. 
---
