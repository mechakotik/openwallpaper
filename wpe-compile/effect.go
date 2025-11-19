package main

import (
	"encoding/json"
	"errors"
	"fmt"
	"strings"
)

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
	var object map[string]json.RawMessage
	err := json.Unmarshal(raw, &object)
	if err != nil {
		return fmt.Errorf("cannot parse effect command: %w", err)
	}
	commandRaw, err := getRequiredField(object, "command")
	if err != nil {
		return err
	}
	targetRaw, err := getRequiredField(object, "target")
	if err != nil {
		return err
	}
	sourceRaw, err := getRequiredField(object, "source")
	if err != nil {
		return err
	}
	commandName, err := parseStringFromRaw(commandRaw)
	if err != nil {
		return fmt.Errorf("cannot parse effect command name: %w", err)
	}
	target, err := parseStringFromRaw(targetRaw)
	if err != nil {
		return fmt.Errorf("cannot parse effect command target: %w", err)
	}
	source, err := parseStringFromRaw(sourceRaw)
	if err != nil {
		return fmt.Errorf("cannot parse effect command source: %w", err)
	}
	command.Command = commandName
	command.Target = target
	command.Source = source
	return nil
}

func (fbo *EffectFBO) parseFromJSON(raw json.RawMessage) error {
	var object map[string]json.RawMessage
	err := json.Unmarshal(raw, &object)
	if err != nil {
		return fmt.Errorf("cannot parse effect fbo: %w", err)
	}
	nameRaw, err := getRequiredField(object, "name")
	if err != nil {
		return err
	}
	name, err := parseStringFromRaw(nameRaw)
	if err != nil {
		return fmt.Errorf("cannot parse fbo name: %w", err)
	}
	fbo.Name = name

	if formatRaw, exists := getOptionalField(object, "format"); exists {
		format, parseErr := parseStringFromRaw(formatRaw)
		if parseErr != nil {
			return fmt.Errorf("cannot parse fbo format: %w", parseErr)
		}
		fbo.Format = format
	}
	if scaleRaw, exists := getOptionalField(object, "scale"); exists {
		scale, parseErr := parseIntFromRaw(scaleRaw)
		if parseErr != nil {
			return fmt.Errorf("cannot parse fbo scale: %w", parseErr)
		}
		fbo.Scale = scale
	} else {
		fbo.Scale = 1
	}
	return nil
}

