package main

import (
	"encoding/json"
	"errors"
	"fmt"
	"strconv"
	"strings"
)

func ParseFloat32AnyJSON(raw json.RawMessage) ([]float32, error) {
	if len(raw) == 0 {
		return nil, errors.New("empty")
	}
	b := []byte(strings.TrimSpace(string(raw)))
	if string(b) == "null" {
		return []float32{}, nil
	}
	var arr []float64
	if err := json.Unmarshal(b, &arr); err == nil {
		out := make([]float32, len(arr))
		for i, v := range arr {
			out[i] = float32(v)
		}
		return out, nil
	}
	var n float64
	if err := json.Unmarshal(b, &n); err == nil {
		return []float32{float32(n)}, nil
	}
	var s string
	if err := json.Unmarshal(b, &s); err == nil {
		return ParseFloat32sFromString(s)
	}
	var obj struct {
		User  string          `json:"user"`
		Value json.RawMessage `json:"value"`
	}
	if err := json.Unmarshal(b, &obj); err == nil && (obj.User != "" || obj.Value != nil) {
		return ParseFloat32AnyJSON(obj.Value)
	}
	return nil, fmt.Errorf("unsupported float slice: %s", string(b))
}

func ParseFloat64sFromString(s string) ([]float64, error) {
	s = strings.TrimSpace(s)
	if s == "" {
		return nil, errors.New("empty")
	}
	parts := strings.Fields(s)
	out := make([]float64, 0, len(parts))
	for _, p := range parts {
		f, err := strconv.ParseFloat(p, 64)
		if err != nil {
			return nil, fmt.Errorf("parse float %q: %w", p, err)
		}
		out = append(out, f)
	}
	return out, nil
}

func ParseFloat32sFromString(s string) ([]float32, error) {
	s = strings.TrimSpace(s)
	if s == "" {
		return []float32{}, nil
	}
	parts := strings.Fields(s)
	out := make([]float32, 0, len(parts))
	for _, p := range parts {
		f, err := strconv.ParseFloat(p, 64)
		if err != nil {
			return nil, fmt.Errorf("bad float %q", p)
		}
		out = append(out, float32(f))
	}
	return out, nil
}
