-- +goose Up
CREATE TABLE "users" (
  "id" UUID NOT NULL,
  "username" TEXT NOT NULL UNIQUE,
  "password_hash" TEXT NOT NULL,
  "highscore" INTEGER NOT NULL,
  "fingerprint_scanner_hash_code" TEXT NOT NULL UNIQUE,
  "fingerprint_scanner_access_level" TEXT NOT NULL
);

-- +goose Down
DROP TABLE "users";
