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
	"strconv"
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
		Output      string `arg:"positional"`
		Project     string `arg:"--project"`
		Particles   bool   `arg:"--particles" default:"true"`
		KeepSources bool   `arg:"--keep-sources"`
		ListObjects bool   `arg:"--list-objects"`
		SkipObjects string `arg:"--skip-objects"`
		SkipEffects string `arg:"--skip-effects"`
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

//go:embed module/renderer.c
var rendererCode []byte

//go:embed module/common.c
var commonCode []byte

//go:embed module/uniform.c
var uniformCode []byte

//go:embed module/image.c
var imageCode []byte

//go:embed module/transform.c
var transformCode []byte

//go:embed module/scene_data.c
var sceneDataCode []byte

//go:embed module/defs.h
var defsCode []byte

//go:embed module/scene.tmpl
var sceneTemplateCode []byte

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

	if args.ListObjects {
		printObjectList()
		return
	}
	if args.SkipEffects != "" {
		if err := applySkipEffects(args.SkipEffects); err != nil {
			panic("invalid --skip-effects: " + err.Error())
		}
	}
	if args.SkipObjects != "" {
		if err := applySkipObjects(args.SkipObjects); err != nil {
			panic("invalid --skip-objects: " + err.Error())
		}
	}
	if args.Output == "" {
		panic("output path is required")
	}

	preprocessScene()
	fmt.Printf("\r\033[K[%d/%d] compiling scene module\n", len(state.Tasks), len(state.Tasks))

	sceneTemplate, err := template.New("scene.tmpl").Parse(string(sceneTemplateCode))
	if err != nil {
		panic("parsing scene template failed: " + err.Error())
	}
	sceneCodeBuffer := bytes.Buffer{}
	err = sceneTemplate.ExecuteTemplate(&sceneCodeBuffer, "scene", state.Scene)
	if err != nil {
		panic("executing scene template failed: " + err.Error())
	}
	sceneCode := sceneCodeBuffer.Bytes()

	if args.KeepSources {
		state.OutputMap["scene.h"] = sceneCode
	}

	tempDirBytes, err := exec.Command("mktemp", "-d").Output()
	if err != nil {
		panic("mktemp failed: " + err.Error())
	}
	tempDir := strings.TrimSuffix(string(tempDirBytes), "\n")

	files := map[string][]byte{
		"main.c":       mainCode,
		"renderer.c":   rendererCode,
		"common.c":     commonCode,
		"uniform.c":    uniformCode,
		"image.c":      imageCode,
		"transform.c":  transformCode,
		"scene_data.c": sceneDataCode,
		"defs.h":       defsCode,
		"scene.h":      sceneCode,
	}

	for name, content := range files {
		err = os.WriteFile(tempDir+"/"+name, content, 0644)
		if err != nil {
			panic(fmt.Errorf("write %s failed: %s", name, err))
		}
	}

	compileArgs := []string{
		tempDir + "/main.c",
		tempDir + "/renderer.c",
		tempDir + "/common.c",
		tempDir + "/uniform.c",
		tempDir + "/image.c",
		tempDir + "/transform.c",
		tempDir + "/scene_data.c",
		"-o",
		tempDir + "/scene.wasm",
		"-DSCENE",
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
	state.Scene.Types = []int{}
	state.Scene.Shaders = nil
	state.Scene.Textures = nil
	state.Scene.PassthroughShader = -1

	needsPassthroughShader := false
	for _, object := range state.Scene.Objects {
		if imageObject, ok := object.(*ImageObject); ok {
			processImageObject(imageObject)
			needsPassthroughShader = true
			state.Scene.Types = append(state.Scene.Types, 0)
		} else if _, ok := object.(*ParticleObject); ok {
			state.Scene.Types = append(state.Scene.Types, 1)
		} else {
			state.Scene.Types = append(state.Scene.Types, 2)
		}
	}

	if needsPassthroughShader {
		state.Scene.PassthroughShader = addCompileShaderTask(&CompileShaderTask{
			Name:          "passthrough",
			Preprocess:    true,
			Defines:       map[string]int{"TRANSFORM": 1},
			BoundTextures: []bool{true},
		})
	}

	executeTasksFrom(0)
	firstTaskCount := len(state.Tasks)
	fmt.Printf("\r\033[K[%d/%d] collecting additional textures", firstTaskCount, firstTaskCount)
	addSamplerDefaultTextureTasks()
	if len(state.Tasks) > firstTaskCount {
		executeTasksFrom(firstTaskCount)
	}
	collectSceneTaskResults()
}

