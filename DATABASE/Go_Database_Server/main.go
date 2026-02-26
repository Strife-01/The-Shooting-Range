package main

import _ "github.com/lib/pq"
import (
	"database/sql"
	"fmt"
	"net/http"
	"os"

	"github.com/joho/godotenv"
	"github.com/Strife-01/Go_Shooting_Range_DB_Server/internal/database"
)

func main() {
	godotenv.Load()
	serverPort := os.Getenv("SERVER_PORT")
	dbURL := os.Getenv("DB_URL")
	jwtSecret := os.Getenv("JWT_SECRET_STRING")

	db, err := sql.Open("postgres", dbURL)
	if err != nil {
		fmt.Printf("Database failed to open: %v\n", err)
	}
	defer db.Close()

	dbQueries := database.New(db)

	apiCfg := apiConfig{
		db: dbQueries,
		rawDB: db,
		jwt_secret_string: jwtSecret,
	}

	mux := http.NewServeMux()
	mux.HandleFunc("GET /api/highscores", apiCfg.handlerGetHighscores)
	mux.HandleFunc("GET /api/highscores/{id}", apiCfg.handlerGetPlayerHighscore)
	mux.HandleFunc("PUT /api/highscores/{id}", apiCfg.handlerUpdatePlayerHighscore)

	mux.HandleFunc("POST /api/players", apiCfg.handlerCreatePlayer)
	//mux.HandleFunc("PUT /api/players/{id}", apiCfg.handlerUpdatePlayerStats)

	mux.HandleFunc("POST /api/login", apiCfg.handlerLogin)

	server := &http.Server{
		Addr:    fmt.Sprintf(":%v", serverPort),
		Handler: mux,
	}
	defer server.Close()

	fmt.Printf("Server listening on :%v\n", serverPort)
	err = server.ListenAndServe()
	if err != nil && err != http.ErrServerClosed {
		fmt.Printf("Server failed to start: %v\n", err)
	}
}

