package main

import (
	"encoding/json"
	"fmt"
	"strings"
)

type ParticleControlPointFlags uint32

type ParticleControlPoint struct {
	Flags  ParticleControlPointFlags
	ID     int
	Offset Vector3
}

type ParticleRender struct {
	Name        string
	Length      float32
	MaxLength   float32
	Subdivision float32
}

type Initializer struct {
	Name string
	Max  Vector3
	Min  Vector3
}

type EmitterFlags uint32

type Emitter struct {
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

type Particle struct {
	Emitters           []Emitter
	Initializers       []json.RawMessage
	Operators          []json.RawMessage
	Renderers          []ParticleRender
	ControlPoints      []ParticleControlPoint
	Material           Material
	Children           []ParticleChild
	AnimationMode      uint32
	SequenceMultiplier float32
	MaxCount           uint32
	StartTime          float32
	Flags              ParticleFlags
}

type ParticleInstanceOverride struct {
	Enabled        bool
	OverrideColor  bool
	OverrideColorn bool
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
	var object map[string]json.RawMessage
	err := json.Unmarshal(raw, &object)
	if err != nil {
		return fmt.Errorf("cannot parse particle instance override: %w", err)
	}
	override.Enabled = true

	if alphaRaw, exists := getOptionalField(object, "alpha"); exists {
		alpha, parseErr := parseFloat64FromRaw(alphaRaw)
		if parseErr == nil {
			override.Alpha = float32(alpha)
		}
	}
	if sizeRaw, exists := getOptionalField(object, "size"); exists {
		size, parseErr := parseFloat64FromRaw(sizeRaw)
		if parseErr == nil {
			override.Size = float32(size)
		}
	}
	if lifetimeRaw, exists := getOptionalField(object, "lifetime"); exists {
		lifetime, parseErr := parseFloat64FromRaw(lifetimeRaw)
		if parseErr == nil {
			override.Lifetime = float32(lifetime)
		}
	}
	if rateRaw, exists := getOptionalField(object, "rate"); exists {
		rate, parseErr := parseFloat64FromRaw(rateRaw)
		if parseErr == nil {
			override.Rate = float32(rate)
		}
	}
	if speedRaw, exists := getOptionalField(object, "speed"); exists {
		speed, parseErr := parseFloat64FromRaw(speedRaw)
		if parseErr == nil {
			override.Speed = float32(speed)
		}
	}
	if countRaw, exists := getOptionalField(object, "count"); exists {
		count, parseErr := parseFloat64FromRaw(countRaw)
		if parseErr == nil {
			override.Count = float32(count)
		}
	}
	if colorRaw, exists := getOptionalField(object, "color"); exists {
		color, parseErr := parseVector3FromRaw(colorRaw, override.Color)
		if parseErr == nil {
			override.Color = color
			override.OverrideColor = true
		}
	} else if colorNRaw, exists := getOptionalField(object, "colorn"); exists {
		colorN, parseErr := parseVector3FromRaw(colorNRaw, override.ColorN)
		if parseErr == nil {
			override.ColorN = colorN
			override.OverrideColorn = true
		}
	}
	return nil
}

func (controlPoint *ParticleControlPoint) parseFromJSON(raw json.RawMessage) error {
	var object map[string]json.RawMessage
	err := json.Unmarshal(raw, &object)
	if err != nil {
		return fmt.Errorf("cannot parse particle control point: %w", err)
	}
	idRaw, err := getRequiredField(object, "id")
	if err != nil {
		return err
	}
	id, err := parseIntFromRaw(idRaw)
	if err != nil {
		return fmt.Errorf("cannot parse control point id: %w", err)
	}
	controlPoint.ID = id

	if flagsRaw, exists := getOptionalField(object, "flags"); exists {
		flagsValue, parseErr := parseIntFromRaw(flagsRaw)
		if parseErr == nil {
			controlPoint.Flags = ParticleControlPointFlags(flagsValue)
		}
	}
	if offsetRaw, exists := getOptionalField(object, "offset"); exists {
		offset, parseErr := parseVector3FromRaw(offsetRaw, controlPoint.Offset)
		if parseErr == nil {
			controlPoint.Offset = offset
		}
	}
	return nil
}

func (render *ParticleRender) parseFromJSON(raw json.RawMessage) error {
	var object map[string]json.RawMessage
	err := json.Unmarshal(raw, &object)
	if err != nil {
		return fmt.Errorf("cannot parse particle render: %w", err)
	}
	nameRaw, err := getRequiredField(object, "name")
	if err != nil {
		return err
	}
	name, err := parseStringFromRaw(nameRaw)
	if err != nil {
		return fmt.Errorf("cannot parse particle render name: %w", err)
	}
	if name == "ropetrail" {
		name = "spritetrail"
	}
	render.Name = name

	if lengthRaw, exists := getOptionalField(object, "length"); exists {
		value, parseErr := parseFloat64FromRaw(lengthRaw)
		if parseErr == nil {
			render.Length = float32(value)
		}
	}
	if maxRaw, exists := getOptionalField(object, "maxlength"); exists {
		value, parseErr := parseFloat64FromRaw(maxRaw)
		if parseErr == nil {
			render.MaxLength = float32(value)
		}
	}
	if subdivisionRaw, exists := getOptionalField(object, "subdivision"); exists {
		value, parseErr := parseFloat64FromRaw(subdivisionRaw)
		if parseErr == nil {
			render.Subdivision = float32(value)
		}
	}
	return nil
}

func (emitter *Emitter) parseFromJSON(raw json.RawMessage) error {
	var object map[string]json.RawMessage
	err := json.Unmarshal(raw, &object)
	if err != nil {
		return fmt.Errorf("cannot parse particle emitter: %w", err)
	}
	if directionsRaw, exists := getOptionalField(object, "directions"); exists {
		directions, parseErr := parseVector3FromRaw(directionsRaw, emitter.Directions)
		if parseErr == nil {
			emitter.Directions = directions
		}
	}
	if maxRaw, exists := getOptionalField(object, "distancemax"); exists {
		max, parseErr := parseVector3FromRaw(maxRaw, emitter.DistanceMax)
		if parseErr == nil {
			emitter.DistanceMax = max
		}
	}
	if minRaw, exists := getOptionalField(object, "distancemin"); exists {
		min, parseErr := parseVector3FromRaw(minRaw, emitter.DistanceMin)
		if parseErr == nil {
			emitter.DistanceMin = min
		}
	}
	if originRaw, exists := getOptionalField(object, "origin"); exists {
		origin, parseErr := parseVector3FromRaw(originRaw, emitter.Origin)
		if parseErr == nil {
			emitter.Origin = origin
		}
	}
	if signRaw, exists := getOptionalField(object, "sign"); exists {
		values, parseErr := parseFloatSliceFromRaw(signRaw)
		if parseErr == nil && len(values) == 3 {
			emitter.Sign[0] = int32(values[0])
			emitter.Sign[1] = int32(values[1])
			emitter.Sign[2] = int32(values[2])
		}
	}
	if instantaneousRaw, exists := getOptionalField(object, "instantaneous"); exists {
		value, parseErr := parseIntFromRaw(instantaneousRaw)
		if parseErr == nil {
			emitter.Instantaneous = uint32(value)
		}
	}
	if speedMinRaw, exists := getOptionalField(object, "speedmin"); exists {
		value, parseErr := parseFloat64FromRaw(speedMinRaw)
		if parseErr == nil {
			emitter.SpeedMin = float32(value)
		}
	}
	if speedMaxRaw, exists := getOptionalField(object, "speedmax"); exists {
		value, parseErr := parseFloat64FromRaw(speedMaxRaw)
		if parseErr == nil {
			emitter.SpeedMax = float32(value)
		}
	}
	if audioModeRaw, exists := getOptionalField(object, "audioprocessingmode"); exists {
		value, parseErr := parseIntFromRaw(audioModeRaw)
		if parseErr == nil {
			emitter.AudioProcessingMode = uint32(value)
		}
	}
	if controlPointRaw, exists := getOptionalField(object, "controlpoint"); exists {
		value, parseErr := parseIntFromRaw(controlPointRaw)
		if parseErr == nil {
			emitter.ControlPoint = value
		}
	}
	if idRaw, exists := getOptionalField(object, "id"); exists {
		value, parseErr := parseIntFromRaw(idRaw)
		if parseErr == nil {
			emitter.ID = value
		}
	}
	if flagsRaw, exists := getOptionalField(object, "flags"); exists {
		value, parseErr := parseIntFromRaw(flagsRaw)
		if parseErr == nil {
			emitter.Flags = EmitterFlags(value)
		}
	}
	if nameRaw, exists := getOptionalField(object, "name"); exists {
		name, parseErr := parseStringFromRaw(nameRaw)
		if parseErr == nil {
			emitter.Name = name
		}
	}
	if rateRaw, exists := getOptionalField(object, "rate"); exists {
		value, parseErr := parseFloat64FromRaw(rateRaw)
		if parseErr == nil {
			emitter.Rate = float32(value)
		}
	}
	return nil
}

func (child *ParticleChild) parseFromJSON(raw json.RawMessage, pkgMap *map[string][]byte) error {
	var object map[string]json.RawMessage
	err := json.Unmarshal(raw, &object)
	if err != nil {
		return fmt.Errorf("cannot parse particle child: %w", err)
	}
	nameRaw, err := getRequiredField(object, "name")
	if err != nil {
		return err
	}
	name, err := parseStringFromRaw(nameRaw)
	if err != nil {
		return fmt.Errorf("cannot parse child name: %w", err)
	}
	childType := "static"
	if typeRaw, typeExists := getOptionalField(object, "type"); typeExists {
		childType, err = parseStringFromRaw(typeRaw)
		if err != nil {
			return fmt.Errorf("cannot parse child type: %w", err)
		}
	}
	if strings.TrimSpace(name) == "" {
		return nil
	}
	child.Name = name
	child.Type = childType

	particleDocument, err := loadJSONDocument(pkgMap, name)
	if err != nil {
		return fmt.Errorf("cannot load child particle %s: %w", name, err)
	}
	particleBytes, err := json.Marshal(particleDocument)
	if err != nil {
		return fmt.Errorf("cannot re-encode child particle JSON: %w", err)
	}
	err = child.Object.parseFromJSON(particleBytes, pkgMap)
	if err != nil {
		return err
	}

	if maxCountRaw, exists := getOptionalField(object, "maxcount"); exists {
		value, parseErr := parseIntFromRaw(maxCountRaw)
		if parseErr == nil {
			child.MaxCount = uint32(value)
		}
	}
	if cpRaw, exists := getOptionalField(object, "controlpointstartindex"); exists {
		value, parseErr := parseIntFromRaw(cpRaw)
		if parseErr == nil {
			child.ControlPointStart = value
		}
	}
	if probabilityRaw, exists := getOptionalField(object, "probability"); exists {
		value, parseErr := parseFloat64FromRaw(probabilityRaw)
		if parseErr == nil {
			child.Probability = float32(value)
		}
	}
	if originRaw, exists := getOptionalField(object, "origin"); exists {
		origin, parseErr := parseVector3FromRaw(originRaw, child.Origin)
		if parseErr == nil {
			child.Origin = origin
		}
	}
	if scaleRaw, exists := getOptionalField(object, "scale"); exists {
		scale, parseErr := parseVector3FromRaw(scaleRaw, child.Scale)
		if parseErr == nil {
			child.Scale = scale
		}
	}
	if anglesRaw, exists := getOptionalField(object, "angles"); exists {
		angles, parseErr := parseVector3FromRaw(anglesRaw, child.Angles)
		if parseErr == nil {
			child.Angles = angles
		}
	}
	return nil
}

func (particle *Particle) parseFromJSON(raw json.RawMessage, pkgMap *map[string][]byte) error {
	var object map[string]json.RawMessage
	err := json.Unmarshal(raw, &object)
	if err != nil {
		return fmt.Errorf("cannot parse particle: %w", err)
	}

	emitterRaw, exists := object["emitter"]
	if !exists {
		return fmt.Errorf("particle has no emitter array")
	}
	var emitterArray []json.RawMessage
	err = json.Unmarshal(emitterRaw, &emitterArray)
	if err != nil {
		return fmt.Errorf("cannot parse emitter array: %w", err)
	}
	for _, emitterElement := range emitterArray {
		var emitter Emitter
		parseErr := emitter.parseFromJSON(emitterElement)
		if parseErr != nil {
			return parseErr
		}
		particle.Emitters = append(particle.Emitters, emitter)
	}

	if rendererRaw, exists := getOptionalField(object, "renderer"); exists {
		var rendererArray []json.RawMessage
		err = json.Unmarshal(rendererRaw, &rendererArray)
		if err != nil {
			return fmt.Errorf("cannot parse renderer array: %w", err)
		}
		for _, rendererElement := range rendererArray {
			var renderer ParticleRender
			parseErr := renderer.parseFromJSON(rendererElement)
			if parseErr != nil {
				return parseErr
			}
			particle.Renderers = append(particle.Renderers, renderer)
		}
	}
	if len(particle.Renderers) == 0 {
		particle.Renderers = append(particle.Renderers, ParticleRender{
			Name: "sprite",
		})
	}

	if initializerRaw, exists := getOptionalField(object, "initializer"); exists {
		var initializerArray []json.RawMessage
		err = json.Unmarshal(initializerRaw, &initializerArray)
		if err != nil {
			return fmt.Errorf("cannot parse initializer array: %w", err)
		}
		particle.Initializers = append(particle.Initializers, initializerArray...)
	}
	if operatorRaw, exists := getOptionalField(object, "operator"); exists {
		var operatorArray []json.RawMessage
		err = json.Unmarshal(operatorRaw, &operatorArray)
		if err != nil {
			return fmt.Errorf("cannot parse operator array: %w", err)
		}
		particle.Operators = append(particle.Operators, operatorArray...)
	}
	if controlRaw, exists := getOptionalField(object, "controlpoint"); exists {
		var controlArray []json.RawMessage
		err = json.Unmarshal(controlRaw, &controlArray)
		if err != nil {
			return fmt.Errorf("cannot parse controlpoint array: %w", err)
		}
		for _, controlElement := range controlArray {
			var controlPoint ParticleControlPoint
			parseErr := controlPoint.parseFromJSON(controlElement)
			if parseErr != nil {
				return parseErr
			}
			particle.ControlPoints = append(particle.ControlPoints, controlPoint)
		}
	}
	if childrenRaw, exists := getOptionalField(object, "children"); exists {
		var childrenArray []json.RawMessage
		err = json.Unmarshal(childrenRaw, &childrenArray)
		if err != nil {
			return fmt.Errorf("cannot parse children array: %w", err)
		}
		for _, childElement := range childrenArray {
			var child ParticleChild
			parseErr := child.parseFromJSON(childElement, pkgMap)
			if parseErr != nil {
				return parseErr
			}
			if child.Name != "" {
				particle.Children = append(particle.Children, child)
			}
		}
	}

	if materialPathRaw, exists := getOptionalField(object, "material"); exists {
		materialPath, parseErr := parseStringFromRaw(materialPathRaw)
		if parseErr != nil {
			return fmt.Errorf("cannot parse particle material path: %w", parseErr)
		}
		materialDocument, err := loadJSONDocument(pkgMap, materialPath)
		if err != nil {
			return fmt.Errorf("cannot load particle material %s: %w", materialPath, err)
		}
		materialBytes, err := json.Marshal(materialDocument)
		if err != nil {
			return fmt.Errorf("cannot re-encode particle material JSON: %w", err)
		}
		err = particle.Material.parseFromJSON(materialBytes)
		if err != nil {
			return fmt.Errorf("cannot parse particle material %s: %w", materialPath, err)
		}
	} else {
		return fmt.Errorf("particle has no material field")
	}

	if modeRaw, exists := getOptionalField(object, "animationmode"); exists {
		value, parseErr := parseIntFromRaw(modeRaw)
		if parseErr == nil {
			particle.AnimationMode = uint32(value)
		}
	}
	if multiplierRaw, exists := getOptionalField(object, "sequencemultiplier"); exists {
		value, parseErr := parseFloat64FromRaw(multiplierRaw)
		if parseErr == nil {
			particle.SequenceMultiplier = float32(value)
		}
	}
	if maxCountRaw, exists := getOptionalField(object, "maxcount"); exists {
		value, parseErr := parseIntFromRaw(maxCountRaw)
		if parseErr == nil {
			particle.MaxCount = uint32(value)
		}
	}
	if startTimeRaw, exists := getOptionalField(object, "starttime"); exists {
		value, parseErr := parseFloat64FromRaw(startTimeRaw)
		if parseErr == nil {
			particle.StartTime = float32(value)
		}
	}
	if flagsRaw, exists := getOptionalField(object, "flags"); exists {
		value, parseErr := parseIntFromRaw(flagsRaw)
		if parseErr == nil {
			particle.Flags = ParticleFlags(value)
		}
	}

	return nil
}

func (particleObject *ParticleObject) parseFromSceneJSON(raw json.RawMessage, pkgMap *map[string][]byte) error {
	var object map[string]json.RawMessage
	err := json.Unmarshal(raw, &object)
	if err != nil {
		return fmt.Errorf("cannot parse particle object: %w", err)
	}

	particlePathRaw, err := getRequiredField(object, "particle")
	if err != nil {
		return err
	}
	particlePath, err := parseStringFromRaw(particlePathRaw)
	if err != nil {
		return fmt.Errorf("cannot parse particle path: %w", err)
	}
	particleObject.ParticlePath = particlePath

	if visibleRaw, exists := getOptionalField(object, "visible"); exists {
		visible, parseErr := parseBoolFromRaw(visibleRaw)
		if parseErr == nil {
			particleObject.Visible = visible
		}
	} else {
		particleObject.Visible = true
	}

	if nameRaw, exists := getOptionalField(object, "name"); exists {
		name, parseErr := parseStringFromRaw(nameRaw)
		if parseErr == nil {
			particleObject.Name = name
		}
	}
	if idRaw, exists := getOptionalField(object, "id"); exists {
		idValue, parseErr := parseIntFromRaw(idRaw)
		if parseErr == nil {
			particleObject.ID = idValue
		}
	}
	if originRaw, exists := getOptionalField(object, "origin"); exists {
		origin, parseErr := parseVector3FromRaw(originRaw, particleObject.Origin)
		if parseErr == nil {
			particleObject.Origin = origin
		}
	}
	if scaleRaw, exists := getOptionalField(object, "scale"); exists {
		scale, parseErr := parseVector3FromRaw(scaleRaw, particleObject.Scale)
		if parseErr == nil {
			particleObject.Scale = scale
		}
	}
	if anglesRaw, exists := getOptionalField(object, "angles"); exists {
		angles, parseErr := parseVector3FromRaw(anglesRaw, particleObject.Angles)
		if parseErr == nil {
			particleObject.Angles = angles
		}
	}
	if parallaxRaw, exists := getOptionalField(object, "parallaxDepth"); exists {
		parallax, parseErr := parseVector2FromRaw(parallaxRaw, particleObject.ParallaxDepth)
		if parseErr == nil {
			particleObject.ParallaxDepth = parallax
		}
	}

	if instanceOverrideRaw, exists := getOptionalField(object, "instanceoverride"); exists && bytesFromRawNullAware(instanceOverrideRaw) != nil {
		var instanceOverride ParticleInstanceOverride
		parseErr := instanceOverride.parseFromJSON(instanceOverrideRaw)
		if parseErr != nil {
			return parseErr
		}
		particleObject.InstanceOverride = instanceOverride
	}

	particleDocument, err := loadJSONDocument(pkgMap, particlePath)
	if err != nil {
		return fmt.Errorf("cannot load particle file %s: %w", particlePath, err)
	}
	particleBytes, err := json.Marshal(particleDocument)
	if err != nil {
		return fmt.Errorf("cannot re-encode particle JSON: %w", err)
	}
	err = particleObject.ParticleData.parseFromJSON(particleBytes, pkgMap)
	if err != nil {
		return err
	}

	return nil
}
