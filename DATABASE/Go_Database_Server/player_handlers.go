package main

import (
	"encoding/json"
	"net/http"
	"time"

	"github.com/Strife-01/Go_Shooting_Range_DB_Server/internal/auth"
	"github.com/Strife-01/Go_Shooting_Range_DB_Server/internal/database"
	"github.com/google/uuid"
)

type player struct {
	ID                            uuid.UUID `json:"id"`
	Username                      string    `json:"username"`
	Highscore                     int32     `json:"highscore"`
	FingerprintScannerHashCode    string    `json:"fingerprint_scanner_hash_code"`
	FingerprintScannerAccessLevel string    `json:"fingerprint_scanner_access_level"`
}

type playerCreate struct {
	Username                      string `json:"username"`
	Password                      string `json:"password"`
	FingerprintScannerHashCode    string `json:"fingerprint_scanner_hash_code"`
	FingerprintScannerAccessLevel string `json:"fingerprint_scanner_access_level"`
}
type loginRequest struct {
	Username                   string `json:"username"`
	Password                   string `json:"password"`
	FingerprintScannerHashCode string `json:"fingerprint_scanner_hash_code"`
}

type loginResponse struct {
	Token string 		 `json:"token"`
	UserID uuid.UUID `json:"user_id"`
}

type highscoreUpdate struct {
	Highscore int32 `json:"highscore"`
}

type highscoreResponse struct {
	ID        uuid.UUID `json:"id"`
	Username  string    `json:"username"`
	Highscore int32     `json:"highscore"`
}

// GET /api/highscores
func (cfg *apiConfig) handlerGetHighscores(respw http.ResponseWriter, req *http.Request) {
	tx, err := cfg.rawDB.BeginTx(req.Context(), nil)
	if err != nil {
		respondWithError(respw, http.StatusInternalServerError, "Could not start transaction")
		return
	}
	qtx := cfg.db.WithTx(tx)

	users, err := qtx.GetUsers(req.Context())
	if err != nil {
		tx.Rollback()
		respondWithError(respw, http.StatusInternalServerError, "Could not retrieve highscores")
		return
	}

	if err := tx.Commit(); err != nil {
		respondWithError(respw, http.StatusInternalServerError, "Could not commit transaction")
		return
	}

	highscores := make([]highscoreResponse, len(users))
	for i, usr := range users {
		highscores[i] = highscoreResponse{
			ID:        usr.ID,
			Username:  usr.Username,
			Highscore: usr.Highscore,
		}
	}

	respondWithJSON(respw, http.StatusOK, highscores)
}

// GET /api/highscores/{id}
func (cfg *apiConfig) handlerGetPlayerHighscore(respw http.ResponseWriter, req *http.Request) {
	idStr := req.PathValue("id")
	if idStr == "" {
		respondWithError(respw, http.StatusBadRequest, "Missing player ID")
		return
	}

	playerID, err := uuid.Parse(idStr)
	if err != nil {
		respondWithError(respw, http.StatusBadRequest, "Invalid player ID format")
		return
	}

	tx, err := cfg.rawDB.BeginTx(req.Context(), nil)
	if err != nil {
		respondWithError(respw, http.StatusInternalServerError, "Could not start transaction")
		return
	}
	qtx := cfg.db.WithTx(tx)

	usr, err := qtx.GetUserFromId(req.Context(), playerID)
	if err != nil {
		tx.Rollback()
		respondWithError(respw, http.StatusNotFound, "Player not found")
		return
	}

	if err := tx.Commit(); err != nil {
		respondWithError(respw, http.StatusInternalServerError, "Could not commit transaction")
		return
	}

	response := highscoreResponse{
		ID:        usr.ID,
		Username:  usr.Username,
		Highscore: usr.Highscore,
	}

	respondWithJSON(respw, http.StatusOK, response)
}

// PUT /api/highscores/{id}
func (cfg *apiConfig) handlerUpdatePlayerHighscore(respw http.ResponseWriter, req *http.Request) {
	idStr := req.PathValue("id")
	if idStr == "" {
		respondWithError(respw, http.StatusBadRequest, "Missing player ID")
		return
	}
	playerID, err := uuid.Parse(idStr)
	if err != nil {
		respondWithError(respw, http.StatusBadRequest, "Invalid player ID format")
		return
	}

	// Extract & validate JWT
	tokenString, err := auth.GetBearerToken(req.Header)
	if err != nil {
		respondWithError(respw, http.StatusUnauthorized, "Missing or invalid Authorization header")
		return
	}

	claimsUserID, err := auth.ValidateJWT(tokenString, cfg.jwt_secret_string)
	if err != nil {
		respondWithError(respw, http.StatusUnauthorized, "Invalid or expired token")
		return
	}

	if claimsUserID != playerID {
		respondWithError(respw, http.StatusForbidden, "You can only update your own highscore")
		return
	}

	// proceed as before (transaction, update highscore)
	var reqData highscoreUpdate
	if err := json.NewDecoder(req.Body).Decode(&reqData); err != nil {
		respondWithError(respw, http.StatusBadRequest, "Error decoding request body")
		return
	}

	if reqData.Highscore < 0 {
		respondWithError(respw, http.StatusBadRequest, "Highscore must be non-negative")
		return
	}

	tx, err := cfg.rawDB.BeginTx(req.Context(), nil)
	if err != nil {
		respondWithError(respw, http.StatusInternalServerError, "Could not start transaction")
		return
	}
	qtx := cfg.db.WithTx(tx)

	err = qtx.UpdateHighScoreFromId(req.Context(), database.UpdateHighScoreFromIdParams{
		Highscore: reqData.Highscore,
		ID:        playerID,
	})
	if err != nil {
		tx.Rollback()
		respondWithError(respw, http.StatusInternalServerError, "Could not update highscore")
		return
	}

	if err := tx.Commit(); err != nil {
		respondWithError(respw, http.StatusInternalServerError, "Could not commit transaction")
		return
	}

	respondWithJSON(respw, http.StatusOK, map[string]string{"status": "ok"})
}

