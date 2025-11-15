package main

import (
	"bytes"
	_ "embed"
	"fmt"
	"os"
	"os/exec"
	"regexp"
	"strings"
	"text/template"

	"github.com/alexflint/go-arg"
)

type CompiledShader struct {
	ID               int
	NameInfo         string
	VertexUniforms   []UniformInfo
	FragmentUniforms []UniformInfo
	Samplers         []string
}

type ImportedTexture struct {
	ID          int
	Name        string
	Width       int
	Height      int
	PixelFormat string
}

type UniformCMapping struct {
	Type       string
	Dimensions []int
	Size       int
	Alignment  int
}

type EffectPassMeta struct {
	Shader    CompiledShader
	Constants map[string][]float32
	Textures  []ImportedTexture
}

type CodegenTextureBindingData struct {
	Slot    int
	Texture string
}

type CodegenTransformData struct {
	Enabled                bool
	SceneWidth             float32
	SceneHeight            float32
	OriginX                float32
	OriginY                float32
	OriginZ                float32
	SizeX                  float32
	SizeY                  float32
	ScaleX                 float32
	ScaleY                 float32
	ScaleZ                 float32
	ParallaxDepthX         float32
	ParallaxDepthY         float32
	ParallaxEnabled        bool
	ParallaxAmount         float32
	ParallaxMouseInfluence float32
}

type CodegenPassData struct {
	ObjectID          int
	PassID            int
	ShaderID          int
	ColorTarget       string
	ColorTargetFormat string
	ClearColor        bool
	TextureBindings   []CodegenTextureBindingData
	UniformSetupCode  string
	Transform         CodegenTransformData
}

type TempBufferParameters struct {
	Width  int
	Height int
	Number int
}

type CodegenData struct {
	Textures    []ImportedTexture
	Shaders     []CompiledShader
	Passes      []CodegenPassData
	TempBuffers []TempBufferParameters
}

var (
	args struct {
		Input       string `arg:"positional,required"`
		Output      string `arg:"positional,required"`
		KeepSources bool   `arg:"--keep-sources"`
	}

	pkgMap       map[string][]byte
	scene        *Scene
	outputMap    = map[string][]byte{}
	textureMap   = map[string]ImportedTexture{}
	lastObjectID = -1
	lastPassID   = 0
	lastShaderID = -1
	codegenData  CodegenData
)

//go:embed scene.tmpl
var sceneTemplateCode []uint8

//go:embed scene_utils.h
var sceneUtilsCode []uint8

