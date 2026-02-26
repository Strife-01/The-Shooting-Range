# 🐳 Shooting Range Database & Go Server Setup Guide

This directory contains everything needed to run the **PostgreSQL database** and **Go backend API** for the Shooting Range project using Docker Compose.

---

## Prerequisites

Make sure you have the following installed on your system:
- **Docker** (≥ 24.x)
- **Docker Compose**
- **Go** (≥ 1.24) — only required for local testing, not to run Docker

---

## Step 1 — Clone the Repository

```bash
git clone <your_repo_url>
cd cs25-01-main/DATABASE
```

---

## Step 2 — Environment Setup

There are **two `.env` files** that must be created before running Docker.

### `DATABASE/.env`
Used by Docker Compose and Postgres.

Create the file:
```bash
cp .env.example .env
```

Example content:
```env
POSTGRES_USER=postgres
POSTGRES_PASSWORD=postgres
POSTGRES_DB=shooting_game

# Used internally by Postgres and Go service
DB_HOST=db
DB_PORT=5432
DB_USER=postgres
DB_PASSWORD=postgres
PG_USER=postgres
PG_USER_PASSWORD=postgres
DATABASE_NAME=shooting_game
```

---

### `DATABASE/Go_Database_Server/.env`
Used by the Go server to connect to Postgres.

Create the file:
```bash
cp Go_Database_Server/.env.example Go_Database_Server/.env
```

Example content:
```env
DB_HOST=db
DB_PORT=5432
DB_USER=postgres
DB_PASSWORD=postgres
DB_NAME=shooting_game

SERVER_PORT=8080
DB_URL=postgres://postgres:postgres@db:5432/shooting_game?sslmode=disable
```

---

## Step 3 — Build and Run with Docker

From inside the `DATABASE/` folder:
```bash
docker compose up --build
```

### What happens:
- A **PostgreSQL 15** container is created (`shooting_db`).
- A **Go server** container is built and started (`shooting_go_server`).
- The Go server automatically connects to the Postgres database.

You should see logs similar to:
```
shooting_go_server | Server listening on :8080
```

---

## Step 4 — Test the Connection

Once the containers are up, open a new terminal and run:
```bash
curl http://localhost:2501/api/highscores
```

Expected output:
```json
[]
```

This confirms both containers are running and communicating correctly.

---

## Step 5 — Stop the Containers

To stop and clean up:
```bash
docker compose down
```

If you want to **reset the database completely**:
```bash
docker compose down -v
```
(`-v` removes the Postgres data volume.)

---

## Optional: Run Tests Locally (requires Go)

If you want to run the unit tests (optional, for developers):
```bash
cd Go_Database_Server
go test ./... -v -cover
```

---

## Troubleshooting

| Problem | Solution |
|----------|-----------|
| `Could not start transaction` | Make sure both containers are running (`docker ps`). |
| `connection to server on socket ... failed` | The DB may not have finished initializing. Wait a few seconds and retry. |
| Go server exits instantly | Check `.env` files — `DB_URL` must point to `db`, **not** `localhost`. |
| Stale database or wrong schema | Run `docker compose down -v && docker compose up --build`. |

---

## Summary

| Service | Description | Port |
|----------|-------------|------|
| `shooting_db` | PostgreSQL database | `5432` |
| `shooting_go_server` | Go REST API backend | `2501` |

After running:
```
docker compose up --build
```
Visit your backend at: [http://localhost:2501](http://localhost:2501)

---
