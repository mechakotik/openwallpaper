package main

import (
	"encoding/json"
	"errors"
	"fmt"
	"maps"
	"os"
	"os/exec"
	"regexp"
	"strconv"
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
	Name      string
	Type      string
	ArraySize int
}

type SamplerInfo struct {
	Name        string
	Default     string
	TextureSlot int
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
	vertexSource = removeUnmatchedEndifs(vertexSource)
	fragmentSource = removeUnmatchedEndifs(fragmentSource)

	vertexCombos, vertexDefaults := parseSamplerCombos(vertexSource, boundTextures)
	fragmentCombos, fragmentDefaults := parseSamplerCombos(fragmentSource, boundTextures)

	combos := map[string]int{}
	maps.Copy(combos, parseCombos(vertexSource))
	maps.Copy(combos, parseCombos(fragmentSource))
	maps.Copy(combos, vertexCombos)
	maps.Copy(combos, fragmentCombos)

	for combo, value := range comboOverrides {
		combos[combo] = value
	}

	vertexSource, err := runGLSLCPreprocessor(vertexSource, includePath, combos)
	if err != nil {
		return PreprocessedShader{}, err
	}
	fragmentSource, err = runGLSLCPreprocessor(fragmentSource, includePath, combos)
	if err != nil {
		return PreprocessedShader{}, err
	}

	vertexSource = rewriteHLSLImplicitConversions(vertexSource)
	fragmentSource = rewriteHLSLImplicitConversions(fragmentSource)

	vertexSource, attributes := preprocessVertexAttributes(vertexSource)
	vertexSource, vertexVarying := findAndRemoveVarying(vertexSource)
	fragmentSource, fragmentVarying := findAndRemoveVarying(fragmentSource)

	for name, info := range fragmentVarying {
		vertexInfo, exists := vertexVarying[name]
		if exists {
			if vertexInfo != info {
				mergedInfo, ok := mergeVaryingInfo(vertexInfo, info)
				if !ok {
					return PreprocessedShader{}, fmt.Errorf("varying %s has different type in vertex and fragment shader", name)
				}
				vertexVarying[name] = mergedInfo
			}
		} else {
			vertexVarying[name] = info
		}
	}

	vertexSource = rewriteTypedVectorAssignments(vertexSource, shaderVariableTypes(attributes, vertexVarying))
	fragmentAliases := map[string]string{}
	fragmentSource, fragmentAliases = rewriteWritableFragmentVaryings(fragmentSource, vertexVarying)
	fragmentTypes := shaderVariableTypes(nil, vertexVarying)
	maps.Copy(fragmentTypes, fragmentAliases)
	fragmentSource = rewriteTypedVectorAssignments(fragmentSource, fragmentTypes)

	varying := []AttributeInfo{}
	for _, info := range vertexVarying {
		varying = append(varying, info)
	}

	vertexSource, fragmentSource, samplers := preprocessShaderSamplers(vertexSource, fragmentSource)
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

	for idx := range samplers {
		samplerName := samplers[idx].Name
		defaultTexture := ""
		if texture, exists := fragmentDefaults[samplerName]; exists {
			defaultTexture = texture
		} else if texture, exists := vertexDefaults[samplerName]; exists {
			defaultTexture = texture
		}
		samplers[idx].Default = defaultTexture
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

func removeUnmatchedEndifs(source string) string {
	lines := strings.SplitAfter(source, "\n")
	reIf := regexp.MustCompile(`^\s*#\s*if(?:def|ndef)?\b`)
	reEndif := regexp.MustCompile(`^\s*#\s*endif\b`)
	depth := 0
	out := strings.Builder{}
	for _, line := range lines {
		switch {
		case reIf.MatchString(line):
			depth++
		case reEndif.MatchString(line):
			if depth == 0 {
				continue
			}
			depth--
		}
		out.WriteString(line)
	}
	return out.String()
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
#define CASTU(x) (uint(x))

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

func rewriteHLSLImplicitConversions(source string) string {
	source = renameUserFunction(source, "mod", "mod_")
	source = rewriteNonConstantConstFloats(source)
	source = rewriteIntCastsStoredInFloat(source)
	source = rewriteStepStoredInInt(source)
	source = rewriteUintModulo(source)
	source = rewriteBoolMultiplication(source)
	source = rewriteBoolCompoundMultiply(source)
	source = rewriteBoolCompoundAdd(source)
	source = rewriteVectorToScalarAssignments(source)
	source = rewriteTextureVectorAssignments(source)
	source = rewriteVectorFunctionArguments(source)
	source = rewriteVectorMixOperands(source)
	source = rewriteVectorMaxCalls(source)
	source = rewriteVectorBinaryOperands(source)
	source = rewriteVectorScalarAdditions(source)
	source = rewriteVec2ResolutionOperands(source)
	return source
}

func renameUserFunction(source string, name string, replacement string) string {
	reDefinition := regexp.MustCompile(`\b(float|vec[234]|int|uint)\s+` + regexp.QuoteMeta(name) + `\s*\(`)
	if !reDefinition.MatchString(source) {
		return source
	}
	source = reDefinition.ReplaceAllString(source, "$1 "+replacement+"(")

	reCall := regexp.MustCompile(`\b` + regexp.QuoteMeta(name) + `\s*\(`)
	return reCall.ReplaceAllString(source, replacement+"(")
}

func rewriteNonConstantConstFloats(source string) string {
	reConst := regexp.MustCompile(`\bconst\s+float\s+([A-Z_][A-Z0-9_]*)\s*=\s*([^;\n]*(?:g_|u_)[^;\n]*)\s*;`)
	return reConst.ReplaceAllString(source, "#define $1 ($2)")
}

func rewriteIntCastsStoredInFloat(source string) string {
	reIntCast := regexp.MustCompile(`\bfloat\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*int\s*\(`)
	return reIntCast.ReplaceAllString(source, "int $1 = int(")
}

func rewriteStepStoredInInt(source string) string {
	reStep := regexp.MustCompile(`\bint\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*((?:smoothstep|step)\s*\()`)
	return reStep.ReplaceAllString(source, "float $1 = $2")
}

func rewriteUintModulo(source string) string {
	reFloatModulo := regexp.MustCompile(`\buint\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*%\s*([a-zA-Z_][a-zA-Z0-9_]*|[0-9]+)\s*;`)
	source = reFloatModulo.ReplaceAllString(source, "uint $1 = uint(mod($2, float($3)));")

	reUintModulo := regexp.MustCompile(`\buint\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*\(([a-zA-Z_][a-zA-Z0-9_]*)\s*\+\s*([0-9]+)\)\s*%\s*([a-zA-Z_][a-zA-Z0-9_]*|[0-9]+)\s*;`)
	return reUintModulo.ReplaceAllString(source, "uint $1 = ($2 + ${3}u) % uint($4);")
}

func rewriteBoolMultiplication(source string) string {
	reBoolProduct := regexp.MustCompile(`\(([^()\n;]*(?:<=|>=|==|!=|<|>)[^()\n;]*)\)\s*\*`)
	return reBoolProduct.ReplaceAllString(source, "(($1) ? 1.0 : 0.0) *")
}

func rewriteBoolCompoundMultiply(source string) string {
	reBoolMultiply := regexp.MustCompile(`\b([a-zA-Z_][a-zA-Z0-9_]*)\s*\*=\s*([^;\n]*(?:&&|\|\|)[^;\n]*)\s*;`)
	source = reBoolMultiply.ReplaceAllString(source, "$1 *= (($2) ? 1.0 : 0.0);")

	boolVariables := declaredBoolVariables(source)
	for name := range boolVariables {
		reBoolVariableMultiply := regexp.MustCompile(`\b([a-zA-Z_][a-zA-Z0-9_]*)\s*\*=\s*` + regexp.QuoteMeta(name) + `\s*;`)
		source = reBoolVariableMultiply.ReplaceAllString(source, "$1 *= ("+name+" ? 1.0 : 0.0);")
	}

	return source
}

func rewriteBoolCompoundAdd(source string) string {
	reBoolAdd := regexp.MustCompile(`\b([a-zA-Z_][a-zA-Z0-9_]*(?:\.[xyzwrgba]+)?)\s*\+=\s*([^;\n]*(?:<=|>=|==|!=|<|>)[^;\n]*)\s*;`)
	return reBoolAdd.ReplaceAllString(source, "$1 += (($2) ? 1.0 : 0.0);")
}

func rewriteVectorToScalarAssignments(source string) string {
	reVectorScalar := regexp.MustCompile(`\bfloat\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*([^;\n]*(?:\.[xyzwrgba]{2,4}|vec[234]\s*\()[^;\n]*)\s*;`)
	return reVectorScalar.ReplaceAllString(source, "float $1 = ($2).x;")
}

func rewriteTextureVectorAssignments(source string) string {
	reTextureVector := regexp.MustCompile(`\bvec([23])\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*(texture\s*\([^;\n]+?\))\s*;`)
	return reTextureVector.ReplaceAllStringFunc(source, func(match string) string {
		submatches := reTextureVector.FindStringSubmatch(match)
		swizzle := "xy"
		if submatches[1] == "3" {
			swizzle = "rgb"
		}
		return fmt.Sprintf("vec%s %s = %s.%s;", submatches[1], submatches[2], submatches[3], swizzle)
	})
}

func rewriteVectorFunctionArguments(source string) string {
	reRotateVec2 := regexp.MustCompile(`\brotateVec2\s*\(\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*,`)
	source = reRotateVec2.ReplaceAllString(source, "rotateVec2($1.xy,")

	reTextureCoord := regexp.MustCompile(`\btexture\s*\(\s*([^,\n]+?)\s*,\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*\)`)
	return reTextureCoord.ReplaceAllString(source, "texture($1, $2.xy)")
}

func rewriteVectorMixOperands(source string) string {
	types := declaredVariableTypes(source)

	reReturnMix := regexp.MustCompile(`\breturn\s+mix\s*\(\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*,\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*,`)
	source = reReturnMix.ReplaceAllStringFunc(source, func(match string) string {
		submatches := reReturnMix.FindStringSubmatch(match)
		first := submatches[1]
		second := submatches[2]
		firstComponents, firstOK := vectorComponentCount(types[first])
		secondComponents, secondOK := vectorComponentCount(types[second])
		if !firstOK || !secondOK || firstComponents == secondComponents {
			return match
		}
		if firstComponents == 1 && secondComponents > 1 {
			return strings.Replace(match, "mix("+first+",", fmt.Sprintf("mix(%s(%s),", types[second], first), 1)
		}
		if secondComponents == 1 && firstComponents > 1 {
			return strings.Replace(match, ", "+second+",", fmt.Sprintf(", %s(%s),", types[first], second), 1)
		}
		return match
	})

	reRGBMix := regexp.MustCompile(`\b([a-zA-Z_][a-zA-Z0-9_]*\.rgb\s*=\s*mix\s*\()\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*,`)
	return reRGBMix.ReplaceAllStringFunc(source, func(match string) string {
		submatches := reRGBMix.FindStringSubmatch(match)
		components, ok := vectorComponentCount(types[submatches[2]])
		if !ok || components <= 3 {
			return match
		}
		return submatches[1] + submatches[2] + ".rgb,"
	})
}

func rewriteVectorMaxCalls(source string) string {
	reVectorMax := regexp.MustCompile(`\bmax\s*\(\s*([0-9]+(?:\.[0-9]+)?)\s*,\s*([a-zA-Z_][a-zA-Z0-9_]*(?:\.[rgbaxyzw]{2,4}))\s*\)`)
	return reVectorMax.ReplaceAllStringFunc(source, func(match string) string {
		submatches := reVectorMax.FindStringSubmatch(match)
		components := componentSelectorCount(submatches[2])
		if components <= 1 {
			return match
		}
		typeName, ok := vectorTypeWithComponentCount(components)
		if !ok {
			return match
		}
		value := submatches[1]
		if !strings.Contains(value, ".") {
			value += ".0"
		}
		return fmt.Sprintf("max(%s(%s), %s)", typeName, value, submatches[2])
	})
}

func rewriteVectorBinaryOperands(source string) string {
	reRoundMultiply := regexp.MustCompile(`\bround\s*\(\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*\)\s*\*\s*([a-zA-Z_][a-zA-Z0-9_]*)\b`)
	source = reRoundMultiply.ReplaceAllString(source, "round($1.xy) * $2.xy")

	reVectorMinusVec2 := regexp.MustCompile(`\b([a-zA-Z_][a-zA-Z0-9_]*)\s*-\s*(\(?vec2\s*\()`)
	return reVectorMinusVec2.ReplaceAllString(source, "$1.xy - $2")
}

func rewriteVectorScalarAdditions(source string) string {
	types := declaredVariableTypes(source)
	reVec2ScalarProduct := regexp.MustCompile(`\b([a-zA-Z_][a-zA-Z0-9_]*)\s*\+\s*([a-zA-Z_][a-zA-Z0-9_]*\s*\*\s*[a-zA-Z_][a-zA-Z0-9_]*)`)
	source = reVec2ScalarProduct.ReplaceAllStringFunc(source, func(match string) string {
		submatches := reVec2ScalarProduct.FindStringSubmatch(match)
		leftComponents, leftOK := vectorComponentCount(types[submatches[1]])
		if !leftOK || leftComponents < 2 {
			return match
		}
		productParts := strings.Split(submatches[2], "*")
		if len(productParts) != 2 {
			return match
		}
		for _, part := range productParts {
			part = strings.TrimSpace(part)
			components, ok := vectorComponentCount(types[part])
			if !ok || components != 1 {
				return match
			}
		}
		typeName, ok := vectorTypeWithComponentCount(leftComponents)
		if !ok {
			return match
		}
		return fmt.Sprintf("%s + %s(%s)", submatches[1], typeName, submatches[2])
	})

	reVec2SelectorScalar := regexp.MustCompile(`\b([a-zA-Z_][a-zA-Z0-9_]*\.[xyzwrgba]{2})\s*\+\s*([a-zA-Z_][a-zA-Z0-9_]*)\b`)
	source = reVec2SelectorScalar.ReplaceAllStringFunc(source, func(match string) string {
		submatches := reVec2SelectorScalar.FindStringSubmatch(match)
		components, ok := vectorComponentCount(types[submatches[2]])
		if !ok || components != 1 {
			return match
		}
		return fmt.Sprintf("%s + vec2(%s)", submatches[1], submatches[2])
	})

	reVec2Scalar := regexp.MustCompile(`\b([a-zA-Z_][a-zA-Z0-9_]*)\s*\+\s*([a-zA-Z_][a-zA-Z0-9_]*)\b`)
	return reVec2Scalar.ReplaceAllStringFunc(source, func(match string) string {
		submatches := reVec2Scalar.FindStringSubmatch(match)
		leftComponents, leftOK := vectorComponentCount(types[submatches[1]])
		rightComponents, rightOK := vectorComponentCount(types[submatches[2]])
		if !leftOK || !rightOK || leftComponents < 2 || rightComponents != 1 {
			return match
		}
		typeName, ok := vectorTypeWithComponentCount(leftComponents)
		if !ok {
			return match
		}
		return fmt.Sprintf("%s + %s(%s)", submatches[1], typeName, submatches[2])
	})
}

func rewriteVec2ResolutionOperands(source string) string {
	reResolution := regexp.MustCompile(`((?:CAST2\s*\([^;\n]*?\)|\(?vec2\s*\([^;\n]*?\)\)?)\s*/\s*)(g_Texture[0-9]+Resolution)(\.[xyzwrgba]+)?`)
	return reResolution.ReplaceAllStringFunc(source, func(match string) string {
		submatches := reResolution.FindStringSubmatch(match)
		if submatches[3] != "" {
			return match
		}
		return submatches[1] + submatches[2] + ".xy"
	})
}

func declaredVariableTypes(source string) map[string]string {
	types := map[string]string{}
	reDeclaration := regexp.MustCompile(`\b(float|vec[234])\s+([a-zA-Z_][a-zA-Z0-9_]*)\b`)
	for _, match := range reDeclaration.FindAllStringSubmatch(source, -1) {
		types[match[2]] = match[1]
	}
	return types
}

func declaredBoolVariables(source string) map[string]bool {
	variables := map[string]bool{}
	reBool := regexp.MustCompile(`\bbool\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=`)
	for _, match := range reBool.FindAllStringSubmatch(source, -1) {
		variables[match[1]] = true
	}
	return variables
}

func shaderVariableTypes(attributes []AttributeInfo, varying map[string]AttributeInfo) map[string]string {
	types := map[string]string{}
	for _, attribute := range attributes {
		types[attribute.Name] = attribute.Type
	}
	for name, info := range varying {
		types[name] = info.Type
	}
	return types
}

func rewriteTypedVectorAssignments(source string, types map[string]string) string {
	reAssignment := regexp.MustCompile(`\b([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*;`)
	return reAssignment.ReplaceAllStringFunc(source, func(match string) string {
		submatches := reAssignment.FindStringSubmatch(match)
		lhs := submatches[1]
		rhs := submatches[2]
		lhsComponents, lhsOK := vectorComponentCount(types[lhs])
		rhsComponents, rhsOK := vectorComponentCount(types[rhs])
		if !lhsOK || !rhsOK || lhsComponents == rhsComponents {
			return match
		}
		if lhsComponents < rhsComponents {
			return fmt.Sprintf("%s = %s.%s;", lhs, rhs, vectorSwizzle(lhsComponents))
		}
		return fmt.Sprintf("%s = %s.%s;", lhs, rhs, expandedVectorSwizzle(rhsComponents, lhsComponents))
	})
}

func rewriteWritableFragmentVaryings(source string, varying map[string]AttributeInfo) (string, map[string]string) {
	aliases := map[string]string{}
	init := ""
	localTypes := declaredVariableTypes(source)
	for name, info := range varying {
		if localType, exists := localTypes[name]; exists {
			alias := name + "_local"
			source = replaceIdentifier(source, name, alias)
			aliases[alias] = localType
			continue
		}

		reWrite := regexp.MustCompile(`\b` + regexp.QuoteMeta(name) + `(?:\.[xyzwrgba]+)?\s*(?:[+\-*/]?=|\+\+|--)`)
		if !reWrite.MatchString(source) {
			continue
		}

		alias := name + "_local"
		source = replaceIdentifier(source, name, alias)
		aliases[alias] = info.Type
		init += fmt.Sprintf(" %s %s = %s;\n", info.Type, alias, name)
	}
	if init == "" {
		return source, aliases
	}

	return insertAtMainStart(source, init), aliases
}

func replaceIdentifier(source string, from string, to string) string {
	reIdentifier := regexp.MustCompile(`\b` + regexp.QuoteMeta(from) + `\b`)
	return reIdentifier.ReplaceAllString(source, to)
}

func insertAtMainStart(source string, text string) string {
	reMain := regexp.MustCompile(`void\s+main\s*\(\s*\)\s*\{`)
	inserted := false
	return reMain.ReplaceAllStringFunc(source, func(match string) string {
		if inserted {
			return match
		}
		inserted = true
		return match + "\n" + text
	})
}

func mergeVaryingInfo(vertexInfo AttributeInfo, fragmentInfo AttributeInfo) (AttributeInfo, bool) {
	if vertexInfo.Name != fragmentInfo.Name || vertexInfo.ArraySize != fragmentInfo.ArraySize {
		return AttributeInfo{}, false
	}
	vertexComponents, vertexOK := vectorComponentCount(vertexInfo.Type)
	fragmentComponents, fragmentOK := vectorComponentCount(fragmentInfo.Type)
	if !vertexOK || !fragmentOK {
		return AttributeInfo{}, false
	}
	components := vertexComponents
	if fragmentComponents > components {
		components = fragmentComponents
	}
	mergedType, ok := vectorTypeWithComponentCount(components)
	if !ok {
		return AttributeInfo{}, false
	}
	return AttributeInfo{
		Name:      vertexInfo.Name,
		Type:      mergedType,
		ArraySize: vertexInfo.ArraySize,
	}, true
}

func vectorComponentCount(typeName string) (int, bool) {
	switch typeName {
	case "float":
		return 1, true
	case "vec2":
		return 2, true
	case "vec3":
		return 3, true
	case "vec4":
		return 4, true
	default:
		return 0, false
	}
}

func componentSelectorCount(expression string) int {
	dot := strings.LastIndex(expression, ".")
	if dot < 0 || dot+1 >= len(expression) {
		return 1
	}

	return len(expression[dot+1:])
}

func vectorTypeWithComponentCount(components int) (string, bool) {
	switch components {
	case 1:
		return "float", true
	case 2:
		return "vec2", true
	case 3:
		return "vec3", true
	case 4:
		return "vec4", true
	default:
		return "", false
	}
}

func vectorSwizzle(components int) string {
	switch components {
	case 1:
		return "x"
	case 2:
		return "xy"
	case 3:
		return "xyz"
	default:
		return "xyzw"
	}
}

func expandedVectorSwizzle(sourceComponents int, targetComponents int) string {
	switch sourceComponents {
	case 1:
		return strings.Repeat("x", targetComponents)
	case 2:
		if targetComponents == 3 {
			return "xyx"
		}
		return "xyxy"
	case 3:
		if targetComponents == 4 {
			return "xyzx"
		}
	}
	return vectorSwizzle(targetComponents)
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
		textureSlot := idx
		if parsedSlot, ok := parseTextureSlotFromSamplerName(match[1]); ok {
			textureSlot = parsedSlot
		}
		if meta.Combo != "" && textureSlot >= 0 && textureSlot < len(boundTextures) && boundTextures[textureSlot] {
			combos[meta.Combo] = 1
		}
		defaults[match[1]] = meta.Default
	}
	return combos, defaults
}

func parseTextureSlotFromSamplerName(name string) (int, bool) {
	if !strings.HasPrefix(name, "g_Texture") {
		return 0, false
	}

	slotText, ok := strings.CutPrefix(name, "g_Texture")
	if !ok || slotText == "" {
		return 0, false
	}

	slot, err := strconv.Atoi(slotText)
	if err != nil {
		return 0, false
	}

	return slot, true
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
	reVarying := regexp.MustCompile(`varying\s+([a-z|A-Z|0-9|_]*)\s+([a-z|A-Z|0-9|_|\s]*)\s*;`)
	varying := map[string]AttributeInfo{}
	source = reVarying.ReplaceAllStringFunc(source, func(match string) string {
		submatches := reVarying.FindStringSubmatch(match)
		varying[submatches[2]] = AttributeInfo{
			Name:      string(submatches[2]),
			Type:      string(submatches[1]),
			ArraySize: 0,
		}
		return ""
	})

	reVaryingArray := regexp.MustCompile(`varying\s+([a-z|A-Z|0-9|_]*)\s+([a-z|A-Z|0-9|_|\s]*)\s*\[([0-9]*)\];`)
	source = reVaryingArray.ReplaceAllStringFunc(source, func(match string) string {
		submatches := reVaryingArray.FindStringSubmatch(match)
		arraySize, err := strconv.Atoi(submatches[3])
		if err != nil {
			fmt.Printf("warning: unable to parse varying %s array size (%s): %s\n", match, submatches[3], err.Error())
			return match
		}
		varying[submatches[2]] = AttributeInfo{
			Name:      string(submatches[2]),
			Type:      string(submatches[1]),
			ArraySize: arraySize,
		}
		return ""
	})

	return normalizeNewlines(source), varying
}

func insertIntermediateAttributes(source string, attributes []AttributeInfo, side string) string {
	attributesString := ""
	slot := 0
	for _, attribute := range attributes {
		if attribute.ArraySize >= 1 {
			attributesString += fmt.Sprintf("layout(location = %d) %s %s %s[%d];\n", slot, side, attribute.Type, attribute.Name, attribute.ArraySize)
			slot += attribute.ArraySize
		} else {
			attributesString += fmt.Sprintf("layout(location = %d) %s %s %s;\n", slot, side, attribute.Type, attribute.Name)
			slot++
		}
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

func preprocessShaderSamplers(vertexSource string, fragmentSource string) (string, string, []SamplerInfo) {
	vertexSource, vertexDeclaredSamplers := removeDeclaredSamplers(vertexSource)
	fragmentSource, fragmentDeclaredSamplers := removeDeclaredSamplers(fragmentSource)

	vertexSearchSource := stripGLSLComments(vertexSource)
	fragmentSearchSource := stripGLSLComments(fragmentSource)
	samplerBindings := map[string]int{}
	samplers := []SamplerInfo{}

	appendUsedSampler := func(sampler SamplerInfo) {
		if _, exists := samplerBindings[sampler.Name]; exists {
			return
		}
		if !sourceUsesIdentifier(vertexSearchSource, sampler.Name) && !sourceUsesIdentifier(fragmentSearchSource, sampler.Name) {
			return
		}

		samplerBindings[sampler.Name] = len(samplers)
		samplers = append(samplers, sampler)
	}

	for _, sampler := range fragmentDeclaredSamplers {
		appendUsedSampler(sampler)
	}
	for _, sampler := range vertexDeclaredSamplers {
		appendUsedSampler(sampler)
	}

	vertexHeader := ""
	fragmentHeader := ""
	for idx, sampler := range samplers {
		if sourceUsesIdentifier(vertexSearchSource, sampler.Name) {
			vertexHeader += fmt.Sprintf("layout(set = 2, binding = %d) uniform sampler2D %s;\n", idx, sampler.Name)
		}
		if sourceUsesIdentifier(fragmentSearchSource, sampler.Name) {
			fragmentHeader += fmt.Sprintf("layout(set = 2, binding = %d) uniform sampler2D %s;\n", idx, sampler.Name)
		}
	}

	vertexSource = insertSamplerHeader(vertexSource, vertexHeader)
	fragmentSource = insertSamplerHeader(fragmentSource, fragmentHeader)
	return vertexSource, fragmentSource, samplers
}

func removeDeclaredSamplers(source string) (string, []SamplerInfo) {
	reSampler := regexp.MustCompile(`uniform\s*sampler2D\s*([A-Za-z0-9_]*)\s*;`)
	declaredSamplers := []SamplerInfo{}
	source = reSampler.ReplaceAllStringFunc(source, func(match string) string {
		submatches := reSampler.FindStringSubmatch(match)
		name := submatches[1]
		textureSlot := len(declaredSamplers)
		if parsedSlot, ok := parseTextureSlotFromSamplerName(name); ok {
			textureSlot = parsedSlot
		}
		declaredSamplers = append(declaredSamplers, SamplerInfo{
			Name:        name,
			TextureSlot: textureSlot,
		})
		return ""
	})

	return source, declaredSamplers
}

func insertSamplerHeader(source string, header string) string {
	if header == "" {
		return source
	}
	if strings.Contains(source, "#version 450") {
		return strings.Replace(source, "#version 450", "#version 450\n"+header, 1)
	}
	return header + source
}

func stripGLSLComments(source string) string {
	reBlockComment := regexp.MustCompile(`(?s)/\*.*?\*/`)
	source = reBlockComment.ReplaceAllString(source, "")

	reLineComment := regexp.MustCompile(`//.*`)
	return reLineComment.ReplaceAllString(source, "")
}

func sourceUsesIdentifier(source string, identifier string) bool {
	reIdentifier := regexp.MustCompile(`(^|[^a-zA-Z0-9_])` + regexp.QuoteMeta(identifier) + `([^a-zA-Z0-9_]|$)`)
	return reIdentifier.FindStringIndex(source) != nil
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

	reArrayUniform := regexp.MustCompile(`uniform\s*([a-z|A-Z|0-9|_]*)\s*([a-z|A-Z|0-9|_]*)\s*\[(.*)\]\s*;`)
	matches = reArrayUniform.FindAllStringSubmatch(source, -1)
	for _, match := range matches {
		if match[1] == "sampler2D" {
			continue
		}
		arraySize, err := strconv.Atoi(match[3])
		if err != nil {
			fmt.Printf("warning: unable to parse array size %s: %s\n", match[3], err.Error())
			continue
		}
		uniforms = append(uniforms, UniformInfo{
			Name:      string(match[2]),
			Type:      string(match[1]),
			ArraySize: arraySize,
		})
	}

	uniformBlock := fmt.Sprintf("layout(std140, set = %d, binding = 0) uniform uniforms_t {\n", set)
	for _, uniform := range uniforms {
		if uniform.ArraySize == 0 {
			uniformBlock += fmt.Sprintf("    %s %s;\n", uniform.Type, uniform.Name)
		} else {
			uniformBlock += fmt.Sprintf("    %s %s[%d];\n", uniform.Type, uniform.Name, uniform.ArraySize)
		}
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
	source = reArrayUniform.ReplaceAllStringFunc(source, func(match string) string {
		submatches := reArrayUniform.FindStringSubmatch(match)
		if submatches[1] == "sampler2D" {
			return match
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
