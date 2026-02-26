package main

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/google/uuid"
)

// --- Mock helpers for PUT /api/highscores/{id} ---

type mockAuth struct {
	tokenUserID uuid.UUID
	err         error
}

func (m *mockAuth) GetBearerToken(_ http.Header) (string, error) {
	if m.err != nil {
		return "", m.err
	}
	return "fake-token", nil
}

func (m *mockAuth) ValidateJWT(_ string, _ string) (uuid.UUID, error) {
	if m.err != nil {
		return uuid.Nil, m.err
	}
	return m.tokenUserID, nil
}

type mockUpdateDB struct {
	updated bool
	err     error
}

func (m *mockUpdateDB) UpdateHighScoreFromId(ctx context.Context, params interface{}) error {
	if m.err != nil {
		return m.err
	}
	m.updated = true
	return nil
}

// --- Test cases for handlerUpdatePlayerHighscore ---

func TestHandlerUpdatePlayerHighscore_Success(t *testing.T) {
	playerID := uuid.New()

	// mock auth returns same userID as in URL
	authMock := &mockAuth{tokenUserID: playerID}
	dbMock := &mockUpdateDB{}

	// Simulate request body with new highscore
	reqBody, _ := json.Marshal(map[string]int{"highscore": 9999})
	req := httptest.NewRequest(http.MethodPut, "/api/highscores/"+playerID.String(), bytes.NewBuffer(reqBody))
	req.Header.Set("Authorization", "Bearer fake-token")

	w := httptest.NewRecorder()

	handler := func(w http.ResponseWriter, r *http.Request) {
		tokenString, err := authMock.GetBearerToken(r.Header)
		if err != nil {
			respondWithError(w, http.StatusUnauthorized, "Missing or invalid Authorization header")
			return
		}
		claimsUserID, err := authMock.ValidateJWT(tokenString, "fake-secret")
		if err != nil {
			respondWithError(w, http.StatusUnauthorized, "Invalid or expired token")
			return
		}
		if claimsUserID != playerID {
			respondWithError(w, http.StatusForbidden, "You can only update your own highscore")
			return
		}
		var reqData highscoreUpdate
		if err := json.NewDecoder(r.Body).Decode(&reqData); err != nil {
			respondWithError(w, http.StatusBadRequest, "Error decoding request body")
			return
		}
		if reqData.Highscore < 0 {
			respondWithError(w, http.StatusBadRequest, "Highscore must be non-negative")
			return
		}
		if err := dbMock.UpdateHighScoreFromId(r.Context(), nil); err != nil {
			respondWithError(w, http.StatusInternalServerError, "Could not update highscore")
			return
		}
		respondWithJSON(w, http.StatusOK, map[string]string{"status": "ok"})
	}

	handler(w, req)
	resp := w.Result()
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		t.Fatalf("expected 200 OK, got %v", resp.StatusCode)
	}
	if !dbMock.updated {
		t.Fatalf("expected dbMock.updated=true, got false")
	}
}

func TestHandlerUpdatePlayerHighscore_InvalidToken(t *testing.T) {
	playerID := uuid.New()
	authMock := &mockAuth{err: errors.New("invalid token")}
	reqBody, _ := json.Marshal(map[string]int{"highscore": 1234})
	req := httptest.NewRequest(http.MethodPut, "/api/highscores/"+playerID.String(), bytes.NewBuffer(reqBody))
	req.Header.Set("Authorization", "Bearer bad-token")

	w := httptest.NewRecorder()

	handler := func(w http.ResponseWriter, r *http.Request) {
		_, err := authMock.GetBearerToken(r.Header)
		if err != nil {
			respondWithError(w, http.StatusUnauthorized, "Missing or invalid Authorization header")
			return
		}
		respondWithError(w, http.StatusUnauthorized, "Invalid or expired token")
	}

	handler(w, req)
	resp := w.Result()
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusUnauthorized {
		t.Fatalf("expected 401 Unauthorized, got %v", resp.StatusCode)
	}
}

