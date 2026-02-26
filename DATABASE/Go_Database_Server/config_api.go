package main

import (
	"database/sql"
	"github.com/Strife-01/Go_Shooting_Range_DB_Server/internal/database"
)

type apiConfig struct {
	db *database.Queries
	rawDB    *sql.DB
	jwt_secret_string string
}
