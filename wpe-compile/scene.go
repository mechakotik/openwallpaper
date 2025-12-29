package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"maps"
	"math"
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
	Blending   string
	CullMode   string
	DepthTest  string
	DepthWrite string
	Shader     string
	Textures   []string
	Combos     map[string]int
	Constants  map[string][]float32
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
	if material.Constants == nil {
		material.Constants = make(map[string][]float32)
	}
	maps.Copy(material.Constants, other.Constants)
}

func (material *Material) parseFromJSON(raw json.RawMessage) error {
	type materialPassJSON struct {
		Blending             StringValue           `json:"blending"`
		CullMode             StringValue           `json:"cullmode"`
		DepthTest            StringValue           `json:"depthtest"`
		DepthWrite           StringValue           `json:"depthwrite"`
		Shader               StringValue           `json:"shader"`
		Textures             []NullString          `json:"textures"`
		ConstantShaderValues map[string]FloatSlice `json:"constantshadervalues"`
		Combos               map[string]IntValue   `json:"combos"`
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
	if firstPass.Blending == "" || firstPass.CullMode == "" || firstPass.DepthTest == "" || firstPass.DepthWrite == "" || firstPass.Shader == "" {
		return fmt.Errorf("material pass is missing required fields")
	}

	material.Blending = string(firstPass.Blending)
	material.CullMode = string(firstPass.CullMode)
	material.DepthTest = string(firstPass.DepthTest)
	material.DepthWrite = string(firstPass.DepthWrite)
	material.Shader = string(firstPass.Shader)

	for _, texture := range firstPass.Textures {
		material.Textures = append(material.Textures, string(texture))
	}
	if len(firstPass.ConstantShaderValues) > 0 {
		if material.Constants == nil {
			material.Constants = make(map[string][]float32)
		}
		for key, value := range firstPass.ConstantShaderValues {
			material.Constants[key] = value
		}
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

type EffectCommand struct {
	Command  string
	Target   string
	Source   string
	AfterPos int
}

type EffectFBO struct {
	Name   string
	Scale  int
	Format string
}

type ImageEffect struct {
	Version   int
	ID        string
	Name      string
	Visible   bool
	Passes    []MaterialPass
	FBOs      []EffectFBO
	Materials []Material
	Commands  []EffectCommand
}

func (command *EffectCommand) parseFromJSON(raw json.RawMessage) error {
	payload := struct {
		Command StringValue `json:"command"`
		Target  StringValue `json:"target"`
		Source  StringValue `json:"source"`
	}{}

	if err := json.Unmarshal(raw, &payload); err != nil {
		return fmt.Errorf("cannot parse effect command: %w", err)
	}
	if payload.Command == "" || payload.Target == "" || payload.Source == "" {
		return fmt.Errorf("effect command missing required fields")
	}
	command.Command = string(payload.Command)
	command.Target = string(payload.Target)
	command.Source = string(payload.Source)
	return nil
}

func (fbo *EffectFBO) parseFromJSON(raw json.RawMessage) error {
	payload := struct {
		Name   StringValue `json:"name"`
		Scale  IntValue    `json:"scale"`
		Format StringValue `json:"format"`
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
	fbo.Format = string(payload.Format)
	return nil
}

func (effect *ImageEffect) parseFromFileJSON(raw json.RawMessage, pkgMap *map[string][]byte) error {
	payload := struct {
		Version IntValue          `json:"version"`
		Name    StringValue       `json:"name"`
		FBOs    []json.RawMessage `json:"fbos"`
		Passes  []json.RawMessage `json:"passes"`
	}{}

	if err := json.Unmarshal(raw, &payload); err != nil {
		return fmt.Errorf("cannot parse effect file JSON: %w", err)
	}

	effect.Version = int(payload.Version)
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
				var command EffectCommand
				if err := command.parseFromJSON(passRaw); err != nil {
					return err
				}
				command.AfterPos = len(effect.Passes)
				effect.Commands = append(effect.Commands, command)
				continue
			}
			return errors.New("effect pass has no material or command")
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
		File    StringValue       `json:"file"`
		ID      json.RawMessage   `json:"id"`
		Name    StringValue       `json:"name"`
		Visible BoolValue         `json:"visible"`
		Passes  []json.RawMessage `json:"passes"`
	}{
		Visible: true,
	}

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

	if len(payload.ID) > 0 {
		if idValue, parseErr := parseStringFromRaw(payload.ID); parseErr == nil {
			effect.ID = idValue
		}
	}

	if strings.TrimSpace(string(payload.Name)) != "" {
		effect.Name = string(payload.Name)
	}

	effect.Visible = bool(payload.Visible)

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
	Name           string
	Origin         Vector3
	Scale          Vector3
	Angles         Vector3
	Size           Vector2
	ParallaxDepth  Vector2
	Color          Vector3
	ColorBlendMode int
	Alpha          float32
	Brightness     float32
	Fullscreen     bool
	NoPadding      bool
	Visible        bool
	ImagePath      string
	Alignment      string
	Material       Material
	Effects        []ImageEffect
	Config         ImageConfig
	Puppet         string
	PuppetLayers   []PuppetAnimationLayer
}

type PuppetAnimationLayer struct {
	ID      int
	Blend   float32
	Rate    float32
	Visible bool
}

func (imageObject *ImageObject) parseFromSceneJSON(raw json.RawMessage, pkgMap *map[string][]byte) error {
	type layerJSON struct {
		Animation IntValue   `json:"animation"`
		Blend     FloatValue `json:"blend"`
		Rate      FloatValue `json:"rate"`
		Visible   BoolValue  `json:"visible"`
	}

	type configJSON struct {
		Passthrough BoolValue `json:"passthrough"`
	}

	type imageSceneJSON struct {
		ID              IntValue          `json:"id"`
		Name            StringValue       `json:"name"`
		Image           StringValue       `json:"image"`
		Origin          Vector3           `json:"origin"`
		Scale           Vector3           `json:"scale"`
		Angles          Vector3           `json:"angles"`
		Size            Vector2           `json:"size"`
		ParallaxDepth   Vector2           `json:"parallaxDepth"`
		Color           Vector3           `json:"color"`
		ColorBlendMode  IntValue          `json:"colorBlendMode"`
		Alpha           FloatValue        `json:"alpha"`
		Brightness      FloatValue        `json:"brightness"`
		Alignment       StringValue       `json:"alignment"`
		Visible         BoolValue         `json:"visible"`
		Effects         []json.RawMessage `json:"effects"`
		AnimationLayers []layerJSON       `json:"animationlayers"`
		Config          configJSON        `json:"config"`
	}

	type imageModelJSON struct {
		Fullscreen BoolValue   `json:"fullscreen"`
		Width      IntValue    `json:"width"`
		Height     IntValue    `json:"height"`
		NoPadding  BoolValue   `json:"nopadding"`
		Puppet     StringValue `json:"puppet"`
		Material   StringValue `json:"material"`
	}

	payload := imageSceneJSON{
		Visible:    true,
		Alignment:  "center",
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
	imageObject.ImagePath = imagePath

	imageObject.Visible = bool(payload.Visible)
	imageObject.Alignment = string(payload.Alignment)
	imageObject.Name = string(payload.Name)
	imageObject.ID = int(payload.ID)
	imageObject.ColorBlendMode = int(payload.ColorBlendMode)
	imageObject.Origin = payload.Origin
	imageObject.Angles = payload.Angles
	imageObject.Scale = payload.Scale
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
	imageObject.Puppet = string(model.Puppet)
	imageObject.NoPadding = bool(model.NoPadding)

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

	for _, layerRaw := range payload.AnimationLayers {
		if layerRaw.Animation == 0 {
			return fmt.Errorf("puppet animation layer missing animation id")
		}
		layer := PuppetAnimationLayer{
			ID:      int(layerRaw.Animation),
			Visible: true,
		}
		layer.Blend = float32(layerRaw.Blend)
		layer.Rate = float32(layerRaw.Rate)
		layer.Visible = bool(layerRaw.Visible)
		imageObject.PuppetLayers = append(imageObject.PuppetLayers, layer)
	}

	imageObject.Config.Passthrough = bool(payload.Config.Passthrough)

	return nil
}

type ParticleControlPointFlags uint32

type ParticleControlPoint struct {
	Flags  ParticleControlPointFlags
	ID     int
	Offset Vector3
}

type ParticleRenderer struct {
	Name        string
	Length      float32
	MaxLength   float32
	Subdivision float32
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
	TurbulentVelocity  bool
	TurbulentScale     float32
	TurbulentTimeScale float32
	TurbulentOffset    float32
	TurbulentSpeedMin  float32
	TurbulentSpeedMax  float32
	TurbulentPhaseMin  float32
	TurbulentPhaseMax  float32
	TurbulentForward   [3]float32
	TurbulentRight     [3]float32
	TurbulentUp        [3]float32
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

type OscillatePositionOperator struct {
	Enabled      bool
	Mask         Vector3
	FrequencyMin float32
	FrequencyMax float32
	ScaleMin     float32
	ScaleMax     float32
	PhaseMin     float32
	PhaseMax     float32
}

type AlphaFadeOperator struct {
	Enabled     bool
	FadeInTime  float32
	FadeOutTime float32
}

type ParticleOperator struct {
	Movement          MovementOperator
	AngularMovement   AngularMovementOperator
	OscillatePosition OscillatePositionOperator
	AlphaFade         AlphaFadeOperator
}

type EmitterFlags uint32

type ParticleEmitter struct {
	Directions          Vector3
	DistanceMax         Vector3
	DistanceMin         Vector3
	Origin              Vector3
	Sign                [3]int32
	Instantaneous       uint32
	SpeedMin            float32
	SpeedMax            float32
	AudioProcessingMode uint32
	ControlPoint        int
	ID                  int
	Flags               EmitterFlags
	Name                string
	Rate                float32
}

type ParticleChild struct {
	Type              string
	Name              string
	MaxCount          uint32
	ControlPointStart int
	Probability       float32
	Origin            Vector3
	Scale             Vector3
	Angles            Vector3
	Object            Particle
}

type ParticleFlags uint32

const (
	ParticleFlagWorldSpace ParticleFlags = 1 << iota
	ParticleFlagSpriteNoFrameBlending
	ParticleFlagPerspective
)

type ParticleAnimationMode int

const (
	ParticleAnimationSequence ParticleAnimationMode = iota
	ParticleAnimationRandomFrame
	ParticleAnimationOnce
)

type Particle struct {
	Emitters           []ParticleEmitter
	Initializer        ParticleInitializer
	Operator           ParticleOperator
	Renderers          []ParticleRenderer
	ControlPoints      []ParticleControlPoint
	Material           Material
	Children           []ParticleChild
	AnimationMode      ParticleAnimationMode
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
	Name             string
	Origin           Vector3
	Scale            Vector3
	Angles           Vector3
	ParallaxDepth    Vector2
	Visible          bool
	ParticlePath     string
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

func (controlPoint *ParticleControlPoint) parseFromJSON(raw json.RawMessage) error {
	payload := struct {
		Flags  IntValue `json:"flags"`
		ID     IntValue `json:"id"`
		Offset Vector3  `json:"offset"`
	}{}

	if err := json.Unmarshal(raw, &payload); err != nil {
		return fmt.Errorf("cannot parse particle control point: %w", err)
	}
	controlPoint.ID = int(payload.ID)
	controlPoint.Flags = ParticleControlPointFlags(payload.Flags)
	controlPoint.Offset = payload.Offset
	return nil
}

func (render *ParticleRenderer) parseFromJSON(raw json.RawMessage) error {
	payload := struct {
		Name        StringValue `json:"name"`
		Length      FloatValue  `json:"length"`
		MaxLength   FloatValue  `json:"maxlength"`
		Subdivision FloatValue  `json:"subdivision"`
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
	render.Length = float32(payload.Length)
	render.MaxLength = float32(payload.MaxLength)
	render.Subdivision = float32(payload.Subdivision)
	return nil
}

func (init *ParticleInitializer) parseFromJSON(raw json.RawMessage) error {
	payload := struct {
		Name      StringValue     `json:"name"`
		Min       json.RawMessage `json:"min"`
		Max       json.RawMessage `json:"max"`
		Scale     json.RawMessage `json:"scale"`
		TimeScale json.RawMessage `json:"timescale"`
		Offset    json.RawMessage `json:"offset"`
		SpeedMin  json.RawMessage `json:"speedmin"`
		SpeedMax  json.RawMessage `json:"speedmax"`
		PhaseMin  json.RawMessage `json:"phasemin"`
		PhaseMax  json.RawMessage `json:"phasemax"`
		Forward   Vector3         `json:"forward"`
		Right     Vector3         `json:"right"`
		Up        Vector3         `json:"up"`
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
	case "turbulentvelocityrandom":
		init.TurbulentVelocity = true
		if bytesFromRawNullAware(payload.Scale) != nil {
			scale, err := parseFloat64FromRaw(payload.Scale)
			if err != nil {
				return fmt.Errorf("cannot parse scale value for turbulentvelocityrandom initializer: %w", err)
			}
			init.TurbulentScale = float32(scale)
		}
		if bytesFromRawNullAware(payload.TimeScale) != nil {
			timeScale, err := parseFloat64FromRaw(payload.TimeScale)
			if err != nil {
				return fmt.Errorf("cannot parse timescale value for turbulentvelocityrandom initializer: %w", err)
			}
			init.TurbulentTimeScale = float32(timeScale)
		}
		if bytesFromRawNullAware(payload.Offset) != nil {
			offset, err := parseFloat64FromRaw(payload.Offset)
			if err != nil {
				return fmt.Errorf("cannot parse offset value for turbulentvelocityrandom initializer: %w", err)
			}
			init.TurbulentOffset = float32(offset)
		}
		if bytesFromRawNullAware(payload.SpeedMin) != nil {
			speedMin, err := parseFloat64FromRaw(payload.SpeedMin)
			if err != nil {
				return fmt.Errorf("cannot parse speedmin value for turbulentvelocityrandom initializer: %w", err)
			}
			init.TurbulentSpeedMin = float32(speedMin)
		}
		if bytesFromRawNullAware(payload.SpeedMax) != nil {
			speedMax, err := parseFloat64FromRaw(payload.SpeedMax)
			if err != nil {
				return fmt.Errorf("cannot parse speedmax value for turbulentvelocityrandom initializer: %w", err)
			}
			init.TurbulentSpeedMax = float32(speedMax)
		}
		if bytesFromRawNullAware(payload.PhaseMin) != nil {
			phaseMin, err := parseFloat64FromRaw(payload.PhaseMin)
			if err != nil {
				return fmt.Errorf("cannot parse phasemin value for turbulentvelocityrandom initializer: %w", err)
			}
			init.TurbulentPhaseMin = float32(phaseMin)
		}
		if bytesFromRawNullAware(payload.PhaseMax) != nil {
			phaseMax, err := parseFloat64FromRaw(payload.PhaseMax)
			if err != nil {
				return fmt.Errorf("cannot parse phasemax value for turbulentvelocityrandom initializer: %w", err)
			}
			init.TurbulentPhaseMax = float32(phaseMax)
		}
		if payload.Forward != (Vector3{}) {
			init.TurbulentForward = payload.Forward
		}
		if payload.Right != (Vector3{}) {
			init.TurbulentRight = payload.Right
		}
		if payload.Up != (Vector3{}) {
			init.TurbulentUp = payload.Up
		}
	default:
		fmt.Printf("warning: unknown particle initializer %s\n", name)
	}

	return nil
}

func (operator *ParticleOperator) parseFromJSON(raw json.RawMessage) error {
	payload := struct {
		Name         string          `json:"name"`
		Gravity      json.RawMessage `json:"gravity"`
		Drag         json.RawMessage `json:"drag"`
		Force        json.RawMessage `json:"force"`
		FrequencyMin json.RawMessage `json:"frequencymin"`
		FrequencyMax json.RawMessage `json:"frequencymax"`
		ScaleMin     json.RawMessage `json:"scalemin"`
		ScaleMax     json.RawMessage `json:"scalemax"`
		PhaseMin     json.RawMessage `json:"phasemin"`
		PhaseMax     json.RawMessage `json:"phasemax"`
		Mask         json.RawMessage `json:"mask"`
		FadeInTime   json.RawMessage `json:"fadeintime"`
		FadeOutTime  json.RawMessage `json:"fadeouttime"`
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
	case "oscillateposition":
		op := OscillatePositionOperator{
			Enabled:      true,
			Mask:         Vector3{1, 1, 0},
			FrequencyMax: 5,
			ScaleMax:     1,
			PhaseMax:     float32(2 * math.Pi),
		}
		if bytesFromRawNullAware(payload.FrequencyMin) != nil {
			frequencyMin, err := parseFloat64FromRaw(payload.FrequencyMin)
			if err != nil {
				return fmt.Errorf("cannot parse frequencymin for oscillateposition operator: %w", err)
			}
			op.FrequencyMin = float32(frequencyMin)
		}
		if bytesFromRawNullAware(payload.FrequencyMax) != nil {
			frequencyMax, err := parseFloat64FromRaw(payload.FrequencyMax)
			if err != nil {
				return fmt.Errorf("cannot parse frequencymax for oscillateposition operator: %w", err)
			}
			op.FrequencyMax = float32(frequencyMax)
		}
		if op.FrequencyMax == 0 {
			op.FrequencyMax = op.FrequencyMin
		}
		if bytesFromRawNullAware(payload.ScaleMin) != nil {
			scaleMin, err := parseFloat64FromRaw(payload.ScaleMin)
			if err != nil {
				return fmt.Errorf("cannot parse scalemin for oscillateposition operator: %w", err)
			}
			op.ScaleMin = float32(scaleMin)
		}
		if bytesFromRawNullAware(payload.ScaleMax) != nil {
			scaleMax, err := parseFloat64FromRaw(payload.ScaleMax)
			if err != nil {
				return fmt.Errorf("cannot parse scalemax for oscillateposition operator: %w", err)
			}
			op.ScaleMax = float32(scaleMax)
		}
		if bytesFromRawNullAware(payload.PhaseMin) != nil {
			phaseMin, err := parseFloat64FromRaw(payload.PhaseMin)
			if err != nil {
				return fmt.Errorf("cannot parse phasemin for oscillateposition operator: %w", err)
			}
			op.PhaseMin = float32(phaseMin)
		}
		if bytesFromRawNullAware(payload.PhaseMax) != nil {
			phaseMax, err := parseFloat64FromRaw(payload.PhaseMax)
			if err != nil {
				return fmt.Errorf("cannot parse phasemax for oscillateposition operator: %w", err)
			}
			op.PhaseMax = float32(phaseMax)
		}
		if bytesFromRawNullAware(payload.Mask) != nil {
			mask, err := parseVector3FromRaw(payload.Mask, op.Mask)
			if err != nil {
				return fmt.Errorf("cannot parse mask for oscillateposition operator: %w", err)
			}
			op.Mask = mask
		}
		operator.OscillatePosition = op
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
		Directions          Vector3     `json:"directions"`
		DistanceMax         Vector3     `json:"distancemax"`
		DistanceMin         Vector3     `json:"distancemin"`
		Origin              Vector3     `json:"origin"`
		Sign                FloatSlice  `json:"sign"`
		Instantaneous       IntValue    `json:"instantaneous"`
		SpeedMin            FloatValue  `json:"speedmin"`
		SpeedMax            FloatValue  `json:"speedmax"`
		AudioProcessingMode IntValue    `json:"audioprocessingmode"`
		ControlPoint        IntValue    `json:"controlpoint"`
		ID                  IntValue    `json:"id"`
		Flags               IntValue    `json:"flags"`
		Name                StringValue `json:"name"`
		Rate                FloatValue  `json:"rate"`
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
	emitter.Instantaneous = uint32(payload.Instantaneous)
	emitter.SpeedMin = float32(payload.SpeedMin)
	emitter.SpeedMax = float32(payload.SpeedMax)
	emitter.AudioProcessingMode = uint32(payload.AudioProcessingMode)
	emitter.ControlPoint = int(payload.ControlPoint)
	emitter.ID = int(payload.ID)
	emitter.Flags = EmitterFlags(payload.Flags)
	emitter.Name = string(payload.Name)
	emitter.Rate = float32(payload.Rate)
	return nil
}

func (child *ParticleChild) parseFromJSON(raw json.RawMessage, pkgMap *map[string][]byte) error {
	payload := struct {
		Type              StringValue `json:"type"`
		Name              StringValue `json:"name"`
		MaxCount          IntValue    `json:"maxcount"`
		ControlPointStart IntValue    `json:"controlpointstartindex"`
		Probability       FloatValue  `json:"probability"`
		Origin            Vector3     `json:"origin"`
		Scale             Vector3     `json:"scale"`
		Angles            Vector3     `json:"angles"`
	}{
		Type: "static",
	}

	if err := json.Unmarshal(raw, &payload); err != nil {
		return fmt.Errorf("cannot parse particle child: %w", err)
	}
	name := strings.TrimSpace(string(payload.Name))
	if name == "" {
		return nil
	}

	child.Name = name
	child.Type = string(payload.Type)

	particleBytes, err := loadBytesFromPackage(pkgMap, name)
	if err != nil {
		return fmt.Errorf("cannot load child particle %s: %w", name, err)
	}
	if err := child.Object.parseFromJSON(particleBytes, pkgMap); err != nil {
		return err
	}

	child.MaxCount = uint32(payload.MaxCount)
	child.ControlPointStart = int(payload.ControlPointStart)
	child.Probability = float32(payload.Probability)
	child.Origin = payload.Origin
	child.Scale = payload.Scale
	child.Angles = payload.Angles
	return nil
}

func parseParticleAnimationMode(raw json.RawMessage) ParticleAnimationMode {
	if len(raw) == 0 {
		return ParticleAnimationSequence
	}

	if name, err := parseStringFromRaw(raw); err == nil {
		switch strings.ToLower(name) {
		case "randomframe":
			return ParticleAnimationRandomFrame
		case "once":
			return ParticleAnimationOnce
		case "loop", "sequence":
			return ParticleAnimationSequence
		default:
			return ParticleAnimationSequence
		}
	}

	if value, err := parseIntFromRaw(raw); err == nil {
		switch value {
		case 1:
			return ParticleAnimationRandomFrame
		case 2:
			return ParticleAnimationOnce
		default:
			return ParticleAnimationSequence
		}
	}

	return ParticleAnimationSequence
}

func (particle *Particle) parseFromJSON(raw json.RawMessage, pkgMap *map[string][]byte) error {
	payload := struct {
		Emitters           []json.RawMessage `json:"emitter"`
		Renderers          []json.RawMessage `json:"renderer"`
		Initializers       []json.RawMessage `json:"initializer"`
		Operators          []json.RawMessage `json:"operator"`
		ControlPoints      []json.RawMessage `json:"controlpoint"`
		Children           []json.RawMessage `json:"children"`
		Material           StringValue       `json:"material"`
		AnimationMode      json.RawMessage   `json:"animationmode"`
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
		MaxRotation:        [3]float32{0, 0, float32(2 * math.Pi)},
		MinAngularVelocity: [3]float32{0, 0, -5},
		MaxAngularVelocity: [3]float32{0, 0, 5},
		MinColor:           [3]float32{1, 1, 1},
		MaxColor:           [3]float32{1, 1, 1},
		MinAlpha:           1,
		MaxAlpha:           1,
		TurbulentScale:     1,
		TurbulentTimeScale: 1,
		TurbulentOffset:    0,
		TurbulentSpeedMin:  100,
		TurbulentSpeedMax:  250,
		TurbulentPhaseMin:  0,
		TurbulentPhaseMax:  0.1,
		TurbulentForward:   [3]float32{0, 1, 0},
		TurbulentRight:     [3]float32{0, 0, 1},
		TurbulentUp:        [3]float32{1, 0, 0},
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

	for _, controlRaw := range payload.ControlPoints {
		var controlPoint ParticleControlPoint
		if err := controlPoint.parseFromJSON(controlRaw); err != nil {
			return err
		}
		particle.ControlPoints = append(particle.ControlPoints, controlPoint)
	}

	for _, childRaw := range payload.Children {
		var child ParticleChild
		if err := child.parseFromJSON(childRaw, pkgMap); err != nil {
			return err
		}
		if child.Name != "" {
			particle.Children = append(particle.Children, child)
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

	particle.AnimationMode = parseParticleAnimationMode(payload.AnimationMode)

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
		Name             StringValue     `json:"name"`
		Origin           Vector3         `json:"origin"`
		Scale            Vector3         `json:"scale"`
		Angles           Vector3         `json:"angles"`
		ParallaxDepth    Vector2         `json:"parallaxDepth"`
		Visible          BoolValue       `json:"visible"`
		Particle         StringValue     `json:"particle"`
		InstanceOverride json.RawMessage `json:"instanceoverride"`
	}{
		Visible: true,
	}

	if err := json.Unmarshal(raw, &payload); err != nil {
		return fmt.Errorf("cannot parse particle object: %w", err)
	}
	if payload.Particle == "" {
		return fmt.Errorf("particle object missing particle path")
	}

	particlePath := string(payload.Particle)
	particleObject.ParticlePath = particlePath

	particleObject.Visible = bool(payload.Visible)
	particleObject.Name = string(payload.Name)
	particleObject.ID = int(payload.ID)
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

type SoundPlaybackMode string

type ScriptValue struct {
	Value            float32
	Script           string
	ScriptProperties map[string]json.RawMessage
}

type SoundObject struct {
	ID            int
	Name          string
	Origin        Vector3
	Scale         Vector3
	Angles        Vector3
	ParallaxDepth Vector2
	SoundFiles    []string
	PlaybackMode  SoundPlaybackMode
	Volume        ScriptValue
	StartSilent   bool
	MinTime       float32
	MaxTime       float32
	MuteInEditor  bool
}

type LightObject struct {
	ID            int
	Name          string
	Origin        Vector3
	Scale         Vector3
	Angles        Vector3
	Color         Vector3
	LightType     string
	Radius        float32
	Intensity     float32
	Visible       bool
	ParallaxDepth Vector2
}

func (value *ScriptValue) UnmarshalJSON(raw []byte) error {
	*value = ScriptValue{
		Value:            0,
		Script:           "",
		ScriptProperties: make(map[string]json.RawMessage),
	}

	trimmed := bytesFromRawNullAware(raw)
	if trimmed == nil {
		return nil
	}

	type scriptJSON struct {
		Value            FloatValue                 `json:"value"`
		Script           StringValue                `json:"script"`
		ScriptProperties map[string]json.RawMessage `json:"scriptproperties"`
	}

	var payload scriptJSON
	if err := json.Unmarshal(trimmed, &payload); err == nil {
		value.Value = float32(payload.Value)
		value.Script = string(payload.Script)
		if payload.ScriptProperties != nil {
			value.ScriptProperties = payload.ScriptProperties
		}
		return nil
	}

	numericValue, err := parseFloat64FromRaw(trimmed)
	if err != nil {
		return fmt.Errorf("cannot parse script value: %w", err)
	}
	value.Value = float32(numericValue)
	return nil
}

func (sound *SoundObject) parseFromSceneJSON(raw json.RawMessage) error {
	payload := struct {
		ID            IntValue      `json:"id"`
		Name          StringValue   `json:"name"`
		Origin        Vector3       `json:"origin"`
		Scale         Vector3       `json:"scale"`
		Angles        Vector3       `json:"angles"`
		ParallaxDepth Vector2       `json:"parallaxDepth"`
		Sound         []StringValue `json:"sound"`
		PlaybackMode  StringValue   `json:"playbackmode"`
		Volume        ScriptValue   `json:"volume"`
		StartSilent   BoolValue     `json:"startsilent"`
		MinTime       FloatValue    `json:"mintime"`
		MaxTime       FloatValue    `json:"maxtime"`
		MuteInEditor  BoolValue     `json:"muteineditor"`
	}{}

	if err := json.Unmarshal(raw, &payload); err != nil {
		return fmt.Errorf("cannot parse sound object: %w", err)
	}

	for _, entry := range payload.Sound {
		if entry == "" {
			continue
		}
		sound.SoundFiles = append(sound.SoundFiles, string(entry))
	}
	sound.Name = string(payload.Name)
	sound.ID = int(payload.ID)
	sound.Origin = payload.Origin
	sound.Scale = payload.Scale
	sound.Angles = payload.Angles
	sound.ParallaxDepth = payload.ParallaxDepth
	sound.PlaybackMode = SoundPlaybackMode(payload.PlaybackMode)
	sound.Volume = payload.Volume
	sound.StartSilent = bool(payload.StartSilent)
	sound.MinTime = float32(payload.MinTime)
	sound.MaxTime = float32(payload.MaxTime)
	sound.MuteInEditor = bool(payload.MuteInEditor)

	return nil
}

func (light *LightObject) parseFromSceneJSON(raw json.RawMessage) error {
	payload := struct {
		Origin        Vector3     `json:"origin"`
		Scale         Vector3     `json:"scale"`
		Angles        Vector3     `json:"angles"`
		Color         Vector3     `json:"color"`
		Light         StringValue `json:"light"`
		Radius        FloatValue  `json:"radius"`
		Intensity     FloatValue  `json:"intensity"`
		Visible       BoolValue   `json:"visible"`
		Name          StringValue `json:"name"`
		ID            IntValue    `json:"id"`
		ParallaxDepth Vector2     `json:"parallaxDepth"`
	}{
		Visible: true,
	}

	if err := json.Unmarshal(raw, &payload); err != nil {
		return fmt.Errorf("cannot parse light object: %w", err)
	}
	if payload.Light == "" {
		return fmt.Errorf("light object missing light type")
	}

	light.Origin = payload.Origin
	light.Scale = payload.Scale
	light.Color = payload.Color
	light.LightType = string(payload.Light)
	light.Radius = float32(payload.Radius)
	light.Intensity = float32(payload.Intensity)

	light.Angles = payload.Angles
	light.Visible = bool(payload.Visible)
	light.Name = string(payload.Name)
	light.ID = int(payload.ID)
	light.ParallaxDepth = payload.ParallaxDepth

	return nil
}

type OrthogonalProjection struct {
	Width  int
	Height int
	Auto   bool
}

type SceneCamera struct {
	Center Vector3
	Eye    Vector3
	Up     Vector3
}

type SceneGeneral struct {
	ClearColor             Vector3
	Parallax               bool
	ParallaxAmount         float32
	ParallaxDelay          float32
	ParallaxMouseInfluence float32
	Orthographic           bool
	Ortho                  OrthogonalProjection
	Zoom                   float32
	FOV                    float32
	NearZ                  float32
	FarZ                   float32
	AmbientColor           Vector3
	SkyLightColor          Vector3
}

type SceneObject any

type Scene struct {
	Objects []SceneObject
	Camera  SceneCamera
	General SceneGeneral
	Version int
}

func parseOrthogonalProjection(raw json.RawMessage) (OrthogonalProjection, bool, error) {
	type orthogonalProjectionJSON struct {
		Width  IntValue  `json:"width"`
		Height IntValue  `json:"height"`
		Auto   BoolValue `json:"auto"`
	}

	var projection OrthogonalProjection
	if bytesFromRawNullAware(raw) == nil {
		return projection, true, nil
	}
	var payload orthogonalProjectionJSON
	if err := json.Unmarshal(raw, &payload); err != nil {
		return projection, false, fmt.Errorf("cannot parse orthogonal projection: %w", err)
	}
	if payload.Auto {
		projection.Auto = true
		return projection, true, nil
	}
	projection.Width = int(payload.Width)
	projection.Height = int(payload.Height)
	return projection, true, nil
}

func parseSceneCamera(raw json.RawMessage) (SceneCamera, error) {
	type cameraJSON struct {
		Center Vector3 `json:"center"`
		Eye    Vector3 `json:"eye"`
		Up     Vector3 `json:"up"`
	}

	var camera SceneCamera
	var payload cameraJSON
	if err := json.Unmarshal(raw, &payload); err != nil {
		return camera, fmt.Errorf("cannot parse camera: %w", err)
	}
	camera.Center = payload.Center
	camera.Eye = payload.Eye
	camera.Up = payload.Up

	return camera, nil
}

func parseSceneGeneral(raw json.RawMessage) (SceneGeneral, error) {
	type generalJSON struct {
		AmbientColor            Vector3         `json:"ambientcolor"`
		SkyLightColor           Vector3         `json:"skylightcolor"`
		ClearColor              Vector3         `json:"clearcolor"`
		CameraParallax          BoolValue       `json:"cameraparallax"`
		CameraParallaxAmount    FloatValue      `json:"cameraparallaxamount"`
		CameraParallaxDelay     FloatValue      `json:"cameraparallaxdelay"`
		CameraParallaxMouse     FloatValue      `json:"cameraparallaxmouseinfluence"`
		Zoom                    FloatValue      `json:"zoom"`
		FOV                     FloatValue      `json:"perspectiveoverridefov"`
		NearZ                   FloatValue      `json:"nearz"`
		FarZ                    FloatValue      `json:"farz"`
		OrthogonalProjectionRaw json.RawMessage `json:"orthogonalprojection"`
	}

	general := SceneGeneral{
		Ortho:         OrthogonalProjection{Width: 1920, Height: 1080},
		Zoom:          1,
		FOV:           90,
		NearZ:         0.01,
		FarZ:          10000,
		AmbientColor:  Vector3{0.2, 0.2, 0.2},
		SkyLightColor: Vector3{0.3, 0.3, 0.3},
		Orthographic:  true,
	}

	var payload generalJSON
	if err := json.Unmarshal(raw, &payload); err != nil {
		return general, fmt.Errorf("cannot parse general block: %w", err)
	}

	general.AmbientColor = payload.AmbientColor
	general.SkyLightColor = payload.SkyLightColor
	general.ClearColor = payload.ClearColor
	general.Parallax = bool(payload.CameraParallax)
	general.ParallaxAmount = float32(payload.CameraParallaxAmount)
	general.ParallaxDelay = float32(payload.CameraParallaxDelay)
	general.ParallaxMouseInfluence = float32(payload.CameraParallaxMouse)

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

	if payload.OrthogonalProjectionRaw != nil {
		projection, present, err := parseOrthogonalProjection(payload.OrthogonalProjectionRaw)
		if err != nil {
			return general, err
		}
		if present {
			if bytesFromRawNullAware(payload.OrthogonalProjectionRaw) == nil {
				general.Orthographic = false
			} else {
				general.Ortho = projection
				general.Orthographic = true
			}
		}
	}

	return general, nil
}

func ParseScene(pkgMap *map[string][]byte) (Scene, error) {
	sceneBytes, err := loadBytesFromPackage(pkgMap, "scene.json")
	if err != nil {
		return Scene{}, err
	}
	type sceneJSON struct {
		Camera  json.RawMessage   `json:"camera"`
		General json.RawMessage   `json:"general"`
		Objects []json.RawMessage `json:"objects"`
		Version IntValue          `json:"version"`
	}

	var payload sceneJSON
	if err := json.Unmarshal(sceneBytes, &payload); err != nil {
		return Scene{}, fmt.Errorf("cannot parse scene JSON: %w", err)
	}

	scene := Scene{}

	if payload.Camera != nil {
		camera, parseErr := parseSceneCamera(payload.Camera)
		if parseErr != nil {
			return Scene{}, parseErr
		}
		scene.Camera = camera
	} else {
		return Scene{}, fmt.Errorf("scene has no camera object")
	}

	if payload.General != nil {
		general, parseErr := parseSceneGeneral(payload.General)
		if parseErr != nil {
			return Scene{}, parseErr
		}
		scene.General = general
	} else {
		return Scene{}, fmt.Errorf("scene has no general block")
	}

	scene.Version = int(payload.Version)

	for _, objectRaw := range payload.Objects {
		var objectProbe struct {
			Particle json.RawMessage `json:"particle"`
			Image    json.RawMessage `json:"image"`
			Sound    json.RawMessage `json:"sound"`
			Light    json.RawMessage `json:"light"`
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
			continue
		}
		if len(objectProbe.Image) > 0 {
			var imageObject ImageObject
			if err := imageObject.parseFromSceneJSON(objectRaw, pkgMap); err != nil {
				return Scene{}, err
			}
			scene.Objects = append(scene.Objects, &imageObject)
			continue
		}
		if len(objectProbe.Sound) > 0 {
			var soundObject SoundObject
			if err := soundObject.parseFromSceneJSON(objectRaw); err != nil {
				return Scene{}, err
			}
			scene.Objects = append(scene.Objects, &soundObject)
			continue
		}
		if len(objectProbe.Light) > 0 {
			var lightObject LightObject
			if err := lightObject.parseFromSceneJSON(objectRaw); err != nil {
				return Scene{}, err
			}
			scene.Objects = append(scene.Objects, &lightObject)
			continue
		}
	}

	return scene, nil
}
