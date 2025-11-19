package main

import (
	"bytes"
	"encoding/json"
	"fmt"
)

type MaterialPassBindItem struct {
	Name  string
	Index int
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

func (bindItem *MaterialPassBindItem) parseFromJSON(raw json.RawMessage) error {
	var object map[string]json.RawMessage
	err := json.Unmarshal(raw, &object)
	if err != nil {
		return fmt.Errorf("cannot parse material bind item: %w", err)
	}
	nameRaw, err := getRequiredField(object, "name")
	if err != nil {
		return err
	}
	indexRaw, err := getRequiredField(object, "index")
	if err != nil {
		return err
	}
	name, err := parseStringFromRaw(nameRaw)
	if err != nil {
		return fmt.Errorf("cannot parse bind name: %w", err)
	}
	index, err := parseIntFromRaw(indexRaw)
	if err != nil {
		return fmt.Errorf("cannot parse bind index: %w", err)
	}
	bindItem.Name = name
	bindItem.Index = index
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
	for key, value := range other.Combos {
		materialPass.Combos[key] = value
	}
	if materialPass.Constants == nil {
		materialPass.Constants = make(map[string][]float32)
	}
	for key, value := range other.Constants {
		materialPass.Constants[key] = value
	}
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
	for key, value := range other.Combos {
		material.Combos[key] = value
	}
	if material.Constants == nil {
		material.Constants = make(map[string][]float32)
	}
	for key, value := range other.Constants {
		material.Constants[key] = value
	}
}

func (materialPass *MaterialPass) parseFromJSON(raw json.RawMessage) error {
	var object map[string]json.RawMessage
	err := json.Unmarshal(raw, &object)
	if err != nil {
		return fmt.Errorf("cannot parse material pass: %w", err)
	}

	if texturesRaw, exists := getOptionalField(object, "textures"); exists {
		var texturesArray []json.RawMessage
		err = json.Unmarshal(texturesRaw, &texturesArray)
		if err != nil {
			return fmt.Errorf("cannot parse textures array: %w", err)
		}
		for _, textureRaw := range texturesArray {
			if bytes := bytesFromRawNullAware(textureRaw); bytes == nil {
				materialPass.Textures = append(materialPass.Textures, "")
			} else {
				textureName, parseErr := parseStringFromRaw(textureRaw)
				if parseErr != nil {
					return fmt.Errorf("cannot parse texture name: %w", parseErr)
				}
				materialPass.Textures = append(materialPass.Textures, textureName)
			}
		}
	}

	if constantValuesRaw, exists := getOptionalField(object, "constantshadervalues"); exists {
		values, parseErr := parseFloatSliceMapFromRaw(constantValuesRaw)
		if parseErr != nil {
			return fmt.Errorf("cannot parse constant shader values: %w", parseErr)
		}
		if materialPass.Constants == nil {
			materialPass.Constants = make(map[string][]float32)
		}
		for key, value := range values {
			materialPass.Constants[key] = value
		}
	}

	if combosRaw, exists := getOptionalField(object, "combos"); exists {
		combos, parseErr := parseCombosMapFromRaw(combosRaw)
		if parseErr != nil {
			return fmt.Errorf("cannot parse combos: %w", parseErr)
		}
		if materialPass.Combos == nil {
			materialPass.Combos = make(map[string]int)
		}
		for key, value := range combos {
			materialPass.Combos[key] = value
		}
	}

	if targetRaw, exists := getOptionalField(object, "target"); exists {
		target, parseErr := parseStringFromRaw(targetRaw)
		if parseErr != nil {
			return fmt.Errorf("cannot parse material pass target: %w", parseErr)
		}
		materialPass.Target = target
	}

	if bindRaw, exists := getOptionalField(object, "bind"); exists {
		var bindArray []json.RawMessage
		err = json.Unmarshal(bindRaw, &bindArray)
		if err != nil {
			return fmt.Errorf("cannot parse bind array: %w", err)
		}
		for _, bindItemRaw := range bindArray {
			var bindItem MaterialPassBindItem
			parseErr := bindItem.parseFromJSON(bindItemRaw)
			if parseErr != nil {
				return parseErr
			}
			materialPass.Bind = append(materialPass.Bind, bindItem)
		}
	}

	return nil
}

func (material *Material) parseFromJSON(raw json.RawMessage) error {
	var root map[string]json.RawMessage
	err := json.Unmarshal(raw, &root)
	if err != nil {
		return fmt.Errorf("cannot parse material document: %w", err)
	}
	passesRaw, exists := root["passes"]
	if !exists {
		return fmt.Errorf("material has no passes field")
	}
	var passesArray []json.RawMessage
	err = json.Unmarshal(passesRaw, &passesArray)
	if err != nil {
		return fmt.Errorf("cannot parse material passes array: %w", err)
	}
	if len(passesArray) == 0 {
		return fmt.Errorf("material has empty passes array")
	}
	firstPassRaw := passesArray[0]
	var firstPass map[string]json.RawMessage
	err = json.Unmarshal(firstPassRaw, &firstPass)
	if err != nil {
		return fmt.Errorf("cannot parse material first pass: %w", err)
	}

	blendingRaw, err := getRequiredField(firstPass, "blending")
	if err != nil {
		return err
	}
	cullModeRaw, err := getRequiredField(firstPass, "cullmode")
	if err != nil {
		return err
	}
	depthTestRaw, err := getRequiredField(firstPass, "depthtest")
	if err != nil {
		return err
	}
	depthWriteRaw, err := getRequiredField(firstPass, "depthwrite")
	if err != nil {
		return err
	}
	shaderRaw, err := getRequiredField(firstPass, "shader")
	if err != nil {
		return err
	}

	blending, err := parseStringFromRaw(blendingRaw)
	if err != nil {
		return fmt.Errorf("cannot parse material blending: %w", err)
	}
	cullMode, err := parseStringFromRaw(cullModeRaw)
	if err != nil {
		return fmt.Errorf("cannot parse material cull mode: %w", err)
	}
	depthTest, err := parseStringFromRaw(depthTestRaw)
	if err != nil {
		return fmt.Errorf("cannot parse material depth test: %w", err)
	}
	depthWrite, err := parseStringFromRaw(depthWriteRaw)
	if err != nil {
		return fmt.Errorf("cannot parse material depth write: %w", err)
	}
	shader, err := parseStringFromRaw(shaderRaw)
	if err != nil {
		return fmt.Errorf("cannot parse material shader: %w", err)
	}

	material.Blending = blending
	material.CullMode = cullMode
	material.DepthTest = depthTest
	material.DepthWrite = depthWrite
	material.Shader = shader

	if texturesRaw, exists := getOptionalField(firstPass, "textures"); exists {
		var texturesArray []json.RawMessage
		err = json.Unmarshal(texturesRaw, &texturesArray)
		if err != nil {
			return fmt.Errorf("cannot parse material textures array: %w", err)
		}
		for _, textureRaw := range texturesArray {
			if bytes := bytesFromRawNullAware(textureRaw); bytes == nil {
				material.Textures = append(material.Textures, "")
			} else {
				textureName, parseErr := parseStringFromRaw(textureRaw)
				if parseErr != nil {
					return fmt.Errorf("cannot parse material texture: %w", parseErr)
				}
				material.Textures = append(material.Textures, textureName)
			}
		}
	}
	if constantValuesRaw, exists := getOptionalField(firstPass, "constantshadervalues"); exists {
		values, parseErr := parseFloatSliceMapFromRaw(constantValuesRaw)
		if parseErr != nil {
			return fmt.Errorf("cannot parse material constant values: %w", parseErr)
		}
		if material.Constants == nil {
			material.Constants = make(map[string][]float32)
		}
		for key, value := range values {
			material.Constants[key] = value
		}
	}
	if combosRaw, exists := getOptionalField(firstPass, "combos"); exists {
		combos, parseErr := parseCombosMapFromRaw(combosRaw)
		if parseErr != nil {
			return fmt.Errorf("cannot parse material combos: %w", parseErr)
		}
		if material.Combos == nil {
			material.Combos = make(map[string]int)
		}
		for key, value := range combos {
			material.Combos[key] = value
		}
	}

	return nil
}

func bytesFromRawNullAware(raw json.RawMessage) []byte {
	trimmed := bytes.TrimSpace(raw)
	if bytes.Equal(trimmed, []byte("null")) {
		return nil
	}
	return raw
}
