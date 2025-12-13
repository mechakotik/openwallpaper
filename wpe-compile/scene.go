package main

import (
	"encoding/json"
	"fmt"
)

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

func parseOrthogonalProjection(raw json.RawMessage) (OrthogonalProjection, error) {
	var projection OrthogonalProjection
	if bytesFromRawNullAware(raw) == nil {
		return projection, nil
	}
	var object map[string]json.RawMessage
	err := json.Unmarshal(raw, &object)
	if err != nil {
		return projection, fmt.Errorf("cannot parse orthogonal projection: %w", err)
	}
	if autoRaw, exists := getOptionalField(object, "auto"); exists {
		value, parseErr := parseBoolFromRaw(autoRaw)
		if parseErr != nil {
			return projection, fmt.Errorf("cannot parse orthogonal auto flag: %w", parseErr)
		}
		projection.Auto = value
	} else {
		widthRaw, err := getRequiredField(object, "width")
		if err != nil {
			return projection, err
		}
		heightRaw, err := getRequiredField(object, "height")
		if err != nil {
			return projection, err
		}
		width, err := parseIntFromRaw(widthRaw)
		if err != nil {
			return projection, fmt.Errorf("cannot parse orthogonal width: %w", err)
		}
		height, err := parseIntFromRaw(heightRaw)
		if err != nil {
			return projection, fmt.Errorf("cannot parse orthogonal height: %w", err)
		}
		projection.Width = width
		projection.Height = height
	}
	return projection, nil
}

func parseSceneCamera(raw json.RawMessage) (SceneCamera, error) {
	var camera SceneCamera
	var object map[string]json.RawMessage
	err := json.Unmarshal(raw, &object)
	if err != nil {
		return camera, fmt.Errorf("cannot parse camera: %w", err)
	}
	centerRaw, err := getRequiredField(object, "center")
	if err != nil {
		return camera, err
	}
	eyeRaw, err := getRequiredField(object, "eye")
	if err != nil {
		return camera, err
	}
	upRaw, err := getRequiredField(object, "up")
	if err != nil {
		return camera, err
	}

	center, err := parseVector3FromRaw(centerRaw, camera.Center)
	if err != nil {
		return camera, fmt.Errorf("cannot parse camera center: %w", err)
	}
	eye, err := parseVector3FromRaw(eyeRaw, camera.Eye)
	if err != nil {
		return camera, fmt.Errorf("cannot parse camera eye: %w", err)
	}
	up, err := parseVector3FromRaw(upRaw, camera.Up)
	if err != nil {
		return camera, fmt.Errorf("cannot parse camera up: %w", err)
	}

	camera.Center = center
	camera.Eye = eye
	camera.Up = up

	return camera, nil
}

func parseSceneGeneral(raw json.RawMessage) (SceneGeneral, error) {
	var general SceneGeneral
	general.Ortho.Width = 1920
	general.Ortho.Height = 1080
	general.Zoom = 1
	general.FOV = 50
	general.NearZ = 0.01
	general.FarZ = 10000
	general.AmbientColor = Vector3{0.2, 0.2, 0.2}
	general.SkyLightColor = Vector3{0.3, 0.3, 0.3}

	var object map[string]json.RawMessage
	err := json.Unmarshal(raw, &object)
	if err != nil {
		return general, fmt.Errorf("cannot parse general block: %w", err)
	}

	ambientRaw, err := getRequiredField(object, "ambientcolor")
	if err != nil {
		return general, err
	}
	skylightRaw, err := getRequiredField(object, "skylightcolor")
	if err != nil {
		return general, err
	}
	clearRaw, err := getRequiredField(object, "clearcolor")
	if err != nil {
		return general, err
	}
	cameraParallaxRaw, err := getRequiredField(object, "cameraparallax")
	if err != nil {
		return general, err
	}
	cameraParallaxAmountRaw, err := getRequiredField(object, "cameraparallaxamount")
	if err != nil {
		return general, err
	}
	cameraParallaxDelayRaw, err := getRequiredField(object, "cameraparallaxdelay")
	if err != nil {
		return general, err
	}
	cameraParallaxMouseRaw, err := getRequiredField(object, "cameraparallaxmouseinfluence")
	if err != nil {
		return general, err
	}

	ambient, err := parseVector3FromRaw(ambientRaw, general.AmbientColor)
	if err != nil {
		return general, fmt.Errorf("cannot parse ambient color: %w", err)
	}
	skylight, err := parseVector3FromRaw(skylightRaw, general.SkyLightColor)
	if err != nil {
		return general, fmt.Errorf("cannot parse skylight color: %w", err)
	}
	clearColor, err := parseVector3FromRaw(clearRaw, general.ClearColor)
	if err != nil {
		return general, fmt.Errorf("cannot parse clear color: %w", err)
	}
	cameraParallaxValue, err := parseBoolFromRaw(cameraParallaxRaw)
	if err != nil {
		return general, fmt.Errorf("cannot parse camera parallax flag: %w", err)
	}
	cameraParallaxAmountValue, err := parseFloat64FromRaw(cameraParallaxAmountRaw)
	if err != nil {
		return general, fmt.Errorf("cannot parse camera parallax amount: %w", err)
	}
	cameraParallaxDelayValue, err := parseFloat64FromRaw(cameraParallaxDelayRaw)
	if err != nil {
		return general, fmt.Errorf("cannot parse camera parallax delay: %w", err)
	}
	cameraParallaxMouseValue, err := parseFloat64FromRaw(cameraParallaxMouseRaw)
	if err != nil {
		return general, fmt.Errorf("cannot parse camera parallax mouse influence: %w", err)
	}

	general.AmbientColor = ambient
	general.SkyLightColor = skylight
	general.ClearColor = clearColor
	general.Parallax = cameraParallaxValue
	general.ParallaxAmount = float32(cameraParallaxAmountValue)
	general.ParallaxDelay = float32(cameraParallaxDelayValue)
	general.ParallaxMouseInfluence = float32(cameraParallaxMouseValue)

	if zoomRaw, exists := getOptionalField(object, "zoom"); exists {
		value, parseErr := parseFloat64FromRaw(zoomRaw)
		if parseErr == nil {
			general.Zoom = float32(value)
		}
	}
	if fovRaw, exists := getOptionalField(object, "fov"); exists {
		value, parseErr := parseFloat64FromRaw(fovRaw)
		if parseErr == nil {
			general.FOV = float32(value)
		}
	}
	if nearRaw, exists := getOptionalField(object, "nearz"); exists {
		value, parseErr := parseFloat64FromRaw(nearRaw)
		if parseErr == nil {
			general.NearZ = float32(value)
		}
	}
	if farRaw, exists := getOptionalField(object, "farz"); exists {
		value, parseErr := parseFloat64FromRaw(farRaw)
		if parseErr == nil {
			general.FarZ = float32(value)
		}
	}

	if orthoRaw, exists := getOptionalField(object, "orthogonalprojection"); exists {
		if bytesFromRawNullAware(orthoRaw) == nil {
			general.Orthographic = false
		} else {
			projection, parseErr := parseOrthogonalProjection(orthoRaw)
			if parseErr != nil {
				return general, parseErr
			}
			general.Orthographic = true
			general.Ortho = projection
		}
	} else {
		general.Orthographic = true
	}

	return general, nil
}

