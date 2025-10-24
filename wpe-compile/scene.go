package main

import (
	"encoding/json"
	"fmt"
	"strings"
)

type (
	Vec2 struct{ X, Y float64 }
	Vec3 struct{ X, Y, Z float64 }
)

func (v *Vec2) UnmarshalJSON(b []byte) error {
	var arr []float64
	if err := json.Unmarshal(b, &arr); err == nil {
		if len(arr) != 2 {
			return fmt.Errorf("vec2: expected 2 items, got %d", len(arr))
		}
		v.X, v.Y = arr[0], arr[1]
		return nil
	}
	var s string
	if err := json.Unmarshal(b, &s); err == nil {
		vals, err := ParseFloat64sFromString(s)
		if err != nil {
			return err
		}
		if len(vals) != 2 {
			return fmt.Errorf("vec2: expected 2 numbers in string, got %d", len(vals))
		}
		v.X, v.Y = vals[0], vals[1]
		return nil
	}
	return fmt.Errorf("vec2: unsupported json: %s", string(b))
}

func (v *Vec3) UnmarshalJSON(b []byte) error {
	var arr []float64
	if err := json.Unmarshal(b, &arr); err == nil {
		if len(arr) != 3 {
			return fmt.Errorf("vec3: expected 3 items, got %d", len(arr))
		}
		v.X, v.Y, v.Z = arr[0], arr[1], arr[2]
		return nil
	}
	var s string
	if err := json.Unmarshal(b, &s); err == nil {
		vals, err := ParseFloat64sFromString(s)
		if err != nil {
			return err
		}
		if len(vals) != 3 {
			return fmt.Errorf("vec3: expected 3 numbers in string, got %d", len(vals))
		}
		v.X, v.Y, v.Z = vals[0], vals[1], vals[2]
		return nil
	}
	return fmt.Errorf("vec3: unsupported json: %s", string(b))
}

type Param[T any] struct {
	User    string
	Value   T
	IsParam bool
}

func (p *Param[T]) UnmarshalJSON(b []byte) error {
	if len(b) > 0 && b[0] == '{' {
		var tmp struct {
			User  string          `json:"user"`
			Value json.RawMessage `json:"value"`
		}
		if err := json.Unmarshal(b, &tmp); err == nil && tmp.User != "" {
			var v T
			if err := json.Unmarshal(tmp.Value, &v); err != nil {
				return fmt.Errorf("param.value: %w", err)
			}
			p.User = tmp.User
			p.Value = v
			p.IsParam = true
			return nil
		}
	}
	var v T
	if err := json.Unmarshal(b, &v); err != nil {
		return err
	}
	p.Value = v
	p.IsParam = false
	return nil
}

type ScalarKind int

const (
	ScalarInvalid ScalarKind = iota
	ScalarNumber
	ScalarString
	ScalarBool
)

type Scalar struct {
	Kind  ScalarKind
	Num   float64
	Str   string
	Bool  bool
	Valid bool
}

func (s *Scalar) UnmarshalJSON(b []byte) error {
	b = []byte(strings.TrimSpace(string(b)))
	if string(b) == "null" {
		*s = Scalar{}
		return nil
	}
	if len(b) > 0 && b[0] == '"' {
		var v string
		if err := json.Unmarshal(b, &v); err != nil {
			return err
		}
		s.Kind, s.Str, s.Valid = ScalarString, v, true
		return nil
	}
	if string(b) == "true" || string(b) == "false" {
		var v bool
		if err := json.Unmarshal(b, &v); err != nil {
			return err
		}
		s.Kind, s.Bool, s.Valid = ScalarBool, v, true
		return nil
	}
	var f float64
	if err := json.Unmarshal(b, &f); err == nil {
		s.Kind, s.Num, s.Valid = ScalarNumber, f, true
		return nil
	}
	return fmt.Errorf("scalar: unsupported %s", string(b))
}

func (s Scalar) AsFloat(def float64) float64 {
	if s.Kind == ScalarNumber {
		return s.Num
	}
	return def
}

func (s Scalar) AsBool(def bool) bool {
	if s.Kind == ScalarBool {
		return s.Bool
	}
	return def
}

func (s Scalar) AsString(def string) string {
	if s.Kind == ScalarString {
		return s.Str
	}
	return def
}

func (s Scalar) AsVec2() (Vec2, bool) {
	if s.Kind == ScalarString {
		vals, err := ParseFloat64sFromString(s.Str)
		if err == nil && len(vals) == 2 {
			return Vec2{vals[0], vals[1]}, true
		}
	}
	return Vec2{}, false
}

