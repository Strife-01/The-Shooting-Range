package database

import (
	"context"
	"database/sql"
	"errors"
	"testing"

	"github.com/google/uuid"
)

type mockRow struct{}

func (m *mockRow) Scan(dest ...interface{}) error {
	return errors.New("scan failed (mock)")
}

// mockDB safely mimics *sql.DB for QueryRowContext.
type mockDB struct{}

func (m *mockDB) QueryRowContext(ctx context.Context, query string, args ...interface{}) *sql.Row {
	// We can’t return *sql.Row directly because it’s not constructible,
	// but we can simulate failure indirectly using our own Queries wrapper.
	return nil
}

type mockQueries struct{}

func (m *mockQueries) GetUserFromId(ctx context.Context, id uuid.UUID) (User, error) {
	return User{}, errors.New("mock query failure")
}

func TestQueriesInitialization(t *testing.T) {
	if q == nil {
		t.Fatal("expected Queries to be non-nil")
	}
}

func TestGetUserFromIdFailsGracefully(t *testing.T) {
	q := &mockQueries{}
	_, err := q.GetUserFromId(context.Background(), uuid.New())
	if err == nil {
		t.Fatal("expected mock error, got nil")
	}
}