func main() {
	arg.MustParse(&args)

	pkgFile, err := os.ReadFile(args.Input)
	if err != nil {
		panic("open pkg failed: " + err.Error())
	}
	pkgMap, err = extractPkg(pkgFile)
	if err != nil {
		panic("extract pkg failed: " + err.Error())
	}

	scene, err = parseSceneJSON(string(pkgMap["scene.json"]))
	if err != nil {
		panic("parse scene.json failed: " + err.Error())
	}

	screenCleared := false
	for _, object := range scene.Objects {
		if object.Image == "" || object.Size == nil {
			continue
		}

		tempBuffers := [2]int{}
		tempBuffers[0] = getTempBuffer(TempBufferParameters{
			Width:  int(object.Size.X + 0.5),
			Height: int(object.Size.Y + 0.5),
			Number: 0,
		})
		tempBuffers[1] = getTempBuffer(TempBufferParameters{
			Width:  int(object.Size.X + 0.5),
			Height: int(object.Size.Y + 0.5),
			Number: 1,
		})

		initialPass, err := processObjectInit(object, &tempBuffers)
		if err != nil {
			fmt.Printf("warning: skipping object %s because processing initial passes failed: %s\n", object.Name, err)
			continue
		}
		codegenData.Passes = append(codegenData.Passes, initialPass)

		for _, effectInstance := range object.Effects {
			passes, err := processEffect(object, effectInstance, &tempBuffers)
			if err != nil {
				fmt.Printf("warning: skipping object %s effect %s because processing effect failed: %s\n", object.Name, effectInstance.Name, err)
				continue
			}
			codegenData.Passes = append(codegenData.Passes, passes...)
		}

		lastIdx := len(codegenData.Passes) - 1
		if !screenCleared {
			codegenData.Passes[lastIdx].ClearColor = true
			screenCleared = true
		} else {
			codegenData.Passes[lastIdx].ClearColor = false
		}
		codegenData.Passes[lastIdx].ColorTarget = "0"
		codegenData.Passes[lastIdx].ColorTargetFormat = "OW_TEXTURE_SWAPCHAIN"
		codegenData.Passes[lastIdx].Transform.Enabled = true
	}

	sceneTemplate, err := template.New("scene.tmpl").Parse(string(sceneTemplateCode))
	if err != nil {
		panic("parsing scene template failed: " + err.Error())
	}
	sceneSourceBuffer := bytes.Buffer{}
	err = sceneTemplate.ExecuteTemplate(&sceneSourceBuffer, "scene", codegenData)
	if err != nil {
		panic("executing scene template failed: " + err.Error())
	}
	sceneSource := sceneSourceBuffer.Bytes()

	if args.KeepSources {
		outputMap["scene.c"] = sceneSource
	}

	println("compiling scene module")

	tempDirBytes, err := exec.Command("mktemp", "-d").Output()
	if err != nil {
		panic("mktemp failed: " + err.Error())
	}
	tempDir := strings.TrimSuffix(string(tempDirBytes), "\n")

	err = os.WriteFile(tempDir+"/scene.c", sceneSource, 0644)
	if err != nil {
		panic("write scene.c failed: " + err.Error())
	}
	err = os.WriteFile(tempDir+"/scene_utils.h", sceneUtilsCode, 0644)
	if err != nil {
		panic("write scene_utils.h failed" + err.Error())
	}

	logBytes, err := exec.Command("/opt/wasi-sdk/bin/clang", tempDir+"/scene.c", "-o", tempDir+"/scene.wasm", "-I../include", "-Wl,--allow-undefined").CombinedOutput()
	if err != nil {
		panic("compiling scene module failed: " + string(logBytes))
	}

	wasmBytes, err := os.ReadFile(tempDir + "/scene.wasm")
	if err != nil {
		panic("reading scene module failed: " + err.Error())
	}

	outputMap["scene.wasm"] = wasmBytes

	zip, err := zipBytes(outputMap)
	if err != nil {
		panic("zip failed: " + err.Error())
	}

	err = os.WriteFile(args.Output, zip, 0644)
	if err != nil {
		panic("write output failed: " + err.Error())
	}

	err = os.RemoveAll(tempDir)
	if err != nil {
		println("warning: failed to remove temp dir: " + err.Error())
	}
}

func processObjectInit(object SceneObject, tempBuffers *[2]int) (CodegenPassData, error) {
	lastObjectID++
	lastPassID = 0

	materialPath := replaceByRegex(object.Image, "models/(.*)", "materials/$1")
	materialBytes, err := getAssetBytes(materialPath)
	if err != nil {
		return CodegenPassData{}, fmt.Errorf("loading material file %s failed: %s", materialPath, err)
	}

	material, err := parseMaterialJSON(materialBytes)
	if err != nil {
		return CodegenPassData{}, fmt.Errorf("parsing material JSON %s failed: %s", materialPath, err)
	}

	textures := []ImportedTexture{}
	for _, textureName := range material.Textures {
		texture, err := importTexture(textureName)
		if err != nil {
			return CodegenPassData{}, fmt.Errorf("loading texture %s failed: %s", textureName, err)
		}
		if texture.ID < 0 {
			continue
		}
		textures = append(textures, texture)
	}

	shader, err := compileShader(material.Shader, []bool{}, material.Combos)
	if err != nil {
		fmt.Printf("warning: skipping material %s because compiling shader failed: %s\n", materialPath, err)
		return CodegenPassData{}, fmt.Errorf("compiling shader %s failed: %s", material.Shader, err)
	}

	passData := CodegenPassData{
		ObjectID:          lastObjectID,
		PassID:            0,
		ShaderID:          shader.ID,
		ColorTarget:       fmt.Sprintf("temp_buffers[%d]", tempBuffers[0]),
		ColorTargetFormat: "OW_TEXTURE_RGBA8_UNORM",
		ClearColor:        true,
	}

	for slot, texture := range textures {
		passData.TextureBindings = append(passData.TextureBindings, CodegenTextureBindingData{
			Slot:    slot,
			Texture: fmt.Sprintf("texture%d", texture.ID),
		})
	}

	passData.Transform = CodegenTransformData{
		Enabled:                false,
		SceneWidth:             float32(scene.General.Ortho.Width),
		SceneHeight:            float32(scene.General.Ortho.Height),
		OriginX:                float32(object.Origin.X),
		OriginY:                float32(object.Origin.Y),
		OriginZ:                float32(object.Origin.Z),
		SizeX:                  float32(object.Size.X),
		SizeY:                  float32(object.Size.Y),
		ScaleX:                 float32(object.Scale.X),
		ScaleY:                 float32(object.Scale.Y),
		ScaleZ:                 float32(object.Scale.Z),
		ParallaxDepthX:         float32(object.ParallaxDepth.X),
		ParallaxDepthY:         float32(object.ParallaxDepth.Y),
		ParallaxEnabled:        scene.General.CameraParallax,
		ParallaxAmount:         float32(scene.General.CameraParallaxAmount),
		ParallaxMouseInfluence: float32(scene.General.CameraParallaxMouseInfluence),
	}

	passData.UniformSetupCode = generateUniformSetupCode("vertex_uniforms", shader.VertexUniforms, map[string][]float32{})
	passData.UniformSetupCode += generateUniformSetupCode("fragment_uniforms", shader.FragmentUniforms, map[string][]float32{})
	return passData, nil
}

