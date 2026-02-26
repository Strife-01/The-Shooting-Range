#!/usr/bin/env bash
set -e

PGPASSWORD="CHOOSE A PASSWORD"
export PGPASSWORD

# Run the SQL script inside the Postgres container as the postgres superuser
sudo docker exec -i shooting_db psql -U postgres -f /db_setup.sql
