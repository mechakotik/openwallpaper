package main

import (
	"fmt"
	"io/fs"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strings"

	"github.com/alexflint/go-arg"
)

type CompiledShader struct {
	ID               int
	VertexUniforms   []UniformInfo
	FragmentUniforms []UniformInfo
	Samplers         []string
}

type ImportedTexture struct {
	ID          int
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
	glslHeaders  map[string]string
	lastShaderID = -1

	headerSource string
	initSource   string
	updateSource string
)

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

	headerSource += `typedef struct {
    float position[3];
    float texcoord[2];
} vertex_t;

vertex_t vertex_quad[4] = {
    { {-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f} },
    { { 1.0f, -1.0f, 0.0f}, {1.0f, 1.0f} },
    { {-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f} },
    { { 1.0f,  1.0f, 0.0f}, {1.0f, 0.0f} },
};

ow_vertex_attribute vertex_attributes[2] = {
    { .slot = 0, .location = 0, .type = OW_ATTRIBUTE_FLOAT3, .offset = 0 },
    { .slot = 0, .location = 1, .type = OW_ATTRIBUTE_FLOAT2, .offset = sizeof(float) * 3 },
};

ow_id vertex_quad_buffer;

ow_texture_info texture_info = {
    .mip_levels = 1,
    .format = OW_TEXTURE_RGBA8_UNORM,
};

`

	initSource += `    ow_begin_copy_pass();

    vertex_quad_buffer = ow_create_buffer(OW_BUFFER_VERTEX, sizeof(vertex_quad));
    ow_update_buffer(vertex_quad_buffer, 0, vertex_quad, sizeof(vertex_quad));

`

	glslHeaders = readGLSLHeaders()
	lastObjectID := -1

	for _, object := range scene.Objects {
		lastObjectID++
		if object.Image == "" {
			continue
		}
		materialPath := replaceByRegex(object.Image, "models/(.*)", "materials/$1")
		materialBytes, err := getAssetBytes(materialPath)
		if err != nil {
			fmt.Printf("warning: skipping material %s because loading material file failed: %s\n", materialPath, err)
			continue
		}

		material, err := parseMaterialJSON(materialBytes)
		if err != nil {
			fmt.Printf("warning: skipping material %s because parsing material JSON failed: %s\n", materialPath, err)
			continue
		}

		ok := true
		textures := []ImportedTexture{}
		for _, textureName := range material.Textures {
			texture, err := importTexture(textureName)
			if err != nil {
				fmt.Printf("warning: skipping material %s because loading texture failed: %s\n", materialPath, err)
				ok = false
				break
			}
			if texture.ID < 0 {
				continue
			}
			textures = append(textures, texture)
		}

		if !ok || len(textures) == 0 {
			continue
		}

		shader, err := compileShader(material.Shader, material.Combos)
		if err != nil {
			fmt.Printf("warning: skipping material %s because compiling shader failed: %s\n", materialPath, err)
			continue
		}

		headerSource += fmt.Sprintf("ow_id object%d_image_sampler;\n", lastObjectID)

		initSource += fmt.Sprintf(`
    {
        ow_sampler_info sampler_info = {
            .min_filter = OW_FILTER_LINEAR,
            .mag_filter = OW_FILTER_LINEAR,
            .mip_filter = OW_FILTER_LINEAR,
            .wrap_x = OW_WRAP_REPEAT,
            .wrap_y = OW_WRAP_REPEAT,
        };

        object%d_image_sampler = ow_create_sampler(&sampler_info);
    }

`, lastObjectID)

		headerSource += fmt.Sprintf("ow_id object%d_image_pipeline;\n", lastObjectID)

		initSource += fmt.Sprintf(`
    {
        ow_vertex_binding_info vertex_binding_info = {
            .slot = 0,
            .stride = sizeof(vertex_t),
        };

        ow_pipeline_info pipeline_info = {
            .vertex_bindings = &vertex_binding_info,
            .vertex_bindings_count = 1,
            .vertex_attributes = vertex_attributes,
            .vertex_attributes_count = 2,
            .vertex_shader = shader%d_vertex,
            .fragment_shader = shader%d_fragment,
            .blend_mode = OW_BLEND_ALPHA,
            .topology = OW_TOPOLOGY_TRIANGLES_STRIP,
        };

        object%d_image_pipeline = ow_create_pipeline(&pipeline_info);
    }

`, shader.ID, shader.ID, lastObjectID)

		updateSource += fmt.Sprintf(`
    {
        ow_pass_info pass_info = {0};
        ow_begin_render_pass(&pass_info);

        ow_texture_binding texture_bindings[%d];
`, len(textures))

		for i, texture := range textures {
			updateSource += fmt.Sprintf(`        texture_bindings[%d] = (ow_texture_binding){
            .slot = %d,
            .texture = texture%d,
            .sampler = object%d_image_sampler,
        };
`, i, i, texture.ID, lastObjectID)
		}

		updateSource += fmt.Sprintf(`
        ow_bindings_info bindings_info = {
            .vertex_buffers = &vertex_quad_buffer,
            .vertex_buffers_count = 1,
            .texture_bindings = texture_bindings,
            .texture_bindings_count = %d
        };

`, len(textures))

		/*updateSource += fmt.Sprintf(`        shader%d_vertex_uniforms_t vertex_uniforms = {0};
		        shader%d_fragment_uniforms_t fragment_uniforms = {0};

		`, shader.ID, shader.ID)*/

		updateSource += `shader0_vertex_uniforms_t vertex_uniforms = {
    .g_ModelViewProjectionMatrix = (glsl_mat4){ .at = {
        {1, 0, 0, 0},
        {0, 1, 0, 0},
        {0, 0, 1, 0},
        {0, 0, 0, 1},
    }},
    .g_Texture0Rotation = (glsl_vec4){.x = 1.0f, .y = 0.0f, .z = 0.0f, .w = 1.0f},
    .g_Texture0Translation = (glsl_vec2){.x = 0.0f, .y = 0.0f},
};
`

		updateSource += `shader0_fragment_uniforms_t fragment_uniforms = {
    .g_Brightness = (glsl_float){.x = 1.0f},
    .g_UserAlpha  = (glsl_float){.x = 1.0f},
};
`

		updateSource += fmt.Sprintf(`        ow_push_uniform_data(OW_SHADER_VERTEX, 0, &vertex_uniforms, sizeof(vertex_uniforms));
        ow_push_uniform_data(OW_SHADER_FRAGMENT, 0, &fragment_uniforms, sizeof(fragment_uniforms));

        ow_render_geometry(object%d_image_pipeline, &bindings_info, 0, 4, 1);
        ow_end_render_pass();
    }

`, lastObjectID)

		/*for _, effect := range object.Effects {
			for _, pass := range effect.Passes {
				for _, textureName := range pass.Textures {
					if textureName == nil {
						continue
					}
					err := addTexture(*textureName)
					if err != nil {
						fmt.Printf("warning: skipping material %s because loading texture failed: %s\n", materialPath, err)
						ok = false
						break
					}
				}
			}
		}

		if !ok {
			continue
		}*/
	}

	initSource += "    ow_end_copy_pass();\n"

	sceneSource := fmt.Sprintf(`// autogenerated by wpe-compile

#include "openwallpaper.h"
#include "openwallpaper_std140.h"

%s
__attribute__((export_name("init"))) void init() {
%s}

__attribute__((export_name("update"))) void update(float delta) {
%s}
`, headerSource, initSource, updateSource)

	if args.KeepSources {
		outputMap["scene.c"] = []byte(sceneSource)
	}

	println("compiling scene module")

	tempDirBytes, err := exec.Command("mktemp", "-d").Output()
	if err != nil {
		panic("mktemp failed: " + err.Error())
	}
	tempDir := strings.TrimSuffix(string(tempDirBytes), "\n")
	defer os.RemoveAll(tempDir)

	os.WriteFile(tempDir+"/scene.c", []byte(sceneSource), 0644)

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
	if textureName == "_rt_imageLayerComposite_id_a" {
		return ImportedTexture{ID: -1}, nil
	}
	if textureName == "_rt_imageLayerComposite_id_b" {
		return ImportedTexture{ID: -2}, nil
	}
	if textureName == "_rt_FullFrameBuffer" {
		return ImportedTexture{ID: -3}, nil
	}
	if textureName == "_rt_FullCompoBuffer1_fullscreen_id" {
		return ImportedTexture{ID: -4}, nil
	}
	if textureName == "_rt_FullCompoBuffer1_id_id" {
		return ImportedTexture{ID: -5}, nil
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
		Width:       converted.Width,
		Height:      converted.Height,
		PixelFormat: converted.PixelFormat,
	}

	outputMap[fmt.Sprintf("assets/texture%d.webp", textureMap[textureName].ID)] = converted.Data

	headerSource += fmt.Sprintf("ow_id texture%d;\n", textureMap[textureName].ID)
	initSource += fmt.Sprintf("    texture%d = ow_create_texture_from_png(\"assets/texture%d.webp\", &texture_info);\n", textureMap[textureName].ID, textureMap[textureName].ID)

	return textureMap[textureName], nil
}

