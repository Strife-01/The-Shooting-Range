package auth

import (
	"crypto/rand"
	"crypto/sha512"
	"encoding/hex"
	"fmt"
	"net/http"
	"strings"
	"time"

	"github.com/golang-jwt/jwt/v5"
	"github.com/google/uuid"
	"golang.org/x/crypto/bcrypt"
)

type MyClaim struct {
	UserID uuid.UUID `json:"user_id"`
	jwt.RegisteredClaims
}

func HashPassword(password string) (string, error) {
	hashed_pass, err := bcrypt.GenerateFromPassword([]byte(password), 10)
	if err != nil {
		return "", fmt.Errorf("failed to hash password: %w", err)
	}
	return string(hashed_pass), nil
}

func CheckPasswordHash(hash, password string) error {
	return bcrypt.CompareHashAndPassword([]byte(hash), []byte(password))
}

func HashAccessKey(accessKey string) (string, error) {
	hash := sha512.Sum512([]byte(accessKey))
	return hex.EncodeToString(hash[:]), nil
}

func CheckAccessKeyHash(storedHash, accessKey string) error {
	hash := sha512.Sum512([]byte(accessKey))
	computedHash := hex.EncodeToString(hash[:])
	
	if storedHash != computedHash {
		return fmt.Errorf("access key does not match")
	}
	return nil
}

func MakeJWT(userID uuid.UUID, tokenSecret string, expiresIn time.Duration) (string, error) {
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, MyClaim{
		UserID: userID,
		RegisteredClaims: jwt.RegisteredClaims{
			Issuer:    "the_shooting_range",
			ExpiresAt: jwt.NewNumericDate(time.Now().Add(expiresIn)),
			IssuedAt:  jwt.NewNumericDate(time.Now()),
		},
	})

	tokenString, err := token.SignedString([]byte(tokenSecret))
	if err != nil {
		return "", fmt.Errorf("failed to sign JWT: %w", err)
	}
	return tokenString, nil
}

func ValidateJWT(tokenString, tokenSecret string) (uuid.UUID, error) {
	token, err := jwt.ParseWithClaims(tokenString, &MyClaim{}, func(token *jwt.Token) (interface{}, error) {
		if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {
			return nil, fmt.Errorf("unexpected signing method: %v", token.Header["alg"])
		}
		return []byte(tokenSecret), nil
	})
	if err != nil {
		return uuid.UUID{}, fmt.Errorf("failed to parse or validate JWT: %w", err)
	}

	if claims, ok := token.Claims.(*MyClaim); ok && token.Valid {
		if claims.UserID == uuid.Nil {
			return uuid.UUID{}, fmt.Errorf("JWT user_id claim is empty (uuid.Nil)")
		}
		return claims.UserID, nil
	}

	return uuid.UUID{}, fmt.Errorf("invalid token or claims type assertion failed")
}

func GetBearerToken(header http.Header) (string, error) {
	auth_header_string := header.Get("Authorization")
	if len(auth_header_string) == 0 {
		return "", fmt.Errorf("Authorization header is missing")
	}

	if !strings.HasPrefix(auth_header_string, "Bearer ") {
		return "", fmt.Errorf("Authorization header must be in 'Bearer <token>' format")
	}

	token_string := strings.TrimPrefix(auth_header_string, "Bearer ")
	return token_string, nil
}

func MakeRefreshToken() (string, error) {
	key := make([]byte, 32)
	rand.Read(key)

	return hex.EncodeToString(key), nil
}

func GetAPIKey(header http.Header) (string, error) {
	auth_header_string := header.Get("Authorization")
	if len(auth_header_string) == 0 {
		return "", fmt.Errorf("Authorization header is missing")
	}

	if !strings.HasPrefix(auth_header_string, "ApiKey ") {
		return "", fmt.Errorf("Authorization header must be in 'ApiKey <api_key>' format")
	}

	api_key := strings.TrimPrefix(auth_header_string, "ApiKey ")
	return api_key, nil
}