func ParseScene(pkgMap *map[string][]byte) (Scene, error) {
	sceneBytes, err := loadBytesFromPackage(pkgMap, "scene.json")
	if err != nil {
		return Scene{}, err
	}
	var root map[string]json.RawMessage
	err = json.Unmarshal(sceneBytes, &root)
	if err != nil {
		return Scene{}, fmt.Errorf("cannot parse scene JSON: %w", err)
	}

	scene := Scene{}

	if cameraRaw, exists := root["camera"]; exists {
		camera, parseErr := parseSceneCamera(cameraRaw)
		if parseErr != nil {
			return Scene{}, parseErr
		}
		scene.Camera = camera
	} else {
		return Scene{}, fmt.Errorf("scene has no camera object")
	}

	if generalRaw, exists := root["general"]; exists {
		general, parseErr := parseSceneGeneral(generalRaw)
		if parseErr != nil {
			return Scene{}, parseErr
		}
		scene.General = general
	} else {
		return Scene{}, fmt.Errorf("scene has no general block")
	}

	if versionRaw, exists := root["version"]; exists {
		version, parseErr := parseIntFromRaw(versionRaw)
		if parseErr == nil {
			scene.Version = version
		}
	}

	objectsRaw, exists := root["objects"]
	if !exists {
		return Scene{}, nil
	}
	var objectsArray []json.RawMessage
	err = json.Unmarshal(objectsRaw, &objectsArray)
	if err != nil {
		return Scene{}, fmt.Errorf("cannot parse scene objects array: %w", err)
	}

	for _, objectRaw := range objectsArray {
		var object map[string]json.RawMessage
		parseErr := json.Unmarshal(objectRaw, &object)
		if parseErr != nil {
			return Scene{}, fmt.Errorf("cannot parse object entry: %w", parseErr)
		}
		if _, hasParticle := object["particle"]; hasParticle {
			var particleObject ParticleObject
			parseErr = particleObject.parseFromSceneJSON(objectRaw, pkgMap)
			if parseErr != nil {
				return Scene{}, parseErr
			}
			scene.Objects = append(scene.Objects, &particleObject)
			continue
		}
		if _, hasImage := object["image"]; hasImage {
			var imageObject ImageObject
			parseErr = imageObject.parseFromSceneJSON(objectRaw, pkgMap)
			if parseErr != nil {
				return Scene{}, parseErr
			}
			scene.Objects = append(scene.Objects, &imageObject)
			continue
		}
		if _, hasSound := object["sound"]; hasSound {
			var soundObject SoundObject
			parseErr = soundObject.parseFromSceneJSON(objectRaw)
			if parseErr != nil {
				return Scene{}, parseErr
			}
			scene.Objects = append(scene.Objects, &soundObject)
			continue
		}
		if _, hasLight := object["light"]; hasLight {
			var lightObject LightObject
			parseErr = lightObject.parseFromSceneJSON(objectRaw)
			if parseErr != nil {
				return Scene{}, parseErr
			}
			scene.Objects = append(scene.Objects, &lightObject)
			continue
		}
	}

	return scene, nil
}
