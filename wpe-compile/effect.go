package main

import (
	"encoding/json"
	"errors"
	"fmt"
	"strings"
)

type Effect struct {
	Version        int            `json:"version"`
	ReplacementKey string         `json:"replacementkey"`
	Name           string         `json:"name"`
	Description    string         `json:"description"`
	Group          string         `json:"group"`
	Preview        string         `json:"preview"`
	Passes         []Pass         `json:"passes"`
	FBOs           map[string]FBO `json:"fbos"`
	Dependencies   []string       `json:"dependencies"`
}

type Pass struct {
	Material string         `json:"material"`
	Target   string         `json:"target,omitempty"`
	Bind     map[string]int `json:"bind"`
}

type FBO struct {
	Name   string  `json:"name"`
	Scale  float64 `json:"scale"`
	Format string  `json:"format"`
}

type rawEffect struct {
	Version        int       `json:"version"`
	ReplacementKey string    `json:"replacementkey"`
	Name           string    `json:"name"`
	Description    string    `json:"description"`
	Group          string    `json:"group"`
	Preview        string    `json:"preview"`
	Passes         []rawPass `json:"passes"`
	FBOs           []FBO     `json:"fbos"`
	Dependencies   []string  `json:"dependencies"`
}

type rawPass struct {
	Material string `json:"material"`
	Target   string `json:"target,omitempty"`
	Bind     []Bind `json:"bind"`
}

type Bind struct {
	Name  string `json:"name"`
	Index int    `json:"index"`
}

func parseEffect(jsonBytes []byte) (*Effect, error) {
	var r rawEffect
	if err := json.Unmarshal(jsonBytes, &r); err != nil {
		return nil, fmt.Errorf("invalid JSON: %w", err)
	}
	if r.Version <= 0 {
		return nil, errors.New("missing or invalid 'version'")
	}
	if len(r.Passes) == 0 {
		return nil, errors.New("missing 'passes'")
	}

	out := Effect{
		Version:        r.Version,
		ReplacementKey: r.ReplacementKey,
		Name:           r.Name,
		Description:    r.Description,
		Group:          r.Group,
		Preview:        r.Preview,
		Dependencies:   r.Dependencies,
		FBOs:           map[string]FBO{},
	}

	for _, f := range r.FBOs {
		if strings.TrimSpace(f.Name) == "" {
			return nil, errors.New("fbos: missing 'name'")
		}
		if _, exists := out.FBOs[f.Name]; exists {
			return nil, fmt.Errorf("fbos: duplicate name '%s'", f.Name)
		}
		out.FBOs[f.Name] = f
	}

	out.Passes = make([]Pass, len(r.Passes))
	for i, rp := range r.Passes {
		if strings.TrimSpace(rp.Material) == "" {
			return nil, fmt.Errorf("pass %d: missing 'material'", i)
		}
		bmap := make(map[string]int, len(rp.Bind))
		for _, b := range rp.Bind {
			if strings.TrimSpace(b.Name) == "" {
				return nil, fmt.Errorf("pass %d: bind entry with empty 'name'", i)
			}
			if _, dup := bmap[b.Name]; dup {
				return nil, fmt.Errorf("pass %d: duplicate bind '%s'", i, b.Name)
			}
			bmap[b.Name] = b.Index
		}
		out.Passes[i] = Pass{
			Material: rp.Material,
			Target:   rp.Target,
			Bind:     bmap,
		}
	}

	return &out, nil
}