func processEffect(object SceneObject, effectInstance EffectInstance, tempBuffers *[2]int) ([]CodegenPassData, error) {
	effect, err := parseEffect(pkgMap[effectInstance.File])
	if err != nil {
		return []CodegenPassData{}, fmt.Errorf("parsing effect JSON %s failed: %s", effectInstance.File, err)
	}

	allBoundTextures := [][]bool{}
	for _, pass := range effectInstance.Passes {
		boundTextures := []bool{}
		for _, textureName := range pass.Textures {
			boundTextures = append(boundTextures, textureName != nil)
		}
		allBoundTextures = append(allBoundTextures, boundTextures)
	}

	meta := make([]EffectPassMeta, len(effect.Passes))
	for idx, pass := range effect.Passes {
		effectMaterial, err := parseMaterialJSON(pkgMap[pass.Material])
		if err != nil {
			return []CodegenPassData{}, fmt.Errorf("parsing effect material JSON %s failed: %s", pass.Material, err)
		}
		meta[idx].Shader, err = compileShader(effectMaterial.Shader, allBoundTextures[idx], effectMaterial.Combos)
		if err != nil {
			return []CodegenPassData{}, fmt.Errorf("compiling effect shader %s failed: %s", effectMaterial.Shader, err)
		}
	}

	for idx, pass := range effectInstance.Passes {
		for _, textureName := range pass.Textures {
			if textureName == nil {
				meta[idx].Textures = append(meta[idx].Textures, ImportedTexture{ID: -1})
				continue
			}
			texture, err := importTexture(*textureName)
			if err != nil {
				return []CodegenPassData{}, fmt.Errorf("loading texture %s failed: %s", *textureName, err)
			}
			meta[idx].Textures = append(meta[idx].Textures, texture)
		}
		for len(meta[idx].Textures) < len(meta[idx].Shader.Samplers) {
			meta[idx].Textures = append(meta[idx].Textures, ImportedTexture{ID: -1})
		}
		meta[idx].Constants = pass.Constants
	}

	passes := []CodegenPassData{}

	for _, pass := range meta {
		lastPassID++

		passData := CodegenPassData{
			ObjectID:          lastObjectID,
			PassID:            lastPassID,
			ShaderID:          pass.Shader.ID,
			ColorTargetFormat: "OW_TEXTURE_RGBA8_UNORM",
			ColorTarget:       fmt.Sprintf("temp_buffers[%d]", tempBuffers[1]),
			ClearColor:        true,
		}

		for slot, texture := range pass.Textures {
			if texture.ID == -1 {
				passData.TextureBindings = append(passData.TextureBindings, CodegenTextureBindingData{
					Slot:    slot,
					Texture: fmt.Sprintf("temp_buffers[%d]", tempBuffers[0]),
				})
			} else {
				passData.TextureBindings = append(passData.TextureBindings, CodegenTextureBindingData{
					Slot:    slot,
					Texture: fmt.Sprintf("texture%d", texture.ID),
				})
			}
		}

		passData.Transform = CodegenTransformData{
			Enabled:                false,
			SceneWidth:             float32(scene.General.Ortho.Width),
			SceneHeight:            float32(scene.General.Ortho.Height),
			OriginX:                float32(object.Origin.X),
			OriginY:                float32(object.Origin.Y),
			OriginZ:                float32(object.Origin.Z),
			SizeX:                  float32(object.Size.X),
			SizeY:                  float32(object.Size.Y),
			ScaleX:                 float32(object.Scale.X),
			ScaleY:                 float32(object.Scale.Y),
			ScaleZ:                 float32(object.Scale.Z),
			ParallaxDepthX:         float32(object.ParallaxDepth.X),
			ParallaxDepthY:         float32(object.ParallaxDepth.Y),
			ParallaxEnabled:        scene.General.CameraParallax,
			ParallaxAmount:         float32(scene.General.CameraParallaxAmount),
			ParallaxMouseInfluence: float32(scene.General.CameraParallaxMouseInfluence),
		}

		passData.UniformSetupCode = generateUniformSetupCode("vertex_uniforms", pass.Shader.VertexUniforms, pass.Constants)
		passData.UniformSetupCode += generateUniformSetupCode("fragment_uniforms", pass.Shader.FragmentUniforms, pass.Constants)
		passes = append(passes, passData)
		tempBuffers[0], tempBuffers[1] = tempBuffers[1], tempBuffers[0]
	}

	return passes, nil
}

