package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"maps"
	"os"
	"strconv"
	"strings"
)

type (
	Vector2 [2]float32
	Vector3 [3]float32
)

type (
	BoolValue   bool
	IntValue    int
	FloatValue  float32
	StringValue string
	FloatSlice  []float32
	NullString  string
)

func (value *StringValue) UnmarshalJSON(raw []byte) error {
	parsed, err := parseStringFromRaw(raw)
	if err != nil {
		return err
	}
	*value = StringValue(parsed)
	return nil
}

func (value *BoolValue) UnmarshalJSON(raw []byte) error {
	parsed, err := parseBoolFromRaw(raw)
	if err != nil {
		return err
	}
	*value = BoolValue(parsed)
	return nil
}

func (value *FloatValue) UnmarshalJSON(raw []byte) error {
	parsed, err := parseFloat64FromRaw(raw)
	if err != nil {
		return err
	}
	*value = FloatValue(parsed)
	return nil
}

func (value *IntValue) UnmarshalJSON(raw []byte) error {
	parsed, err := parseIntFromRaw(raw)
	if err != nil {
		return err
	}
	*value = IntValue(parsed)
	return nil
}

func (value *FloatSlice) UnmarshalJSON(raw []byte) error {
	parsed, err := parseFloatSliceFromRaw(raw)
	if err != nil {
		return err
	}
	*value = FloatSlice(parsed)
	return nil
}

func (value *NullString) UnmarshalJSON(raw []byte) error {
	if bytesFromRawNullAware(raw) == nil {
		*value = ""
		return nil
	}
	var str StringValue
	if err := json.Unmarshal(raw, &str); err != nil {
		return err
	}
	*value = NullString(str)
	return nil
}

func (vector *Vector2) UnmarshalJSON(raw []byte) error {
	parsed, err := parseVector2FromRaw(raw, *vector)
	if err != nil {
		return err
	}
	*vector = parsed
	return nil
}

func (vector *Vector3) UnmarshalJSON(raw []byte) error {
	parsed, err := parseVector3FromRaw(raw, *vector)
	if err != nil {
		return err
	}
	*vector = parsed
	return nil
}

func bytesFromRawNullAware(raw json.RawMessage) []byte {
	trimmed := bytes.TrimSpace(raw)
	if bytes.Equal(trimmed, []byte("null")) {
		return nil
	}
	return raw
}

func unwrapJSONValue(raw json.RawMessage) json.RawMessage {
	trimmed := bytes.TrimSpace(raw)
	if len(trimmed) == 0 || trimmed[0] != '{' {
		return raw
	}
	trimmed = bytes.Trim(raw, ",")
	if len(trimmed) == 0 || trimmed[0] != '{' {
		return raw
	}

	var object map[string]json.RawMessage
	err := json.Unmarshal(raw, &object)
	if err != nil {
		return raw
	}
	if value, exists := object["value"]; exists {
		return value
	}
	return raw
}

func parseStringFromRaw(raw json.RawMessage) (string, error) {
	if bytesFromRawNullAware(raw) == nil {
		return "", nil
	}
	raw = unwrapJSONValue(raw)
	var value string
	err := json.Unmarshal(raw, &value)
	if err != nil {
		return "", fmt.Errorf("value is not a string: %w", err)
	}
	return value, nil
}

func parseBoolFromRaw(raw json.RawMessage) (bool, error) {
	raw = unwrapJSONValue(raw)
	var value bool
	err := json.Unmarshal(raw, &value)
	if err == nil {
		return value, nil
	}
	var text string
	err = json.Unmarshal(raw, &text)
	if err == nil {
		text = strings.TrimSpace(strings.ToLower(text))
		if text == "true" {
			return true, nil
		}
		if text == "false" {
			return false, nil
		}
		return false, fmt.Errorf("cannot parse bool from string %q", text)
	}
	return false, errors.New("value is not a bool or string")
}

func parseFloat64FromRaw(raw json.RawMessage) (float64, error) {
	raw = unwrapJSONValue(raw)
	var number float64
	err := json.Unmarshal(raw, &number)
	if err == nil {
		return number, nil
	}
	var text string
	err = json.Unmarshal(raw, &text)
	if err == nil {
		text = strings.TrimSpace(text)
		text = strings.Trim(text, ",")
		if text == "" {
			return 0, errors.New("empty string for float value")
		}
		value, parseErr := strconv.ParseFloat(text, 64)
		if parseErr != nil {
			return 0, fmt.Errorf("cannot parse float from string %q: %w", text, parseErr)
		}
		return value, nil
	}
	return 0, errors.New("value is not a number or string")
}

func parseIntFromRaw(raw json.RawMessage) (int, error) {
	raw = unwrapJSONValue(raw)
	var integer int
	err := json.Unmarshal(raw, &integer)
	if err == nil {
		return integer, nil
	}
	var number float64
	err = json.Unmarshal(raw, &number)
	if err == nil {
		return int(number), nil
	}
	var text string
	err = json.Unmarshal(raw, &text)
	if err == nil {
		text = strings.TrimSpace(text)
		if text == "" {
			return 0, errors.New("empty string for int value")
		}
		parsed, parseErr := strconv.ParseInt(text, 10, 64)
		if parseErr != nil {
			return 0, fmt.Errorf("cannot parse int from string %q: %w", text, parseErr)
		}
		return int(parsed), nil
	}
	return 0, errors.New("value is not an int, number or string")
}

func parseFloatSliceFromRaw(raw json.RawMessage) ([]float32, error) {
	raw = unwrapJSONValue(raw)

	var arrayOfNumbers []float64
	err := json.Unmarshal(raw, &arrayOfNumbers)
	if err == nil {
		result := make([]float32, len(arrayOfNumbers))
		for index, value := range arrayOfNumbers {
			result[index] = float32(value)
		}
		return result, nil
	}

	var singleNumber float64
	err = json.Unmarshal(raw, &singleNumber)
	if err == nil {
		return []float32{float32(singleNumber)}, nil
	}

	var text string
	err = json.Unmarshal(raw, &text)
	if err == nil {
		text = strings.TrimSpace(text)
		if text == "" {
			return nil, errors.New("empty string for float slice")
		}
		parts := strings.Fields(text)
		result := make([]float32, len(parts))
		for index, part := range parts {
			part = strings.Trim(part, ",")
			value, parseErr := strconv.ParseFloat(part, 64)
			if parseErr != nil {
				return nil, fmt.Errorf("cannot parse float from string %q: %w", part, parseErr)
			}
			result[index] = float32(value)
		}
		return result, nil
	}

	return nil, errors.New("value is not an array, number or string")
}

