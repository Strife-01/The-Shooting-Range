FROM golang:1.24.3 AS builder

WORKDIR /go_game_server

# Cache dependencies first
COPY go.mod go.sum ./
RUN go mod download

# Copy the rest of the source code
COPY . .

# Build the binary
RUN go build -o game_server .


FROM debian:bookworm-slim

WORKDIR /go_game_server

# Copy binary from builder
COPY --from=builder /go_game_server/game_server /go_game_server/game_server

# Copy .env for runtime
COPY .env /go_game_server/.env

EXPOSE 8080

CMD ["/go_game_server/game_server"]

