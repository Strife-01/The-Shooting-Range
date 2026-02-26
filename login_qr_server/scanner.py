#!/usr/bin/env python3
# -*- coding:utf-8 -*-

import serial
import time
import threading
import sys
import RPi.GPIO as GPIO
# ### IPC + OS (filesystem-only)
import json, os
from pathlib import Path
import uuid
import hashlib

# ### IPC + OS (filesystem-only)
import json, os
from pathlib import Path

TRUE = 1
FALSE = 0

# Response codes
ACK_SUCCESS = 0x00
ACK_FAIL    = 0x01
ACK_FULL    = 0x04
ACK_NO_USER = 0x05
ACK_TIMEOUT = 0x08
ACK_GO_OUT  = 0x0F

USER_MAX_CNT = 1000

# Commands
CMD_HEAD    = 0xF5
CMD_TAIL    = 0xF5
CMD_ADD_1   = 0x01
CMD_ADD_2   = 0x02
CMD_ADD_3   = 0x03
CMD_DEL     = 0x04   
CMD_MATCH   = 0x0C
CMD_DEL_ALL = 0x05
CMD_USER_CNT= 0x09
CMD_COM_LEV = 0x28

Finger_WAKE_Pin = 23
Finger_RST_Pin  = 24

GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)
GPIO.setup(Finger_WAKE_Pin, GPIO.IN)
GPIO.setup(Finger_RST_Pin, GPIO.OUT)
GPIO.setup(Finger_RST_Pin, GPIO.OUT, initial=GPIO.HIGH)

g_rx_buf = []

# Mapping: ID to Name
user_names = {}

# Serial port (hardware UART on Pi)
ser = serial.Serial("/dev/ttyAMA0", 19200, timeout=1)

# IPC CONFIG (filesystem only)
DATA_FILE = Path("~/GameMakerStudio2/vm/TestGM/assets/fpscan/data.json")
DATA_FILE.parent.mkdir(parents=True, exist_ok=True)

COMMAND_FILE = Path("~/GameMakerStudio2/vm/TestGM/assets/fpscan/command.json")

# Monotonic version generator 
_last_version = 0
def _next_version() -> int:
    global _last_version
    now = int(time.time() * 1000)
    if now <= _last_version:
        _last_version += 1
    else:
        _last_version = now
    return _last_version

def _atomic_write_json(path: Path, obj: dict):
    tmp = path.with_suffix(".tmp")
    with tmp.open("w", encoding="utf-8") as f:
        json.dump(obj, f, ensure_ascii=False, separators=(",", ":"))
        f.flush()
        os.fsync(f.fileno())
    os.replace(tmp, path)  # atomic on Linux

def publish_event(kind: str, payload: dict):
    """
    kind: 'match' | 'unknown' | 'enrolled' | 'cleared' | 'error' | 'ready'
    payload: dict with details (user_id, name, message, etc.)
    """
    evt = {
        "version": _next_version(),   # strictly increasing
        "ts": time.time(),
        "event": kind,
        "payload": payload or {},
    }
    try:
        _atomic_write_json(DATA_FILE, evt)
        print(f"[IPC] Wrote event to {DATA_FILE}: {evt}")
    except Exception as e:
        print(f"[IPC] WRITE FAILED: {e}")

#***************************************************************************
# @brief send a command, and wait for the response
#***************************************************************************
def TxAndRxCmd(command_buf, rx_bytes_need, timeout):
    global g_rx_buf
    CheckSum = 0
    tx_buf = []

    tx_buf.append(CMD_HEAD)
    for byte in command_buf:
        tx_buf.append(byte)
        CheckSum ^= byte
    tx_buf.append(CheckSum)
    tx_buf.append(CMD_TAIL)

    ser.reset_input_buffer()
    ser.write(bytes(tx_buf))

    g_rx_buf = []
    start = time.time()
    while time.time() - start < timeout and len(g_rx_buf) < rx_bytes_need:
        if ser.in_waiting > 0:
            g_rx_buf += list(ser.read(ser.in_waiting))

    if len(g_rx_buf) != rx_bytes_need:
        return ACK_TIMEOUT
    if g_rx_buf[0] != CMD_HEAD or g_rx_buf[-1] != CMD_TAIL:
        return ACK_FAIL

    # Debug raw packet
    print("RX:", [hex(x) for x in g_rx_buf])

    return ACK_SUCCESS

#***************************************************************************
def SetCompareLevel(level):
    global g_rx_buf
    command_buf = [CMD_COM_LEV, 0, level, 0, 0]
    r = TxAndRxCmd(command_buf, 8, 0.1)
    if r == ACK_SUCCESS and g_rx_buf[4] == ACK_SUCCESS:
        return g_rx_buf[3]
    return r


