package main

import (
	"encoding/json"
	"fmt"
)

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

func parseScriptValue(raw json.RawMessage) (ScriptValue, error) {
	result := ScriptValue{
		Value:            0,
		Script:           "",
		ScriptProperties: make(map[string]json.RawMessage),
	}

	trimmed := bytesFromRawNullAware(raw)
	if trimmed == nil {
		return result, nil
	}

	var maybeObject map[string]json.RawMessage
	err := json.Unmarshal(trimmed, &maybeObject)
	if err != nil {
		value, parseErr := parseFloat64FromRaw(trimmed)
		if parseErr != nil {
			return result, fmt.Errorf("cannot parse numeric script value: %w", parseErr)
		}
		result.Value = float32(value)
		return result, nil
	}

	if valueRaw, exists := maybeObject["value"]; exists {
		value, parseErr := parseFloat64FromRaw(valueRaw)
		if parseErr == nil {
			result.Value = float32(value)
		}
	}
	if scriptRaw, exists := maybeObject["script"]; exists {
		script, parseErr := parseStringFromRaw(scriptRaw)
		if parseErr == nil {
			result.Script = script
		}
	}
	if propsRaw, exists := maybeObject["scriptproperties"]; exists {
		var props map[string]json.RawMessage
		parseErr := json.Unmarshal(propsRaw, &props)
		if parseErr == nil {
			result.ScriptProperties = props
		}
	}
	return result, nil
}

func (sound *SoundObject) parseFromSceneJSON(raw json.RawMessage) error {
	var object map[string]json.RawMessage
	err := json.Unmarshal(raw, &object)
	if err != nil {
		return fmt.Errorf("cannot parse sound object: %w", err)
	}

	if soundRaw, exists := getOptionalField(object, "sound"); exists {
		var soundArray []json.RawMessage
		err := json.Unmarshal(soundRaw, &soundArray)
		if err != nil {
			return fmt.Errorf("cannot parse sound file array: %w", err)
		}
		for _, entry := range soundArray {
			if bytesFromRawNullAware(entry) == nil {
				continue
			}
			path, parseErr := parseStringFromRaw(entry)
			if parseErr != nil {
				return fmt.Errorf("cannot parse sound file path: %w", parseErr)
			}
			sound.SoundFiles = append(sound.SoundFiles, path)
		}
	}

	if nameRaw, exists := getOptionalField(object, "name"); exists {
		name, parseErr := parseStringFromRaw(nameRaw)
		if parseErr == nil {
			sound.Name = name
		}
	}
	if idRaw, exists := getOptionalField(object, "id"); exists {
		idValue, parseErr := parseIntFromRaw(idRaw)
		if parseErr == nil {
			sound.ID = idValue
		}
	}
	if originRaw, exists := getOptionalField(object, "origin"); exists {
		origin, parseErr := parseVector3FromRaw(originRaw, sound.Origin)
		if parseErr == nil {
			sound.Origin = origin
		}
	}
	if scaleRaw, exists := getOptionalField(object, "scale"); exists {
		scale, parseErr := parseVector3FromRaw(scaleRaw, sound.Scale)
		if parseErr == nil {
			sound.Scale = scale
		}
	}
	if anglesRaw, exists := getOptionalField(object, "angles"); exists {
		angles, parseErr := parseVector3FromRaw(anglesRaw, sound.Angles)
		if parseErr == nil {
			sound.Angles = angles
		}
	}
	if parallaxRaw, exists := getOptionalField(object, "parallaxDepth"); exists {
		parallax, parseErr := parseVector2FromRaw(parallaxRaw, sound.ParallaxDepth)
		if parseErr == nil {
			sound.ParallaxDepth = parallax
		}
	}
	if modeRaw, exists := getOptionalField(object, "playbackmode"); exists {
		mode, parseErr := parseStringFromRaw(modeRaw)
		if parseErr == nil {
			sound.PlaybackMode = SoundPlaybackMode(mode)
		}
	}
	if volumeRaw, exists := getOptionalField(object, "volume"); exists {
		value, parseErr := parseScriptValue(volumeRaw)
		if parseErr == nil {
			sound.Volume = value
		}
	}
	if startSilentRaw, exists := getOptionalField(object, "startsilent"); exists {
		value, parseErr := parseBoolFromRaw(startSilentRaw)
		if parseErr == nil {
			sound.StartSilent = value
		}
	}
	if minTimeRaw, exists := getOptionalField(object, "mintime"); exists {
		value, parseErr := parseFloat64FromRaw(minTimeRaw)
		if parseErr == nil {
			sound.MinTime = float32(value)
		}
	}
	if maxTimeRaw, exists := getOptionalField(object, "maxtime"); exists {
		value, parseErr := parseFloat64FromRaw(maxTimeRaw)
		if parseErr == nil {
			sound.MaxTime = float32(value)
		}
	}
	if muteRaw, exists := getOptionalField(object, "muteineditor"); exists {
		value, parseErr := parseBoolFromRaw(muteRaw)
		if parseErr == nil {
			sound.MuteInEditor = value
		}
	}

	return nil
}

