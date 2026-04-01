package main

import (
	"bytes"
	_ "embed"
	"fmt"
	"maps"
	"os"
	"os/exec"
	"runtime"
	"slices"
	"strings"
	"sync"
	"text/template"

	"github.com/alexflint/go-arg"
)

type ImportTextureTask struct {
	// in
	Name string
	ID   int

	// out
	Error               error
	Width               int
	Height              int
	Format              texFormat
	ClampUV             bool
	Interpolation       bool
	SpritesheetCols     int
	SpritesheetRows     int
	SpritesheetFrames   int
	SpritesheetDuration float32
}

type CompileShaderTask struct {
	// in
	Name          string
	Preprocess    bool
	Defines       map[string]int
	BoundTextures []bool
	ID            int

	// out
	Error            error
	VertexUniforms   []UniformInfo
	FragmentUniforms []UniformInfo
	Samplers         []SamplerInfo
}

var (
	env struct {
		Assets string
		WasmCC string
	}
	args struct {
		Input       string `arg:"positional,required"`
		Output      string `arg:"positional,required"`
		Project     string `arg:"--project"`
		Particles   bool   `arg:"--particles" default:"true"`
		KeepSources bool   `arg:"--keep-sources"`
	}
	state struct {
		PKGMap    map[string][]byte
		Scene     Scene
		Tasks     []any
		OutputMap map[string][]byte
		Mutex     sync.Mutex
	}
)

//go:embed module/main.c
var mainCode []byte

//go:embed module/scene.tmpl
var sceneTemplateCode []byte

//go:embed module/scene_utils.h
var sceneUtilsCode []byte