#***************************************************************************
def GetUserCount():
    global g_rx_buf
    command_buf = [CMD_USER_CNT, 0, 0, 0, 0]
    r = TxAndRxCmd(command_buf, 8, 0.2)
    if r == ACK_SUCCESS and g_rx_buf[4] == ACK_SUCCESS:
        return g_rx_buf[3]
    return 0


#***************************************************************************
def AddUser():
    global g_rx_buf

    publish_event("enroll_progress", {"step": 0, "total": 3, "message": "Starting enrollment..."})

    try:
        count = GetUserCount()
        if count >= USER_MAX_CNT:
            publish_event("userAddFailed", {"message": "Enroll failed: Library is full"})
            return ACK_FULL

        next_id = count + 1

        # Step 1
        publish_event("enroll_progress", {"step": 1, "total": 3, "message": "Place your finger on the sensor"})
        r = TxAndRxCmd([CMD_ADD_1, 0, next_id, 3, 0], 8, 15)
        if r != ACK_SUCCESS or g_rx_buf[4] != ACK_SUCCESS:
            publish_event("userAddFailed", {"message": "Enroll step 1 failed"})
            return ACK_FAIL
        time.sleep(1)

        # Step 2
        publish_event("enroll_progress", {"step": 2, "total": 3, "message": "Place the same finger again"})
        r = TxAndRxCmd([CMD_ADD_2, 0, next_id, 3, 0], 8, 6)
        if r != ACK_SUCCESS or g_rx_buf[4] != ACK_SUCCESS:
            publish_event("userAddFailed", {"message": "Enroll step 2 failed"})
            return ACK_FAIL
        time.sleep(1)

        # Step 3
        publish_event("enroll_progress", {"step": 3, "total": 3, "message": "Place the finger one last time"})
        r = TxAndRxCmd([CMD_ADD_3, 0, next_id, 3, 0], 8, 6)
        if r != ACK_SUCCESS or g_rx_buf[4] != ACK_SUCCESS:
            publish_event("userAddFailed", {"message": "Enroll step 3 failed"})
            return ACK_FAIL

        # Success report ONLY the scanner's numeric ID
        new_id = GetUserCount()
        hashed_id = bytes(f"{new_id}", "utf-8")
        secureId = hashlib.sha256(hashed_id).hexdigest()
        publish_event("enrolled", {"user_id": secureId})
        return ACK_SUCCESS

    except Exception as e:
        publish_event("userAddFailed", {"message": f"Enroll failed: {e}"})
        return ACK_FAIL

#***************************************************************************
def ClearAllUser():
    global g_rx_buf
    print("\n[ClearAllUser] Function called. Sending command to sensor...")
    command_buf = [CMD_DEL_ALL, 0, 0, 0, 0]
    
    # This function (TxAndRxCmd) will print the "RX: [...]" packet
    r = TxAndRxCmd(command_buf, 8, 5)

    # DEBUGGING: Check the first part of the failure
    if r != ACK_SUCCESS:
        print(f"[ClearAllUser] FAILED: The command 'TxAndRxCmd' failed.")
        if r == ACK_TIMEOUT:
            print("[ClearAllUser] Reason: The sensor timed out and did not respond.")
        else:
            print(f"[ClearAllUser] Reason: Got bad packet data. Code: {hex(r)}")
        
        publish_event("error", {"message": "Clear all failed: TX/RX Error"})
        return ACK_FAIL

    # DEBUGGING: Check the second part of the failure
    sensor_ack_code = g_rx_buf[4] # This is the sensor's internal status
    if sensor_ack_code != ACK_SUCCESS:
        print(f"[ClearAllUser] FAILED: The sensor received the command but REJECTED it.")
        print(f"[ClearAllUser] Reason: Sensor returned error code: {hex(sensor_ack_code)}")
        
        publish_event("error", {"message": f"Clear all failed: Sensor Error {hex(sensor_ack_code)}"})
        return ACK_FAIL

    # If we get here, both checks passed!
    print("[ClearAllUser] SUCCESS: Sensor confirmed deletion.")
    user_names.clear()
    publish_event("cleared", {})
    return ACK_SUCCESS
