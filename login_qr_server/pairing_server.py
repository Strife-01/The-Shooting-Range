#!/usr/bin/env python3
import os
import base64
import json
import secrets
import time
from io import BytesIO
from flask import Flask, request, jsonify, render_template_string
from cryptography.hazmat.primitives import serialization, hashes
from cryptography.hazmat.primitives.asymmetric import rsa, padding
import qrcode
from qrcode import constants         
from qrcode.image.pil import PilImage    

# -----------------------------
# Config
# -----------------------------
PAIR_TTL_SECONDS = 60
FORCE_HOST = os.getenv("FORCE_HOST")
PORT = int(os.getenv("PORT", "9443"))

# Where to drop the credentials JSON for GM to read later
# Change ~ with /home/user_name
OUTBOX_DIR = os.getenv("OUTBOX_DIR", os.path.abspath("~/GameMakerStudio2/vm/TestGM/assets/gm_outbox"))

# -----------------------------
# App
# -----------------------------
app = Flask(__name__, static_url_path="/static", static_folder="static")
SESSIONS = {}  # token -> {priv, pub_pem, ts, used}

def new_session():
    priv = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    pub = priv.public_key()
    pub_pem = pub.public_bytes(
        serialization.Encoding.PEM,
        serialization.PublicFormat.SubjectPublicKeyInfo
    ).decode()
    token = f"{secrets.randbelow(10**6):06d}"
    SESSIONS[token] = {"priv": priv, "pub_pem": pub_pem, "ts": time.time(), "used": False}
    return token, pub_pem

def cleanup_sessions():
    now = time.time()
    for t in list(SESSIONS.keys()):
        if now - SESSIONS[t]["ts"] > PAIR_TTL_SECONDS:
            del SESSIONS[t]

def _public_host_from_request():
    """
    Decide which host to embed in the QR. Prefer FORCE_HOST if provided.
    Otherwise, use request.host (strips port) so phones can reach it.
    """
    if FORCE_HOST:
        return FORCE_HOST
    host = request.host.split(":")[0]
    return host

def _qr_png_b64(payload: str) -> str:
    # Build QR with PIL backend
    qr = qrcode.QRCode(
        error_correction=constants.ERROR_CORRECT_M,
        border=1,
        box_size=8,
        image_factory=PilImage,  # ensure PIL, not pure-png backend
    )
    qr.add_data(payload)
    qr.make(fit=True)

    # Create image and FORCE 8-bit (not 1-bit)
    img = qr.make_image(fill_color="black", back_color="white")
    if hasattr(img, "get_image"):     # unwrap PilImage wrapper
        img = img.get_image()
    img = img.convert("L")            # 8-bit grayscale (or "RGB"/"RGBA")

    buf = BytesIO()
    img.save(buf, format="PNG", optimize=True)
    return base64.b64encode(buf.getvalue()).decode("ascii")

def _ensure_outbox():
    os.makedirs(OUTBOX_DIR, exist_ok=True)

def _atomic_write_json(path: str, obj: dict):
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(obj, f, ensure_ascii=False)
        f.flush()
        os.fsync(f.fileno())
    os.replace(tmp, path)  # atomic on Windows & POSIX


# -----------------------------
# API: GameMaker requests QR here
# -----------------------------
@app.route("/pair_request", methods=["GET"])
def pair_request():
    cleanup_sessions()
    token, pub_pem = new_session()
    host = _public_host_from_request()

    # Use https only if the current request is secure; otherwise http
    proto = "https" if request.is_secure else "http"
    url = f"{proto}://{host}:{PORT}/pair?token={token}"

    qr_payload = json.dumps({"url": url, "token": token})
    b64_png   = _qr_png_b64(qr_payload)   # 8-bit PNG now

    return jsonify({"token": token, "qr_png_b64": b64_png})


