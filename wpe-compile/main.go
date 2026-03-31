package main

import (
	"bytes"
	_ "embed"
	"fmt"
	"os"
	"os/exec"
	"strings"
	"text/template"

	"github.com/alexflint/go-arg"
)

//go:embed module/main.c
var mainCode []byte

//go:embed module/scene.tmpl
var sceneTemplateCode []byte

//go:embed module/scene_utils.h
var sceneUtilsCode []byte

var env struct {
	Assets string
	WasmCC string
}

var args struct {
	Input       string `arg:"positional,required"`
	Output      string `arg:"positional,required"`
	Project     string `arg:"--project"`
	Particles   bool   `arg:"--particles" default:"true"`
	KeepSources bool   `arg:"--keep-sources"`
}

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
	pkgMap, err := extractPkg(pkgFile)
	if err != nil {
		panic("extract pkg failed: " + err.Error())
	}

	outputMap := map[string][]byte{}
	if args.Project != "" {
		makeMetadata(args.Project, &outputMap)
	}

	scene, err := ParseScene(&pkgMap)
	if err != nil {
		panic("parse scene.json failed: " + err.Error())
	}

	sceneTemplate, err := template.New("scene.tmpl").Parse(string(sceneTemplateCode))
	if err != nil {
		panic("parsing scene template failed: " + err.Error())
	}
	sceneSourceBuffer := bytes.Buffer{}
	err = sceneTemplate.ExecuteTemplate(&sceneSourceBuffer, "scene", scene)
	if err != nil {
		panic("executing scene template failed: " + err.Error())
	}
	sceneSource := sceneSourceBuffer.Bytes()

	if args.KeepSources {
		outputMap["main.c"] = mainCode
		outputMap["scene.h"] = sceneSource
	}

	println("compiling scene module")

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
