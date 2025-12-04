package main

import (
	"encoding/json"
	"errors"
	"fmt"
	"maps"
	"os"
	"os/exec"
	"regexp"
	"strings"
)

type UniformInfo struct {
	Name         string
	ConstantName string
	Type         string
	ArraySize    int
	Default      []float32
	DefaultSet   bool
}

type AttributeInfo struct {
	Name string
	Type string
}

type SamplerInfo struct {
	Name    string
	Default string
}

type PreprocessedShader struct {
	VertexGLSL       string
	FragmentGLSL     string
	VertexUniforms   []UniformInfo
	FragmentUniforms []UniformInfo
	Attributes       []AttributeInfo
	Samplers         []SamplerInfo
}

type comboMeta struct {
	Combo   string `json:"combo"`
	Default int    `json:"default"`
}

type samplerMeta struct {
	Combo   string `json:"combo"`
	Default string `json:"default"`
}

func preprocessShader(vertexSource, fragmentSource, includePath string, boundTextures []bool, comboOverrides map[string]int) (PreprocessedShader, error) {
	vertexSource = renameSymbol(vertexSource, "sample", "sample_")
	fragmentSource = renameSymbol(fragmentSource, "sample", "sample_")

	vertexSource = removeRequireDirectives(vertexSource)
	fragmentSource = removeRequireDirectives(fragmentSource)

	vertexSource = removePrecisionSpecifiers(vertexSource)
	fragmentSource = removePrecisionSpecifiers(fragmentSource)

	vertexUniformConstantNames := parseUniformConstantNames(vertexSource)
	vertexUniformDefaults := parseUniformDefaults(vertexSource)
	fragmentUniformConstantNames := parseUniformConstantNames(fragmentSource)
	fragmentUniformDefaults := parseUniformDefaults(fragmentSource)

	vertexSource = appendGLSL450Header(vertexSource)
	fragmentSource = appendGLSL450Header(fragmentSource)
	vertexSource = takeAbsFromPowBase(vertexSource)
	fragmentSource = takeAbsFromPowBase(fragmentSource)

	vertexCombos, vertexDefaults := parseSamplerCombos(vertexSource, boundTextures)
	fragmentCombos, fragmentDefaults := parseSamplerCombos(fragmentSource, boundTextures)

	combos := map[string]int{}
	maps.Copy(combos, parseCombos(vertexSource))
	maps.Copy(combos, parseCombos(fragmentSource))
	maps.Copy(combos, vertexCombos)
	maps.Copy(combos, fragmentCombos)

	for combo, value := range comboOverrides {
		if _, exists := combos[combo]; exists {
			combos[combo] = value
		}
	}

	vertexSource, err := runGLSLCPreprocessor(vertexSource, includePath, combos)
	if err != nil {
		return PreprocessedShader{}, err
	}
	fragmentSource, err = runGLSLCPreprocessor(fragmentSource, includePath, combos)
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

	fragmentSource, samplerNames := preprocessSamplers(fragmentSource)
	vertexSource = insertIntermediateAttributes(vertexSource, varying, "out")
	fragmentSource = insertIntermediateAttributes(fragmentSource, varying, "in")

	vertexSource, vertexUniforms := preprocessUniforms(vertexSource, 1)
	fragmentSource, fragmentUniforms := preprocessUniforms(fragmentSource, 3)
	fragmentSource = preprocessFragColor(fragmentSource)

	for i := range vertexUniforms {
		if constantName, exists := vertexUniformConstantNames[vertexUniforms[i].Name]; exists {
			vertexUniforms[i].ConstantName = constantName
		} else {
			constantName, _ := strings.CutPrefix(vertexUniforms[i].Name, "g_")
			constantName = strings.ToLower(constantName)
			vertexUniforms[i].ConstantName = constantName
		}
		if value, exists := vertexUniformDefaults[vertexUniforms[i].Name]; exists {
			vertexUniforms[i].Default = value
			vertexUniforms[i].DefaultSet = true
		}
	}
	for i := range fragmentUniforms {
		if constantName, exists := fragmentUniformConstantNames[fragmentUniforms[i].Name]; exists {
			fragmentUniforms[i].ConstantName = constantName
		} else {
			constantName, _ := strings.CutPrefix(fragmentUniforms[i].Name, "g_")
			constantName = strings.ToLower(constantName)
			fragmentUniforms[i].ConstantName = constantName
		}
		if value, exists := fragmentUniformDefaults[fragmentUniforms[i].Name]; exists {
			fragmentUniforms[i].Default = value
			fragmentUniforms[i].DefaultSet = true
		}
	}

	samplers := []SamplerInfo{}
	for _, samplerName := range samplerNames {
		defaultTexture := ""
		if texture, exists := fragmentDefaults[samplerName]; exists {
			defaultTexture = texture
		} else if texture, exists := vertexDefaults[samplerName]; exists {
			defaultTexture = texture
		}
		samplers = append(samplers, SamplerInfo{
			Name:    samplerName,
			Default: defaultTexture,
		})
	}

	return PreprocessedShader{
		VertexGLSL:       vertexSource,
		FragmentGLSL:     fragmentSource,
		Attributes:       attributes,
		Samplers:         samplers,
		VertexUniforms:   vertexUniforms,
		FragmentUniforms: fragmentUniforms,
	}, nil
}

func renameSymbol(source string, from string, to string) string {
	reSymbol := regexp.MustCompile(fmt.Sprintf(`([^a-zA-Z0-9_])(%s)([^a-zA-Z0-9_])`, from))
	source = reSymbol.ReplaceAllStringFunc(source, func(match string) string {
		submatches := reSymbol.FindStringSubmatch(match)
		return submatches[1] + to + submatches[3]
	})
	return source
}

