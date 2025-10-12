package main

import (
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"os/exec"
	"regexp"
	"strings"
)

type UniformInfo struct {
	Name      string
	Type      string
	ArraySize int
	Default   []float64
}

type AttributeInfo struct {
	Name string
	Type string
}

type PreprocessedShader struct {
	VertexGLSL       string
	FragmentGLSL     string
	VertexUniforms   []UniformInfo
	FragmentUniforms []UniformInfo
	Attributes       []AttributeInfo
	Samplers         []string
}

type comboMeta struct {
	Combo   string `json:"combo"`
	Default int    `json:"default"`
}

type uniformMeta struct {
	Default json.RawMessage `json:"default"`
	Int     bool            `json:"int"`
}

func preprocessShader(vertexSource, fragmentSource, includePath string, comboOverrides map[string]int) (PreprocessedShader, error) {
	vertexSource = appendGLSL450Header(vertexSource)
	fragmentSource = appendGLSL450Header(fragmentSource)

	vertexCombos := parseCombos(vertexSource)
	fragmentCombos := parseCombos(fragmentSource)
	for combo, value := range comboOverrides {
		if _, exists := vertexCombos[combo]; exists {
			vertexCombos[combo] = value
		}
		if _, exists := fragmentCombos[combo]; exists {
			fragmentCombos[combo] = value
		}
	}

	vertexSource, err := runGLSLCPreprocessor(vertexSource, includePath, vertexCombos)
	if err != nil {
		return PreprocessedShader{}, err
	}
	fragmentSource, err = runGLSLCPreprocessor(fragmentSource, includePath, fragmentCombos)
	if err != nil {
		return PreprocessedShader{}, err
	}

	vertexSource, attributes := preprocessVertexAttributes(vertexSource)
	vertexSource, vertexVarying := findAndRemoveVarying(vertexSource)
	fragmentSource, fragmentVarying := findAndRemoveVarying(fragmentSource)

	for name, info := range fragmentVarying {
		vertexInfo, exists := vertexVarying[name]
		if exists {
			if vertexInfo != info {
				return PreprocessedShader{}, fmt.Errorf("varying %s has different type in vertex and fragment shader", name)
			}
		} else {
			vertexVarying[name] = info
		}
	}

	varying := []AttributeInfo{}
	for _, info := range vertexVarying {
		varying = append(varying, info)
	}

	fragmentSource, samplers := preprocessSamplers(fragmentSource)
	vertexSource = insertIntermediateAttributes(vertexSource, varying, "out")
	fragmentSource = insertIntermediateAttributes(fragmentSource, varying, "in")

	vertexSource, vertexUniforms := preprocessUniforms(vertexSource, 1)
	fragmentSource, fragmentUniforms := preprocessUniforms(fragmentSource, 3)
	fragmentSource = preprocessFragColor(fragmentSource)

	return PreprocessedShader{
		VertexGLSL:       vertexSource,
		FragmentGLSL:     fragmentSource,
		Attributes:       attributes,
		Samplers:         samplers,
		VertexUniforms:   vertexUniforms,
		FragmentUniforms: fragmentUniforms,
	}, nil
}

func appendGLSL450Header(source string) string {
	return `#version 450

#define GLSL 1
#define HLSL 0
#define highp

#define CAST2(x) (vec2(x))
#define CAST3(x) (vec3(x))
#define CAST4(x) (vec4(x))
#define CAST3X3(x) (mat3(x))

#define texSample2D texture
#define texSample2DLod textureLod
#define mul(x, y) ((y) * (x))
#define frac fract
#define atan2 atan
#define fmod(x, y) (x-y*trunc(x/y))
#define ddx dFdx
#define ddy(x) dFdy(-(x))
#define saturate(x) (clamp(x, 0.0, 1.0))

#define float1 float
#define float2 vec2
#define float3 vec3
#define float4 vec4
#define lerp mix

` + source
}

func parseCombos(source string) map[string]int {
	combos := make(map[string]int)
	reCombo := regexp.MustCompile(`//\s*\[COMBO\]\s*({.*})`)
	matches := reCombo.FindAllStringSubmatch(source, -1)
	for _, match := range matches {
		comboJSON := match[1]
		combo := comboMeta{}
		err := json.Unmarshal([]byte(comboJSON), &combo)
		if err != nil {
			fmt.Printf("warning: unable to parse combo JSON %s: %s\n", comboJSON, err.Error())
			continue
		}
		combos[combo.Combo] = combo.Default
	}
	return combos
}

func runGLSLCPreprocessor(source, includePath string, defines map[string]int) (string, error) {
	tempDirBytes, err := exec.Command("mktemp", "-d").Output()
	if err != nil {
		return "", errors.New("mktemp failed: " + err.Error())
	}
	tempDir := strings.TrimSuffix(string(tempDirBytes), "\n")
	defer os.RemoveAll(tempDir)

	err = os.WriteFile(tempDir+"/shader.glsl", []byte(source), 0644)
	if err != nil {
		return "", errors.New("writing shader failed: " + err.Error())
	}

	glslcArgs := []string{"-E", tempDir + "/shader.glsl", "-I", includePath}
	for name, value := range defines {
		glslcArgs = append(glslcArgs, fmt.Sprintf("-D%s=%d", name, value))
	}
	resultBytes, err := exec.Command("glslc", glslcArgs...).CombinedOutput()
	if err != nil {
		return "", fmt.Errorf("glslc preprocessor failed: %s", resultBytes)
	}
	return normalizeNewlines(string(resultBytes)), nil
}