func (light *LightObject) parseFromSceneJSON(raw json.RawMessage) error {
	var object map[string]json.RawMessage
	err := json.Unmarshal(raw, &object)
	if err != nil {
		return fmt.Errorf("cannot parse light object: %w", err)
	}
	originRaw, err := getRequiredField(object, "origin")
	if err != nil {
		return err
	}
	scaleRaw, err := getRequiredField(object, "scale")
	if err != nil {
		return err
	}
	colorRaw, err := getRequiredField(object, "color")
	if err != nil {
		return err
	}
	lightRaw, err := getRequiredField(object, "light")
	if err != nil {
		return err
	}
	radiusRaw, err := getRequiredField(object, "radius")
	if err != nil {
		return err
	}
	intensityRaw, err := getRequiredField(object, "intensity")
	if err != nil {
		return err
	}

	origin, err := parseVector3FromRaw(originRaw, light.Origin)
	if err != nil {
		return fmt.Errorf("cannot parse light origin: %w", err)
	}
	scale, err := parseVector3FromRaw(scaleRaw, light.Scale)
	if err != nil {
		return fmt.Errorf("cannot parse light scale: %w", err)
	}
	color, err := parseVector3FromRaw(colorRaw, light.Color)
	if err != nil {
		return fmt.Errorf("cannot parse light color: %w", err)
	}
	lightType, err := parseStringFromRaw(lightRaw)
	if err != nil {
		return fmt.Errorf("cannot parse light type: %w", err)
	}
	radius, err := parseFloat64FromRaw(radiusRaw)
	if err != nil {
		return fmt.Errorf("cannot parse light radius: %w", err)
	}
	intensity, err := parseFloat64FromRaw(intensityRaw)
	if err != nil {
		return fmt.Errorf("cannot parse light intensity: %w", err)
	}

	light.Origin = origin
	light.Scale = scale
	light.Color = color
	light.LightType = lightType
	light.Radius = float32(radius)
	light.Intensity = float32(intensity)

	if anglesRaw, exists := getOptionalField(object, "angles"); exists {
		angles, err := parseVector3FromRaw(anglesRaw, light.Angles)
		if err != nil {
			return fmt.Errorf("cannot parse light angles: %w", err)
		}
		light.Angles = angles
	}

	if visibleRaw, exists := getOptionalField(object, "visible"); exists {
		value, parseErr := parseBoolFromRaw(visibleRaw)
		if parseErr == nil {
			light.Visible = value
		}
	}
	if nameRaw, exists := getOptionalField(object, "name"); exists {
		name, parseErr := parseStringFromRaw(nameRaw)
		if parseErr == nil {
			light.Name = name
		}
	}
	if idRaw, exists := getOptionalField(object, "id"); exists {
		value, parseErr := parseIntFromRaw(idRaw)
		if parseErr == nil {
			light.ID = value
		}
	}
	if parallaxRaw, exists := getOptionalField(object, "parallaxDepth"); exists {
		parallax, parseErr := parseVector2FromRaw(parallaxRaw, light.ParallaxDepth)
		if parseErr == nil {
			light.ParallaxDepth = parallax
		}
	}

	return nil
}