func parseVector2FromRaw(raw json.RawMessage, defaultValue Vector2) (Vector2, error) {
	values, err := parseFloatSliceFromRaw(raw)
	if err != nil {
		return defaultValue, err
	}
	if len(values) != 2 {
		return defaultValue, fmt.Errorf("expected 2 components, got %d", len(values))
	}
	return Vector2{values[0], values[1]}, nil
}

func parseVector3FromRaw(raw json.RawMessage, defaultValue Vector3) (Vector3, error) {
	values, err := parseFloatSliceFromRaw(raw)
	if err != nil {
		return defaultValue, err
	}
	if len(values) == 1 {
		return Vector3{values[0], values[0], values[0]}, nil
	}
	if len(values) == 3 {
		return Vector3{values[0], values[1], values[2]}, nil
	}
	return defaultValue, fmt.Errorf("expected 3 components, got %d", len(values))
}

func loadBytesFromPackage(pkgMap *map[string][]byte, path string) ([]byte, error) {
	if pkgMap == nil {
		return nil, errors.New("package map pointer is nil")
	}
	if data, exists := (*pkgMap)[path]; exists {
		return data, nil
	}
	if data, exists := (*pkgMap)["/assets/"+path]; exists {
		return data, nil
	}
	if data, exists := (*pkgMap)["assets/"+path]; exists {
		return data, nil
	}
	data, err := os.ReadFile(env.Assets + "/" + path)
	if err == nil {
		return data, nil
	}
	return nil, fmt.Errorf("file %s not found", path)
}

type MaterialPassBindItem struct {
	Name  string `json:"name"`
	Index int    `json:"index"`
}

type MaterialPass struct {
	Textures  []string
	Combos    map[string]int
	Constants map[string][]float32
	Target    string
	Bind      []MaterialPassBindItem
}

type Material struct {
	Blending string
	Shader   string
	Textures []string
	Combos   map[string]int
}

func (materialPass *MaterialPass) parseFromJSON(raw json.RawMessage) error {
	payload := struct {
		Textures             []NullString           `json:"textures"`
		ConstantShaderValues map[string]FloatSlice  `json:"constantshadervalues"`
		Combos               map[string]IntValue    `json:"combos"`
		Target               StringValue            `json:"target"`
		Bind                 []MaterialPassBindItem `json:"bind"`
	}{}

	if err := json.Unmarshal(raw, &payload); err != nil {
		return fmt.Errorf("cannot parse material pass: %w", err)
	}

	for _, texture := range payload.Textures {
		materialPass.Textures = append(materialPass.Textures, string(texture))
	}

	if len(payload.ConstantShaderValues) > 0 {
		if materialPass.Constants == nil {
			materialPass.Constants = make(map[string][]float32)
		}
		for key, value := range payload.ConstantShaderValues {
			materialPass.Constants[key] = value
		}
	}

	if len(payload.Combos) > 0 {
		if materialPass.Combos == nil {
			materialPass.Combos = make(map[string]int)
		}
		for key, value := range payload.Combos {
			materialPass.Combos[key] = int(value)
		}
	}

	if payload.Target != "" {
		materialPass.Target = string(payload.Target)
	}

	if len(payload.Bind) > 0 {
		materialPass.Bind = append(materialPass.Bind, payload.Bind...)
	}

	return nil
}

func (materialPass *MaterialPass) updateFrom(other MaterialPass) {
	if len(other.Textures) > len(materialPass.Textures) {
		newTextures := make([]string, len(other.Textures))
		copy(newTextures, materialPass.Textures)
		materialPass.Textures = newTextures
	}
	for index, textureName := range other.Textures {
		if textureName != "" {
			materialPass.Textures[index] = textureName
		}
	}
	if materialPass.Combos == nil {
		materialPass.Combos = make(map[string]int)
	}
	maps.Copy(materialPass.Combos, other.Combos)
	if materialPass.Constants == nil {
		materialPass.Constants = make(map[string][]float32)
	}
	maps.Copy(materialPass.Constants, other.Constants)
	if other.Target != "" {
		materialPass.Target = other.Target
	}
	if len(other.Bind) > 0 {
		materialPass.Bind = other.Bind
	}
}

func (material *Material) mergePass(other MaterialPass) {
	if len(other.Textures) > len(material.Textures) {
		newTextures := make([]string, len(other.Textures))
		copy(newTextures, material.Textures)
		material.Textures = newTextures
	}
	for index, textureName := range other.Textures {
		if textureName != "" {
			material.Textures[index] = textureName
		}
	}
	if material.Combos == nil {
		material.Combos = make(map[string]int)
	}
	maps.Copy(material.Combos, other.Combos)
}

func (material *Material) parseFromJSON(raw json.RawMessage) error {
	type materialPassJSON struct {
		Blending StringValue         `json:"blending"`
		Shader   StringValue         `json:"shader"`
		Textures []NullString        `json:"textures"`
		Combos   map[string]IntValue `json:"combos"`
	}

	type materialJSON struct {
		Passes []materialPassJSON `json:"passes"`
	}

	var payload materialJSON
	if err := json.Unmarshal(raw, &payload); err != nil {
		return fmt.Errorf("cannot parse material document: %w", err)
	}
	if len(payload.Passes) == 0 {
		return fmt.Errorf("material has empty passes array")
	}

	firstPass := payload.Passes[0]
	if firstPass.Blending == "" || firstPass.Shader == "" {
		return fmt.Errorf("material pass is missing required fields")
	}

	material.Blending = string(firstPass.Blending)
	material.Shader = string(firstPass.Shader)

	for _, texture := range firstPass.Textures {
		material.Textures = append(material.Textures, string(texture))
	}
	if len(firstPass.Combos) > 0 {
		if material.Combos == nil {
			material.Combos = make(map[string]int)
		}
		for key, value := range firstPass.Combos {
			material.Combos[key] = int(value)
		}
	}

	return nil
}

type EmptyObject struct {
	ID     int
	Parent int
	Origin Vector3
	Scale  Vector3
	Angles Vector3
}