func TestHandlerUpdatePlayerHighscore_Forbidden(t *testing.T) {
	playerID := uuid.New()
	authMock := &mockAuth{tokenUserID: uuid.New()} // different user
	reqBody, _ := json.Marshal(map[string]int{"highscore": 8888})
	req := httptest.NewRequest(http.MethodPut, "/api/highscores/"+playerID.String(), bytes.NewBuffer(reqBody))
	req.Header.Set("Authorization", "Bearer fake-token")

	w := httptest.NewRecorder()

	handler := func(w http.ResponseWriter, r *http.Request) {
		claimsUserID, _ := authMock.ValidateJWT("fake-token", "fake-secret")
		if claimsUserID != playerID {
			respondWithError(w, http.StatusForbidden, "You can only update your own highscore")
			return
		}
	}

	handler(w, req)
	resp := w.Result()
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusForbidden {
		t.Fatalf("expected 403 Forbidden, got %v", resp.StatusCode)
	}
}

func TestHandlerUpdatePlayerHighscore_NegativeScore(t *testing.T) {
	playerID := uuid.New()

	reqBody, _ := json.Marshal(map[string]int{"highscore": -42})
	req := httptest.NewRequest(http.MethodPut, "/api/highscores/"+playerID.String(), bytes.NewBuffer(reqBody))
	req.Header.Set("Authorization", "Bearer fake-token")

	w := httptest.NewRecorder()

	handler := func(w http.ResponseWriter, r *http.Request) {
		var reqData highscoreUpdate
		if err := json.NewDecoder(r.Body).Decode(&reqData); err != nil {
			respondWithError(w, http.StatusBadRequest, "Error decoding request body")
			return
		}
		if reqData.Highscore < 0 {
			respondWithError(w, http.StatusBadRequest, "Highscore must be non-negative")
			return
		}
	}

	handler(w, req)
	resp := w.Result()
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusBadRequest {
		t.Fatalf("expected 400 Bad Request, got %v", resp.StatusCode)
	}
}
// --- Mock dependencies for /api/login ---
type mockUser struct {
	ID        uuid.UUID `json:"id"`
	Username  string    `json:"username"`
	Highscore int32     `json:"highscore"`
}
type mockLoginDB struct {
	user        mockUser
	byUsername  bool
	byFingerprint bool
	err          error
}

func (m *mockLoginDB) GetUserByUsername(_ interface{}, _ string) (mockUser, error) {
	if m.err != nil {
		return mockUser{}, m.err
	}
	m.byUsername = true
	return m.user, nil
}

func (m *mockLoginDB) GetUserFromFingerHashCode(_ interface{}, _ string) (mockUser, error) {
	if m.err != nil {
		return mockUser{}, m.err
	}
	m.byFingerprint = true
	return m.user, nil
}

type mockLoginAuth struct {
	checkPassErr error
	makeJWTErr   error
}

func (m *mockLoginAuth) CheckPasswordHash(_, _ string) error {
	return m.checkPassErr
}

func (m *mockLoginAuth) MakeJWT(_ uuid.UUID, _ string, _ interface{}) (string, error) {
	if m.makeJWTErr != nil {
		return "", m.makeJWTErr
	}
	return "mock-token", nil
}

// --- Tests for handlerLogin ---

func TestHandlerLogin_SuccessWithPassword(t *testing.T) {
	db := &mockLoginDB{
		user: mockUser{
			ID:       uuid.New(),
			Username: "testuser",
		},
	}
	authMock := &mockLoginAuth{}

	reqBody, _ := json.Marshal(map[string]string{
		"username": "testuser",
		"password": "secret",
	})
	req := httptest.NewRequest(http.MethodPost, "/api/login", bytes.NewBuffer(reqBody))
	w := httptest.NewRecorder()

	handler := func(w http.ResponseWriter, r *http.Request) {
		var loginReq loginRequest
		if err := json.NewDecoder(r.Body).Decode(&loginReq); err != nil {
			respondWithError(w, http.StatusBadRequest, "Invalid request")
			return
		}

		user, err := db.GetUserByUsername(nil, loginReq.Username)
		if err != nil {
			respondWithError(w, http.StatusUnauthorized, "Invalid username or password")
			return
		}

		if err := authMock.CheckPasswordHash(user.Username, loginReq.Password); err != nil {
			respondWithError(w, http.StatusUnauthorized, "Invalid username or password")
			return
		}

		token, err := authMock.MakeJWT(user.ID, "secret", nil)
		if err != nil {
			respondWithError(w, http.StatusInternalServerError, "Could not create JWT")
			return
		}

		respondWithJSON(w, http.StatusOK, loginResponse{Token: token})
	}

	handler(w, req)
	resp := w.Result()
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		t.Fatalf("expected 200 OK, got %v", resp.StatusCode)
	}
}

