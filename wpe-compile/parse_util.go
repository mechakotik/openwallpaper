package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"strconv"
	"strings"
)

type (
	Vector2 [2]float32
	Vector3 [3]float32
)

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
	if len(values) != 3 {
		return defaultValue, fmt.Errorf("expected 3 components, got %d", len(values))
	}
	return Vector3{values[0], values[1], values[2]}, nil
}

func getRequiredField(object map[string]json.RawMessage, fieldName string) (json.RawMessage, error) {
	raw, exists := object[fieldName]
	if !exists {
		return nil, fmt.Errorf("required field %q is missing", fieldName)
	}
	return raw, nil
}

func getOptionalField(object map[string]json.RawMessage, fieldName string) (json.RawMessage, bool) {
	raw, exists := object[fieldName]
	if !exists {
		return nil, false
	}
	return raw, true
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
	data, err := os.ReadFile("assets/" + path)
	if err == nil {
		return data, nil
	}
	return nil, fmt.Errorf("file %s not found", path)
}

func loadJSONDocument(pkgMap *map[string][]byte, path string) (map[string]json.RawMessage, error) {
	bytesData, err := loadBytesFromPackage(pkgMap, path)
	if err != nil {
		return nil, err
	}
	var document map[string]json.RawMessage
	err = json.Unmarshal(bytesData, &document)
	if err != nil {
		return nil, fmt.Errorf("cannot parse JSON file %s: %w", path, err)
	}
	return document, nil
}

func parseCombosMapFromRaw(raw json.RawMessage) (map[string]int, error) {
	result := make(map[string]int)
	var rawMap map[string]json.RawMessage
	err := json.Unmarshal(raw, &rawMap)
	if err != nil {
		return nil, fmt.Errorf("cannot parse combos object: %w", err)
	}
	for key, valueRaw := range rawMap {
		value, parseErr := parseIntFromRaw(valueRaw)
		if parseErr != nil {
			return nil, fmt.Errorf("cannot parse combo value for %q: %w", key, parseErr)
		}
		result[key] = value
	}
	return result, nil
}

func parseFloatSliceMapFromRaw(raw json.RawMessage) (map[string][]float32, error) {
	result := make(map[string][]float32)
	var rawMap map[string]json.RawMessage
	err := json.Unmarshal(raw, &rawMap)
	if err != nil {
		return nil, fmt.Errorf("cannot parse float slice map: %w", err)
	}
	for key, valueRaw := range rawMap {
		value, parseErr := parseFloatSliceFromRaw(valueRaw)
		if parseErr != nil {
			return nil, fmt.Errorf("cannot parse float slice for key %q: %w", key, parseErr)
		}
		result[key] = value
	}
	return result, nil
}