func (object *EmptyObject) parseFromSceneJSON(raw json.RawMessage) error {
	payload := struct {
		ID     IntValue `json:"id"`
		Parent IntValue `json:"parent"`
		Origin Vector3
		Scale  Vector3
		Angles Vector3
	}{
		Parent: -1,
		Scale:  Vector3{1, 1, 1},
	}

	if err := json.Unmarshal(raw, &payload); err != nil {
		return fmt.Errorf("cannot parse empty object: %w", err)
	}
	object.ID = int(payload.ID)
	object.Parent = int(payload.Parent)
	object.Origin = payload.Origin
	object.Scale = payload.Scale
	object.Angles = payload.Angles
	return nil
}

type EffectFBO struct {
	Name  string
	Scale int
}

type ImageEffect struct {
	Name      string
	Passes    []MaterialPass
	FBOs      []EffectFBO
	Materials []Material
}

func (fbo *EffectFBO) parseFromJSON(raw json.RawMessage) error {
	payload := struct {
		Name  StringValue `json:"name"`
		Scale IntValue    `json:"scale"`
	}{
		Scale: 1,
	}

	if err := json.Unmarshal(raw, &payload); err != nil {
		return fmt.Errorf("cannot parse effect fbo: %w", err)
	}
	if payload.Name == "" {
		return fmt.Errorf("effect fbo missing name")
	}
	fbo.Name = string(payload.Name)
	fbo.Scale = int(payload.Scale)
	return nil
}

func (effect *ImageEffect) parseFromFileJSON(raw json.RawMessage, pkgMap *map[string][]byte) error {
	payload := struct {
		Name   StringValue       `json:"name"`
		FBOs   []json.RawMessage `json:"fbos"`
		Passes []json.RawMessage `json:"passes"`
	}{}

	if err := json.Unmarshal(raw, &payload); err != nil {
		return fmt.Errorf("cannot parse effect file JSON: %w", err)
	}

	if payload.Name == "" {
		return fmt.Errorf("effect has no name")
	}
	effect.Name = string(payload.Name)

	for _, fboRaw := range payload.FBOs {
		var fbo EffectFBO
		if err := fbo.parseFromJSON(fboRaw); err != nil {
			return err
		}
		effect.FBOs = append(effect.FBOs, fbo)
	}

	if len(payload.Passes) == 0 {
		return errors.New("effect has no passes field")
	}

	composeEnabled := false

	for _, passRaw := range payload.Passes {
		passProbe := struct {
			Material StringValue `json:"material"`
			Command  StringValue `json:"command"`
			Compose  BoolValue   `json:"compose"`
		}{}
		if err := json.Unmarshal(passRaw, &passProbe); err != nil {
			return fmt.Errorf("cannot parse effect pass object: %w", err)
		}

		if passProbe.Material == "" {
			if passProbe.Command != "" {
				continue
			}
			return errors.New("effect pass has no material")
		}

		materialPath := string(passProbe.Material)
		materialBytes, err := loadBytesFromPackage(pkgMap, materialPath)
		if err != nil {
			return fmt.Errorf("cannot load material file %s: %w", materialPath, err)
		}
		var material Material
		if err := material.parseFromJSON(materialBytes); err != nil {
			return fmt.Errorf("cannot parse material %s: %w", materialPath, err)
		}
		effect.Materials = append(effect.Materials, material)

		var pass MaterialPass
		if err := pass.parseFromJSON(passRaw); err != nil {
			return err
		}
		effect.Passes = append(effect.Passes, pass)

		if bool(passProbe.Compose) {
			composeEnabled = true
		}
	}

	if composeEnabled {
		if len(effect.Passes) != 2 {
			return errors.New("effect compose option error: expected exactly 2 passes")
		}
		fbo := EffectFBO{
			Name:  "_rt_FullCompoBuffer1",
			Scale: 1,
		}
		effect.FBOs = append(effect.FBOs, fbo)

		firstPass := &effect.Passes[0]
		firstPass.Bind = append(firstPass.Bind, MaterialPassBindItem{
			Name:  "previous",
			Index: 0,
		})
		firstPass.Target = "_rt_FullCompoBuffer1"

		secondPass := &effect.Passes[1]
		secondPass.Bind = append(secondPass.Bind, MaterialPassBindItem{
			Name:  "_rt_FullCompoBuffer1",
			Index: 0,
		})
	}

	return nil
}

func (effect *ImageEffect) parseFromSceneJSON(raw json.RawMessage, pkgMap *map[string][]byte) error {
	payload := struct {
		File   StringValue       `json:"file"`
		Name   StringValue       `json:"name"`
		Passes []json.RawMessage `json:"passes"`
	}{}

	if err := json.Unmarshal(raw, &payload); err != nil {
		return fmt.Errorf("cannot parse image effect from scene: %w", err)
	}
	if payload.File == "" {
		return fmt.Errorf("effect entry missing file path")
	}
	effectPath := string(payload.File)

	effectBytes, err := loadBytesFromPackage(pkgMap, effectPath)
	if err != nil {
		return fmt.Errorf("cannot load effect file %s: %w", effectPath, err)
	}
	if err := effect.parseFromFileJSON(effectBytes, pkgMap); err != nil {
		return err
	}

	if strings.TrimSpace(string(payload.Name)) != "" {
		effect.Name = string(payload.Name)
	}

	for index, passOverrideRaw := range payload.Passes {
		if index >= len(effect.Passes) {
			break
		}
		var overridePass MaterialPass
		parseErr := overridePass.parseFromJSON(passOverrideRaw)
		if parseErr != nil {
			return parseErr
		}
		effect.Passes[index].updateFrom(overridePass)
		if index < len(effect.Materials) {
			effect.Materials[index].mergePass(overridePass)
		}
	}

	return nil
}

type ImageConfig struct {
	Passthrough bool
}

type ImageObject struct {
	ID             int
	Parent         int
	Name           string
	Origin         Vector3
	Scale          Vector3
	Angles         Vector3
	Size           Vector2
	Perspective    bool
	ParallaxDepth  Vector2
	Color          Vector3
	ColorBlendMode int
	Alpha          float32
	Brightness     float32
	Fullscreen     bool
	Material       Material
	Effects        []ImageEffect
	Config         ImageConfig
}