type objectInfo struct {
	index    int
	id       int
	parent   int
	name     string
	typeName string
}

func sceneObjectInfo(index int, object SceneObject) objectInfo {
	switch object := object.(type) {
	case *ImageObject:
		return objectInfo{
			index:    index,
			id:       object.ID,
			parent:   object.Parent,
			name:     object.Name,
			typeName: "image",
		}
	case *ParticleObject:
		return objectInfo{
			index:    index,
			id:       object.ID,
			parent:   object.Parent,
			name:     object.Name,
			typeName: "particle",
		}
	case *EmptyObject:
		return objectInfo{
			index:    index,
			id:       object.ID,
			parent:   object.Parent,
			name:     "",
			typeName: "empty",
		}
	default:
		return objectInfo{
			index:    index,
			parent:   -1,
			typeName: "unknown",
		}
	}
}

func printObjectList() {
	for objectIndex, object := range state.Scene.Objects {
		info := sceneObjectInfo(objectIndex, object)
		parent := ""
		if info.parent != -1 {
			parent = fmt.Sprintf(" parent=%d", info.parent)
		}
		extra := ""
		if imageObject, ok := object.(*ImageObject); ok {
			extra = fmt.Sprintf(" effects=%d", len(imageObject.Effects))
		}
		fmt.Printf("object %d id=%d%s type=%s name=%q%s\n",
			info.index, info.id, parent, info.typeName, info.name, extra)
		if imageObject, ok := object.(*ImageObject); ok {
			for effectIndex, effect := range imageObject.Effects {
				fmt.Printf("  effect %d: %s (%d passes)\n", effectIndex, effectDisplayName(effect), len(effect.Passes))
			}
		}
	}
}

func effectDisplayName(effect ImageEffect) string {
	effectPath := strings.TrimSpace(effect.Path)
	if effectPath != "" {
		effectPath = strings.TrimSuffix(effectPath, "/effect.json")
		effectPath = strings.TrimSuffix(effectPath, "/")
		if slash := strings.LastIndexByte(effectPath, '/'); slash >= 0 {
			effectPath = effectPath[slash+1:]
		}
		if effectPath != "" {
			return effectPath
		}
	}
	return effect.Name
}

func applySkipObjects(spec string) error {
	selectors, err := parseIndexRanges(spec, func(r rune) bool {
		return r == ',' || r == ';' || r == '\n'
	})
	if err != nil {
		return err
	}

	skippedIndexes := map[int]bool{}
	skippedIDs := map[int]bool{}
	for objectIndex, object := range state.Scene.Objects {
		info := sceneObjectInfo(objectIndex, object)
		if indexRangesMatch(selectors, objectIndex) {
			skippedIndexes[objectIndex] = true
			skippedIDs[info.id] = true
		}
	}

	for changed := true; changed; {
		changed = false
		for objectIndex, object := range state.Scene.Objects {
			if skippedIndexes[objectIndex] {
				continue
			}
			info := sceneObjectInfo(objectIndex, object)
			if info.parent >= 0 && skippedIDs[info.parent] {
				skippedIndexes[objectIndex] = true
				skippedIDs[info.id] = true
				changed = true
			}
		}
	}

	filteredObjects := state.Scene.Objects[:0]
	for objectIndex, object := range state.Scene.Objects {
		if skippedIndexes[objectIndex] {
			continue
		}
		filteredObjects = append(filteredObjects, object)
	}
	state.Scene.Objects = filteredObjects
	return nil
}

type indexRange struct {
	start int
	end   int
}

type effectSkipRule struct {
	objectRanges []indexRange
	effectRanges []indexRange
}

