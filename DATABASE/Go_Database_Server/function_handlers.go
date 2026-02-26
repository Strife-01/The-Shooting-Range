package main

import (
	"encoding/json"
	"fmt"
	"net/http"
)

/*
func handlerHealthz(respw http.ResponseWriter, req *http.Request) {
	respw.Header().Set("Content-Type", "text/plain; charset=utf-8")
	respw.WriteHeader(http.StatusOK)
	respw.Write([]byte(http.StatusText(http.StatusOK)))
}
*/

func respondWithError(w http.ResponseWriter, code int, msg string) {
	type errorResponse struct {
		Error string `json:"error"`
	}
	respondWithJSON(w, code, errorResponse{Error: msg})
}

func respondWithJSON(w http.ResponseWriter, code int, payload interface{}) {
	data, err := json.Marshal(payload)
	if err != nil {
		fmt.Printf("Error marshalling JSON: %v\n", err)
		w.WriteHeader(http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(code)
	w.Write(data)
}