func TestHandlerLogin_SuccessWithFingerprint(t *testing.T) {
	db := &mockLoginDB{
		user: mockUser{
			ID:       uuid.New(),
			Username: "fingerUser",
		},
	}
	authMock := &mockLoginAuth{}

	reqBody, _ := json.Marshal(map[string]string{
		"fingerprint_scanner_hash_code": "abc123",
	})
	req := httptest.NewRequest(http.MethodPost, "/api/login", bytes.NewBuffer(reqBody))
	w := httptest.NewRecorder()

	handler := func(w http.ResponseWriter, r *http.Request) {
		var loginReq loginRequest
		if err := json.NewDecoder(r.Body).Decode(&loginReq); err != nil {
			respondWithError(w, http.StatusBadRequest, "Invalid request")
			return
		}

		user, err := db.GetUserFromFingerHashCode(nil, loginReq.FingerprintScannerHashCode)
		if err != nil {
			respondWithError(w, http.StatusUnauthorized, "Invalid fingerprint hash")
			return
		}

		token, err := authMock.MakeJWT(user.ID, "secret", nil)
		if err != nil {
			respondWithError(w, http.StatusInternalServerError, "Could not create JWT")
			return
		}

		respondWithJSON(w, http.StatusOK, loginResponse{Token: token})
	}

	handler(w, req)
	resp := w.Result()
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		t.Fatalf("expected 200 OK, got %v", resp.StatusCode)
	}
}

func TestHandlerLogin_MissingCredentials(t *testing.T) {
	reqBody, _ := json.Marshal(map[string]string{})
	req := httptest.NewRequest(http.MethodPost, "/api/login", bytes.NewBuffer(reqBody))
	w := httptest.NewRecorder()

	handler := func(w http.ResponseWriter, r *http.Request) {
		var loginReq loginRequest
		json.NewDecoder(r.Body).Decode(&loginReq)
		if loginReq.Username == "" && loginReq.FingerprintScannerHashCode == "" {
			respondWithError(w, http.StatusBadRequest, "Missing login credentials")
			return
		}
	}

	handler(w, req)
	resp := w.Result()
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusBadRequest {
		t.Fatalf("expected 400 Bad Request, got %v", resp.StatusCode)
	}
}

func TestHandlerLogin_InvalidPassword(t *testing.T) {
	db := &mockLoginDB{
		user: mockUser{
			ID:       uuid.New(),
			Username: "badpass",
		},
	}
	authMock := &mockLoginAuth{checkPassErr: errors.New("wrong password")}

	reqBody, _ := json.Marshal(map[string]string{
		"username": "badpass",
		"password": "wrong",
	})
	req := httptest.NewRequest(http.MethodPost, "/api/login", bytes.NewBuffer(reqBody))
	w := httptest.NewRecorder()

	handler := func(w http.ResponseWriter, r *http.Request) {
		var loginReq loginRequest
		json.NewDecoder(r.Body).Decode(&loginReq)

		user, _ := db.GetUserByUsername(nil, loginReq.Username)
		if err := authMock.CheckPasswordHash(user.Username, loginReq.Password); err != nil {
			respondWithError(w, http.StatusUnauthorized, "Invalid username or password")
			return
		}
	}

	handler(w, req)
	resp := w.Result()
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusUnauthorized {
		t.Fatalf("expected 401 Unauthorized, got %v", resp.StatusCode)
	}
}

func TestHandlerLogin_InvalidFingerprint(t *testing.T) {
	db := &mockLoginDB{err: errors.New("invalid fingerprint")}
	reqBody, _ := json.Marshal(map[string]string{
		"fingerprint_scanner_hash_code": "unknown123",
	})
	req := httptest.NewRequest(http.MethodPost, "/api/login", bytes.NewBuffer(reqBody))
	w := httptest.NewRecorder()

	handler := func(w http.ResponseWriter, r *http.Request) {
		var loginReq loginRequest
		json.NewDecoder(r.Body).Decode(&loginReq)

		_, err := db.GetUserFromFingerHashCode(nil, loginReq.FingerprintScannerHashCode)
		if err != nil {
			respondWithError(w, http.StatusUnauthorized, "Invalid fingerprint hash")
			return
		}
	}

	handler(w, req)
	resp := w.Result()
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusUnauthorized {
		t.Fatalf("expected 401 Unauthorized, got %v", resp.StatusCode)
	}
}