func (imageObject *ImageObject) parseFromSceneJSON(raw json.RawMessage, pkgMap *map[string][]byte) error {
	type configJSON struct {
		Passthrough BoolValue `json:"passthrough"`
	}

	type imageSceneJSON struct {
		ID             IntValue          `json:"id"`
		Parent         IntValue          `json:"parent"`
		Name           StringValue       `json:"name"`
		Image          StringValue       `json:"image"`
		Origin         Vector3           `json:"origin"`
		Scale          Vector3           `json:"scale"`
		Angles         Vector3           `json:"angles"`
		Size           Vector2           `json:"size"`
		Perspective    BoolValue         `json:"perspective"`
		ParallaxDepth  Vector2           `json:"parallaxDepth"`
		Color          Vector3           `json:"color"`
		ColorBlendMode IntValue          `json:"colorBlendMode"`
		Alpha          FloatValue        `json:"alpha"`
		Brightness     FloatValue        `json:"brightness"`
		Effects        []json.RawMessage `json:"effects"`
		Config         configJSON        `json:"config"`
	}

	type imageModelJSON struct {
		Fullscreen BoolValue   `json:"fullscreen"`
		Width      IntValue    `json:"width"`
		Height     IntValue    `json:"height"`
		Material   StringValue `json:"material"`
	}

	payload := imageSceneJSON{
		Parent:     -1,
		Scale:      Vector3{1, 1, 1},
		Color:      Vector3{1, 1, 1},
		Alpha:      1,
		Brightness: 1,
	}

	if err := json.Unmarshal(raw, &payload); err != nil {
		return fmt.Errorf("cannot parse image object: %w", err)
	}
	if payload.Image == "" {
		return fmt.Errorf("image object missing image path")
	}
	imagePath := string(payload.Image)

	imageObject.Name = string(payload.Name)
	imageObject.ID = int(payload.ID)
	imageObject.Parent = int(payload.Parent)
	imageObject.ColorBlendMode = int(payload.ColorBlendMode)
	imageObject.Origin = payload.Origin
	imageObject.Angles = payload.Angles
	imageObject.Scale = payload.Scale
	imageObject.Perspective = bool(payload.Perspective)
	imageObject.ParallaxDepth = payload.ParallaxDepth
	imageObject.Color = payload.Color
	imageObject.Alpha = float32(payload.Alpha)
	imageObject.Brightness = float32(payload.Brightness)

	modelBytes, err := loadBytesFromPackage(pkgMap, imagePath)
	if err != nil {
		return fmt.Errorf("cannot load model image JSON %s: %w", imagePath, err)
	}
	model := imageModelJSON{}
	if err := json.Unmarshal(modelBytes, &model); err != nil {
		return fmt.Errorf("cannot parse model image JSON %s: %w", imagePath, err)
	}

	imageObject.Fullscreen = bool(model.Fullscreen)

	if !imageObject.Fullscreen {
		if imageObject.Origin == (Vector3{}) {
			return fmt.Errorf("image object %s missing origin", imagePath)
		}

		switch {
		case model.Width != 0 && model.Height != 0:
			imageObject.Size = Vector2{float32(model.Width), float32(model.Height)}
		case payload.Size != (Vector2{}):
			imageObject.Size = payload.Size
		default:
			imageObject.Size = Vector2{imageObject.Origin[0] * 2, imageObject.Origin[1] * 2}
		}
	}

	if model.Material == "" {
		return fmt.Errorf("image object has no material in model JSON %s", imagePath)
	}
	materialPath := string(model.Material)
	materialBytes, err := loadBytesFromPackage(pkgMap, materialPath)
	if err != nil {
		return fmt.Errorf("cannot load material %s: %w", materialPath, err)
	}
	if err := imageObject.Material.parseFromJSON(materialBytes); err != nil {
		return fmt.Errorf("cannot parse material %s: %w", materialPath, err)
	}

	for _, effectRaw := range payload.Effects {
		var effect ImageEffect
		if err := effect.parseFromSceneJSON(effectRaw, pkgMap); err != nil {
			return err
		}
		imageObject.Effects = append(imageObject.Effects, effect)
	}

	imageObject.Config.Passthrough = bool(payload.Config.Passthrough)

	return nil
}

type ParticleRenderer struct {
	Name string
}

type ParticleInitializer struct {
	MinLifetime        float32
	MaxLifetime        float32
	MinSize            float32
	MaxSize            float32
	MinVelocity        [3]float32
	MaxVelocity        [3]float32
	MinRotation        [3]float32
	MaxRotation        [3]float32
	MinAngularVelocity [3]float32
	MaxAngularVelocity [3]float32
	MinColor           [3]float32
	MaxColor           [3]float32
	MinAlpha           float32
	MaxAlpha           float32
}

type MovementOperator struct {
	Enabled bool
	Drag    float32
	Speed   float32
	Gravity Vector3
}

type AngularMovementOperator struct {
	Enabled bool
	Drag    float32
	Force   Vector3
}

type AlphaFadeOperator struct {
	Enabled     bool
	FadeInTime  float32
	FadeOutTime float32
}

type SizeChangeOperator struct {
	Enabled    bool
	StartTime  float32
	EndTime    float32
	StartValue float32
	EndValue   float32
}

type ColorChangeOperator struct {
	Enabled    bool
	StartTime  float32
	EndTime    float32
	StartValue Vector3
	EndValue   Vector3
}

type ParticleOperator struct {
	Movement        MovementOperator
	AngularMovement AngularMovementOperator
	AlphaFade       AlphaFadeOperator
	SizeChange      SizeChangeOperator
	ColorChange     ColorChangeOperator
}

type ParticleEmitter struct {
	Directions  Vector3
	DistanceMax Vector3
	DistanceMin Vector3
	Origin      Vector3
	Sign        [3]int32
	SpeedMin    float32
	SpeedMax    float32
	Rate        float32
}

type ParticleFlags uint32

const (
	ParticleFlagWorldSpace ParticleFlags = 1 << iota
	ParticleFlagSpriteNoFrameBlending
	ParticleFlagPerspective
)

type Particle struct {
	Emitters           []ParticleEmitter
	Initializer        ParticleInitializer
	Operator           ParticleOperator
	Renderers          []ParticleRenderer
	Material           Material
	RandomFrame        bool
	SequenceMultiplier float32
	MaxCount           uint32
	StartTime          float32
	Flags              ParticleFlags
}

type ParticleInstanceOverride struct {
	Enabled        bool
	OverrideColor  bool
	OverrideColorN bool
	Alpha          float32
	Count          float32
	Lifetime       float32
	Rate           float32
	Speed          float32
	Size           float32
	Color          Vector3
	ColorN         Vector3
}

type ParticleObject struct {
	ID               int
	Parent           int
	Name             string
	Origin           Vector3
	Scale            Vector3
	Angles           Vector3
	ParallaxDepth    Vector2
	ParticleData     Particle
	InstanceOverride ParticleInstanceOverride
}