func (s Scalar) AsVec3() (Vec3, bool) {
	if s.Kind == ScalarString {
		vals, err := ParseFloat64sFromString(s.Str)
		if err == nil && len(vals) == 3 {
			return Vec3{vals[0], vals[1], vals[2]}, true
		}
	}
	return Vec3{}, false
}

type ScalarOrParam struct {
	IsParam bool
	User    string
	Scalar  Scalar
}

func (sp *ScalarOrParam) UnmarshalJSON(b []byte) error {
	if len(b) > 0 && b[0] == '{' {
		var tmp struct {
			User  string          `json:"user"`
			Value json.RawMessage `json:"value"`
		}
		if err := json.Unmarshal(b, &tmp); err == nil && tmp.User != "" {
			var s Scalar
			if err := json.Unmarshal(tmp.Value, &s); err != nil {
				return fmt.Errorf("scalarOrParam.value: %w", err)
			}
			sp.IsParam = true
			sp.User = tmp.User
			sp.Scalar = s
			return nil
		}
	}
	var s Scalar
	if err := json.Unmarshal(b, &s); err != nil {
		return err
	}
	sp.IsParam = false
	sp.Scalar = s
	return nil
}

type ScriptedNumber struct {
	Script           string                   `json:"script"`
	ScriptProperties map[string]ScalarOrParam `json:"scriptproperties"`
	Value            float64                  `json:"value"`
}

type NumberOrScript struct {
	HasScript bool
	Number    float64
	Script    *ScriptedNumber
}

func (ns *NumberOrScript) UnmarshalJSON(b []byte) error {
	var f float64
	if err := json.Unmarshal(b, &f); err == nil {
		ns.HasScript = false
		ns.Number = f
		return nil
	}
	if len(b) > 0 && b[0] == '{' {
		var tmp ScriptedNumber
		if err := json.Unmarshal(b, &tmp); err != nil {
			return err
		}
		ns.HasScript = true
		ns.Script = &tmp
		return nil
	}
	return fmt.Errorf("NumberOrScript: unsupported %s", string(b))
}

type Camera struct {
	Center Vec3 `json:"center"`
	Eye    Vec3 `json:"eye"`
	Up     Vec3 `json:"up"`
}

type OrthoProjection struct {
	Width  int `json:"width"`
	Height int `json:"height"`
}

type General struct {
	AmbientColor                 Vec3            `json:"ambientcolor"`
	Bloom                        Param[bool]     `json:"bloom"`
	BloomHDRFeather              float64         `json:"bloomhdrfeather"`
	BloomHDRIterations           int             `json:"bloomhdriterations"`
	BloomHDRScatter              float64         `json:"bloomhdrscatter"`
	BloomHDRStrength             float64         `json:"bloomhdrstrength"`
	BloomHDRThreshold            float64         `json:"bloomhdrthreshold"`
	BloomStrength                Param[float64]  `json:"bloomstrength"`
	BloomThreshold               Param[float64]  `json:"bloomthreshold"`
	CameraFade                   bool            `json:"camerafade"`
	CameraParallax               bool            `json:"cameraparallax"`
	CameraParallaxAmount         float64         `json:"cameraparallaxamount"`
	CameraParallaxDelay          float64         `json:"cameraparallaxdelay"`
	CameraParallaxMouseInfluence float64         `json:"cameraparallaxmouseinfluence"`
	CameraPreview                bool            `json:"camerapreview"`
	CameraShake                  Param[bool]     `json:"camerashake"`
	CameraShakeAmplitude         float64         `json:"camerashakeamplitude"`
	CameraShakeRoughness         float64         `json:"camerashakeroughness"`
	CameraShakeSpeed             Param[float64]  `json:"camerashakespeed"`
	ClearColor                   Vec3            `json:"clearcolor"`
	ClearEnabled                 bool            `json:"clearenabled"`
	FarZ                         float64         `json:"farz"`
	FOV                          float64         `json:"fov"`
	HDR                          bool            `json:"hdr"`
	NearZ                        float64         `json:"nearz"`
	Ortho                        OrthoProjection `json:"orthogonalprojection"`
	SkyLightColor                Vec3            `json:"skylightcolor"`
	Zoom                         NumberOrScript  `json:"zoom"`
}

type EffectPass struct {
	ID        int                  `json:"id"`
	Combos    map[string]int       `json:"combos,omitempty"`
	Constants map[string][]float32 `json:"constantshadervalues"`
	Textures  []*string            `json:"textures,omitempty"`
}