# -----------------------------
# Form Imput Page
# -----------------------------
PAGE = """
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>The Shooting Range — Login</title>
<meta name="viewport" content="width=device-width, initial-scale=1">

<style>
  :root{
    --bg1:#F6D39C; --bg2:#C88435; --ink:#3B2610; --wood:#8C5B2D; --btn1:#C43C24; --btn2:#7D2418;
  }
  *{box-sizing:border-box}
  html,body{height:100%; margin:0; font-family:system-ui,-apple-system,Segoe UI,Roboto,Ubuntu,Cantarell,"Helvetica Neue",Arial}
  body{
    display:flex; align-items:center; justify-content:center;
    background:
      radial-gradient(80% 60% at 50% 30%, rgba(255,255,255,0.35), transparent 60%),
      linear-gradient(180deg, var(--bg1), var(--bg2));
    color:var(--ink);
  }
  .frame{
    width:min(92vw, 420px);
    background:
      linear-gradient(0deg, rgba(255,255,255,0.05), rgba(255,255,255,0)),
      repeating-linear-gradient( 90deg,
        rgba(165,112,57,0.2) 0 10px, rgba(165,112,57,0.25) 10px 20px
      ),
      linear-gradient(180deg, #EFCF98, #D8A15C);
    border:4px solid #3B2610;
    border-radius:16px;
    padding:22px 20px;
    box-shadow: 0 10px 30px rgba(0,0,0,0.45), inset 0 6px 18px rgba(255,255,255,0.25);
    text-align:center;
    animation:pop .45s ease-out both;
  }
  @keyframes pop{from{transform:translateY(18px); opacity:0} to{transform:translateY(0);opacity:1}}

  .badge{
    display:inline-flex; gap:10px; align-items:center; justify-content:center;
    margin-bottom:14px; user-select:none;
  }
  .badge svg{width:40px; height:40px; filter: drop-shadow(0 1px 0 #0008)}
  h2{
    margin:6px 0 14px; font-size:clamp(20px, 4.5vw, 26px);
    font-weight:800; letter-spacing:.5px;
    text-shadow: 2px 2px #E2A457;
  }
  .field{
    width:100%; margin-top:10px;
    border:3px solid var(--wood); border-radius:10px; overflow:hidden;
    background:#FFECC6; box-shadow: inset 0 5px 12px rgba(0,0,0,0.18);
  }
  .field input{
    display:block; width:100%; border:0; outline:none; background:transparent;
    padding:14px 12px; text-align:center; color:var(--ink);
    font-size:clamp(16px, 4.5vw, 20px); font-weight:700;
  }
  .btn{
    width:100%; margin-top:18px; border:3px solid #3B1B12; border-radius:12px; overflow:hidden;
    background:linear-gradient(180deg, var(--btn1), var(--btn2)); color:#fff;
    text-shadow: 2px 2px #000; font-weight:900;
    padding:14px; font-size:clamp(18px, 5vw, 22px); cursor:pointer;
    box-shadow: 0 6px 0 #4a120d, 0 12px 24px rgba(0,0,0,0.35);
    transition: transform .05s ease, box-shadow .05s ease;
  }
  .btn:active{ transform:translateY(2px); box-shadow: 0 3px 0 #4a120d, 0 8px 18px rgba(0,0,0,0.35); }
  .status{ min-height:26px; margin-top:12px; font-weight:800; font-size:clamp(14px, 4vw, 16px) }
  .fineprint{ margin-top:8px; opacity:.6; font-size:12px }
</style>
</head>
<body>

<div class="frame" role="form" aria-label="Login form">
  <div class="badge" aria-hidden="true">
    <!-- sheriff star -->
    <svg viewBox="0 0 100 100" fill="#F5D469"><path d="M50 5l9 22 24-2-15 19 11 21-22-7-18 14 1-23-20-11 22-6z"/><circle cx="50" cy="50" r="10" fill="#E2B53C"/></svg>
    <h2>Enter the Range</h2>
    <!-- cowboy hat -->
    <svg viewBox="0 0 128 64" fill="#6A3C12"><path d="M8 40c18 12 94 12 112 0 6-4-6-10-16-8-10 2-14-6-18-14-4-8-12-10-24-6-12-4-20-2-24 6-4 8-8 16-18 14-10-2-22 4-16 8z"/></svg>
  </div>

  <div class="field"><input id="u" placeholder="Username" inputmode="text" autocomplete="username"></div>
  <div class="field"><input id="p" type="password" placeholder="Password" autocomplete="current-password"></div>
  <div class="status" id="password_strength"></div>

  <button class="btn" id="goBtn">ENTER THE RANGE</button>
  <div class="status" id="status" aria-live="polite"></div>
  <div class="fineprint">Tip: press <b>Enter</b> to submit. Fields clear after send.</div>
</div>

<script>
  // Grab token from URL and hook up DOM elements
  const token  = new URLSearchParams(location.search).get('token');
  const elUser = document.getElementById('u');
  const elPass = document.getElementById('p');
  const elBtn  = document.getElementById('goBtn');
  const elStat = document.getElementById('status');
  const passStat = document.getElementById('password_strength');
  
  elBtn.disabled = true;

  // Safety: make sure elements are present
  if (!elUser || !elPass || !elBtn || !elStat) {
    console.error("Login form elements not found in DOM.");
  } else {
    elBtn.addEventListener('click', submitLogin);
    document.addEventListener('keydown', (e)=>{
      if(e.key === 'Enter') { e.preventDefault(); submitLogin(); }
    });
    // optional: select text on focus
    [elUser, elPass].forEach(i => i.addEventListener('focus', () => i.select()));
    elPass.addEventListener('keyup', (e) => {
        const maxNrOfCharacters = 94; // all printable ASCII
        const specialChars = `!"#$%&'()*+,-./:;<=>?@[\\]^_\`{|}~`;

        const pass = e.target.value;
        const entropy = Math.log(maxNrOfCharacters) / Math.log(2) * pass.length;

        let passwordGoodEntropy = false;
        let passwordGoodEnough = false;

        // flags
        let passwordLongEnough = false;
        let passwordContainsSpecial = false;
        let passwordContainsDigit = false;
        let passwordContainsLower = false;
        let passwordContainsUpper = false;

        // ===== ENTROPY CHECK =====
        if (entropy < 40) {
            passStat.style.color = "red";
            passStat.textContent = "Very weak";
        } else if (entropy < 60) {
            passStat.style.color = "orange";
            passStat.textContent = "Weak";
        } else if (entropy < 80) {
            passStat.style.color = "yellow";
            passStat.textContent = "Reasonable";
            passwordGoodEntropy = true;
        } else if (entropy < 100) {
            passStat.style.color = "green";
            passStat.textContent = "Strong";
            passwordGoodEntropy = true;
        } else {
            passStat.style.color = "green";
            passStat.textContent = "Very strong";
            passwordGoodEntropy = true;
        }

        let current_status = elStat.textContent;

        // ===== LENGTH CHECK =====
        if (pass.length >= 8) {
            passwordLongEnough = true;
            elStat.textContent = "";
        } else {
            elStat.textContent = "Minimum password length: 8 characters";
            current_status = elStat.textContent;
        }

        // ===== CHARACTER TYPE CHECKS =====
        for (let c of pass) {
            if (specialChars.includes(c)) passwordContainsSpecial = true;
            else if (/[0-9]/.test(c)) passwordContainsDigit = true;
            else if (/[a-z]/.test(c)) passwordContainsLower = true;
            else if (/[A-Z]/.test(c)) passwordContainsUpper = true;
        }

        // ===== REQUIREMENTS FEEDBACK =====
        if (!passwordContainsSpecial) {
            elStat.textContent = "Please include at least one special character";
            current_status = elStat.textContent;
        } else if (!passwordContainsDigit) {
            elStat.textContent = "Please include at least one digit";
            current_status = elStat.textContent;
        } else if (!passwordContainsLower) {
            elStat.textContent = "Please include at least one lowercase letter";
            current_status = elStat.textContent;
        } else if (!passwordContainsUpper) {
            elStat.textContent = "Please include at least one uppercase letter";
            current_status = elStat.textContent;
        } else if (!passwordLongEnough) {
            elStat.textContent = "Minimum password length: 8 characters";
            current_status = elStat.textContent;
        } else {
            elStat.textContent = "";
        }

        // ===== FINAL VALIDATION =====
        if (
            passwordGoodEntropy &&
            passwordLongEnough &&
            passwordContainsSpecial &&
            passwordContainsDigit &&
            passwordContainsLower &&
            passwordContainsUpper
        ) {
            passwordGoodEnough = true;
        }

        elBtn.disabled = !passwordGoodEnough;
    });
  }

  async function submitLogin(){
    try{
      const u = elUser.value.trim();
      const p = elPass.value;
      if(!u || !p){ elStat.textContent = "Please enter both fields."; return; }

      elStat.textContent = "Preparing…";

      // Get pubkey (fine on HTTP too)
      const infoResp = await fetch('/pair_info?token=' + encodeURIComponent(token));
      if(!infoResp.ok){ elStat.textContent = "Invalid or expired session."; return; }
      const { pub_pem } = await infoResp.json();

      // Use WebCrypto if available (HTTPS/secure context)
      const hasSubtle = (window.isSecureContext && window.crypto && window.crypto.subtle);

      if (hasSubtle) {
        elStat.textContent = "Encrypting (RSA)…";
        const b64 = pub_pem.replace(/-----.*-----/g,'').replace(/\s+/g,'');
        const raw = Uint8Array.from(atob(b64), c => c.charCodeAt(0)).buffer;

        const key = await crypto.subtle.importKey(
          'spki', raw, { name:'RSA-OAEP', hash:'SHA-256' }, false, ['encrypt']
        );

        const data = new TextEncoder().encode(JSON.stringify({u, p}));
        const enc  = await crypto.subtle.encrypt({name:'RSA-OAEP'}, key, data);
        const enc64 = btoa(String.fromCharCode(...new Uint8Array(enc)));

        elStat.textContent = "Sending…";
        const resp = await fetch('/submit', {
          method:'POST', headers:{'Content-Type':'application/json'},
          body: JSON.stringify({ token, enc64 })
        });

        if(resp.ok){
          elStat.textContent = "✅ Credentials Sent Securely!";
          elUser.value = ""; elPass.value = "";
        } else {
          elStat.textContent = "⚠ " + await resp.text();
        }
        return;
      }

      // DEV fallback for HTTP/non-secure browsers (no WebCrypto)
      elStat.textContent = "No secure crypto — sending DEV plaintext…";
      const resp = await fetch('/submit_plain', {
        method:'POST', headers:{'Content-Type':'application/json'},
        body: JSON.stringify({ token, u, p })
      });

      if(resp.ok){
        elStat.textContent = "✅ Credentials Sent (DEV plaintext)";
        elUser.value = ""; elPass.value = "";
      } else {
        elStat.textContent = "⚠ " + await resp.text();
      }
    }catch(err){
      elStat.textContent = "⚠ " + (err?.message || err);
      console.error(err);
    }
  }
</script>


</body>
</html>
"""