func (override *ParticleInstanceOverride) parseFromJSON(raw json.RawMessage) error {
	payload := struct {
		Alpha    FloatValue `json:"alpha"`
		Size     FloatValue `json:"size"`
		Lifetime FloatValue `json:"lifetime"`
		Rate     FloatValue `json:"rate"`
		Speed    FloatValue `json:"speed"`
		Count    FloatValue `json:"count"`
		Color    Vector3    `json:"color"`
		ColorN   Vector3    `json:"colorn"`
	}{}

	if err := json.Unmarshal(raw, &payload); err != nil {
		return fmt.Errorf("cannot parse particle instance override: %w", err)
	}
	override.Enabled = true
	override.Alpha = float32(payload.Alpha)
	override.Size = float32(payload.Size)
	override.Lifetime = float32(payload.Lifetime)
	override.Rate = float32(payload.Rate)
	override.Speed = float32(payload.Speed)
	override.Count = float32(payload.Count)
	if payload.Color != (Vector3{}) {
		override.Color = payload.Color
		override.OverrideColor = true
	} else if payload.ColorN != (Vector3{}) {
		override.ColorN = payload.ColorN
		override.OverrideColorN = true
	}
	return nil
}

func (render *ParticleRenderer) parseFromJSON(raw json.RawMessage) error {
	payload := struct {
		Name StringValue `json:"name"`
	}{}

	if err := json.Unmarshal(raw, &payload); err != nil {
		return fmt.Errorf("cannot parse particle render: %w", err)
	}
	if payload.Name == "" {
		return fmt.Errorf("particle render missing name")
	}
	name := string(payload.Name)
	if name == "ropetrail" {
		name = "spritetrail"
	}
	render.Name = name
	return nil
}

func (init *ParticleInitializer) parseFromJSON(raw json.RawMessage) error {
	payload := struct {
		Name StringValue     `json:"name"`
		Min  json.RawMessage `json:"min"`
		Max  json.RawMessage `json:"max"`
	}{}

	if err := json.Unmarshal(raw, &payload); err != nil {
		return fmt.Errorf("cannot parse particle initializer: %w", err)
	}

	name := strings.TrimSpace(string(payload.Name))
	if name == "" {
		return fmt.Errorf("particle initializer missing name")
	}

	switch name {
	case "lifetimerandom":
		if bytesFromRawNullAware(payload.Min) != nil {
			min, err := parseFloat64FromRaw(payload.Min)
			if err != nil {
				return fmt.Errorf("cannot parse min value for lifetimerandom initializer: %w", err)
			}
			init.MinLifetime = float32(min)
		}
		if bytesFromRawNullAware(payload.Max) != nil {
			max, err := parseFloat64FromRaw(payload.Max)
			if err != nil {
				return fmt.Errorf("cannot parse max value for lifetimerandom initializer: %w", err)
			}
			init.MaxLifetime = float32(max)
		}
	case "sizerandom":
		if bytesFromRawNullAware(payload.Min) != nil {
			min, err := parseFloat64FromRaw(payload.Min)
			if err != nil {
				return fmt.Errorf("cannot parse min value for sizerandom initializer: %w", err)
			}
			init.MinSize = float32(min)
		}
		if bytesFromRawNullAware(payload.Max) != nil {
			max, err := parseFloat64FromRaw(payload.Max)
			if err != nil {
				return fmt.Errorf("cannot parse max value for sizerandom initializer: %w", err)
			}
			init.MaxSize = float32(max)
		}
	case "alpharandom":
		init.MinAlpha = 0.0
		init.MaxAlpha = 1.0
		if bytesFromRawNullAware(payload.Min) != nil {
			min, err := parseFloat64FromRaw(payload.Min)
			if err != nil {
				return fmt.Errorf("cannot parse min value for alpharandom initializer: %w", err)
			}
			init.MinAlpha = float32(min)
		}
		if bytesFromRawNullAware(payload.Max) != nil {
			max, err := parseFloat64FromRaw(payload.Max)
			if err != nil {
				return fmt.Errorf("cannot parse max value for alpharandom initializer: %w", err)
			}
			init.MaxAlpha = float32(max)
		}
	case "velocityrandom":
		if bytesFromRawNullAware(payload.Min) != nil {
			min, err := parseVector3FromRaw(payload.Min, init.MinVelocity)
			if err != nil {
				return fmt.Errorf("cannot parse min value for velocityrandom initializer: %w", err)
			}
			init.MinVelocity = min
		}
		if bytesFromRawNullAware(payload.Max) != nil {
			max, err := parseVector3FromRaw(payload.Max, init.MaxVelocity)
			if err != nil {
				return fmt.Errorf("cannot parse max value for velocityrandom initializer: %w", err)
			}
			init.MaxVelocity = max
		}
	case "colorrandom":
		if bytesFromRawNullAware(payload.Min) != nil {
			min, err := parseVector3FromRaw(payload.Min, init.MinColor)
			if err != nil {
				return fmt.Errorf("cannot parse min value for colorrandom initializer: %w", err)
			}
			for i := range 3 {
				init.MinColor[i] = min[i] / 255
			}
		}
		if bytesFromRawNullAware(payload.Max) != nil {
			max, err := parseVector3FromRaw(payload.Max, init.MaxColor)
			if err != nil {
				return fmt.Errorf("cannot parse max value for colorrandom initializer: %w", err)
			}
			for i := range 3 {
				init.MaxColor[i] = max[i] / 255
			}
		}
	case "rotationrandom":
		if bytesFromRawNullAware(payload.Min) != nil {
			min, err := parseVector3FromRaw(payload.Min, init.MinRotation)
			if err != nil {
				return fmt.Errorf("cannot parse min value for rotationrandom initializer: %w", err)
			}
			init.MinRotation = min
		}
		if bytesFromRawNullAware(payload.Max) != nil {
			max, err := parseVector3FromRaw(payload.Max, init.MaxRotation)
			if err != nil {
				return fmt.Errorf("cannot parse max value for rotationrandom initializer: %w", err)
			}
			init.MaxRotation = max
		}
	case "angularvelocityrandom":
		if bytesFromRawNullAware(payload.Min) != nil {
			min, err := parseVector3FromRaw(payload.Min, init.MinAngularVelocity)
			if err != nil {
				return fmt.Errorf("cannot parse min value for angularvelocityrandom initializer: %w", err)
			}
			init.MinAngularVelocity = min
		}
		if bytesFromRawNullAware(payload.Max) != nil {
			max, err := parseVector3FromRaw(payload.Max, init.MaxAngularVelocity)
			if err != nil {
				return fmt.Errorf("cannot parse max value for angularvelocityrandom initializer: %w", err)
			}
			init.MaxAngularVelocity = max
		}
	default:
		fmt.Printf("warning: unknown particle initializer %s\n", name)
	}

	return nil
}

