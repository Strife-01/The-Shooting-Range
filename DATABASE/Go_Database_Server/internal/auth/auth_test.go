package auth

import (
	"testing"
	"time"
	"net/http"
	
	"github.com/google/uuid"
)

func TestHashAndCheckPassword(t *testing.T) {
	hash, err := HashPassword("secret123")
	if err != nil {
		t.Fatalf("failed to hash password: %v", err)
	}
	if err := CheckPasswordHash(hash, "secret123"); err != nil {
		t.Fatalf("password check failed: %v", err)
	}
}

func TestMakeAndValidateJWT(t *testing.T) {
	userID := uuid.New()
	secret := "testsecret"
	token, err := MakeJWT(userID, secret, time.Hour)
	if err != nil {
		t.Fatalf("failed to create JWT: %v", err)
	}

	validID, err := ValidateJWT(token, secret)
	if err != nil {
		t.Fatalf("failed to validate JWT: %v", err)
	}
	if validID != userID {
		t.Fatalf("expected userID %v, got %v", userID, validID)
	}
}

func TestGetBearerToken(t *testing.T) {
	t.Run("valid bearer token", func(t *testing.T) {
		h := http.Header{}
		h.Set("Authorization", "Bearer mytoken123")
		token, err := GetBearerToken(h)
		if err != nil {
			t.Fatalf("expected no error, got %v", err)
		}
		if token != "mytoken123" {
			t.Fatalf("expected token 'mytoken123', got '%v'", token)
		}
	})

	t.Run("missing header", func(t *testing.T) {
		h := http.Header{}
		_, err := GetBearerToken(h)
		if err == nil {
			t.Fatal("expected error for missing header, got nil")
		}
	})

	t.Run("invalid format", func(t *testing.T) {
		h := http.Header{}
		h.Set("Authorization", "Token mytoken123")
		_, err := GetBearerToken(h)
		if err == nil {
			t.Fatal("expected error for invalid prefix, got nil")
		}
	})
}

func TestGetAPIKey(t *testing.T) {
	t.Run("valid API key", func(t *testing.T) {
		h := http.Header{}
		h.Set("Authorization", "ApiKey 12345abcde")
		apiKey, err := GetAPIKey(h)
		if err != nil {
			t.Fatalf("expected no error, got %v", err)
		}
		if apiKey != "12345abcde" {
			t.Fatalf("expected key '12345abcde', got '%v'", apiKey)
		}
	})

	t.Run("invalid prefix", func(t *testing.T) {
		h := http.Header{}
		h.Set("Authorization", "Bearer 12345abcde")
		_, err := GetAPIKey(h)
		if err == nil {
			t.Fatal("expected error for invalid prefix, got nil")
		}
	})
}

func TestMakeRefreshToken(t *testing.T) {
	tok1, err1 := MakeRefreshToken()
	tok2, err2 := MakeRefreshToken()

	if err1 != nil || err2 != nil {
		t.Fatalf("unexpected error: %v %v", err1, err2)
	}
	if len(tok1) != 64 {
		t.Fatalf("expected 64-char hex string, got %d chars", len(tok1))
	}
	if tok1 == tok2 {
		t.Fatal("expected unique refresh tokens, got identical ones")
	}
}


func TestValidateJWTInvalid(t *testing.T) {

	userID := uuid.New()
	token, _ := MakeJWT(userID, "secret123", time.Second)
	time.Sleep(2 * time.Second)

	_, err := ValidateJWT(token, "secret123")
	if err == nil {
		t.Fatal("expected error for expired token, got nil")
	}

	_, err = ValidateJWT(token, "wrongsecret")
	if err == nil {
		t.Fatal("expected error for invalid secret, got nil")
	}
}