# -----------------------------
# Routes for the page / info / submit
# -----------------------------
@app.route("/pair", methods=["GET"])
def pair_page():
    return render_template_string(PAGE)

@app.route("/pair_info", methods=["GET"])
def pair_info():
    token = request.args.get("token")
    if not token or token not in SESSIONS or SESSIONS[token]["used"]:
        return jsonify({"error":"bad"}), 400
    return jsonify({"pub_pem": SESSIONS[token]["pub_pem"]})

@app.route("/submit", methods=["POST"])
def submit_encrypted():
    j = request.get_json(silent=True) or {}
    token = j.get("token")
    enc64 = j.get("enc64")
    if not token or token not in SESSIONS:
        return "expired", 400
    if SESSIONS[token]["used"]:
        return "already used", 400
    try:
        priv = SESSIONS[token]["priv"]
        ciphertext = base64.b64decode(enc64)
        plaintext = priv.decrypt(
            ciphertext,
            padding.OAEP(mgf=padding.MGF1(algorithm=hashes.SHA256()),
                         algorithm=hashes.SHA256(), label=None)
        )
        creds = json.loads(plaintext.decode())
        record = {
            "token": token,
            "username": creds.get("u", ""),
            "password": creds.get("p", ""),
            "ts": int(time.time()),
        }
        print(f"Saving creds to outbox: {OUTBOX_DIR}")

        # Write to outbox as <token>.json (atomic)
        _ensure_outbox()
        out_path = os.path.join(OUTBOX_DIR, f"{token}.json")
        _atomic_write_json(out_path, record)

        print("✅ RECEIVED LOGIN:", creds)
        SESSIONS[token]["used"] = True
        return "OK"
    except Exception as e:
        return f"decrypt error: {e}", 500
    