func (operator *ParticleOperator) parseFromJSON(raw json.RawMessage) error {
	payload := struct {
		Name        string          `json:"name"`
		Gravity     json.RawMessage `json:"gravity"`
		Drag        json.RawMessage `json:"drag"`
		Force       json.RawMessage `json:"force"`
		FadeInTime  json.RawMessage `json:"fadeintime"`
		FadeOutTime json.RawMessage `json:"fadeouttime"`
		StartTime   json.RawMessage `json:"starttime"`
		EndTime     json.RawMessage `json:"endtime"`
		StartValue  json.RawMessage `json:"startvalue"`
		EndValue    json.RawMessage `json:"endvalue"`
	}{}

	if err := json.Unmarshal(raw, &payload); err != nil {
		return fmt.Errorf("cannot parse particle operator: %w", err)
	}

	name := strings.TrimSpace(string(payload.Name))
	if name == "" {
		return fmt.Errorf("particle operator missing name")
	}

	switch name {
	case "movement":
		operator.Movement.Enabled = true
		if bytesFromRawNullAware(payload.Gravity) != nil {
			gravity, err := parseVector3FromRaw(payload.Gravity, operator.Movement.Gravity)
			if err != nil {
				return fmt.Errorf("cannot parse gravity value for movement operator: %w", err)
			}
			operator.Movement.Gravity = gravity
		}
		if bytesFromRawNullAware(payload.Drag) != nil {
			drag, err := parseFloat64FromRaw(payload.Drag)
			if err != nil {
				return fmt.Errorf("cannot parse drag value for movement operator: %w", err)
			}
			operator.Movement.Drag = float32(drag)
		}
	case "angularmovement":
		operator.AngularMovement.Enabled = true
		if bytesFromRawNullAware(payload.Drag) != nil {
			drag, err := parseFloat64FromRaw(payload.Drag)
			if err != nil {
				return fmt.Errorf("cannot parse drag value for angularmovement operator: %w", err)
			}
			operator.AngularMovement.Drag = float32(drag)
		}
		if bytesFromRawNullAware(payload.Force) != nil {
			force, err := parseVector3FromRaw(payload.Force, operator.AngularMovement.Force)
			if err != nil {
				return fmt.Errorf("cannot parse force value for angularmovement operator: %w", err)
			}
			operator.AngularMovement.Force = force
		}
	case "sizechange":
		op := SizeChangeOperator{
			Enabled:    true,
			StartTime:  0,
			EndTime:    1,
			StartValue: 1,
			EndValue:   0,
		}
		if bytesFromRawNullAware(payload.StartTime) != nil {
			startTime, err := parseFloat64FromRaw(payload.StartTime)
			if err != nil {
				return fmt.Errorf("cannot parse starttime for sizechange operator: %w", err)
			}
			op.StartTime = float32(startTime)
		}
		if bytesFromRawNullAware(payload.EndTime) != nil {
			endTime, err := parseFloat64FromRaw(payload.EndTime)
			if err != nil {
				return fmt.Errorf("cannot parse endtime for sizechange operator: %w", err)
			}
			op.EndTime = float32(endTime)
		}
		if bytesFromRawNullAware(payload.StartValue) != nil {
			startValue, err := parseFloat64FromRaw(payload.StartValue)
			if err != nil {
				return fmt.Errorf("cannot parse startvalue for sizechange operator: %w", err)
			}
			op.StartValue = float32(startValue)
		}
		if bytesFromRawNullAware(payload.EndValue) != nil {
			endValue, err := parseFloat64FromRaw(payload.EndValue)
			if err != nil {
				return fmt.Errorf("cannot parse endvalue for sizechange operator: %w", err)
			}
			op.EndValue = float32(endValue)
		}
		operator.SizeChange = op
	case "colorchange":
		op := ColorChangeOperator{
			Enabled:    true,
			StartTime:  0,
			EndTime:    1,
			StartValue: Vector3{1, 1, 1},
			EndValue:   Vector3{1, 1, 1},
		}
		if bytesFromRawNullAware(payload.StartTime) != nil {
			startTime, err := parseFloat64FromRaw(payload.StartTime)
			if err != nil {
				return fmt.Errorf("cannot parse starttime for colorchange operator: %w", err)
			}
			op.StartTime = float32(startTime)
		}
		if bytesFromRawNullAware(payload.EndTime) != nil {
			endTime, err := parseFloat64FromRaw(payload.EndTime)
			if err != nil {
				return fmt.Errorf("cannot parse endtime for colorchange operator: %w", err)
			}
			op.EndTime = float32(endTime)
		}
		if bytesFromRawNullAware(payload.StartValue) != nil {
			startValue, err := parseVector3FromRaw(payload.StartValue, op.StartValue)
			if err != nil {
				return fmt.Errorf("cannot parse startvalue for colorchange operator: %w", err)
			}
			op.StartValue = startValue
		}
		if bytesFromRawNullAware(payload.EndValue) != nil {
			endValue, err := parseVector3FromRaw(payload.EndValue, op.EndValue)
			if err != nil {
				return fmt.Errorf("cannot parse endvalue for colorchange operator: %w", err)
			}
			op.EndValue = endValue
		}
		operator.ColorChange = op
	case "alphafade":
		op := AlphaFadeOperator{
			Enabled:     true,
			FadeInTime:  0.5,
			FadeOutTime: 0.5,
		}
		if bytesFromRawNullAware(payload.FadeInTime) != nil {
			fadeInTime, err := parseFloat64FromRaw(payload.FadeInTime)
			if err != nil {
				return fmt.Errorf("cannot parse fadeintime for alphafade operator: %w", err)
			}
			op.FadeInTime = float32(fadeInTime)
		}
		if bytesFromRawNullAware(payload.FadeOutTime) != nil {
			fadeOutTime, err := parseFloat64FromRaw(payload.FadeOutTime)
			if err != nil {
				return fmt.Errorf("cannot parse fadeouttime for alphafade operator: %w", err)
			}
			op.FadeOutTime = float32(fadeOutTime)
		}
		operator.AlphaFade = op
	default:
		fmt.Printf("warning: unknown particle operator %s\n", name)
	}

	return nil
}