func generateUniformSetupCode(structName string, uniforms []UniformInfo, constants map[string][]float32) string {
	code := ""
	for _, uniform := range uniforms {
		if uniform.Name == "g_ModelMatrix" {
			code += "        " + structName + ".g_ModelMatrix = matrices.model;\n"
		} else if uniform.Name == "g_ViewProjectionMatrix" {
			code += "        " + structName + ".g_ViewProjectionMatrix = matrices.view_projection;\n"
		} else if uniform.Name == "g_ModelViewProjectionMatrix" {
			code += "        " + structName + ".g_ModelViewProjectionMatrix = matrices.model_view_projection;\n"
		} else if uniform.Name == "g_Texture0Rotation" {
			code += "        " + structName + ".g_Texture0Rotation = (glsl_vec4){.x = 1.0f, .y = 0.0f, .z = 0.0f, .w = 1.0f};\n"
		} else if strings.HasSuffix(uniform.Name, "Resolution") {
			code += fmt.Sprintf("        %s.%s = (glsl_vec4){.x = 1920, .y = 1080, .z = 1920, .w = 1080};\n", structName, uniform.Name)
		} else if uniform.Name == "g_Time" {
			code += "        " + structName + ".g_Time = (glsl_float){.x = time};\n"
		} else if uniform.Name == "g_ParallaxPosition" {
			code += "        " + structName + ".g_ParallaxPosition = (glsl_vec2){ .x = matrices.parallax_position_x, .y = matrices.parallax_position_y };\n"
		} else if uniform.DefaultSet {
			code += fmt.Sprintf("        %s.%s = (glsl_%s){.at = {", structName, uniform.Name, uniform.Type)
			value := uniform.Default
			if override, exists := constants[uniform.ConstantName]; exists {
				value = override
			}
			for _, value := range value {
				code += fmt.Sprintf("%f, ", value)
			}
			code += "}};\n"
		} else {
			fmt.Printf("warning: unknown uniform %s\n", uniform.Name)
		}
	}
	return code
}

func getAssetBytes(path string) ([]byte, error) {
	if bytes, exists := pkgMap[path]; exists {
		return bytes, nil
	}
	asset, err := os.ReadFile("assets/" + path)
	if err == nil {
		return asset, nil
	}
	return nil, fmt.Errorf("open asset %s failed: %s", path, err)
}

func importTexture(textureName string) (ImportedTexture, error) {
	if _, exists := textureMap[textureName]; exists {
		return textureMap[textureName], nil
	}
	if textureName == "_rt_FullFrameBuffer" {
		return ImportedTexture{ID: -1}, nil
	}

	texturePath := "materials/" + textureName + ".tex"
	fmt.Printf("importing texture %s\n", textureName)
	textureBytes, err := getAssetBytes(texturePath)
	if err != nil {
		return ImportedTexture{}, err
	}

	converted, err := texToWEBP(textureBytes)
	if err != nil {
		return ImportedTexture{}, err
	}

	textureMap[textureName] = ImportedTexture{
		ID:          len(textureMap),
		Name:        textureName,
		Width:       converted.Width,
		Height:      converted.Height,
		PixelFormat: converted.PixelFormat,
	}

	outputMap[fmt.Sprintf("assets/texture%d.webp", textureMap[textureName].ID)] = converted.Data
	codegenData.Textures = append(codegenData.Textures, textureMap[textureName])

	return textureMap[textureName], nil
}

func getTempBuffer(params TempBufferParameters) int {
	for idx, curParams := range codegenData.TempBuffers {
		if curParams == params {
			return idx
		}
	}
	codegenData.TempBuffers = append(codegenData.TempBuffers, params)
	return len(codegenData.TempBuffers) - 1
}