@app.post("/submit_plain")
def submit_plain():
    """DEV ONLY: accept plaintext creds when WebCrypto is unavailable (HTTP)."""
    j = request.get_json(silent=True) or {}
    token = j.get("token")
    u = j.get("u") or ""
    p = j.get("p") or ""
    if not token or token not in SESSIONS:
        return "expired", 400
    if SESSIONS[token]["used"]:
        return "already used", 400

    record = {
        "token": token,
        "username": u,
        "password": p,
        "ts": int(time.time()),
        "insecure": True,  # mark as dev/insecure
    }

    _ensure_outbox()
    out_path = os.path.join(OUTBOX_DIR, f"{token}.json")
    _atomic_write_json(out_path, record)
    SESSIONS[token]["used"] = True
    print(f"⚠️ INSECURE WRITE (DEV) → {out_path}")
    return "OK"


@app.route("/check_login")
def check_login():
    token = request.args.get("token")
    if token in SESSIONS and SESSIONS[token]["used"] == True:
        return "YES"
    return "NO"

# -----------------------------
# Run server (HTTPS if certs exist; else HTTP)
# -----------------------------
if __name__ == "__main__":
    ssl_ctx = None
    if os.path.exists("cert.pem") and os.path.exists("key.pem"):
        ssl_ctx = ("cert.pem", "key.pem")
        print(f"🔒 HTTPS enabled on port {PORT} (cert.pem/key.pem found).")
    else:
        print(f"⚠ No cert.pem/key.pem found — running HTTP on port {PORT}. "
              f"WebCrypto may be blocked in some browsers without HTTPS.")
    app.run(host="0.0.0.0", port=PORT, ssl_context=ssl_ctx)