func readGLSLHeaders() map[string]string {
	headerPaths := []string{}

	err := filepath.WalkDir("assets/shaders", func(path string, d fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if !d.IsDir() && strings.HasSuffix(strings.ToLower(d.Name()), ".h") {
			headerPaths = append(headerPaths, path)
		}
		return nil
	})
	if err != nil {
		println("warning: failed to find GLSL headers: " + err.Error())
		return map[string]string{}
	}

	res := map[string]string{}
	for _, headerPath := range headerPaths {
		headerBytes, err := os.ReadFile(headerPath)
		if err != nil {
			println("warning: failed to read GLSL header: " + err.Error())
			continue
		}
		shortPath, _ := strings.CutPrefix(headerPath, "assets/shaders/")
		res[shortPath] = string(headerBytes)
	}

	return res
}

func compileShader(shaderName string, defines map[string]int) (CompiledShader, error) {
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
	if definesInfo == "" {
		fmt.Printf("compiling shader %s\n", shaderName)
	} else {
		fmt.Printf("compiling shader %s (%s)\n", shaderName, definesInfo)
	}

	vertexShaderBytes, err := getAssetBytes(vertexShaderPath)
	if err != nil {
		return CompiledShader{}, err
	}
	fragmentShaderBytes, err := getAssetBytes(fragmentShaderPath)
	if err != nil {
		return CompiledShader{}, err
	}

	transformed, err := preprocessShader(string(vertexShaderBytes), string(fragmentShaderBytes), "assets/shaders", defines)
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
	defer os.RemoveAll(tempDir)

	os.WriteFile(tempDir+"/vertex.glsl", []byte(transformed.VertexGLSL), 0644)
	os.WriteFile(tempDir+"/fragment.glsl", []byte(transformed.FragmentGLSL), 0644)

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

	outputMap[fmt.Sprintf("assets/shader%d_vertex.spv", lastShaderID)] = vertexSPIRVBytes
	outputMap[fmt.Sprintf("assets/shader%d_fragment.spv", lastShaderID)] = fragmentSPIRVBytes

	headerSource += "\ntypedef struct {\n"
	for _, uniform := range transformed.VertexUniforms {
		headerSource += fmt.Sprintf("    glsl_%s %s;\n", uniform.Type, uniform.Name)
	}
	headerSource += fmt.Sprintf("} shader%d_vertex_uniforms_t;\n", lastShaderID)

	headerSource += "\ntypedef struct {\n"
	for _, uniform := range transformed.FragmentUniforms {
		headerSource += fmt.Sprintf("    glsl_%s %s;\n", uniform.Type, uniform.Name)
	}
	headerSource += fmt.Sprintf("} shader%d_fragment_uniforms_t;\n\n", lastShaderID)

	headerSource += fmt.Sprintf("ow_id shader%d_vertex;\n", lastShaderID)
	headerSource += fmt.Sprintf("ow_id shader%d_fragment;\n", lastShaderID)

	initSource += fmt.Sprintf("    shader%d_vertex = ow_create_shader_from_file(\"assets/shader%d_vertex.spv\", OW_SHADER_VERTEX);\n", lastShaderID, lastShaderID)
	initSource += fmt.Sprintf("    shader%d_fragment = ow_create_shader_from_file(\"assets/shader%d_fragment.spv\", OW_SHADER_FRAGMENT);\n", lastShaderID, lastShaderID)

	return CompiledShader{
		ID:               lastShaderID,
		VertexUniforms:   transformed.VertexUniforms,
		FragmentUniforms: transformed.FragmentUniforms,
		Samplers:         transformed.Samplers,
	}, nil
}

func replaceByRegex(source, pattern, replace string) string {
	regex := regexp.MustCompile(pattern)
	return regex.ReplaceAllString(source, replace)
}