#***************************************************************************
def VerifyUser(max_time=8):
    """Try to verify a fingerprint for up to max_time seconds"""
    global g_rx_buf
    start = time.time()

    while time.time() - start < max_time:
        command_buf = [CMD_MATCH, 0, 0, 0, 0]
        r = TxAndRxCmd(command_buf, 8, 1)  # expect 8-byte response

        if r != ACK_SUCCESS:
            continue

        user_id = (g_rx_buf[2] << 8) | g_rx_buf[3]
        result  = g_rx_buf[4]

        if result == 0x03:  # Match success code
            if user_id in user_names:
                name = user_names[user_id]
                print("Matching successful! Hello, %s (ID %d)." % (name, user_id))
            else:
                name = None
                print("Matching successful! (ID %d, no name assigned)" % user_id)
            
            hashed_id = bytes(f"{user_id}", "utf-8")
            secureId = hashlib.sha256(hashed_id).hexdigest()
            publish_event("match", {"user_id": secureId, "name": name})
            return ACK_SUCCESS

        elif result == ACK_NO_USER:
            print("This fingerprint is not in the library.")
            publish_event("unknown", {"hint": "not_enrolled"})
            return ACK_NO_USER

        elif result == ACK_TIMEOUT:
            continue  # retry

        elif result == ACK_GO_OUT:
            print("Finger misaligned, please adjust...")
            time.sleep(0.5)
            continue

    print("Failed: No match after %.1f seconds." % max_time)
    return ACK_TIMEOUT


#***************************************************************************
def ListUsers():
    if not user_names:
        print("No users enrolled yet.")
    else:
        print("Registered users:")
        for uid, name in user_names.items():
            print("  ID %d : %s" % (uid, name))


#***************************************************************************
# Delete a single user
def DeleteUser(user_id):
    print(user_id);
    global g_rx_buf, user_names

    id_high_byte = (user_id >> 8) & 0xFF
    id_low_byte  = user_id & 0xFF
    
    print(f"\n[DeleteUser] Sending command to delete user ID: {user_id}")
    command_buf = [CMD_DEL, id_high_byte, id_low_byte, 0, 0]
    
    r = TxAndRxCmd(command_buf, 8, 1.0) # 1 second timeout
    
    if r != ACK_SUCCESS:
        print(f"[DeleteUser] FAILED: The command 'TxAndRxCmd' failed (Code: {hex(r)}).")
        publish_event("error", {"message": f"Delete user {user_id} failed: TX/RX Error"})
        return ACK_FAIL

    sensor_ack_code = g_rx_buf[4]
    if sensor_ack_code != ACK_SUCCESS:
        print(f"[DeleteUser] FAILED: Sensor rejected command (Code: {hex(sensor_ack_code)}).")
        publish_event("error", {"message": f"Delete user {user_id} failed: Sensor Error {hex(sensor_ack_code)}"})
        return ACK_FAIL

    # If we get here, it worked.
    # Also remove from our local 'user_names' cache if it exists
    user_names.pop(user_id, None) 
        
    print(f"[DeleteUser] SUCCESS: User {user_id} was deleted from sensor.")
    publish_event("user_deleted", {"user_id": user_id})
    return ACK_SUCCESS

#***************************************************************************
def Analysis_PC_Command(command):
    print(f"[Command Processor] Executing: {command}")
    c = str(command).strip().upper()

    if c == "CMD1":
        print("Number of fingerprints: %d" % GetUserCount())

    elif c == "CMD2":
        # Single source of truth for registration:
        # Immediately ACK so GM can update the UI, then run AddUser()
        publish_event("register_ack", {"message": "Credentials received. Get ready to enroll."})
        r = AddUser()
        if r == ACK_SUCCESS:
            print("[Command Processor] Result: User added.")
        elif r == ACK_FULL:
            print("[Command Processor] Result: Library full.")
        else:
            print("[Command Processor] Result: AddUser failed.")

    elif c == "CMD3":
        print("Place your finger for manual verification...")
        r = VerifyUser()
        if r == ACK_NO_USER:
            print("No such user.")
        elif r == ACK_TIMEOUT:
            print("Timeout.")
        elif r == ACK_GO_OUT:
            print("Finger misaligned.")

    elif c == "CMD4":
        print("[Command Processor] Attempting to clear all users...")
        r = ClearAllUser()
        if r == ACK_SUCCESS:
            print("[Command Processor] Result: All users successfully cleared.")
        else:
            print("[Command Processor] Result: FAILED to clear users.")
            
    elif c.startswith("CMD5"):
        parts = c.split()
        if len(parts) < 2:
            print("Usage: CMD5 <user_id>")
            publish_event("error", {"message": "Usage: CMD5 <user_id>"})
        else:
            try:
                user_id_to_del = int(parts[1])
                print(f"[Command Processor] Attempting to delete user {user_id_to_del}...")
                r = DeleteUser(user_id_to_del)
                if r == ACK_SUCCESS:
                    print(f"[Command Processor] Result: User {user_id_to_del} successfully cleared.")
                else:
                    print(f"[Command Processor] Result: FAILED to clear user {user_id_to_del}.")
            except ValueError:
                print(f"Invalid user ID: {parts[1]}")
                publish_event("error", {"message": f"Invalid user ID: {parts[1]}"})
            except Exception as e:
                print(f"Error during deletion: {e}")
                publish_event("error", {"message": f"Error during deletion: {e}"})

    elif c == "CMD7":
        ListUsers()

    elif c == "CMD8":
        pin_state = GPIO.input(Finger_WAKE_Pin)
        print(f"GPIO pin {Finger_WAKE_Pin} state: {pin_state} ({'HIGH' if pin_state == 1 else 'LOW'})")
        print("Place your finger on the sensor and run CMD8 again to see if it changes.")

    elif c.startswith("CMDT"):
        parts = c.split()
        # (keep your existing CMDT test helpers if you like)
        print("CMDT received (test helpers); not relevant to register flow.")

    else:
        print("Invalid command!")