func preprocessVertexAttributes(source string) (string, []AttributeInfo) {
	reAttribute := regexp.MustCompile(`attribute\s+([^\s]+)\s+([^\s;]+)\s*;`)
	attributes := []AttributeInfo{}
	source = reAttribute.ReplaceAllStringFunc(source, func(match string) string {
		submatches := reAttribute.FindStringSubmatch(match)
		attributes = append(attributes, AttributeInfo{
			Name: string(submatches[2]),
			Type: string(submatches[1]),
		})
		return fmt.Sprintf("layout(location = %d) in %s %s;", len(attributes)-1, string(submatches[1]), string(submatches[2]))
	})

	return source, attributes
}

func findAndRemoveVarying(source string) (string, map[string]AttributeInfo) {
	reUniform := regexp.MustCompile(`varying\s*([a-z|A-Z|0-9|_]*)\s*([a-z|A-Z|0-9|_]*)\s*;`)
	varying := map[string]AttributeInfo{}
	source = reUniform.ReplaceAllStringFunc(source, func(match string) string {
		submatches := reUniform.FindStringSubmatch(match)
		varying[submatches[2]] = AttributeInfo{
			Name: string(submatches[2]),
			Type: string(submatches[1]),
		}
		return ""
	})
	return normalizeNewlines(source), varying
}

func insertIntermediateAttributes(source string, attributes []AttributeInfo, side string) string {
	attributesString := ""
	for index, attribute := range attributes {
		attributesString += fmt.Sprintf("layout(location = %d) %s %s %s;\n", index, side, attribute.Type, attribute.Name)
	}
	reInsertPlace := regexp.MustCompile(`\n[layout|uniform]`)
	inserted := false
	source = reInsertPlace.ReplaceAllStringFunc(source, func(match string) string {
		if inserted {
			return match
		}
		inserted = true
		return "\n\n" + attributesString + "\n\n" + match
	})
	return normalizeNewlines(source)
}

func preprocessSamplers(source string) (string, []string) {
	reSampler := regexp.MustCompile(`uniform\s*sampler2D\s*([a-z|A-Z|0-9|_]*)\s*;`)
	samplers := []string{}
	source = reSampler.ReplaceAllStringFunc(source, func(match string) string {
		submatches := reSampler.FindStringSubmatch(match)
		samplers = append(samplers, string(submatches[1]))
		return fmt.Sprintf("layout(set = 2, binding = %d) uniform sampler2D %s;", len(samplers)-1, string(submatches[1]))
	})
	return source, samplers
}

func preprocessUniforms(source string, set int) (string, []UniformInfo) {
	reUniform := regexp.MustCompile(`uniform\s*([a-z|A-Z|0-9|_]*)\s*([a-z|A-Z|0-9|_]*)\s*;`)
	uniforms := []UniformInfo{}

	matches := reUniform.FindAllStringSubmatch(source, -1)
	for _, match := range matches {
		if match[1] == "sampler2D" {
			continue
		}
		uniforms = append(uniforms, UniformInfo{
			Name:      string(match[2]),
			Type:      string(match[1]),
			ArraySize: 0, // TODO: support arrays
		})
	}

	uniformBlock := fmt.Sprintf("layout(std140, set = %d, binding = 0) uniform uniforms_t {\n", set)
	for _, uniform := range uniforms {
		uniformBlock += fmt.Sprintf("    %s %s;\n", uniform.Type, uniform.Name)
	}
	uniformBlock += "};\n\n"

	firstMatch := true
	source = reUniform.ReplaceAllStringFunc(source, func(match string) string {
		submatches := reUniform.FindStringSubmatch(match)
		if submatches[1] == "sampler2D" {
			return match
		}
		if firstMatch {
			firstMatch = false
			return uniformBlock
		}
		return ""
	})

	return normalizeNewlines(source), uniforms
}

func preprocessFragColor(source string) string {
	reFragColor := regexp.MustCompile(`gl_FragColor`)
	source = reFragColor.ReplaceAllString(source, "f_color")

	reInsertPlace := regexp.MustCompile(`\n[layout|uniform]`)
	inserted := false
	source = reInsertPlace.ReplaceAllStringFunc(source, func(match string) string {
		if inserted {
			return match
		}
		inserted = true
		return "\n\nlayout(location = 0) out vec4 f_color;\n\n" + match
	})
	return normalizeNewlines(source)
}

func normalizeNewlines(source string) string {
	result := ""
	cnt := 0
	for _, symbol := range source {
		if symbol == '\n' {
			cnt++
			if cnt >= 3 {
				continue
			}
		} else {
			cnt = 0
		}
		result += string(symbol)
	}
	return result
}