func applySkipEffects(spec string) error {
	rules, err := parseEffectSkipRules(spec)
	if err != nil {
		return err
	}
	for objectIndex, object := range state.Scene.Objects {
		imageObject, ok := object.(*ImageObject)
		if !ok {
			continue
		}
		filteredEffects := imageObject.Effects[:0]
		for effectIndex, effect := range imageObject.Effects {
			skip := false
			for _, rule := range rules {
				if effectRuleMatches(rule, objectIndex, effectIndex) {
					skip = true
					break
				}
			}
			if skip {
				continue
			}
			filteredEffects = append(filteredEffects, effect)
		}
		imageObject.Effects = filteredEffects
	}
	return nil
}

func parseEffectSkipRules(spec string) ([]effectSkipRule, error) {
	clauses := strings.FieldsFunc(spec, func(r rune) bool {
		return r == ';' || r == '\n'
	})
	rules := []effectSkipRule{}
	for _, clause := range clauses {
		clause = strings.TrimSpace(clause)
		if clause == "" {
			continue
		}
		objectSpec, effectSpec, found := strings.Cut(clause, ":")
		if !found {
			return nil, fmt.Errorf("rule %q must use object-index:effect-indices", clause)
		}
		objectRanges, err := parseIndexRanges(objectSpec, func(r rune) bool {
			return r == ','
		})
		if err != nil {
			return nil, fmt.Errorf("rule %q has invalid object indices: %w", clause, err)
		}
		effectRanges, err := parseIndexRanges(effectSpec, func(r rune) bool {
			return r == ','
		})
		if err != nil {
			return nil, fmt.Errorf("rule %q has invalid effect indices: %w", clause, err)
		}
		rules = append(rules, effectSkipRule{
			objectRanges: objectRanges,
			effectRanges: effectRanges,
		})
	}
	if len(rules) == 0 {
		return nil, fmt.Errorf("empty effect skip spec")
	}
	return rules, nil
}

func effectRuleMatches(rule effectSkipRule, objectIndex int, effectIndex int) bool {
	return indexRangesMatch(rule.objectRanges, objectIndex) && indexRangesMatch(rule.effectRanges, effectIndex)
}

func parseIndexRanges(spec string, split func(rune) bool) ([]indexRange, error) {
	parts := strings.FieldsFunc(spec, split)
	ranges := []indexRange{}
	for _, part := range parts {
		part = strings.TrimSpace(part)
		if part == "" {
			continue
		}
		indexRange, err := parseIndexRange(part)
		if err != nil {
			return nil, err
		}
		ranges = append(ranges, indexRange)
	}
	if len(ranges) == 0 {
		return nil, fmt.Errorf("empty index list")
	}
	return ranges, nil
}

func parseIndexRange(spec string) (indexRange, error) {
	spec = strings.TrimSpace(spec)
	if before, after, found := strings.Cut(spec, "-"); found {
		start, startErr := strconv.Atoi(strings.TrimSpace(before))
		end, endErr := strconv.Atoi(strings.TrimSpace(after))
		if startErr != nil || endErr != nil || start < 0 || end < 0 {
			return indexRange{}, fmt.Errorf("invalid index range %q", spec)
		}
		if start > end {
			start, end = end, start
		}
		return indexRange{start: start, end: end}, nil
	}
	index, err := strconv.Atoi(spec)
	if err != nil || index < 0 {
		return indexRange{}, fmt.Errorf("invalid index %q", spec)
	}
	return indexRange{start: index, end: index}, nil
}

func indexRangesMatch(ranges []indexRange, index int) bool {
	for _, indexRange := range ranges {
		if index >= indexRange.start && index <= indexRange.end {
			return true
		}
	}
	return false
}