func removeRequireDirectives(source string) string {
	reRequire := regexp.MustCompile(`#require.*\n`)
	return reRequire.ReplaceAllString(source, "")
}

func removePrecisionSpecifiers(source string) string {
	rePrecision := regexp.MustCompile(`uniform *(lowp|mediump|highp)`)
	return rePrecision.ReplaceAllString(source, "uniform ")
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

func takeAbsFromPowBase(source string) string {
	for {
		replaced := false
		for start := len(source) - len("pow("); start >= 0; start-- {
			if source[start:start+len("pow(")] != "pow(" {
				continue
			}
			balance := 0
			repl := ""
			endPos := 0
			for idx := start + len("pow("); idx < len(source); idx++ {
				if source[idx] == '(' {
					balance++
				} else if source[idx] == ')' {
					balance--
					if balance < 0 {
						break
					}
				}
				if balance == 0 && source[idx] == ',' {
					repl = strings.Clone(source[start+len("pow(") : idx])
					endPos = idx
					break
				}
			}
			if !strings.HasPrefix(repl, "abs(") {
				source = strings.Clone(source[:start+len("pow(")]) + "abs(" + repl + ")" + strings.Clone(source[endPos:])
				replaced = true
			}
		}
		if !replaced {
			break
		}
	}
	return source
}

func parseUniformConstantNames(source string) map[string]string {
	reUniform := regexp.MustCompile(`uniform\s*([a-z|A-Z|0-9|_]*)\s*([a-z|A-Z|0-9|_]*)\s*;\s*\/\/\s*({.*})`)
	matches := reUniform.FindAllStringSubmatch(source, -1)
	constantNames := map[string]string{}
	for _, match := range matches {
		if match[1] == "sampler2D" || match[1] == "sampler2DComparison" {
			continue
		}
		type uniformMeta struct {
			ConstantName json.RawMessage `json:"material"`
		}
		var meta uniformMeta
		err := json.Unmarshal([]byte(match[3]), &meta)
		if err != nil {
			fmt.Printf("warning: unable to parse uniform properties JSON for %s: %s\n", match[2], err.Error())
			continue
		}
		constantName := string(meta.ConstantName)
		if constantName != "null" {
			constantName = strings.Trim(constantName, "\"")
			constantNames[match[2]] = constantName
		}
	}
	return constantNames
}

func parseUniformDefaults(source string) map[string][]float32 {
	reUniform := regexp.MustCompile(`uniform\s*([a-z|A-Z|0-9|_]*)\s*([a-z|A-Z|0-9|_]*)\s*;\s*\/\/\s*({.*})`)
	matches := reUniform.FindAllStringSubmatch(source, -1)
	defaults := map[string][]float32{}
	for _, match := range matches {
		if match[1] == "sampler2D" || match[1] == "sampler2DComparison" {
			continue
		}
		type uniformMeta struct {
			Default json.RawMessage `json:"default"`
		}
		var meta uniformMeta
		err := json.Unmarshal([]byte(match[3]), &meta)
		if err != nil {
			fmt.Printf("warning: unable to parse uniform properties JSON for %s: %s\n", match[2], err.Error())
			continue
		}
		value, err := parseFloatSliceFromRaw(meta.Default)
		if err != nil {
			fmt.Printf("warning: unable to parse uniform properties JSON for %s: %s\n", match[2], err.Error())
			continue
		}
		defaults[match[2]] = value
	}
	return defaults
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

func parseSamplerCombos(source string, boundTextures []bool) (map[string]int, map[string]string) {
	combos := map[string]int{}
	defaults := map[string]string{}
	reSamplerCombo := regexp.MustCompile(`uniform\s*sampler2D\s*([a-z|A-Z|0-9|_]*)\s*;\s*//\s*({.*})`)
	matches := reSamplerCombo.FindAllStringSubmatch(source, -1)
	for idx, match := range matches {
		metaJSON := match[2]
		meta := samplerMeta{}
		err := json.Unmarshal([]byte(metaJSON), &meta)
		if err != nil {
			fmt.Printf("warning: unable to parse combo JSON %s: %s\n", metaJSON, err.Error())
			continue
		}
		if meta.Combo != "" && idx < len(boundTextures) && boundTextures[idx] {
			combos[meta.Combo] = 1
		}
		defaults[match[1]] = meta.Default
	}
	return combos, defaults
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
	reVarying := regexp.MustCompile(`varying\s+([a-z|A-Z|0-9|_]*)\s+([a-z|A-Z|0-9|_|\[|\]|\s]*)\s*;`)
	varying := map[string]AttributeInfo{}
	source = reVarying.ReplaceAllStringFunc(source, func(match string) string {
		submatches := reVarying.FindStringSubmatch(match)
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

func generateUniformsStructCode(uniforms []UniformInfo, structName string) string {
	code := "typedef struct {\n"
	offset := 0
	lastPaddingID := -1
	for _, uniform := range uniforms {
		size, align := getGLSLTypeSizeAndAlignment(uniform.Type)
		if offset%align != 0 {
			lastPaddingID++
			padding := align - (offset % align)
			code += fmt.Sprintf("    uint8_t __pad%d[%d];\n", lastPaddingID, padding)
			offset += padding
		}
		code += fmt.Sprintf("    glsl_%s %s;\n", uniform.Type, uniform.Name)
		offset += size
	}
	code += "} " + structName + ";\n"
	return code
}

func getGLSLTypeSizeAndAlignment(typeName string) (int, int) {
	switch typeName {
	case "float":
		return 4, 4
	case "vec2":
		return 8, 8
	case "vec3":
		return 12, 16
	case "vec4":
		return 16, 16
	case "mat4":
		return 64, 16
	default:
		panic("unknown/unhandled GLSL type: " + typeName)
	}
}