func compileShader(shaderName string, boundTextures []bool, defines map[string]int) (CompiledShader, error) {
	lastShaderID++
	vertexShaderPath := "shaders/" + shaderName + ".vert"
	fragmentShaderPath := "shaders/" + shaderName + ".frag"

	definesInfo := ""
	for name, value := range defines {
		if definesInfo != "" {
			definesInfo += ", "
		}
		definesInfo += fmt.Sprintf("%s=%d", name, value)
	}
	nameInfo := shaderName
	if definesInfo != "" {
		nameInfo += " (" + definesInfo + ")"
	}
	fmt.Printf("compiling shader %s\n", nameInfo)

	vertexShaderBytes, err := getAssetBytes(vertexShaderPath)
	if err != nil {
		return CompiledShader{}, err
	}
	fragmentShaderBytes, err := getAssetBytes(fragmentShaderPath)
	if err != nil {
		return CompiledShader{}, err
	}

	transformed, err := preprocessShader(string(vertexShaderBytes), string(fragmentShaderBytes), "assets/shaders", boundTextures, defines)
	if err != nil {
		return CompiledShader{}, err
	}

	if args.KeepSources {
		outputMap[fmt.Sprintf("assets/shader%d_vertex.glsl", lastShaderID)] = []byte(transformed.VertexGLSL)
		outputMap[fmt.Sprintf("assets/shader%d_fragment.glsl", lastShaderID)] = []byte(transformed.FragmentGLSL)
	}

	tempDirBytes, err := exec.Command("mktemp", "-d").Output()
	if err != nil {
		return CompiledShader{}, err
	}
	tempDir := strings.TrimSuffix(string(tempDirBytes), "\n")

	err = os.WriteFile(tempDir+"/vertex.glsl", []byte(transformed.VertexGLSL), 0644)
	if err != nil {
		panic("write vertex.glsl failed: " + err.Error())
	}
	err = os.WriteFile(tempDir+"/fragment.glsl", []byte(transformed.FragmentGLSL), 0644)
	if err != nil {
		panic("write fragment.glsl failed: " + err.Error())
	}

	glslcArgs := []string{"-fshader-stage=vertex", tempDir + "/vertex.glsl", "-o", tempDir + "/vertex.spv"}
	for name, value := range defines {
		glslcArgs = append(glslcArgs, fmt.Sprintf("-D%s=%d", name, value))
	}
	logBytes, err := exec.Command("glslc", glslcArgs...).CombinedOutput()
	if err != nil {
		return CompiledShader{}, fmt.Errorf("compiling vertex shader failed: %s", logBytes)
	}

	glslcArgs = []string{"-fshader-stage=fragment", tempDir + "/fragment.glsl", "-o", tempDir + "/fragment.spv"}
	for name, value := range defines {
		glslcArgs = append(glslcArgs, fmt.Sprintf("-D%s=%d", name, value))
	}
	logBytes, err = exec.Command("glslc", glslcArgs...).CombinedOutput()
	if err != nil {
		return CompiledShader{}, fmt.Errorf("compiling fragment shader failed: %s", logBytes)
	}

	vertexSPIRVBytes, err := os.ReadFile(tempDir + "/vertex.spv")
	if err != nil {
		return CompiledShader{}, err
	}
	fragmentSPIRVBytes, err := os.ReadFile(tempDir + "/fragment.spv")
	if err != nil {
		return CompiledShader{}, err
	}

	err = os.RemoveAll(tempDir)
	if err != nil {
		println("warning: failed to remove temp dir: " + err.Error())
	}

	outputMap[fmt.Sprintf("assets/shader%d_vertex.spv", lastShaderID)] = vertexSPIRVBytes
	outputMap[fmt.Sprintf("assets/shader%d_fragment.spv", lastShaderID)] = fragmentSPIRVBytes

	compiledShader := CompiledShader{
		ID:               lastShaderID,
		NameInfo:         nameInfo,
		VertexUniforms:   transformed.VertexUniforms,
		FragmentUniforms: transformed.FragmentUniforms,
		Samplers:         transformed.Samplers,
	}

	codegenData.Shaders = append(codegenData.Shaders, compiledShader)
	return compiledShader, nil
}

func replaceByRegex(source, pattern, replace string) string {
	regex := regexp.MustCompile(pattern)
	return regex.ReplaceAllString(source, replace)
}
