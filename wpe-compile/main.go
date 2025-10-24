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

type EffectPassMeta struct {
	Shader    CompiledShader
	Constants map[string][]float32
	Textures  []ImportedTexture
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

ow_id screen_buffers[3];

float time = 0.0f;

`

	initSource += `    ow_begin_copy_pass();

    vertex_quad_buffer = ow_create_buffer(OW_BUFFER_VERTEX, sizeof(vertex_quad));
    ow_update_buffer(vertex_quad_buffer, 0, vertex_quad, sizeof(vertex_quad));

    {
        uint32_t width, height;
        ow_get_screen_size(&width, &height);

        ow_texture_info screen_texture_info = {
            .width = width,
            .height = height,
            .format = OW_TEXTURE_RGBA8_UNORM,
            .render_target = true,
            .mip_levels = 1,
        };

        screen_buffers[1] = ow_create_texture(&screen_texture_info);
        screen_buffers[2] = ow_create_texture(&screen_texture_info);
    }
`

	updateSource += "    time += delta;\n"

	glslHeaders = readGLSLHeaders()
	lastObjectID := -1

	for objectIdx, object := range scene.Objects {
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

		shader, err := compileShader(material.Shader, []bool{}, material.Combos)
		if err != nil {
			fmt.Printf("warning: skipping material %s because compiling shader failed: %s\n", materialPath, err)
			continue
		}

		{
			headerSource += fmt.Sprintf("ow_id object%d_image_sampler;\n", lastObjectID)

			initSource += fmt.Sprintf(`
    {
        ow_sampler_info sampler_info = {
            .min_filter = OW_FILTER_LINEAR,
            .mag_filter = OW_FILTER_LINEAR,
            .mip_filter = OW_FILTER_LINEAR,
            .wrap_x = OW_WRAP_MIRROR,
            .wrap_y = OW_WRAP_MIRROR,
        };

        object%d_image_sampler = ow_create_sampler(&sampler_info);
    }

`, lastObjectID)

			headerSource += fmt.Sprintf("ow_id object%d_image_pipeline;\n", lastObjectID)

			colorTargetFormat := "OW_TEXTURE_SWAPCHAIN"
			if len(object.Effects) >= 1 {
				colorTargetFormat = "OW_TEXTURE_RGBA8_UNORM"
			}

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
            .color_target_format = %s,
            .vertex_shader = shader%d_vertex,
            .fragment_shader = shader%d_fragment,
            .blend_mode = OW_BLEND_ALPHA,
            .topology = OW_TOPOLOGY_TRIANGLES_STRIP,
        };

        object%d_image_pipeline = ow_create_pipeline(&pipeline_info);
    }

`, colorTargetFormat, shader.ID, shader.ID, lastObjectID)

			colorTarget := 0
			if len(object.Effects) >= 1 {
				colorTarget = 1
			}

			clearColor := "true"
			if colorTarget == 0 && objectIdx != 0 {
				clearColor = "false"
			}

			updateSource += fmt.Sprintf(`
    {
        ow_pass_info pass_info = {0};
		pass_info.color_target = screen_buffers[%d];
		pass_info.clear_color = %s;
        ow_begin_render_pass(&pass_info);

        ow_texture_binding texture_bindings[%d];
`, colorTarget, clearColor, len(textures))

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

			updateSource += fmt.Sprintf(`        shader%d_vertex_uniforms_t vertex_uniforms = {0};
        shader%d_fragment_uniforms_t fragment_uniforms = {0};

`, shader.ID, shader.ID)
		}

		for _, uniform := range shader.VertexUniforms {
			if uniform.Name == "g_ModelViewProjectionMatrix" {
				updateSource += `        vertex_uniforms.g_ModelViewProjectionMatrix = (glsl_mat4){ .at = {
            {1, 0, 0, 0},
            {0, 1, 0, 0},
            {0, 0, 1, 0},
            {0, 0, 0, 1},
        }};
`
			} else if uniform.Name == "g_Texture0Rotation" {
				updateSource += "        vertex_uniforms.g_Texture0Rotation = (glsl_vec4){.x = 1.0f, .y = 0.0f, .z = 0.0f, .w = 1.0f};\n"
			} else {
				updateSource += fmt.Sprintf("        vertex_uniforms.%s = (glsl_%s){.at = {", uniform.Name, uniform.Type)
				for _, value := range uniform.Default {
					updateSource += fmt.Sprintf("%f, ", value)
				}
				updateSource += "}};\n"
			}
		}

		for _, uniform := range shader.FragmentUniforms {
			updateSource += fmt.Sprintf("        fragment_uniforms.%s = (glsl_%s){.at = {", uniform.Name, uniform.Type)
			for _, value := range uniform.Default {
				updateSource += fmt.Sprintf("%f, ", value)
			}
			updateSource += "}};\n"
		}

		updateSource += fmt.Sprintf(`        ow_push_uniform_data(OW_SHADER_VERTEX, 0, &vertex_uniforms, sizeof(vertex_uniforms));
        ow_push_uniform_data(OW_SHADER_FRAGMENT, 0, &fragment_uniforms, sizeof(fragment_uniforms));

        ow_render_geometry(object%d_image_pipeline, &bindings_info, 0, 4, 1);
        ow_end_render_pass();
    }

`, lastObjectID)

		lastEffectPassID := -1
		lastColorTarget := 1

		for effectIdx, effectInstance := range object.Effects {
			ok = true
			effect, err := parseEffect(pkgMap[effectInstance.File])
			if err != nil {
				fmt.Printf("warning: skipping effect %s because parsing effect JSON %s failed: %s\n", effectInstance.Name, effectInstance.File, err)
				continue
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
					fmt.Printf("warning: skipping effect %s because parsing effect material JSON %s failed: %s\n", effectInstance.Name, pass.Material, err)
					ok = false
					break
				}
				meta[idx].Shader, err = compileShader(effectMaterial.Shader, allBoundTextures[idx], effectMaterial.Combos)
				if err != nil {
					fmt.Printf("warning: skipping material %s because compiling effect shader %s failed: %s\n", materialPath, effectMaterial.Shader, err)
					ok = false
					break
				}
			}
			if !ok {
				continue
			}

			for idx, pass := range effectInstance.Passes {
				for _, textureName := range pass.Textures {
					if textureName == nil {
						meta[idx].Textures = append(meta[idx].Textures, ImportedTexture{ID: -1})
						continue
					}
					texture, err := importTexture(*textureName)
					if err != nil {
						fmt.Printf("warning: skipping material %s because loading texture failed: %s\n", materialPath, err)
						ok = false
						break
					}
					meta[idx].Textures = append(meta[idx].Textures, texture)
				}
				for len(meta[idx].Textures) < len(meta[idx].Shader.Samplers) {
					meta[idx].Textures = append(meta[idx].Textures, ImportedTexture{ID: -1})
				}
				meta[idx].Constants = pass.Constants
			}
			if !ok {
				continue
			}

			for idx, pass := range meta {
				lastEffectPassID++
				passPrefix := fmt.Sprintf("object%d_effect_pass%d", lastObjectID, lastEffectPassID)
				headerSource += fmt.Sprintf("ow_id %s_sampler;\n", passPrefix)

				initSource += fmt.Sprintf(`
    {
        ow_sampler_info sampler_info = {
            .min_filter = OW_FILTER_LINEAR,
            .mag_filter = OW_FILTER_LINEAR,
            .mip_filter = OW_FILTER_LINEAR,
            .wrap_x = OW_WRAP_MIRROR,
            .wrap_y = OW_WRAP_MIRROR,
        };

        %s_sampler = ow_create_sampler(&sampler_info);
    }

`, passPrefix)

				headerSource += fmt.Sprintf("ow_id %s_pipeline;\n", passPrefix)

				colorTargetFormat := "OW_TEXTURE_SWAPCHAIN"
				if idx != len(meta)-1 || effectIdx != len(object.Effects)-1 {
					colorTargetFormat = "OW_TEXTURE_RGBA8_UNORM"
				}

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
            .color_target_format = %s,
            .vertex_shader = shader%d_vertex,
            .fragment_shader = shader%d_fragment,
            .blend_mode = OW_BLEND_ALPHA,
            .topology = OW_TOPOLOGY_TRIANGLES_STRIP,
        };

        %s_pipeline = ow_create_pipeline(&pipeline_info);
    }