func (emitter *ParticleEmitter) parseFromJSON(raw json.RawMessage) error {
	payload := struct {
		Directions  Vector3    `json:"directions"`
		DistanceMax Vector3    `json:"distancemax"`
		DistanceMin Vector3    `json:"distancemin"`
		Origin      Vector3    `json:"origin"`
		Sign        FloatSlice `json:"sign"`
		SpeedMin    FloatValue `json:"speedmin"`
		SpeedMax    FloatValue `json:"speedmax"`
		Rate        FloatValue `json:"rate"`
	}{
		Directions: Vector3{1, 1, 1},
		Rate:       5,
	}

	if err := json.Unmarshal(raw, &payload); err != nil {
		return fmt.Errorf("cannot parse particle emitter: %w", err)
	}
	emitter.Directions = payload.Directions
	emitter.DistanceMax = payload.DistanceMax
	emitter.DistanceMin = payload.DistanceMin
	emitter.Origin = payload.Origin
	if len(payload.Sign) == 3 {
		emitter.Sign[0] = int32(payload.Sign[0])
		emitter.Sign[1] = int32(payload.Sign[1])
		emitter.Sign[2] = int32(payload.Sign[2])
	}
	emitter.SpeedMin = float32(payload.SpeedMin)
	emitter.SpeedMax = float32(payload.SpeedMax)
	emitter.Rate = float32(payload.Rate)
	return nil
}

func (particle *Particle) parseFromJSON(raw json.RawMessage, pkgMap *map[string][]byte) error {
	payload := struct {
		Emitters           []json.RawMessage `json:"emitter"`
		Renderers          []json.RawMessage `json:"renderer"`
		Initializers       []json.RawMessage `json:"initializer"`
		Operators          []json.RawMessage `json:"operator"`
		Material           StringValue       `json:"material"`
		AnimationMode      StringValue       `json:"animationmode"`
		SequenceMultiplier *FloatValue       `json:"sequencemultiplier"`
		MaxCount           IntValue          `json:"maxcount"`
		StartTime          FloatValue        `json:"starttime"`
		Flags              IntValue          `json:"flags"`
	}{}

	if err := json.Unmarshal(raw, &payload); err != nil {
		return fmt.Errorf("cannot parse particle: %w", err)
	}

	if len(payload.Emitters) == 0 {
		return fmt.Errorf("particle has no emitter array")
	}

	for _, emitterRaw := range payload.Emitters {
		var emitter ParticleEmitter
		if err := emitter.parseFromJSON(emitterRaw); err != nil {
			return err
		}
		particle.Emitters = append(particle.Emitters, emitter)
	}

	for _, rendererRaw := range payload.Renderers {
		var renderer ParticleRenderer
		if err := renderer.parseFromJSON(rendererRaw); err != nil {
			return err
		}
		particle.Renderers = append(particle.Renderers, renderer)
	}
	if len(particle.Renderers) == 0 {
		particle.Renderers = append(particle.Renderers, ParticleRenderer{
			Name: "sprite",
		})
	}

	particle.Initializer = ParticleInitializer{
		MinLifetime:        20,
		MaxLifetime:        20,
		MinSize:            1,
		MaxSize:            1,
		MinVelocity:        [3]float32{0, 0, 0},
		MaxVelocity:        [3]float32{0, 0, 0},
		MinRotation:        [3]float32{0, 0, 0},
		MaxRotation:        [3]float32{0, 0, 0},
		MinAngularVelocity: [3]float32{0, 0, 0},
		MaxAngularVelocity: [3]float32{0, 0, 0},
		MinColor:           [3]float32{1, 1, 1},
		MaxColor:           [3]float32{1, 1, 1},
		MinAlpha:           1,
		MaxAlpha:           1,
	}
	for _, initRaw := range payload.Initializers {
		if err := particle.Initializer.parseFromJSON(initRaw); err != nil {
			return err
		}
	}

	for _, operatorRaw := range payload.Operators {
		if err := particle.Operator.parseFromJSON(operatorRaw); err != nil {
			return err
		}
	}

	if payload.Material == "" {
		return fmt.Errorf("particle has no material field")
	}
	materialBytes, err := loadBytesFromPackage(pkgMap, string(payload.Material))
	if err != nil {
		return fmt.Errorf("cannot load particle material %s: %w", string(payload.Material), err)
	}
	if err := particle.Material.parseFromJSON(materialBytes); err != nil {
		return fmt.Errorf("cannot parse particle material %s: %w", string(payload.Material), err)
	}

	particle.RandomFrame = (payload.AnimationMode == "randomframe")
	particle.SequenceMultiplier = 1.0
	if payload.SequenceMultiplier != nil {
		particle.SequenceMultiplier = float32(*payload.SequenceMultiplier)
	}
	particle.MaxCount = uint32(payload.MaxCount)
	particle.StartTime = float32(payload.StartTime)
	particle.Flags = ParticleFlags(payload.Flags)

	return nil
}

func (particleObject *ParticleObject) parseFromSceneJSON(raw json.RawMessage, pkgMap *map[string][]byte) error {
	payload := struct {
		ID               IntValue        `json:"id"`
		Parent           IntValue        `json:"parent"`
		Name             StringValue     `json:"name"`
		Origin           Vector3         `json:"origin"`
		Scale            Vector3         `json:"scale"`
		Angles           Vector3         `json:"angles"`
		ParallaxDepth    Vector2         `json:"parallaxDepth"`
		Particle         StringValue     `json:"particle"`
		InstanceOverride json.RawMessage `json:"instanceoverride"`
	}{
		Parent: -1,
	}

	if err := json.Unmarshal(raw, &payload); err != nil {
		return fmt.Errorf("cannot parse particle object: %w", err)
	}
	if payload.Particle == "" {
		return fmt.Errorf("particle object missing particle path")
	}

	particlePath := string(payload.Particle)

	particleObject.Name = string(payload.Name)
	particleObject.ID = int(payload.ID)
	particleObject.Parent = int(payload.Parent)
	particleObject.Origin = payload.Origin
	particleObject.Scale = payload.Scale
	particleObject.Angles = payload.Angles
	particleObject.ParallaxDepth = payload.ParallaxDepth

	if bytesFromRawNullAware(payload.InstanceOverride) != nil {
		var instanceOverride ParticleInstanceOverride
		if err := instanceOverride.parseFromJSON(payload.InstanceOverride); err != nil {
			return err
		}
		particleObject.InstanceOverride = instanceOverride
	}

	particleBytes, err := loadBytesFromPackage(pkgMap, particlePath)
	if err != nil {
		return fmt.Errorf("cannot load particle file %s: %w", particlePath, err)
	}
	if err := particleObject.ParticleData.parseFromJSON(particleBytes, pkgMap); err != nil {
		return err
	}

	return nil
}