func (effect *ImageEffect) parseFromFileJSON(raw json.RawMessage, pkgMap *map[string][]byte) error {
	var object map[string]json.RawMessage
	err := json.Unmarshal(raw, &object)
	if err != nil {
		return fmt.Errorf("cannot parse effect file JSON: %w", err)
	}

	if versionRaw, exists := getOptionalField(object, "version"); exists {
		version, parseErr := parseIntFromRaw(versionRaw)
		if parseErr == nil {
			effect.Version = version
		}
	}

	nameRaw, err := getRequiredField(object, "name")
	if err != nil {
		return err
	}
	name, err := parseStringFromRaw(nameRaw)
	if err != nil {
		return fmt.Errorf("cannot parse effect name: %w", err)
	}
	effect.Name = name

	if fbosRaw, exists := getOptionalField(object, "fbos"); exists {
		var fbosArray []json.RawMessage
		err := json.Unmarshal(fbosRaw, &fbosArray)
		if err != nil {
			return fmt.Errorf("cannot parse effect fbos: %w", err)
		}
		for _, fboRaw := range fbosArray {
			var fbo EffectFBO
			parseErr := fbo.parseFromJSON(fboRaw)
			if parseErr != nil {
				return parseErr
			}
			effect.FBOs = append(effect.FBOs, fbo)
		}
	}

	passesRaw, exists := object["passes"]
	if !exists {
		return errors.New("effect has no passes field")
	}
	var passesArray []json.RawMessage
	err = json.Unmarshal(passesRaw, &passesArray)
	if err != nil {
		return fmt.Errorf("cannot parse effect passes: %w", err)
	}

	composeEnabled := false

	for _, passRaw := range passesArray {
		var passObject map[string]json.RawMessage
		err := json.Unmarshal(passRaw, &passObject)
		if err != nil {
			return fmt.Errorf("cannot parse effect pass object: %w", err)
		}

		if _, hasMaterial := passObject["material"]; !hasMaterial {
			if _, hasCommand := passObject["command"]; hasCommand {
				var command EffectCommand
				parseErr := command.parseFromJSON(passRaw)
				if parseErr != nil {
					return parseErr
				}
				command.AfterPos = len(effect.Passes)
				effect.Commands = append(effect.Commands, command)
				continue
			}
			return errors.New("effect pass has no material or command")
		}

		materialPathRaw, err := getRequiredField(passObject, "material")
		if err != nil {
			return err
		}
		materialPath, err := parseStringFromRaw(materialPathRaw)
		if err != nil {
			return fmt.Errorf("cannot parse effect material path: %w", err)
		}

		materialDocument, err := loadJSONDocument(pkgMap, materialPath)
		if err != nil {
			return fmt.Errorf("cannot load material file %s: %w", materialPath, err)
		}
		materialBytes, err := json.Marshal(materialDocument)
		if err != nil {
			return fmt.Errorf("cannot re-encode material JSON: %w", err)
		}
		var material Material
		err = material.parseFromJSON(materialBytes)
		if err != nil {
			return fmt.Errorf("cannot parse material %s: %w", materialPath, err)
		}
		effect.Materials = append(effect.Materials, material)

		var pass MaterialPass
		err = pass.parseFromJSON(passRaw)
		if err != nil {
			return err
		}
		effect.Passes = append(effect.Passes, pass)

		if composeRaw, exists := passObject["compose"]; exists {
			compose, parseErr := parseBoolFromRaw(composeRaw)
			if parseErr == nil && compose {
				composeEnabled = true
			}
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
	var object map[string]json.RawMessage
	err := json.Unmarshal(raw, &object)
	if err != nil {
		return fmt.Errorf("cannot parse image effect from scene: %w", err)
	}

	fileRaw, err := getRequiredField(object, "file")
	if err != nil {
		return err
	}
	effectPath, err := parseStringFromRaw(fileRaw)
	if err != nil {
		return fmt.Errorf("cannot parse effect file path: %w", err)
	}

	effectDocument, err := loadJSONDocument(pkgMap, effectPath)
	if err != nil {
		return fmt.Errorf("cannot load effect file %s: %w", effectPath, err)
	}
	effectBytes, err := json.Marshal(effectDocument)
	if err != nil {
		return fmt.Errorf("cannot re-encode effect JSON: %w", err)
	}
	err = effect.parseFromFileJSON(effectBytes, pkgMap)
	if err != nil {
		return err
	}

	if idRaw, exists := getOptionalField(object, "id"); exists {
		idValue, parseErr := parseStringFromRaw(idRaw)
		if parseErr == nil {
			effect.ID = idValue
		}
	}

	if nameRaw, exists := getOptionalField(object, "name"); exists {
		nameValue, parseErr := parseStringFromRaw(nameRaw)
		if parseErr == nil && strings.TrimSpace(nameValue) != "" {
			effect.Name = nameValue
		}
	}

	effect.Visible = true
	if visibleRaw, exists := getOptionalField(object, "visible"); exists {
		visible, parseErr := parseBoolFromRaw(visibleRaw)
		if parseErr == nil {
			effect.Visible = visible
		}
	}

	if passesRaw, exists := getOptionalField(object, "passes"); exists {
		var passesArray []json.RawMessage
		err := json.Unmarshal(passesRaw, &passesArray)
		if err != nil {
			return fmt.Errorf("cannot parse effect passes overrides: %w", err)
		}
		for index, passOverrideRaw := range passesArray {
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
	}

	return nil
}
