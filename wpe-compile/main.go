package main

import (
	"bytes"
	_ "embed"
	"fmt"
	"os"
	"os/exec"
	"strconv"
	"strings"
	"text/template"

	"github.com/alexflint/go-arg"
)

type CompiledShader struct {
	ID                     int
	NameInfo               string
	VertexUniforms         []UniformInfo
	FragmentUniforms       []UniformInfo
	VertexUniformsStruct   string
	FragmentUniformsStruct string
	Samplers               []SamplerInfo
}

type ImportedTexture struct {
	ID            int
	Name          string
	Width         int
	Height        int
	ClampUV       bool
	Interpolation bool
}

type UniformCMapping struct {
	Type       string
	Dimensions []int
	Size       int
	Alignment  int
}

type UniformCodegenContext struct {
	StructName  string
	Uniforms    []UniformInfo
	Constants   map[string][]float32
	Resolutions [][4]float32
	Color       [4]float32
}

type CodegenTextureBindingData struct {
	Slot    int
	Texture string
	Sampler string
}

type CodegenParallaxData struct {
	Enabled        bool
	Amount         float32
	Delay          float32
	MouseInfluence float32
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
	BlendMode         string
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
	Parallax    CodegenParallaxData
}

var (
	args struct {
		Input       string `arg:"positional,required"`
		Output      string `arg:"positional,required"`
		KeepSources bool   `arg:"--keep-sources"`
	}

	pkgMap        map[string][]byte
	scene         Scene
	outputMap     = map[string][]byte{}
	textureMap    = map[string]ImportedTexture{}
	lastObjectID  = -1
	lastPassID    = 0
	lastShaderID  = -1
	screenCleared = false
	codegenData   CodegenData
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

	scene, err = ParseScene(&pkgMap)
	if err != nil {
		panic("parse scene.json failed: " + err.Error())
	}

	codegenData.Parallax = CodegenParallaxData{
		Enabled:        scene.General.Parallax,
		Amount:         scene.General.ParallaxAmount,
		Delay:          scene.General.ParallaxDelay,
		MouseInfluence: scene.General.ParallaxMouseInfluence,
	}

	for _, object := range scene.Objects {
		if imageObject, ok := object.(*ImageObject); ok {
			processImageObject(*imageObject)
		}
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

func processImageObject(object ImageObject) {
	if object.Fullscreen || object.ImagePath == "" || object.Size[0] == 0 || object.Size[1] == 0 {
		return
	}

	tempBuffers := [2]int{}
	tempBuffers[0] = getTempBuffer(TempBufferParameters{
		Width:  int(object.Size[0] + 0.5),
		Height: int(object.Size[1] + 0.5),
		Number: 0,
	})
	tempBuffers[1] = getTempBuffer(TempBufferParameters{
		Width:  int(object.Size[0] + 0.5),
		Height: int(object.Size[1] + 0.5),
		Number: 1,
	})

	initialPass, err := processImageObjectInit(object, &tempBuffers)
	if err != nil {
		fmt.Printf("warning: skipping image object %s because processing initial passes failed: %s", object.Name, err)
		return
	}
	codegenData.Passes = append(codegenData.Passes, initialPass)

	for _, effect := range object.Effects {
		passes, err := processImageEffect(object, effect, &tempBuffers)
		if err != nil {
			fmt.Printf("warning: skipping image object %s effect %s because processing effect failed: %s\n", object.Name, effect.Name, err)
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

	switch object.Material.Blending {
	case "translucent":
		codegenData.Passes[lastIdx].BlendMode = "OW_BLEND_ALPHA"
	case "additive":
		codegenData.Passes[lastIdx].BlendMode = "OW_BLEND_ADD"
	case "normal", "disabled":
		codegenData.Passes[lastIdx].BlendMode = "OW_BLEND_NONE"
	}
	codegenData.Passes[lastIdx].ColorTarget = "0"
	codegenData.Passes[lastIdx].ColorTargetFormat = "OW_TEXTURE_SWAPCHAIN"
	codegenData.Passes[lastIdx].Transform.Enabled = true
}

func processImageObjectInit(object ImageObject, tempBuffers *[2]int) (CodegenPassData, error) {
	lastObjectID++
	lastPassID = 0

	textures := []ImportedTexture{}
	for _, textureName := range object.Material.Textures {
		texture, err := importTexture(textureName)
		if err != nil {
			return CodegenPassData{}, fmt.Errorf("loading texture %s failed: %s", textureName, err)
		}
		if texture.ID < 0 {
			continue
		}
		textures = append(textures, texture)
	}

	shader, err := compileShader(object.Material.Shader, []bool{}, object.Material.Combos)
	if err != nil {
		return CodegenPassData{}, fmt.Errorf("compiling shader %s failed: %s", object.Material.Shader, err)
	}

	passData := CodegenPassData{
		ObjectID:          lastObjectID,
		PassID:            0,
		ShaderID:          shader.ID,
		ColorTarget:       fmt.Sprintf("temp_buffers[%d]", tempBuffers[0]),
		ColorTargetFormat: "OW_TEXTURE_RGBA8_UNORM",
		ClearColor:        true,
		BlendMode:         "OW_BLEND_NONE",
	}

	resolutions := [][4]float32{}
	for slot, texture := range textures {
		passData.TextureBindings = append(passData.TextureBindings, CodegenTextureBindingData{
			Slot:    slot,
			Texture: fmt.Sprintf("texture%d", texture.ID),
			Sampler: getTextureSampler(texture),
		})
		resolutions = append(resolutions, [4]float32{float32(texture.Width), float32(texture.Height), float32(texture.Width), float32(texture.Height)})
	}

	passData.Transform = CodegenTransformData{
		Enabled:                false,
		SceneWidth:             float32(scene.General.Ortho.Width),
		SceneHeight:            float32(scene.General.Ortho.Height),
		OriginX:                float32(object.Origin[0]),
		OriginY:                float32(object.Origin[1]),
		OriginZ:                float32(object.Origin[2]),
		SizeX:                  float32(object.Size[0]),
		SizeY:                  float32(object.Size[1]),
		ScaleX:                 float32(object.Scale[0]),
		ScaleY:                 float32(object.Scale[1]),
		ScaleZ:                 float32(object.Scale[2]),
		ParallaxDepthX:         float32(object.ParallaxDepth[0]),
		ParallaxDepthY:         float32(object.ParallaxDepth[1]),
		ParallaxEnabled:        scene.General.Parallax,
		ParallaxAmount:         float32(scene.General.ParallaxAmount),
		ParallaxMouseInfluence: float32(scene.General.ParallaxMouseInfluence),
	}

	passData.UniformSetupCode += generateUniformSetupCode(UniformCodegenContext{
		StructName:  "vertex_uniforms",
		Uniforms:    shader.VertexUniforms,
		Constants:   map[string][]float32{},
		Resolutions: resolutions,
		Color:       [4]float32{object.Color[0], object.Color[1], object.Color[2], object.Alpha},
	})
	passData.UniformSetupCode += generateUniformSetupCode(UniformCodegenContext{
		StructName:  "fragment_uniforms",
		Uniforms:    shader.FragmentUniforms,
		Constants:   map[string][]float32{},
		Resolutions: resolutions,
		Color:       [4]float32{object.Color[0], object.Color[1], object.Color[2], object.Alpha},
	})
	return passData, nil
}

func processImageEffect(object ImageObject, effect ImageEffect, tempBuffers *[2]int) ([]CodegenPassData, error) {
	allBoundTextures := [][]bool{}
	for _, pass := range effect.Passes {
		boundTextures := []bool{}
		for _, textureName := range pass.Textures {
			boundTextures = append(boundTextures, textureName != "")
		}
		allBoundTextures = append(allBoundTextures, boundTextures)
	}

	fbos := map[string]int{}
	for idx, fbo := range effect.FBOs {
		fbos[fbo.Name] = getTempBuffer(TempBufferParameters{
			Width:  int(object.Size[0]/float32(fbo.Scale) + 0.5),
			Height: int(object.Size[1]/float32(fbo.Scale) + 0.5),
			Number: idx,
		})
	}

	passes := []CodegenPassData{}

	for idx, pass := range effect.Passes {
		lastPassID++

		passData := CodegenPassData{
			ObjectID:          lastObjectID,
			PassID:            lastPassID,
			ColorTargetFormat: "OW_TEXTURE_RGBA8_UNORM",
			ClearColor:        true,
			BlendMode:         "OW_BLEND_NONE",
		}

		compiledShader, err := compileShader(effect.Materials[idx].Shader, allBoundTextures[idx], pass.Combos)
		if err != nil {
			return []CodegenPassData{}, fmt.Errorf("compiling effect shader %s failed: %s", effect.Materials[idx].Shader, err)
		}
		passData.ShaderID = compiledShader.ID

		resolutions := [][4]float32{}
		textureNames := make([]string, len(compiledShader.Samplers))
		for idx, sampler := range compiledShader.Samplers {
			textureNames[idx] = sampler.Default
		}
		for _, binding := range pass.Bind {
			if binding.Name != "previous" {
				textureNames[binding.Index] = binding.Name
			}
		}
		for slot, texture := range pass.Textures {
			if texture != "" {
				textureNames[slot] = texture
			}
		}

		for slot, textureName := range textureNames {
			if textureName == "" || strings.HasPrefix(textureName, "_rt_imageLayerComposite") {
				passData.TextureBindings = append(passData.TextureBindings, CodegenTextureBindingData{
					Slot:    slot,
					Texture: fmt.Sprintf("temp_buffers[%d]", tempBuffers[0]),
					Sampler: "linear_clamp_sampler",
				})
				resolutions = append(resolutions, [4]float32{
					float32(object.Size[0] + 0.5),
					float32(object.Size[1] + 0.5),
					float32(object.Size[0] + 0.5),
					float32(object.Size[1] + 0.5),
				})
			} else if fbo, exists := fbos[textureName]; exists {
				passData.TextureBindings = append(passData.TextureBindings, CodegenTextureBindingData{
					Slot:    slot,
					Texture: fmt.Sprintf("temp_buffers[%d]", fbo),
					Sampler: "linear_clamp_sampler",
				})
				resolutions = append(resolutions, [4]float32{
					float32(object.Size[0] + 0.5),
					float32(object.Size[1] + 0.5),
					float32(object.Size[0] + 0.5),
					float32(object.Size[1] + 0.5),
				})
			} else {
				texture, err := importTexture(textureName)
				if err != nil {
					return []CodegenPassData{}, fmt.Errorf("importing texture %s failed: %s", textureName, err)
				}
				passData.TextureBindings = append(passData.TextureBindings, CodegenTextureBindingData{
					Slot:    slot,
					Texture: fmt.Sprintf("texture%d", texture.ID),
					Sampler: getTextureSampler(texture),
				})
				resolutions = append(resolutions, [4]float32{
					float32(texture.Width),
					float32(texture.Height),
					float32(texture.Width),
					float32(texture.Height),
				})
			}
		}

		colorTarget := fmt.Sprintf("temp_buffers[%d]", tempBuffers[1])
		if fbo, exists := fbos[pass.Target]; exists {
			colorTarget = fmt.Sprintf("temp_buffers[%d]", fbo)
		} else {
			tempBuffers[0], tempBuffers[1] = tempBuffers[1], tempBuffers[0]
		}
		passData.ColorTarget = colorTarget

		passData.Transform = CodegenTransformData{
			Enabled:                false,
			SceneWidth:             float32(scene.General.Ortho.Width),
			SceneHeight:            float32(scene.General.Ortho.Height),
			OriginX:                float32(object.Origin[0]),
			OriginY:                float32(object.Origin[1]),
			OriginZ:                float32(object.Origin[2]),
			SizeX:                  float32(object.Size[0]),
			SizeY:                  float32(object.Size[1]),
			ScaleX:                 float32(object.Scale[0]),
			ScaleY:                 float32(object.Scale[1]),
			ScaleZ:                 float32(object.Scale[2]),
			ParallaxDepthX:         float32(object.ParallaxDepth[0]),
			ParallaxDepthY:         float32(object.ParallaxDepth[1]),
			ParallaxEnabled:        scene.General.Parallax,
			ParallaxAmount:         float32(scene.General.ParallaxAmount),
			ParallaxMouseInfluence: float32(scene.General.ParallaxMouseInfluence),
		}

		passData.UniformSetupCode += generateUniformSetupCode(UniformCodegenContext{
			StructName:  "vertex_uniforms",
			Uniforms:    compiledShader.VertexUniforms,
			Constants:   pass.Constants,
			Resolutions: resolutions,
			Color:       [4]float32{object.Color[0], object.Color[1], object.Color[2], object.Alpha},
		})
		passData.UniformSetupCode += generateUniformSetupCode(UniformCodegenContext{
			StructName:  "fragment_uniforms",
			Uniforms:    compiledShader.FragmentUniforms,
			Constants:   pass.Constants,
			Resolutions: resolutions,
			Color:       [4]float32{object.Color[0], object.Color[1], object.Color[2], object.Alpha},
		})
		passes = append(passes, passData)
	}

	return passes, nil
}

func generateUniformSetupCode(ctx UniformCodegenContext) string {
	code := ""
	for _, uniform := range ctx.Uniforms {
		if uniform.Name == "g_ModelMatrix" {
			code += "        " + ctx.StructName + ".g_ModelMatrix = matrices.model;\n"
		} else if uniform.Name == "g_ViewProjectionMatrix" {
			code += "        " + ctx.StructName + ".g_ViewProjectionMatrix = matrices.view_projection;\n"
		} else if uniform.Name == "g_ModelViewProjectionMatrix" {
			code += "        " + ctx.StructName + ".g_ModelViewProjectionMatrix = matrices.model_view_projection;\n"
		} else if strings.HasPrefix(uniform.Name, "g_Texture") && strings.HasSuffix(uniform.Name, "Rotation") {
			code += "        " + ctx.StructName + "." + uniform.Name + " = (glsl_vec4){.at = {1, 0, 0, 1}};\n"
		} else if strings.HasPrefix(uniform.Name, "g_Texture") && strings.HasSuffix(uniform.Name, "Translation") {
			// TODO:
		} else if strings.HasPrefix(uniform.Name, "g_Texture") && strings.HasSuffix(uniform.Name, "Resolution") {
			idxString, _ := strings.CutPrefix(uniform.Name, "g_Texture")
			idxString, _ = strings.CutSuffix(idxString, "Resolution")
			idx, _ := strconv.Atoi(idxString)
			if idx < len(ctx.Resolutions) {
				code += fmt.Sprintf("        %s.%s = (glsl_vec4){.at = {%f, %f, %f, %f}};\n", ctx.StructName, uniform.Name,
					ctx.Resolutions[idx][0], ctx.Resolutions[idx][1], ctx.Resolutions[idx][2], ctx.Resolutions[idx][3])
			}
		} else if uniform.Name == "g_Time" {
			code += "        " + ctx.StructName + ".g_Time = (glsl_float){.at = {time}};\n"
		} else if uniform.Name == "g_ParallaxPosition" {
			code += "        " + ctx.StructName + ".g_ParallaxPosition = (glsl_vec2){.at = {matrices.parallax_position_x, matrices.parallax_position_y}};\n"
		} else if uniform.Name == "g_Screen" {
			code += "        " + ctx.StructName + ".g_Screen = (glsl_vec3){.at = {screen_width, screen_height, (float)screen_width / (float)screen_height}};\n"
		} else if uniform.Name == "g_EffectTextureProjectionMatrix" || uniform.Name == "g_EffectTextureProjectionMatrixInverse" {
			code += "        " + ctx.StructName + "." + uniform.Name + " = mat4_identity();\n"
		} else if uniform.Name == "g_EyePosition" {
			code += "        " + ctx.StructName + ".g_EyePosition = (glsl_vec3){.at = {0, 0, 0}};\n"
		} else if uniform.Name == "g_Color4" {
			code += fmt.Sprintf("        %s.g_Color4 = (glsl_vec4){.at = {%f, %f, %f, %f}};\n",
				ctx.StructName, ctx.Color[0], ctx.Color[1], ctx.Color[2], ctx.Color[3])
		} else if uniform.DefaultSet {
			code += fmt.Sprintf("        %s.%s = (glsl_%s){.at = {", ctx.StructName, uniform.Name, uniform.Type)
			value := uniform.Default
			if override, exists := ctx.Constants[uniform.ConstantName]; exists {
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

	converted, err := texToWebp(textureBytes)
	if err != nil {
		return ImportedTexture{}, err
	}

	textureMap[textureName] = ImportedTexture{
		ID:            len(textureMap),
		Name:          textureName,
		Width:         converted.Width,
		Height:        converted.Height,
		ClampUV:       converted.ClampUV,
		Interpolation: converted.Interpolation,
	}

	outputMap[fmt.Sprintf("assets/texture%d.webp", textureMap[textureName].ID)] = converted.Data
	codegenData.Textures = append(codegenData.Textures, textureMap[textureName])

	return textureMap[textureName], nil
}

func getTextureSampler(texture ImportedTexture) string {
	if texture.Interpolation {
		if texture.ClampUV {
			return "linear_clamp_sampler"
		} else {
			return "linear_repeat_sampler"
		}
	} else {
		if texture.ClampUV {
			return "nearest_clamp_sampler"
		} else {
			return "nearest_repeat_sampler"
		}
	}
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

	vertexUniformsStructCode := generateUniformsStructCode(transformed.VertexUniforms, fmt.Sprintf("shader%d_vertex_uniforms_t", lastShaderID))
	fragmentUniformsStructCode := generateUniformsStructCode(transformed.FragmentUniforms, fmt.Sprintf("shader%d_fragment_uniforms_t", lastShaderID))

	compiledShader := CompiledShader{
		ID:                     lastShaderID,
		NameInfo:               nameInfo,
		VertexUniforms:         transformed.VertexUniforms,
		FragmentUniforms:       transformed.FragmentUniforms,
		VertexUniformsStruct:   vertexUniformsStructCode,
		FragmentUniformsStruct: fragmentUniformsStructCode,
		Samplers:               transformed.Samplers,
	}

	codegenData.Shaders = append(codegenData.Shaders, compiledShader)
	return compiledShader, nil
}