// POST /api/players
func (cfg *apiConfig) handlerCreatePlayer(respw http.ResponseWriter, req *http.Request) {
	var reqData playerCreate
	if err := json.NewDecoder(req.Body).Decode(&reqData); err != nil {
		respondWithError(respw, http.StatusBadRequest, "Error decoding request body")
		return
	}

	if reqData.Username == "" {
		respondWithError(respw, http.StatusBadRequest, "Username is required")
		return
	}

	if reqData.Password == "" {
		respondWithError(respw, http.StatusBadRequest, "Password is required")
		return
	}

	if reqData.FingerprintScannerHashCode == "" {
		respondWithError(respw, http.StatusBadRequest, "Fingerprint code is required")
		return
	}

	passHash, err := auth.HashPassword(reqData.Password)
	if err != nil {
		respondWithError(respw, http.StatusInternalServerError, "Could not hash password")
		return
	}
	
	fingerprintCode, err := auth.HashPassword(reqData.FingerprintScannerHashCode)
	if err != nil {
		respondWithError(respw, http.StatusInternalServerError, "Could not hash fingerprint code")
		return
	}
	
	tx, err := cfg.rawDB.BeginTx(req.Context(), nil)
	if err != nil {
		respondWithError(respw, http.StatusInternalServerError, "Could not start transaction")
		return
	}
	qtx := cfg.db.WithTx(tx)

	usr, err := qtx.CreateUser(req.Context(), database.CreateUserParams{
		Username:                      reqData.Username,
		PasswordHash:                  passHash,
		FingerprintScannerHashCode:    fingerprintCode,
		FingerprintScannerAccessLevel: reqData.FingerprintScannerAccessLevel,
	})

	if err != nil {
		tx.Rollback()
		respondWithError(respw, http.StatusBadRequest, "Username must be unique or invalid data")
		return
	}

	if err := tx.Commit(); err != nil {
		respondWithError(respw, http.StatusInternalServerError, "Could not commit player to database")
		return
	}

	respPlayer := player{
		ID:                            usr.ID,
		Username:                      usr.Username,
		Highscore:                     usr.Highscore,
		FingerprintScannerHashCode:    fingerprintCode,
		FingerprintScannerAccessLevel: usr.FingerprintScannerAccessLevel,
	}

	respondWithJSON(respw, http.StatusCreated, respPlayer)
}

// POST /api/login
func (cfg *apiConfig) handlerLogin(respw http.ResponseWriter, req *http.Request) {
	var loginReq loginRequest
	if err := json.NewDecoder(req.Body).Decode(&loginReq); err != nil {
		respondWithError(respw, http.StatusBadRequest, "Invalid request")
		return
	}

	tx, err := cfg.rawDB.BeginTx(req.Context(), nil)
	if err != nil {
		respondWithError(respw, http.StatusInternalServerError, "Could not start transaction")
		return
	}
	qtx := cfg.db.WithTx(tx)

	var user database.User

	// Case 1: Fingerprint login
	if loginReq.FingerprintScannerHashCode != "" {
		if loginReq.Username == "" {
			tx.Rollback()
			respondWithError(respw, http.StatusBadRequest, "Missing login username")
			return
		}

		user, err = qtx.GetUserByUsername(req.Context(), loginReq.Username)
		if err != nil {
			tx.Rollback()
			respondWithError(respw, http.StatusUnauthorized, "Invalid username")
			return
		}
		
		if err := auth.CheckPasswordHash(user.FingerprintScannerHashCode, loginReq.FingerprintScannerHashCode); err != nil {
			tx.Rollback()
			respondWithError(respw, http.StatusUnauthorized, "Invalid fingerprint for current user")
			return
		}
	} else {
		// Case 2: Username/password login
		if loginReq.Username == "" || loginReq.Password == "" {
			tx.Rollback()
			respondWithError(respw, http.StatusBadRequest, "Missing login credentials")
			return
		}

		user, err = qtx.GetUserByUsername(req.Context(), loginReq.Username)
		if err != nil {
			tx.Rollback()
			respondWithError(respw, http.StatusUnauthorized, "Invalid username or password")
			return
		}

		if err := auth.CheckPasswordHash(user.PasswordHash, loginReq.Password); err != nil {
			tx.Rollback()
			respondWithError(respw, http.StatusUnauthorized, "Invalid username or password")
			return
		}
	}

	// Generate JWT for either login method
	token, err := auth.MakeJWT(user.ID, cfg.jwt_secret_string, time.Hour*24)
	if err != nil {
		tx.Rollback()
		respondWithError(respw, http.StatusInternalServerError, "Could not create JWT")
		return
	}

	if err := tx.Commit(); err != nil {
		respondWithError(respw, http.StatusInternalServerError, "Could not commit transaction")
		return
	}

	respondWithJSON(respw, http.StatusOK, loginResponse{Token: token, UserID: user.ID})
}