# Command Watcher Thread
# This function runs in the background, checking for a 'command.json' file.
#***************************************************************************
def Command_Watcher():
    """Continuously monitor for a command file from GameMaker."""
    print("Command watcher started. Waiting for 'command.json'...")
    last_processed_version = 0

    while True:
        command_file_found = False
        try:
            if COMMAND_FILE.is_file():
                command_file_found = True # Mark that we found it
                
                # Give GameMaker a tiny moment to finish writing
                time.sleep(0.1) # 100ms
                
                command_data = {}
                with open(COMMAND_FILE, 'r') as f:
                    command_data = json.load(f)
                
                # Get version, converting from float to int if needed
                new_version = command_data.get("version", 0)
                if isinstance(new_version, float):
                    new_version = int(new_version)
                    
                # inside Command_Watcher(), after you load command_data and compute new_version...
                if new_version > last_processed_version:
                    last_processed_version = new_version
                    command = command_data.get("command")
                    if command:
                        print(f"[Command Watcher] Received command: {command}")
                        # No aliasing here: GM will send exactly "CMD2" for registration
                        Analysis_PC_Command(command)
                    else:
                        print("[Command Watcher] Command file had no 'command' field.")
                else:
                    print("[Command Watcher] Ignoring old command (version not new).")

        except json.JSONDecodeError:
            print(f"[Command Watcher] Error: command.json was not valid JSON. Cleaning it up.")
        except Exception as e:
            print(f"Error in command watcher: {e}")
        
        finally:
            # CRITICAL: Always delete the file if we found it.
            # This stops the error loop, even if the read failed.
            if command_file_found:
                try:
                    os.remove(COMMAND_FILE)
                    # We don't need to print a message here, it's just cleanup
                except Exception as e:
                    print(f"Error removing command file: {e}")
        
        # Wait for 1 second before checking again
        time.sleep(1)


def main():
    # Wake/reset sensor
    GPIO.output(Finger_RST_Pin, GPIO.LOW)
    time.sleep(0.25)
    GPIO.output(Finger_RST_Pin, GPIO.HIGH)
    time.sleep(0.25)

    # Configure compare level
    while SetCompareLevel(5) != 5:
        print("***ERROR***: Check wiring and power.")
        publish_event("error", {"message": "compare level set failed"})
        time.sleep(1)

    publish_event("ready", {"path": str(DATA_FILE)})
    print("*********** Fingerprint Reader Test ***********")
    print("Commands:")
    print("  CMD1 : Query number of fingerprints")
    print("  CMD2 : Register fingerprint (3 scans + name)")
    print("  CMD3 : Manual fingerprint matching")
    print("  CMD4 : Clear all fingerprints")
    print("  CMD5 <ID> : Delete a specific user ID")
    print("  CMD7 : List enrolled users")
    print("  CMD8 : Test GPIO pin state (for debugging)")
    print("  CMDT ... : Test")
    print("***********************************************")
    print("Ready to accept commands.")
    print("***********************************************")

    # The auto-verify thread start has been removed.

    command_thread = threading.Thread(target=Command_Watcher, daemon=True)
    command_thread.start()
    
    # Main command loop
    while True:
        try:
            cmd = input("\nEnter command (CMD1–CMD8 / CMDT ...) or press Enter: ").strip()
            if cmd:
                Analysis_PC_Command(cmd)
        except KeyboardInterrupt:
            break


if __name__ == '__main__':
    try:
        main()
    finally:
        try:
            if ser is not None:
                ser.close()
        except Exception:
            pass
        try:
            GPIO.cleanup()
        except Exception:
            pass
        print("\nTest finished!\n")
        sys.exit()