`, colorTargetFormat, pass.Shader.ID, pass.Shader.ID, passPrefix)

				colorTarget := 0
				if idx != len(meta)-1 || effectIdx != len(object.Effects)-1 {
					if lastColorTarget == 0 {
						colorTarget = 1
					} else {
						colorTarget = 3 - lastColorTarget
					}
				}

				clearColor := "true"
				if colorTarget == 0 && objectIdx != 0 {
					clearColor = "false"
				}

				updateSource += fmt.Sprintf(`
    {
        ow_pass_info pass_info = {0};
		pass_info.color_target = screen_buffers[%d];
		pass_info.clear_color = %s;
        ow_begin_render_pass(&pass_info);

        ow_texture_binding texture_bindings[%d];
`, colorTarget, clearColor, len(pass.Textures))

				for i, texture := range pass.Textures {
					if texture.ID == -1 {
						updateSource += fmt.Sprintf(`        texture_bindings[%d] = (ow_texture_binding){
            .slot = %d,
            .texture = screen_buffers[%d],
            .sampler = %s_sampler,
        };
`, i, i, lastColorTarget, passPrefix)
					} else {
						updateSource += fmt.Sprintf(`        texture_bindings[%d] = (ow_texture_binding){
            .slot = %d,
            .texture = texture%d,
            .sampler = %s_sampler,
        };
`, i, i, texture.ID, passPrefix)
					}
				}

				updateSource += fmt.Sprintf(`
        ow_bindings_info bindings_info = {
            .vertex_buffers = &vertex_quad_buffer,
            .vertex_buffers_count = 1,
            .texture_bindings = texture_bindings,
            .texture_bindings_count = %d
        };

`, len(pass.Textures))

				updateSource += fmt.Sprintf(`        shader%d_vertex_uniforms_t vertex_uniforms = {0};
        shader%d_fragment_uniforms_t fragment_uniforms = {0};

`, pass.Shader.ID, pass.Shader.ID)

				for _, uniform := range pass.Shader.VertexUniforms {
					if uniform.Name == "g_ModelViewProjectionMatrix" {
						updateSource += `        vertex_uniforms.g_ModelViewProjectionMatrix = (glsl_mat4){ .at = {
            {1, 0, 0, 0},
            {0, 1, 0, 0},
            {0, 0, 1, 0},
            {0, 0, 0, 1},
        }};
`
					} else if uniform.Name == "g_Texture0Rotation" {
						updateSource += "        vertex_uniforms.g_Texture0Rotation = (glsl_vec4){.x = 1.0f, .y = 0.0f, .z = 0.0f, .w = 1.0f};\n"
					} else if strings.HasSuffix(uniform.Name, "Resolution") {
						updateSource += fmt.Sprintf("        vertex_uniforms.%s = (glsl_vec4){.x = 1920, .y = 1080, .z = 1920, .w = 1080};\n", uniform.Name)
					} else if uniform.Name == "g_Time" {
						updateSource += "        vertex_uniforms.g_Time = (glsl_float){.x = time};\n"
					} else {
						updateSource += fmt.Sprintf("        vertex_uniforms.%s = (glsl_%s){.at = {", uniform.Name, uniform.Type)
						value := uniform.Default
						constantName, _ := strings.CutPrefix(uniform.Name, "g_")
						constantName = strings.ToLower(constantName)
						if override, exists := pass.Constants[constantName]; exists {
							value = override
						}
						for _, value := range value {
							updateSource += fmt.Sprintf("%f, ", value)
						}
						updateSource += "}};\n"
					}
				}

				for _, uniform := range pass.Shader.FragmentUniforms {
					if uniform.Name == "g_Time" {
						updateSource += "        fragment_uniforms.g_Time = (glsl_float){.x = time};\n"
					} else {
						updateSource += fmt.Sprintf("        fragment_uniforms.%s = (glsl_%s){.at = {", uniform.Name, uniform.Type)
						value := uniform.Default
						constantName, _ := strings.CutPrefix(uniform.Name, "g_")
						constantName = strings.ToLower(constantName)
						if override, exists := pass.Constants[constantName]; exists {
							value = override
						}
						for _, value := range value {
							updateSource += fmt.Sprintf("%f, ", value)
						}
						updateSource += "}};\n"
					}
				}

				updateSource += fmt.Sprintf(`        ow_push_uniform_data(OW_SHADER_VERTEX, 0, &vertex_uniforms, sizeof(vertex_uniforms));
        ow_push_uniform_data(OW_SHADER_FRAGMENT, 0, &fragment_uniforms, sizeof(fragment_uniforms));

        ow_render_geometry(%s_pipeline, &bindings_info, 0, 4, 1);
        ow_end_render_pass();
    }

`, passPrefix)

				lastColorTarget = colorTarget
			}
		}

		if !ok {
			continue
		}
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
