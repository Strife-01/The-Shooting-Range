-- Create the database if not exists
SELECT 'CREATE DATABASE the_shooting_range_db'
WHERE NOT EXISTS (
    SELECT FROM pg_database WHERE datname = 'the_shooting_range_db'
) \gexec

\connect the_shooting_range_db;

-- Create admin user if missing
DO $$
BEGIN
    IF NOT EXISTS (
        SELECT FROM pg_roles WHERE rolname = 'admin'
    ) THEN
        CREATE ROLE admin LOGIN PASSWORD 'CHOOSE A PASSWORD';
    END IF;
END
$$;

GRANT ALL PRIVILEGES ON DATABASE the_shooting_range_db TO admin;

-- Create users table
CREATE TABLE IF NOT EXISTS users (
  id UUID PRIMARY KEY,
  username TEXT UNIQUE NOT NULL,
  password_hash TEXT NOT NULL,
  highscore INTEGER DEFAULT 0,
  fingerprint_scanner_hash_code TEXT,
  fingerprint_scanner_access_level TEXT
);

ALTER TABLE users OWNER TO admin;
GRANT ALL PRIVILEGES ON TABLE users TO admin;

ALTER DEFAULT PRIVILEGES IN SCHEMA public
GRANT ALL ON TABLES TO admin;

