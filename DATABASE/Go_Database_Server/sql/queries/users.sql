-- name: CreateUser :one
INSERT INTO users (id, username, password_hash, highscore, fingerprint_scanner_hash_code, fingerprint_scanner_access_level)
VALUES (
    gen_random_uuid(),
    $1,
    $2,
    0,
    $3,
    $4
)
RETURNING *;

-- name: GetUserFromFingerHashCode :one
SELECT * FROM users WHERE fingerprint_scanner_hash_code = $1;

-- name: DeleteAllUsers :exec
DELETE FROM users;

-- name: GetUsers :many
SELECT * FROM users;

-- name: GetUserFromId :one
SELECT * FROM users
WHERE id = $1;

-- name: UpdateHighScoreFromId :exec
UPDATE users
SET highscore = GREATEST(highscore, $1)
WHERE id = $2;

-- name: UpdateHighScoreFromFingerPrintHash :exec
UPDATE users
SET highscore = GREATEST(highscore, $1)
WHERE fingerprint_scanner_hash_code = $2;

-- name: GetUserByUsername :one
SELECT * FROM users WHERE username = $1;
