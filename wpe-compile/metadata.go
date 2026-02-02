package main

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"image"
	"image/draw"
	_ "image/gif"
	_ "image/jpeg"
	"math"
	"os"
	"path/filepath"

	xdraw "golang.org/x/image/draw"

	"github.com/chai2010/webp"
)

const (
	previewMaxWidth  = 1920
	previewMaxHeight = 1080
	previewQuality   = 98
)

type Project struct {
	Title       string `json:"title"`
	Description string `json:"description"`
	Preview     string `json:"preview"`
}

func makeMetadata(projectPath string, outputMap *map[string][]byte) {
	projectBytes, err := os.ReadFile(projectPath)
	if err != nil {
		fmt.Printf("warning: failed to load project JSON: %s\n", err)
		return
	}

	project := Project{}
	err = json.Unmarshal(projectBytes, &project)
	if err != nil {
		fmt.Printf("warning: failed to parse project JSON: %s\n", err)
		return
	}

	previewPath := filepath.Join(filepath.Dir(projectPath), project.Preview)
	previewBytes, err := os.ReadFile(previewPath)
	if err != nil {
		fmt.Printf("warning: failed to load preview image: %s\n", err)
		return
	}

	previewWEBP, err := makePreviewWEBP(previewBytes)
	if err != nil {
		fmt.Printf("warning: failed to make preview WEBP: %s\n", err)
		return
	}

	(*outputMap)["preview.webp"] = previewWEBP
	(*outputMap)["metadata.toml"] = fmt.Appendf(nil, `[info]
name = """%s"""
description = """%s"""
preview = "preview.webp"
`, project.Title, project.Description)
}

func makePreviewWEBP(imageBytes []byte) ([]byte, error) {
	if len(imageBytes) == 0 {
		return nil, errors.New("image bytes are empty")
	}

	img, format, err := image.Decode(bytes.NewReader(imageBytes))
	if err != nil {
		return nil, fmt.Errorf("decode preview image failed: %w", err)
	}
	switch format {
	case "jpeg", "gif":
	default:
		return nil, fmt.Errorf("unsupported image format: %s", format)
	}

	bounds := img.Bounds()
	srcWidth := bounds.Dx()
	srcHeight := bounds.Dy()
	if srcWidth <= 0 || srcHeight <= 0 {
		return nil, errors.New("invalid image dimensions")
	}

	cropWidth := srcWidth
	cropHeight := srcHeight
	if srcWidth*9 > srcHeight*16 {
		cropWidth = srcHeight * 16 / 9
	} else if srcWidth*9 < srcHeight*16 {
		cropHeight = srcWidth * 9 / 16
	}
	if cropWidth <= 0 || cropHeight <= 0 {
		return nil, errors.New("invalid crop size")
	}

	offsetX := (srcWidth - cropWidth) / 2
	offsetY := (srcHeight - cropHeight) / 2
	cropRect := image.Rect(0, 0, cropWidth, cropHeight)
	cropped := image.NewRGBA(cropRect)
	draw.Draw(cropped, cropRect, img, image.Point{X: bounds.Min.X + offsetX, Y: bounds.Min.Y + offsetY}, draw.Src)

	outWidth := cropWidth
	outHeight := cropHeight
	if outWidth > previewMaxWidth || outHeight > previewMaxHeight {
		scale := math.Min(float64(previewMaxWidth)/float64(outWidth), float64(previewMaxHeight)/float64(outHeight))
		outWidth = int(math.Round(float64(outWidth) * scale))
		outHeight = int(math.Round(float64(outHeight) * scale))
		if outWidth < 1 {
			outWidth = 1
		}
		if outHeight < 1 {
			outHeight = 1
		}
	}

	if outWidth != cropWidth || outHeight != cropHeight {
		resized := image.NewRGBA(image.Rect(0, 0, outWidth, outHeight))
		xdraw.CatmullRom.Scale(resized, resized.Bounds(), cropped, cropped.Bounds(), xdraw.Over, nil)
		cropped = resized
	}

	var buffer bytes.Buffer
	if err := webp.Encode(&buffer, cropped, &webp.Options{Lossless: false, Quality: previewQuality}); err != nil {
		return nil, fmt.Errorf("encode preview webp failed: %w", err)
	}

	return buffer.Bytes(), nil
}
