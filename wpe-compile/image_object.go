package main

import (
	"encoding/json"
	"fmt"
)

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
	var object map[string]json.RawMessage
	err := json.Unmarshal(raw, &object)
	if err != nil {
		return fmt.Errorf("cannot parse image object: %w", err)
	}

	imageRaw, err := getRequiredField(object, "image")
	if err != nil {
		return err
	}
	imagePath, err := parseStringFromRaw(imageRaw)
	if err != nil {
		return fmt.Errorf("cannot parse image path: %w", err)
	}
	imageObject.ImagePath = imagePath

	if visibleRaw, exists := getOptionalField(object, "visible"); exists {
		visible, parseErr := parseBoolFromRaw(visibleRaw)
		if parseErr == nil {
			imageObject.Visible = visible
		}
	} else {
		imageObject.Visible = true
	}
	if alignmentRaw, exists := getOptionalField(object, "alignment"); exists {
		alignment, parseErr := parseStringFromRaw(alignmentRaw)
		if parseErr == nil {
			imageObject.Alignment = alignment
		}
	} else {
		imageObject.Alignment = "center"
	}

	imageDocument, err := loadJSONDocument(pkgMap, imagePath)
	if err != nil {
		return fmt.Errorf("cannot load model image JSON %s: %w", imagePath, err)
	}

	fullscreen := false
	if fullscreenRaw, exists := imageDocument["fullscreen"]; exists && bytesFromRawNullAware(fullscreenRaw) != nil {
		value, parseErr := parseBoolFromRaw(fullscreenRaw)
		if parseErr == nil {
			fullscreen = value
		}
	}
	imageObject.Fullscreen = fullscreen

	if nameRaw, exists := getOptionalField(object, "name"); exists {
		name, parseErr := parseStringFromRaw(nameRaw)
		if parseErr == nil {
			imageObject.Name = name
		}
	}
	if idRaw, exists := getOptionalField(object, "id"); exists {
		value, parseErr := parseIntFromRaw(idRaw)
		if parseErr == nil {
			imageObject.ID = value
		}
	}
	if colorBlendRaw, exists := getOptionalField(object, "colorBlendMode"); exists {
		value, parseErr := parseIntFromRaw(colorBlendRaw)
		if parseErr == nil {
			imageObject.ColorBlendMode = value
		}
	}

	if !fullscreen {
		originRaw, err := getRequiredField(object, "origin")
		if err != nil {
			return err
		}
		origin, err := parseVector3FromRaw(originRaw, imageObject.Origin)
		if err != nil {
			return fmt.Errorf("cannot parse image origin: %w", err)
		}
		imageObject.Origin = origin

		if anglesRaw, exists := getOptionalField(object, "angles"); exists {
			angles, err := parseVector3FromRaw(anglesRaw, imageObject.Angles)
			if err != nil {
				return fmt.Errorf("cannot parse image angles: %w", err)
			}
			imageObject.Angles = angles
		}

		imageObject.Scale = Vector3{1, 1, 1}
		if scaleRaw, exists := getOptionalField(object, "scale"); exists {
			scale, err := parseVector3FromRaw(scaleRaw, imageObject.Scale)
			if err != nil {
				return fmt.Errorf("cannot parse image scale: %w", err)
			}
			imageObject.Scale = scale
		}

		if parallaxRaw, exists := getOptionalField(object, "parallaxDepth"); exists {
			parallax, parseErr := parseVector2FromRaw(parallaxRaw, imageObject.ParallaxDepth)
			if parseErr == nil {
				imageObject.ParallaxDepth = parallax
			}
		}

		if widthRaw, exists := imageDocument["width"]; exists {
			heightRaw, existsHeight := imageDocument["height"]
			if existsHeight {
				widthValue, parseErr := parseIntFromRaw(widthRaw)
				if parseErr != nil {
					return fmt.Errorf("cannot parse image width: %w", parseErr)
				}
				heightValue, parseErr := parseIntFromRaw(heightRaw)
				if parseErr != nil {
					return fmt.Errorf("cannot parse image height: %w", parseErr)
				}
				imageObject.Size = Vector2{float32(widthValue), float32(heightValue)}
			}
		} else if sizeRaw, exists := getOptionalField(object, "size"); exists {
			size, parseErr := parseVector2FromRaw(sizeRaw, imageObject.Size)
			if parseErr != nil {
				return fmt.Errorf("cannot parse image size: %w", parseErr)
			}
			imageObject.Size = size
		} else {
			imageObject.Size = Vector2{imageObject.Origin[0] * 2, imageObject.Origin[1] * 2}
		}
	}

	if noPaddingRaw, exists := imageDocument["nopadding"]; exists {
		value, parseErr := parseBoolFromRaw(noPaddingRaw)
		if parseErr == nil {
			imageObject.NoPadding = value
		}
	}

	if colorRaw, exists := getOptionalField(object, "color"); exists {
		color, parseErr := parseVector3FromRaw(colorRaw, imageObject.Color)
		if parseErr == nil {
			imageObject.Color = color
		}
	}
	if alphaRaw, exists := getOptionalField(object, "alpha"); exists {
		value, parseErr := parseFloat64FromRaw(alphaRaw)
		if parseErr == nil {
			imageObject.Alpha = float32(value)
		}
	} else {
		imageObject.Alpha = 1
	}
	if brightnessRaw, exists := getOptionalField(object, "brightness"); exists {
		value, parseErr := parseFloat64FromRaw(brightnessRaw)
		if parseErr == nil {
			imageObject.Brightness = float32(value)
		}
	} else {
		imageObject.Brightness = 1
	}

	if puppetRaw, exists := imageDocument["puppet"]; exists {
		puppet, parseErr := parseStringFromRaw(puppetRaw)
		if parseErr == nil {
			imageObject.Puppet = puppet
		}
	}

	if materialPathRaw, exists := imageDocument["material"]; exists {
		materialPath, parseErr := parseStringFromRaw(materialPathRaw)
		if parseErr != nil {
			return fmt.Errorf("cannot parse material path from image: %w", parseErr)
		}
		materialDocument, err := loadJSONDocument(pkgMap, materialPath)
		if err != nil {
			return fmt.Errorf("cannot load material %s: %w", materialPath, err)
		}
		materialBytes, err := json.Marshal(materialDocument)
		if err != nil {
			return fmt.Errorf("cannot re-encode material JSON: %w", err)
		}
		err = imageObject.Material.parseFromJSON(materialBytes)
		if err != nil {
			return fmt.Errorf("cannot parse material %s: %w", materialPath, err)
		}
	} else {
		return fmt.Errorf("image object has no material in model JSON %s", imagePath)
	}

	if effectsRaw, exists := getOptionalField(object, "effects"); exists {
		var effectsArray []json.RawMessage
		err := json.Unmarshal(effectsRaw, &effectsArray)
		if err != nil {
			return fmt.Errorf("cannot parse image effects array: %w", err)
		}
		for _, effectRaw := range effectsArray {
			var effect ImageEffect
			parseErr := effect.parseFromSceneJSON(effectRaw, pkgMap)
			if parseErr != nil {
				return parseErr
			}
			imageObject.Effects = append(imageObject.Effects, effect)
		}
	}

	if layersRaw, exists := getOptionalField(object, "animationlayers"); exists {
		var layersArray []json.RawMessage
		err := json.Unmarshal(layersRaw, &layersArray)
		if err != nil {
			return fmt.Errorf("cannot parse puppet animation layers: %w", err)
		}
		for _, layerRaw := range layersArray {
			var layerObject map[string]json.RawMessage
			parseErr := json.Unmarshal(layerRaw, &layerObject)
			if parseErr != nil {
				return fmt.Errorf("cannot parse puppet layer object: %w", parseErr)
			}
			var layer PuppetAnimationLayer
			animationIDRaw, parseErr := getRequiredField(layerObject, "animation")
			if parseErr != nil {
				return parseErr
			}
			animationID, parseErr := parseIntFromRaw(animationIDRaw)
			if parseErr != nil {
				return fmt.Errorf("cannot parse puppet animation id: %w", parseErr)
			}
			layer.ID = animationID

			if blendRaw, existsBlend := getOptionalField(layerObject, "blend"); existsBlend {
				value, vErr := parseFloat64FromRaw(blendRaw)
				if vErr == nil {
					layer.Blend = float32(value)
				}
			}
			if rateRaw, existsRate := getOptionalField(layerObject, "rate"); existsRate {
				value, vErr := parseFloat64FromRaw(rateRaw)
				if vErr == nil {
					layer.Rate = float32(value)
				}
			}
			layer.Visible = true
			if visibleRaw, existsVisible := getOptionalField(layerObject, "visible"); existsVisible {
				v, vErr := parseBoolFromRaw(visibleRaw)
				if vErr == nil {
					layer.Visible = v
				}
			}
			imageObject.PuppetLayers = append(imageObject.PuppetLayers, layer)
		}
	}

	if configRaw, exists := getOptionalField(object, "config"); exists {
		var configObject map[string]json.RawMessage
		parseErr := json.Unmarshal(configRaw, &configObject)
		if parseErr == nil {
			if passthroughRaw, hasPass := getOptionalField(configObject, "passthrough"); hasPass {
				value, vErr := parseBoolFromRaw(passthroughRaw)
				if vErr == nil {
					imageObject.Config.Passthrough = value
				}
			}
		}
	}

	return nil
}