func (p *EffectPass) UnmarshalJSON(b []byte) error {
	var obj map[string]json.RawMessage
	if err := json.Unmarshal(b, &obj); err != nil {
		return err
	}
	if raw, ok := obj["id"]; ok {
		_ = json.Unmarshal(raw, &p.ID)
	}
	if raw, ok := obj["combos"]; ok {
		var m map[string]int
		if err := json.Unmarshal(raw, &m); err == nil {
			p.Combos = m
		}
	}
	if raw, ok := obj["textures"]; ok {
		var arr []*string
		if err := json.Unmarshal(raw, &arr); err == nil {
			p.Textures = arr
		}
	}
	if raw, ok := obj["constantshadervalues"]; ok {
		var m map[string]json.RawMessage
		if err := json.Unmarshal(raw, &m); err != nil {
			return err
		}
		out := make(map[string][]float32, len(m))
		for k, v := range m {
			fs, err := ParseFloat32AnyJSON(v)
			if err != nil {
				return fmt.Errorf("constantshadervalues[%s]: %w", k, err)
			}
			out[k] = fs
		}
		p.Constants = out
	}
	return nil
}

type EffectInstance struct {
	File    string       `json:"file"`
	ID      int          `json:"id"`
	Name    string       `json:"name"`
	Passes  []EffectPass `json:"passes"`
	Visible Param[bool]  `json:"visible"`
}

type InstanceOverride map[string]ScalarOrParam

func (io *InstanceOverride) UnmarshalJSON(b []byte) error {
	if string(b) == "null" {
		*io = nil
		return nil
	}
	var m map[string]json.RawMessage
	if err := json.Unmarshal(b, &m); err != nil {
		return err
	}
	res := make(map[string]ScalarOrParam, len(m))
	for k, raw := range m {
		var v ScalarOrParam
		if err := json.Unmarshal(raw, &v); err != nil {
			return fmt.Errorf("instanceoverride[%s]: %w", k, err)
		}
		res[k] = v
	}
	*io = res
	return nil
}

type SceneObject struct {
	ID               int              `json:"id"`
	Name             string           `json:"name"`
	Alignment        string           `json:"alignment,omitempty"`
	Alpha            float64          `json:"alpha,omitempty"`
	Angles           Vec3             `json:"angles,omitempty"`
	Brightness       float64          `json:"brightness,omitempty"`
	Color            Vec3             `json:"color,omitempty"`
	ColorBlendMode   int              `json:"colorBlendMode,omitempty"`
	CopyBackground   bool             `json:"copybackground,omitempty"`
	Effects          []EffectInstance `json:"effects,omitempty"`
	Image            string           `json:"image,omitempty"`
	LEDSource        bool             `json:"ledsource,omitempty"`
	LockTransforms   bool             `json:"locktransforms,omitempty"`
	Origin           Vec3             `json:"origin,omitempty"`
	ParallaxDepth    Vec2             `json:"parallaxDepth,omitempty"`
	Perspective      bool             `json:"perspective,omitempty"`
	Scale            Vec3             `json:"scale,omitempty"`
	Size             *Vec2            `json:"size,omitempty"`
	Solid            bool             `json:"solid,omitempty"`
	Visible          Param[bool]      `json:"visible,omitempty"`
	Particle         string           `json:"particle,omitempty"`
	InstanceOverride InstanceOverride `json:"instanceoverride,omitempty"`
	MaxTime          float64          `json:"maxtime,omitempty"`
	MinTime          float64          `json:"mintime,omitempty"`
	MuteInEditor     bool             `json:"muteineditor,omitempty"`
	PlaybackMode     string           `json:"playbackmode,omitempty"`
	Sound            []string         `json:"sound,omitempty"`
	StartSilent      bool             `json:"startsilent,omitempty"`
	Volume           NumberOrScript   `json:"volume,omitempty"`
}

type Scene struct {
	Version int           `json:"version"`
	Camera  Camera        `json:"camera"`
	General General       `json:"general"`
	Objects []SceneObject `json:"objects"`
}

func parseSceneJSON(src string) (*Scene, error) {
	var sc Scene
	if err := json.Unmarshal([]byte(src), &sc); err != nil {
		return nil, err
	}
	return &sc, nil
}

func valueOrDefault(p Param[float64], def float64) float64 {
	if p.Value == 0 && !p.IsParam {
		return def
	}
	return p.Value
}

func boolOrDefault(p Param[bool], def bool) bool {
	return p.Value || (def && !p.IsParam)
}