type OrthogonalProjection struct {
	Width  int
	Height int
}

type SceneGeneral struct {
	Parallax               bool
	ParallaxAmount         float32
	ParallaxDelay          float32
	ParallaxMouseInfluence float32
	Shake                  bool
	ShakeAmplitude         float32
	ShakeRoughness         float32
	ShakeSpeed             float32
	Ortho                  OrthogonalProjection
	Zoom                   float32
	FOV                    float32
	NearZ                  float32
	FarZ                   float32
}

type SceneObject any

type Scene struct {
	Objects []SceneObject
	General SceneGeneral
}

func parseOrthogonalProjection(raw json.RawMessage) (OrthogonalProjection, error) {
	type orthogonalProjectionJSON struct {
		Width  IntValue  `json:"width"`
		Height IntValue  `json:"height"`
		Auto   BoolValue `json:"auto"`
	}

	var projection OrthogonalProjection
	var payload orthogonalProjectionJSON
	if err := json.Unmarshal(raw, &payload); err != nil {
		return projection, fmt.Errorf("cannot parse orthogonal projection: %w", err)
	}
	if payload.Auto {
		return projection, nil
	}
	projection.Width = int(payload.Width)
	projection.Height = int(payload.Height)
	return projection, nil
}

func parseSceneGeneral(raw json.RawMessage) (SceneGeneral, error) {
	type generalJSON struct {
		CameraParallax          BoolValue       `json:"cameraparallax"`
		CameraParallaxAmount    FloatValue      `json:"cameraparallaxamount"`
		CameraParallaxDelay     FloatValue      `json:"cameraparallaxdelay"`
		CameraParallaxMouse     FloatValue      `json:"cameraparallaxmouseinfluence"`
		CameraShake             BoolValue       `json:"camerashake"`
		CameraShakeAmplitude    FloatValue      `json:"camerashakeamplitude"`
		CameraShakeRoughness    FloatValue      `json:"camerashakeroughness"`
		CameraShakeSpeed        FloatValue      `json:"camerashakespeed"`
		Zoom                    FloatValue      `json:"zoom"`
		FOV                     FloatValue      `json:"perspectiveoverridefov"`
		NearZ                   FloatValue      `json:"nearz"`
		FarZ                    FloatValue      `json:"farz"`
		OrthogonalProjectionRaw json.RawMessage `json:"orthogonalprojection"`
	}

	general := SceneGeneral{
		Ortho:          OrthogonalProjection{Width: 1920, Height: 1080},
		Zoom:           1,
		FOV:            90,
		NearZ:          0.01,
		FarZ:           10000,
		Shake:          false,
		ShakeAmplitude: 0.5,
		ShakeRoughness: 1,
		ShakeSpeed:     3,
	}

	var payload generalJSON
	if err := json.Unmarshal(raw, &payload); err != nil {
		return general, fmt.Errorf("cannot parse general block: %w", err)
	}

	general.Parallax = bool(payload.CameraParallax)
	general.ParallaxAmount = float32(payload.CameraParallaxAmount)
	general.ParallaxDelay = float32(payload.CameraParallaxDelay)
	general.ParallaxMouseInfluence = float32(payload.CameraParallaxMouse)
	general.Shake = bool(payload.CameraShake)
	if payload.CameraShakeAmplitude != 0 {
		general.ShakeAmplitude = float32(payload.CameraShakeAmplitude)
	}
	if payload.CameraShakeRoughness != 0 {
		general.ShakeRoughness = float32(payload.CameraShakeRoughness)
	}
	if payload.CameraShakeSpeed != 0 {
		general.ShakeSpeed = float32(payload.CameraShakeSpeed)
	}

	if payload.Zoom != 0 {
		general.Zoom = float32(payload.Zoom)
	}
	if payload.FOV != 0 {
		general.FOV = float32(payload.FOV)
	}
	if payload.NearZ != 0 {
		general.NearZ = float32(payload.NearZ)
	}
	if payload.FarZ != 0 {
		general.FarZ = float32(payload.FarZ)
	}

	if bytesFromRawNullAware(payload.OrthogonalProjectionRaw) != nil {
		projection, err := parseOrthogonalProjection(payload.OrthogonalProjectionRaw)
		if err != nil {
			return general, err
		}
		general.Ortho = projection
	}

	return general, nil
}

func ParseScene(pkgMap *map[string][]byte) (Scene, error) {
	sceneBytes, err := loadBytesFromPackage(pkgMap, "scene.json")
	if err != nil {
		return Scene{}, err
	}
	type sceneJSON struct {
		General json.RawMessage   `json:"general"`
		Objects []json.RawMessage `json:"objects"`
	}

	var payload sceneJSON
	if err := json.Unmarshal(sceneBytes, &payload); err != nil {
		return Scene{}, fmt.Errorf("cannot parse scene JSON: %w", err)
	}

	scene := Scene{}

	if payload.General != nil {
		general, parseErr := parseSceneGeneral(payload.General)
		if parseErr != nil {
			return Scene{}, parseErr
		}
		scene.General = general
	} else {
		return Scene{}, fmt.Errorf("scene has no general block")
	}

	for _, objectRaw := range payload.Objects {
		var objectProbe struct {
			Particle json.RawMessage `json:"particle"`
			Image    json.RawMessage `json:"image"`
		}
		parseErr := json.Unmarshal(objectRaw, &objectProbe)
		if parseErr != nil {
			return Scene{}, fmt.Errorf("cannot parse object entry: %w", parseErr)
		}
		if len(objectProbe.Particle) > 0 {
			var particleObject ParticleObject
			if err := particleObject.parseFromSceneJSON(objectRaw, pkgMap); err != nil {
				return Scene{}, err
			}
			scene.Objects = append(scene.Objects, &particleObject)
		} else if len(objectProbe.Image) > 0 {
			var imageObject ImageObject
			if err := imageObject.parseFromSceneJSON(objectRaw, pkgMap); err != nil {
				return Scene{}, err
			}
			scene.Objects = append(scene.Objects, &imageObject)
		} else {
			var emptyObject EmptyObject
			if err := emptyObject.parseFromSceneJSON(objectRaw); err != nil {
				return Scene{}, err
			}
			scene.Objects = append(scene.Objects, &emptyObject)
		}
	}

	return scene, nil
}