func processImageObject(object *ImageObject) {
	object.Material.ImportedTextures = make([]int, len(object.Material.Textures))
	for idx := range object.Material.Textures {
		object.Material.ImportedTextures[idx] = addImportTextureTask(&ImportTextureTask{Name: object.Material.Textures[idx]})
	}
	object.Material.CompiledShader = addCompileShaderTask(&CompileShaderTask{
		Name:          object.Material.Shader,
		Preprocess:    true,
		Defines:       object.Material.Combos,
		BoundTextures: []bool{},
	})

	if object.ColorBlendMode != 0 {
		effectPassthrough, err := makeEffectPassthrough(object.ColorBlendMode)
		if err != nil {
			fmt.Printf("warning: skipping image object %s colorBlendMode because creating effectpassthrough failed: %s\n", object.Name, err)
		} else {
			object.Effects = append(object.Effects, effectPassthrough)
		}
	}
	for effectIdx := range object.Effects {
		effect := &object.Effects[effectIdx]
		for materialIdx := range effect.Materials {
			material := &effect.Materials[materialIdx]
			var pass *MaterialPass
			if materialIdx < len(effect.Passes) {
				pass = &effect.Passes[materialIdx]
			}
			processEffectMaterial(material, pass)
		}
	}
}

func makeEffectPassthrough(colorBlendMode int) (ImageEffect, error) {
	materialBytes, err := getAssetBytes("materials/util/effectpassthrough.json")
	if err != nil {
		return ImageEffect{}, fmt.Errorf("loading effectpassthrough material failed: %w", err)
	}

	var material Material
	if err := material.parseFromJSON(materialBytes); err != nil {
		return ImageEffect{}, fmt.Errorf("parsing effectpassthrough material failed: %w", err)
	}

	if material.Combos == nil {
		material.Combos = map[string]int{}
	}
	material.Combos["BONECOUNT"] = 1
	material.Combos["BLENDMODE"] = colorBlendMode
	material.Blending = "disabled"

	pass := MaterialPass{
		Combos: map[string]int{
			"BONECOUNT": 1,
			"BLENDMODE": colorBlendMode,
		},
	}

	return ImageEffect{
		Name:      "effectpassthrough",
		Passes:    []MaterialPass{pass},
		Materials: []Material{material},
	}, nil
}

func processEffectMaterial(material *Material, pass *MaterialPass) {
	material.ImportedTextures = make([]int, len(material.Textures))
	for idx := range material.Textures {
		material.ImportedTextures[idx] = addImportTextureTask(&ImportTextureTask{Name: material.Textures[idx]})
	}

	defines := material.Combos
	boundTextures := make([]bool, len(material.Textures))
	for slot, textureName := range material.Textures {
		if textureName != "" {
			boundTextures[slot] = true
		}
	}
	if pass != nil {
		pass.ImportedTextures = make([]int, len(pass.Textures))
		for idx := range pass.Textures {
			pass.ImportedTextures[idx] = addImportTextureTask(&ImportTextureTask{Name: pass.Textures[idx]})
		}
		defines = pass.Combos
		boundTextures = getEffectBoundTextures(material, pass)
	}

	material.CompiledShader = addCompileShaderTask(&CompileShaderTask{
		Name:          material.Shader,
		Preprocess:    true,
		Defines:       defines,
		BoundTextures: boundTextures,
	})
}

func collectSceneTaskResults() {
	reportedCompileTasks := skipFailedEffects()
	state.Scene.Textures = nil
	state.Scene.Shaders = nil
	for _, anyTask := range state.Tasks {
		switch task := anyTask.(type) {
		case *ImportTextureTask:
			if task.Error != nil {
				fmt.Printf("warning: skipping texture %s: %s\n", task.Name, task.Error)
				continue
			}
			state.Scene.Textures = append(state.Scene.Textures, *task)
		case *CompileShaderTask:
			if task.Error != nil {
				if reportedCompileTasks[task.ID] {
					continue
				}
				fmt.Printf("warning: skipping shader %s: %s\n", task.Name, task.Error)
				continue
			}
			state.Scene.Shaders = append(state.Scene.Shaders, *task)
		}
	}
}

func skipFailedEffects() map[int]bool {
	reportedCompileTasks := map[int]bool{}
	for _, object := range state.Scene.Objects {
		imageObject, ok := object.(*ImageObject)
		if !ok {
			continue
		}

		filteredEffects := imageObject.Effects[:0]
		for _, effect := range imageObject.Effects {
			failedTasks := failedEffectShaderTasks(&effect)
			if len(failedTasks) > 0 {
				for _, failedTask := range failedTasks {
					if !reportedCompileTasks[failedTask.ID] {
						fmt.Printf("warning: skipping effect %s shader %s: %s\n",
							effectDisplayName(effect), failedTask.Name, failedTask.Error)
						reportedCompileTasks[failedTask.ID] = true
					}
				}
				continue
			}
			filteredEffects = append(filteredEffects, effect)
		}
		imageObject.Effects = filteredEffects
	}
	return reportedCompileTasks
}

