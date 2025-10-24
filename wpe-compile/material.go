package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"maps"
	"strconv"
)

type Material struct {
	Blending             string               `json:"blending"`
	CullMode             string               `json:"cullmode"`
	DepthTest            string               `json:"depthtest"`
	DepthWrite           string               `json:"depthwrite"`
	Shader               string               `json:"shader"`
	Textures             []string             `json:"textures"`
	Combos               map[string]int       `json:"combos"`
	ConstantShaderValues map[string][]float32 `json:"constantshadervalues"`
}

type BindItem struct {
	Name  string `json:"name"`
	Index int    `json:"index"`
}

type MaterialPass struct {
	Textures             []string             `json:"textures"`
	Combos               map[string]int       `json:"combos"`
	ConstantShaderValues map[string][]float32 `json:"constantshadervalues"`
	Target               string               `json:"target"`
	Bind                 []BindItem           `json:"bind"`
}

func parseMaterialJSON(data []byte) (Material, error) {
	var root struct {
		Passes []json.RawMessage `json:"passes"`
	}
	if err := json.Unmarshal(data, &root); err != nil {
		return Material{}, err
	}
	if len(root.Passes) == 0 {
		return Material{}, errors.New("material no data")
	}
	passObj := map[string]json.RawMessage{}
	if err := json.Unmarshal(root.Passes[0], &passObj); err != nil {
		return Material{}, err
	}
	mat := Material{
		Blending:             "translucent",
		CullMode:             "nocull",
		DepthTest:            "disabled",
		DepthWrite:           "disabled",
		Textures:             []string{},
		Combos:               map[string]int{},
		ConstantShaderValues: map[string][]float32{},
	}
	if raw, ok := passObj["shader"]; ok && !isJSONNull(raw) {
		if err := json.Unmarshal(raw, &mat.Shader); err != nil {
			return Material{}, fmt.Errorf("material shader: %w", err)
		}
	} else {
		return Material{}, errors.New("material no shader")
	}
	decodeOptionalString(passObj, "blending", &mat.Blending)
	decodeOptionalString(passObj, "cullmode", &mat.CullMode)
	decodeOptionalString(passObj, "depthtest", &mat.DepthTest)
	decodeOptionalString(passObj, "depthwrite", &mat.DepthWrite)
	if raw, ok := passObj["textures"]; ok && !isJSONNull(raw) {
		var anyArr []json.RawMessage
		if err := json.Unmarshal(raw, &anyArr); err != nil {
			return Material{}, fmt.Errorf("textures: %w", err)
		}
		for _, el := range anyArr {
			if isJSONNull(el) {
				mat.Textures = append(mat.Textures, "")
				continue
			}
			var s string
			if err := json.Unmarshal(el, &s); err != nil {
				mat.Textures = append(mat.Textures, "")
			} else {
				mat.Textures = append(mat.Textures, s)
			}
		}
	}
	if raw, ok := passObj["constantshadervalues"]; ok && !isJSONNull(raw) {
		var m map[string]json.RawMessage
		if err := json.Unmarshal(raw, &m); err != nil {
			return Material{}, fmt.Errorf("constantshadervalues: %w", err)
		}
		for k, v := range m {
			fs, err := ParseFloat32AnyJSON(v)
			if err != nil {
				return Material{}, fmt.Errorf("constantshadervalues[%s]: %w", k, err)
			}
			mat.ConstantShaderValues[k] = fs
		}
	}
	if raw, ok := passObj["combos"]; ok && !isJSONNull(raw) {
		var m map[string]json.RawMessage
		if err := json.Unmarshal(raw, &m); err != nil {
			return Material{}, fmt.Errorf("combos: %w", err)
		}
		for k, v := range m {
			val, err := parseint(v)
			if err != nil {
				return Material{}, fmt.Errorf("combos[%s]: %w", k, err)
			}
			mat.Combos[k] = val
		}
	}
	return mat, nil
}

func ParseMaterialPass(data []byte) (MaterialPass, error) {
	var obj map[string]json.RawMessage
	if err := json.Unmarshal(data, &obj); err != nil {
		return MaterialPass{}, err
	}
	var p MaterialPass
	p.Combos = map[string]int{}
	p.ConstantShaderValues = map[string][]float32{}
	if raw, ok := obj["textures"]; ok && !isJSONNull(raw) {
		var anyArr []json.RawMessage
		if err := json.Unmarshal(raw, &anyArr); err != nil {
			return MaterialPass{}, fmt.Errorf("textures: %w", err)
		}
		for _, el := range anyArr {
			if isJSONNull(el) {
				p.Textures = append(p.Textures, "")
				continue
			}
			var s string
			if err := json.Unmarshal(el, &s); err != nil {
				p.Textures = append(p.Textures, "")
			} else {
				p.Textures = append(p.Textures, s)
			}
		}
	}
	if raw, ok := obj["constantshadervalues"]; ok && !isJSONNull(raw) {
		var m map[string]json.RawMessage
		if err := json.Unmarshal(raw, &m); err != nil {
			return MaterialPass{}, fmt.Errorf("constantshadervalues: %w", err)
		}
		for k, v := range m {
			fs, err := ParseFloat32AnyJSON(v)
			if err != nil {
				return MaterialPass{}, fmt.Errorf("constantshadervalues[%s]: %w", k, err)
			}
			p.ConstantShaderValues[k] = fs
		}
	}
	if raw, ok := obj["combos"]; ok && !isJSONNull(raw) {
		var m map[string]json.RawMessage
		if err := json.Unmarshal(raw, &m); err != nil {
			return MaterialPass{}, fmt.Errorf("combos: %w", err)
		}
		for k, v := range m {
			val, err := parseint(v)
			if err != nil {
				return MaterialPass{}, fmt.Errorf("combos[%s]: %w", k, err)
			}
			p.Combos[k] = val
		}
	}
	decodeOptionalString(obj, "target", &p.Target)
	if raw, ok := obj["bind"]; ok && !isJSONNull(raw) {
		if err := json.Unmarshal(raw, &p.Bind); err != nil {
			return MaterialPass{}, fmt.Errorf("bind: %w", err)
		}
	}
	return p, nil
}

func (m *Material) MergePass(p MaterialPass) {
	if len(p.Textures) > len(m.Textures) {
		tmp := make([]string, len(p.Textures))
		copy(tmp, m.Textures)
		m.Textures = tmp
	}
	for i, el := range p.Textures {
		if el != "" {
			m.Textures[i] = el
		}
	}
	maps.Copy(m.ConstantShaderValues, p.ConstantShaderValues)
	maps.Copy(m.Combos, p.Combos)
}

func isJSONNull(raw json.RawMessage) bool {
	b := bytes.TrimSpace(raw)
	return bytes.Equal(b, []byte("null"))
}

func decodeOptionalString(obj map[string]json.RawMessage, key string, out *string) {
	if raw, ok := obj[key]; ok && !isJSONNull(raw) {
		_ = json.Unmarshal(raw, out)
	}
}

func parseint(raw json.RawMessage) (int, error) {
	var n int64
	if err := json.Unmarshal(raw, &n); err == nil {
		return int(n), nil
	}
	var b bool
	if err := json.Unmarshal(raw, &b); err == nil {
		if b {
			return 1, nil
		}
		return 0, nil
	}
	var s string
	if err := json.Unmarshal(raw, &s); err == nil {
		if s == "" {
			return 0, nil
		}
		f, err := strconv.ParseFloat(s, 64)
		if err != nil {
			return 0, fmt.Errorf("not an int: %q", s)
		}
		return int(f), nil
	}
	return 0, errors.New("unsupported int format")
}