func main() {
	arg.MustParse(&args)
	env.Assets = os.Getenv("WPE_COMPILE_ASSETS")
	env.WasmCC = os.Getenv("WPE_COMPILE_WASM_CC")
	if env.Assets == "" {
		panic("WPE_COMPILE_ASSETS is not set")
	}
	if env.WasmCC == "" {
		panic("WPE_COMPILE_WASM_CC is not set")
	}

	pkgFile, err := os.ReadFile(args.Input)
	if err != nil {
		panic("open pkg failed: " + err.Error())
	}
	state.PKGMap, err = extractPkg(pkgFile)
	if err != nil {
		panic("extract pkg failed: " + err.Error())
	}

	state.OutputMap = map[string][]byte{}
	if args.Project != "" {
		makeMetadata(args.Project, &state.OutputMap)
	}

	state.Scene, err = ParseScene(&state.PKGMap)
	if err != nil {
		panic("parse scene.json failed: " + err.Error())
	}

	preprocessScene()
	fmt.Printf("\r\033[K[%d/%d] compiling scene module\n", len(state.Tasks), len(state.Tasks))

	sceneTemplate, err := template.New("scene.tmpl").Parse(string(sceneTemplateCode))
	if err != nil {
		panic("parsing scene template failed: " + err.Error())
	}
	sceneSourceBuffer := bytes.Buffer{}
	err = sceneTemplate.ExecuteTemplate(&sceneSourceBuffer, "scene", state.Scene)
	if err != nil {
		panic("executing scene template failed: " + err.Error())
	}
	sceneSource := sceneSourceBuffer.Bytes()

	if args.KeepSources {
		state.OutputMap["main.c"] = mainCode
		state.OutputMap["scene.h"] = sceneSource
	}

	tempDirBytes, err := exec.Command("mktemp", "-d").Output()
	if err != nil {
		panic("mktemp failed: " + err.Error())
	}
	tempDir := strings.TrimSuffix(string(tempDirBytes), "\n")

	files := map[string][]byte{
		"main.c":        mainCode,
		"scene.h":       sceneSource,
		"scene_utils.h": sceneUtilsCode,
	}

	for name, content := range files {
		err = os.WriteFile(tempDir+"/"+name, content, 0644)
		if err != nil {
			panic(fmt.Errorf("write %s failed: %s", name, err))
		}
	}

	compileArgs := []string{
		tempDir + "/main.c",
		"-o",
		tempDir + "/scene.wasm",
		"-I../include",
		"-O3",
		"-Wl,--allow-undefined",
		"-Wl,--max-memory=268435456",
		"-Wl,-z,stack-size=1048576",
		"-Wl,--export=malloc",
		"-Wl,--export=free",
		"-Wl,--export=__heap_base",
		"-Wl,--export=__data_end",
	}

	logBytes, err := exec.Command(env.WasmCC, compileArgs...).CombinedOutput()
	if err != nil {
		panic("compiling scene module failed: " + string(logBytes))
	}

	wasmBytes, err := os.ReadFile(tempDir + "/scene.wasm")
	if err != nil {
		panic("reading scene module failed: " + err.Error())
	}

	state.OutputMap["scene.wasm"] = wasmBytes

	zip, err := zipBytes(state.OutputMap)
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

func preprocessScene() {
	state.Tasks = []any{}
	for _, object := range state.Scene.Objects {
		if imageObject, ok := object.(*ImageObject); ok {
			processImageObject(imageObject)
		} else if particleObject, ok := object.(*ParticleObject); ok {
			processParticleObject(particleObject)
		}
	}
	executeTasks()
}

func processImageObject(object *ImageObject) {
	for _, texture := range object.Material.Textures {
		addImportTextureTask(&ImportTextureTask{Name: texture})
	}
	addCompileShaderTask(&CompileShaderTask{
		Name:          object.Material.Shader,
		Preprocess:    true,
		Defines:       object.Material.Combos,
		BoundTextures: []bool{},
	})

	for _, effect := range object.Effects {
		for idx, material := range effect.Materials {
			for _, texture := range material.Textures {
				addImportTextureTask(&ImportTextureTask{Name: texture})
			}
			addCompileShaderTask(&CompileShaderTask{
				Name:          material.Shader,
				Preprocess:    true,
				Defines:       material.Combos,
				BoundTextures: getEffectBoundTextures(material, effect.Passes[idx]),
			})
		}
	}
}

func getEffectBoundTextures(material Material, pass MaterialPass) []bool {
	boundTextures := make([]bool, len(material.Textures))

	for slot, textureName := range material.Textures {
		if textureName != "" {
			boundTextures[slot] = true
		}
	}
	for slot, textureName := range pass.Textures {
		boundTextures = ensureBoolSlots(boundTextures, slot+1)
		if textureName != "" {
			boundTextures[slot] = true
		}
	}
	for _, binding := range pass.Bind {
		if binding.Index < 0 {
			continue
		}
		boundTextures = ensureBoolSlots(boundTextures, binding.Index+1)
		if binding.Name != "" {
			boundTextures[binding.Index] = true
		}
	}

	return boundTextures
}

func ensureBoolSlots(slots []bool, slotCount int) []bool {
	if len(slots) >= slotCount {
		return slots
	}
	resized := make([]bool, slotCount)
	copy(resized, slots)
	return resized
}

func processParticleObject(object *ParticleObject) {
	for _, texture := range object.ParticleData.Material.Textures {
		addImportTextureTask(&ImportTextureTask{Name: texture})
	}
}

func addImportTextureTask(task *ImportTextureTask) {
	if task.Name == "" {
		return
	}
	if strings.HasPrefix(task.Name, "_rt_") {
		return
	}

	for _, anyTask2 := range state.Tasks {
		task2, ok := anyTask2.(*ImportTextureTask)
		if !ok {
			continue
		}
		if task2.Name == task.Name {
			return
		}
	}

	task.ID = len(state.Tasks)
	state.Tasks = append(state.Tasks, task)
}

func addCompileShaderTask(task *CompileShaderTask) {
	if task.Name == "" {
		return
	}

	for _, anyTask2 := range state.Tasks {
		task2, ok := anyTask2.(*CompileShaderTask)
		if !ok {
			continue
		}
		if task2.Name == task.Name &&
			task2.Preprocess == task.Preprocess &&
			maps.Equal(task2.Defines, task.Defines) &&
			slices.Equal(task2.BoundTextures, task.BoundTextures) {
			return
		}
	}

	task.ID = len(state.Tasks)
	state.Tasks = append(state.Tasks, task)
}

func executeTasks() {
	nextTaskIdx := 0
	finishedCount := 0
	descriptions := []string{}
	startTimes := []int{}
	mutex := sync.Mutex{}
	wait := []chan struct{}{}

	for threadIdx := range runtime.NumCPU() {
		finished := make(chan struct{})
		wait = append(wait, finished)
		descriptions = append(descriptions, "")
		startTimes = append(startTimes, 0)

		go func() {
			for {
				mutex.Lock()
				if nextTaskIdx >= len(state.Tasks) {
					mutex.Unlock()
					close(finished)
					return
				}
				taskIdx := nextTaskIdx
				nextTaskIdx++
				startTimes[threadIdx] = taskIdx
				anyTask := state.Tasks[taskIdx]

				if task, ok := anyTask.(*ImportTextureTask); ok {
					descriptions[threadIdx] = fmt.Sprintf("importing texture %s", task.Name)
					mutex.Unlock()

					importTexture(task)

					mutex.Lock()
					if task.Error != nil {
						fmt.Printf("\r\033[Kimport texture %s failed: %s\n", task.Name, task.Error)
					}
				} else if task, ok := anyTask.(*CompileShaderTask); ok {
					descriptions[threadIdx] = fmt.Sprintf("compiling shader %s", task.Name)
					mutex.Unlock()

					compileShader(task)

					mutex.Lock()
					if task.Error != nil {
						fmt.Printf("\r\033[Kcompile shader %s failed: %s\n", task.Name, task.Error)
					}
				}

				descriptions[threadIdx] = ""
				finishedCount++
				lastTask := 0
				for idx := range descriptions {
					if descriptions[idx] != "" && (descriptions[lastTask] == "" || startTimes[lastTask] < startTimes[idx]) {
						lastTask = idx
					}
				}
				fmt.Printf("\r\033[K[%d/%d] %s", finishedCount, len(state.Tasks), descriptions[lastTask])
				mutex.Unlock()
			}
		}()
	}

	for _, finished := range wait {
		<-finished
	}
	fmt.Printf("\r\033[K")
}

func importTexture(task *ImportTextureTask) {
	texturePath := "materials/" + task.Name + ".tex"
	textureBytes, err := getAssetBytes(texturePath)
	if err != nil {
		task.Error = err
		return
	}

	metadataPath := "materials/" + task.Name + ".tex-json"
	metadataBytes, _ := getAssetBytes(metadataPath)

	converted, err := texToWebp(textureBytes, metadataBytes)
	if err != nil {
		task.Error = err
		return
	}

	task.Width = converted.Width
	task.Height = converted.Height
	task.Format = converted.Format
	task.ClampUV = converted.ClampUV
	task.Interpolation = converted.Interpolation
	task.SpritesheetCols = converted.SpritesheetCols
	task.SpritesheetRows = converted.SpritesheetRows
	task.SpritesheetFrames = converted.SpritesheetFrames
	task.SpritesheetDuration = converted.SpritesheetDuration

	state.Mutex.Lock()
	state.OutputMap[fmt.Sprintf("textures/%d.webp", task.ID)] = converted.Data
	state.Mutex.Unlock()
}

func compileShader(task *CompileShaderTask) {
	vertexShaderPath := "shaders/" + task.Name + ".vert"
	fragmentShaderPath := "shaders/" + task.Name + ".frag"

	vertexShaderBytes, err := getAssetBytes(vertexShaderPath)
	if err != nil {
		task.Error = err
		return
	}
	fragmentShaderBytes, err := getAssetBytes(fragmentShaderPath)
	if err != nil {
		task.Error = err
		return
	}

	transformed := PreprocessedShader{
		VertexGLSL:   string(vertexShaderBytes),
		FragmentGLSL: string(fragmentShaderBytes),
	}
	if task.Preprocess {
		transformed, err = preprocessShader(string(vertexShaderBytes), string(fragmentShaderBytes), "assets/shaders", task.BoundTextures, task.Defines)
		if err != nil {
			task.Error = err
			return
		}
	}

	if args.KeepSources {
		state.Mutex.Lock()
		state.OutputMap[fmt.Sprintf("shaders/%d_vertex.glsl", task.ID)] = []byte(transformed.VertexGLSL)
		state.OutputMap[fmt.Sprintf("shaders/%d_fragment.glsl", task.ID)] = []byte(transformed.FragmentGLSL)
		state.Mutex.Unlock()
	}

	glslcArgs := []string{"-fshader-stage=vertex"}
	for name, value := range task.Defines {
		glslcArgs = append(glslcArgs, fmt.Sprintf("-D%s=%d", name, value))
	}
	vertexSPIRVBytes, err := compileRawShader([]byte(transformed.VertexGLSL), glslcArgs)
	if err != nil {
		task.Error = fmt.Errorf("compiling vertex shader failed: %s", err)
		return
	}

	glslcArgs = []string{"-fshader-stage=fragment"}
	for name, value := range task.Defines {
		glslcArgs = append(glslcArgs, fmt.Sprintf("-D%s=%d", name, value))
	}
	fragmentSPIRVBytes, err := compileRawShader([]byte(transformed.FragmentGLSL), glslcArgs)
	if err != nil {
		task.Error = fmt.Errorf("compiling fragment shader failed: %s", err)
		return
	}

	state.Mutex.Lock()
	state.OutputMap[fmt.Sprintf("shaders/%d_vertex.spv", task.ID)] = vertexSPIRVBytes
	state.OutputMap[fmt.Sprintf("shaders/%d_fragment.spv", task.ID)] = fragmentSPIRVBytes
	state.Mutex.Unlock()

	task.VertexUniforms = transformed.VertexUniforms
	task.FragmentUniforms = transformed.FragmentUniforms
	task.Samplers = transformed.Samplers
}

func compileRawShader(source []byte, glslcArgs []string) ([]byte, error) {
	tempDirBytes, err := exec.Command("mktemp", "-d").Output()
	if err != nil {
		return []byte{}, fmt.Errorf("mktemp failed: %s", err)
	}
	tempDir := strings.TrimSuffix(string(tempDirBytes), "\n")

	err = os.WriteFile(tempDir+"/shader.glsl", source, 0644)
	if err != nil {
		return []byte{}, fmt.Errorf("write shader.glsl failed: %s", err)
	}

	glslcArgs = append(glslcArgs, tempDir+"/shader.glsl", "-o", tempDir+"/shader.spv")
	logBytes, err := exec.Command("glslc", glslcArgs...).CombinedOutput()
	if err != nil {
		return []byte{}, fmt.Errorf("glslc failed:\n%s", logBytes)
	}

	SPIRVBytes, err := os.ReadFile(tempDir + "/shader.spv")
	if err != nil {
		return []byte{}, fmt.Errorf("read shader.spv failed: %s", err)
	}

	err = os.RemoveAll(tempDir)
	if err != nil {
		println("warning: failed to remove temp dir: " + err.Error())
	}

	return SPIRVBytes, nil
}

func getAssetBytes(path string) ([]byte, error) {
	if bytes, exists := state.PKGMap[path]; exists {
		return bytes, nil
	}
	asset, err := os.ReadFile(env.Assets + "/" + path)
	if err == nil {
		return asset, nil
	}
	return nil, fmt.Errorf("open asset %s failed: %s", path, err)
}