func failedEffectShaderTasks(effect *ImageEffect) []*CompileShaderTask {
	failedTasks := []*CompileShaderTask{}
	seenTaskIDs := map[int]bool{}
	for materialIdx := range effect.Materials {
		material := &effect.Materials[materialIdx]
		if material.CompiledShader < 0 || material.CompiledShader >= len(state.Tasks) {
			continue
		}
		task, ok := state.Tasks[material.CompiledShader].(*CompileShaderTask)
		if ok && task.Error != nil && !seenTaskIDs[task.ID] {
			failedTasks = append(failedTasks, task)
			seenTaskIDs[task.ID] = true
		}
	}
	return failedTasks
}

func addSamplerDefaultTextureTasks() {
	for _, anyTask := range state.Tasks {
		task, ok := anyTask.(*CompileShaderTask)
		if !ok || task.Error != nil {
			continue
		}
		for _, sampler := range task.Samplers {
			addImportTextureTask(&ImportTextureTask{Name: sampler.Default})
		}
	}
}

func getEffectBoundTextures(material *Material, pass *MaterialPass) []bool {
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

func addImportTextureTask(task *ImportTextureTask) int {
	if task.Name == "" {
		return -1
	}
	if task.Name == "previous" || strings.HasPrefix(task.Name, "_rt_") || strings.HasPrefix(task.Name, "_alias_") {
		return -1
	}

	for _, anyTask2 := range state.Tasks {
		task2, ok := anyTask2.(*ImportTextureTask)
		if !ok {
			continue
		}
		if task2.Name == task.Name {
			return task2.ID
		}
	}

	task.ID = len(state.Tasks)
	state.Tasks = append(state.Tasks, task)
	return task.ID
}

func addCompileShaderTask(task *CompileShaderTask) int {
	if task.Name == "" {
		return -1
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
			return task2.ID
		}
	}

	task.ID = len(state.Tasks)
	state.Tasks = append(state.Tasks, task)
	return task.ID
}

func executeTasksFrom(startTaskIdx int) {
	nextTaskIdx := startTaskIdx
	finishedCount := 0
	totalCount := len(state.Tasks)
	if startTaskIdx >= totalCount {
		return
	}
	descriptions := []string{}
	startTimes := []int{}
	mutex := sync.Mutex{}
	wg := sync.WaitGroup{}

	for threadIdx := range runtime.NumCPU() {
		wg.Add(1)
		descriptions = append(descriptions, "")
		startTimes = append(startTimes, 0)

		go func() {
			for {
				mutex.Lock()
				if nextTaskIdx >= totalCount {
					mutex.Unlock()
					wg.Done()
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
				}

				descriptions[threadIdx] = ""
				finishedCount++
				lastTask := 0
				for idx := range descriptions {
					if descriptions[idx] != "" && (descriptions[lastTask] == "" || startTimes[lastTask] < startTimes[idx]) {
						lastTask = idx
					}
				}
				fmt.Printf("\r\033[K[%d/%d] %s", startTaskIdx+finishedCount, len(state.Tasks), descriptions[lastTask])
				mutex.Unlock()
			}
		}()
	}

	wg.Wait()
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
		task.Error = fmt.Errorf("compile vertex shader failed: %s", err)
		return
	}

	glslcArgs = []string{"-fshader-stage=fragment"}
	for name, value := range task.Defines {
		glslcArgs = append(glslcArgs, fmt.Sprintf("-D%s=%d", name, value))
	}
	fragmentSPIRVBytes, err := compileRawShader([]byte(transformed.FragmentGLSL), glslcArgs)
	if err != nil {
		task.Error = fmt.Errorf("compile fragment shader failed: %s", err)
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
