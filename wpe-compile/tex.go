package main

import (
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
	"image"
	"image/draw"
	_ "image/gif"
	_ "image/jpeg"
	_ "image/png"
	"strconv"

	"github.com/chai2010/webp"
	"github.com/pierrec/lz4/v4"
)

type WebpResult struct {
	Data          []byte
	Width         int
	Height        int
	ClampUV       bool
	Interpolation bool
}

func texToWebp(texBytes []byte) (WebpResult, error) {
	reader := bytes.NewReader(texBytes)

	magic1, err := readCString(reader, 16)
	if err != nil {
		return WebpResult{}, fmt.Errorf("read TEX magic1 failed: %w", err)
	}
	if magic1 != "TEXV0005" {
		return WebpResult{}, fmt.Errorf("unsupported TEX magic1: %q", magic1)
	}

	magic2, err := readCString(reader, 16)
	if err != nil {
		return WebpResult{}, fmt.Errorf("read TEX magic2 failed: %w", err)
	}
	if magic2 != "TEXI0001" {
		return WebpResult{}, fmt.Errorf("unsupported TEX magic2: %q", magic2)
	}

	header, err := readTexHeader(reader)
	if err != nil {
		return WebpResult{}, fmt.Errorf("read TEX header failed: %w", err)
	}

	containerMagic, err := readCString(reader, 16)
	if err != nil {
		return WebpResult{}, fmt.Errorf("read TEXB magic failed: %w", err)
	}

	imageCount, err := readInt32(reader)
	if err != nil {
		return WebpResult{}, fmt.Errorf("read imageCount failed: %w", err)
	}
	if imageCount <= 0 {
		return WebpResult{}, errors.New("tex file has no images")
	}

	imageFormat, containerVersion, err := readImageContainerHeader(reader, containerMagic)
	if err != nil {
		return WebpResult{}, fmt.Errorf("read image container header failed: %w", err)
	}

	if header.Flags&texFlagIsVideoTexture != 0 || imageFormat == freeImageMp4 {
		return WebpResult{}, errors.New("video textures (MP4) are not supported by texToWebp")
	}

	mipmapCount, err := readInt32(reader)
	if err != nil {
		return WebpResult{}, fmt.Errorf("read mipmapCount failed: %w", err)
	}
	if mipmapCount <= 0 {
		return WebpResult{}, errors.New("first image has no mipmaps")
	}

	firstMipmap, err := readMipmap(reader, containerVersion)
	if err != nil {
		return WebpResult{}, fmt.Errorf("read first mipmap failed: %w", err)
	}

	rgbaPixels, effectiveWidth, effectiveHeight, err := decodeMipmapToRGBA(firstMipmap, header, header.Format, imageFormat)
	if err != nil {
		return WebpResult{}, fmt.Errorf("decode mipmap failed: %w", err)
	}

	webpBytes, err := encodeRGBAtoWebP(rgbaPixels, effectiveWidth, effectiveHeight)
	if err != nil {
		return WebpResult{}, fmt.Errorf("encode webp failed: %w", err)
	}

	result := WebpResult{
		Data:          webpBytes,
		Width:         firstMipmap.Width,
		Height:        firstMipmap.Height,
		ClampUV:       header.Flags&texFlagClampUVs != 0,
		Interpolation: header.Flags&texFlagNoInterpolation == 0,
	}

	return result, nil
}

type texFormat int32

const (
	texFormatRGBA8888 texFormat = 0
	texFormatDXT5     texFormat = 4
	texFormatDXT3     texFormat = 6
	texFormatDXT1     texFormat = 7
	texFormatRG88     texFormat = 8
	texFormatR8       texFormat = 9
)

type texFlags uint32

const (
	texFlagNoInterpolation texFlags = 1 << 0
	texFlagClampUVs        texFlags = 1 << 1
	texFlagIsGif           texFlags = 1 << 2
	texFlagIsVideoTexture  texFlags = 1 << 5
)

type texHeader struct {
	Format        texFormat
	Flags         texFlags
	TextureWidth  int
	TextureHeight int
	ImageWidth    int
	ImageHeight   int
	UnkInt0       uint32
}

type texImageContainerVersion int

const (
	texContainerV1 texImageContainerVersion = 1
	texContainerV2 texImageContainerVersion = 2
	texContainerV3 texImageContainerVersion = 3
	texContainerV4 texImageContainerVersion = 4
)

type freeImageFormat int32

const (
	freeImageUnknown freeImageFormat = -1
	freeImageMp4     freeImageFormat = 35
)

type texMipmap struct {
	Width            int
	Height           int
	IsLZ4Compressed  bool
	DecompressedSize int
	Data             []byte
}

func readTexHeader(r *bytes.Reader) (texHeader, error) {
	var header texHeader

	format, err := readInt32(r)
	if err != nil {
		return header, err
	}
	flags, err := readUint32(r)
	if err != nil {
		return header, err
	}
	textureWidth, err := readInt32(r)
	if err != nil {
		return header, err
	}
	textureHeight, err := readInt32(r)
	if err != nil {
		return header, err
	}
	imageWidth, err := readInt32(r)
	if err != nil {
		return header, err
	}
	imageHeight, err := readInt32(r)
	if err != nil {
		return header, err
	}
	unk, err := readUint32(r)
	if err != nil {
		return header, err
	}

	header.Format = texFormat(format)
	header.Flags = texFlags(flags)
	header.TextureWidth = int(textureWidth)
	header.TextureHeight = int(textureHeight)
	header.ImageWidth = int(imageWidth)
	header.ImageHeight = int(imageHeight)
	header.UnkInt0 = unk

	return header, nil
}

func readImageContainerHeader(r *bytes.Reader, magic string) (freeImageFormat, texImageContainerVersion, error) {
	imageFormat := freeImageUnknown

	switch magic {
	case "TEXB0001", "TEXB0002":
	case "TEXB0003":
		formatRaw, err := readInt32(r)
		if err != nil {
			return freeImageUnknown, 0, err
		}
		imageFormat = freeImageFormat(formatRaw)
	case "TEXB0004":
		formatRaw, err := readInt32(r)
		if err != nil {
			return freeImageUnknown, 0, err
		}
		isVideoMp4Int, err := readInt32(r)
		if err != nil {
			return freeImageUnknown, 0, err
		}
		imageFormat = freeImageFormat(formatRaw)
		if imageFormat == freeImageUnknown && isVideoMp4Int == 1 {
			imageFormat = freeImageMp4
		}
	default:
		return freeImageUnknown, 0, fmt.Errorf("unknown TEXB magic: %q", magic)
	}

	versionNumber, err := strconv.Atoi(magic[4:])
	if err != nil {
		return freeImageUnknown, 0, fmt.Errorf("invalid TEXB version in magic %q: %w", magic, err)
	}
	containerVersion := texImageContainerVersion(versionNumber)

	if containerVersion == texContainerV4 && imageFormat != freeImageMp4 {
		containerVersion = texContainerV3
	}

	return imageFormat, containerVersion, nil
}

func readMipmap(r *bytes.Reader, version texImageContainerVersion) (texMipmap, error) {
	switch version {
	case texContainerV1:
		return readMipmapV1(r)
	case texContainerV2, texContainerV3:
		return readMipmapV2V3(r)
	case texContainerV4:
		return readMipmapV4(r)
	default:
		return texMipmap{}, fmt.Errorf("unsupported image container version: %d", version)
	}
}

func readMipmapV1(r *bytes.Reader) (texMipmap, error) {
	width, err := readInt32(r)
	if err != nil {
		return texMipmap{}, err
	}
	height, err := readInt32(r)
	if err != nil {
		return texMipmap{}, err
	}
	size, err := readInt32(r)
	if err != nil {
		return texMipmap{}, err
	}
	if size < 0 {
		return texMipmap{}, fmt.Errorf("negative mipmap size v1: %d", size)
	}

	data, err := readBytes(r, int(size))
	if err != nil {
		return texMipmap{}, err
	}

	return texMipmap{
		Width:            int(width),
		Height:           int(height),
		IsLZ4Compressed:  false,
		DecompressedSize: 0,
		Data:             data,
	}, nil
}

func readMipmapV2V3(r *bytes.Reader) (texMipmap, error) {
	width, err := readInt32(r)
	if err != nil {
		return texMipmap{}, err
	}
	height, err := readInt32(r)
	if err != nil {
		return texMipmap{}, err
	}
	lz4Flag, err := readInt32(r)
	if err != nil {
		return texMipmap{}, err
	}
	decompressedSize, err := readInt32(r)
	if err != nil {
		return texMipmap{}, err
	}
	size, err := readInt32(r)
	if err != nil {
		return texMipmap{}, err
	}
	if size < 0 {
		return texMipmap{}, fmt.Errorf("negative mipmap size v2/v3: %d", size)
	}

	data, err := readBytes(r, int(size))
	if err != nil {
		return texMipmap{}, err
	}

	return texMipmap{
		Width:            int(width),
		Height:           int(height),
		IsLZ4Compressed:  lz4Flag == 1,
		DecompressedSize: int(decompressedSize),
		Data:             data,
	}, nil
}

func readMipmapV4(r *bytes.Reader) (texMipmap, error) {
	param1, err := readInt32(r)
	if err != nil {
		return texMipmap{}, err
	}
	if param1 != 1 {
		return texMipmap{}, fmt.Errorf("unexpected param1 in mipmap v4: %d", param1)
	}
	param2, err := readInt32(r)
	if err != nil {
		return texMipmap{}, err
	}
	if param2 != 2 {
		return texMipmap{}, fmt.Errorf("unexpected param2 in mipmap v4: %d", param2)
	}

	_, err = readCString(r, 0)
	if err != nil {
		return texMipmap{}, fmt.Errorf("read v4 condition json failed: %w", err)
	}

	param3, err := readInt32(r)
	if err != nil {
		return texMipmap{}, err
	}
	if param3 != 1 {
		return texMipmap{}, fmt.Errorf("unexpected param3 in mipmap v4: %d", param3)
	}

	width, err := readInt32(r)
	if err != nil {
		return texMipmap{}, err
	}
	height, err := readInt32(r)
	if err != nil {
		return texMipmap{}, err
	}
	lz4Flag, err := readInt32(r)
	if err != nil {
		return texMipmap{}, err
	}
	decompressedSize, err := readInt32(r)
	if err != nil {
		return texMipmap{}, err
	}
	size, err := readInt32(r)
	if err != nil {
		return texMipmap{}, err
	}
	if size < 0 {
		return texMipmap{}, fmt.Errorf("negative mipmap size v4: %d", size)
	}

	data, err := readBytes(r, int(size))
	if err != nil {
		return texMipmap{}, err
	}

	return texMipmap{
		Width:            int(width),
		Height:           int(height),
		IsLZ4Compressed:  lz4Flag == 1,
		DecompressedSize: int(decompressedSize),
		Data:             data,
	}, nil
}

func decodeMipmapToRGBA(mipmap texMipmap, header texHeader, format texFormat, imageFormat freeImageFormat) ([]byte, int, int, error) {
	data := mipmap.Data

	if mipmap.IsLZ4Compressed {
		if mipmap.DecompressedSize <= 0 {
			return nil, 0, 0, errors.New("lz4 compressed mipmap without decompressed size")
		}
		decompressed, err := lz4DecompressBlock(data, mipmap.DecompressedSize)
		if err != nil {
			return nil, 0, 0, err
		}
		data = decompressed
	}

	if imageFormat != freeImageUnknown && imageFormat != freeImageMp4 {
		img, _, err := image.Decode(bytes.NewReader(data))
		if err != nil {
			return nil, 0, 0, fmt.Errorf("decode embedded image failed: %w", err)
		}
		return imageToRGBA(img, header.ImageWidth, header.ImageHeight)
	}

	switch format {
	case texFormatDXT1:
		rgba := decompressDXT(mipmap.Width, mipmap.Height, data, dxtFlagDXT1)
		return cropRGBA(rgba, mipmap.Width, mipmap.Height, header.ImageWidth, header.ImageHeight)
	case texFormatDXT3:
		rgba := decompressDXT(mipmap.Width, mipmap.Height, data, dxtFlagDXT3)
		return cropRGBA(rgba, mipmap.Width, mipmap.Height, header.ImageWidth, header.ImageHeight)
	case texFormatDXT5:
		rgba := decompressDXT(mipmap.Width, mipmap.Height, data, dxtFlagDXT5)
		return cropRGBA(rgba, mipmap.Width, mipmap.Height, header.ImageWidth, header.ImageHeight)
	case texFormatRGBA8888:
		if len(data) < mipmap.Width*mipmap.Height*4 {
			return nil, 0, 0, fmt.Errorf("rgba8888 data too short: %d", len(data))
		}
		return cropRGBA(data, mipmap.Width, mipmap.Height, header.ImageWidth, header.ImageHeight)
	case texFormatR8:
		rgba := expandR8ToRGBA(data, mipmap.Width, mipmap.Height)
		return cropRGBA(rgba, mipmap.Width, mipmap.Height, header.ImageWidth, header.ImageHeight)
	case texFormatRG88:
		rgba := expandRG88ToRGBA(data, mipmap.Width, mipmap.Height)
		return cropRGBA(rgba, mipmap.Width, mipmap.Height, header.ImageWidth, header.ImageHeight)
	default:
		return nil, 0, 0, fmt.Errorf("unsupported tex format: %d", format)
	}
}

func imageToRGBA(img image.Image, targetWidth, targetHeight int) ([]byte, int, int, error) {
	bounds := img.Bounds()
	width := bounds.Dx()
	height := bounds.Dy()

	if targetWidth <= 0 || targetWidth > width {
		targetWidth = width
	}
	if targetHeight <= 0 || targetHeight > height {
		targetHeight = height
	}

	dstRect := image.Rect(0, 0, targetWidth, targetHeight)
	rgbaImage := image.NewRGBA(dstRect)
	draw.Draw(rgbaImage, dstRect, img, bounds.Min, draw.Src)

	return rgbaImage.Pix, targetWidth, targetHeight, nil
}

func cropRGBA(src []byte, srcWidth, srcHeight, mapWidth, mapHeight int) ([]byte, int, int, error) {
	if srcWidth <= 0 || srcHeight <= 0 {
		return nil, 0, 0, errors.New("invalid source size for crop")
	}
	if mapWidth <= 0 || mapHeight <= 0 {
		return src, srcWidth, srcHeight, nil
	}
	if mapWidth > srcWidth {
		mapWidth = srcWidth
	}
	if mapHeight > srcHeight {
		mapHeight = srcHeight
	}

	if mapWidth == srcWidth && mapHeight == srcHeight {
		return src, srcWidth, srcHeight, nil
	}

	dst := make([]byte, mapWidth*mapHeight*4)
	srcStride := srcWidth * 4
	dstStride := mapWidth * 4

	for y := 0; y < mapHeight; y++ {
		srcStart := y * srcStride
		dstStart := y * dstStride
		copy(dst[dstStart:dstStart+dstStride], src[srcStart:srcStart+dstStride])
	}

	return dst, mapWidth, mapHeight, nil
}

func expandR8ToRGBA(src []byte, width, height int) []byte {
	pixelCount := width * height
	dst := make([]byte, pixelCount*4)
	for i := range pixelCount {
		value := src[i]
		base := i * 4
		dst[base+0] = value
		dst[base+1] = value
		dst[base+2] = value
		dst[base+3] = value
	}
	return dst
}

func expandRG88ToRGBA(src []byte, width, height int) []byte {
	pixelCount := width * height
	dst := make([]byte, pixelCount*4)

	srcIndex := 0
	for i := range pixelCount {
		r := src[srcIndex]
		g := src[srcIndex+1]
		srcIndex += 2

		base := i * 4
		dst[base+0] = r
		dst[base+1] = g
		dst[base+2] = g
		dst[base+3] = g
	}
	return dst
}

func encodeRGBAtoWebP(rgba []byte, width, height int) ([]byte, error) {
	if width <= 0 || height <= 0 {
		return nil, errors.New("invalid size for webp encode")
	}
	if len(rgba) < width*height*4 {
		return nil, fmt.Errorf("rgba buffer too short: have %d, need %d", len(rgba), width*height*4)
	}

	img := &image.RGBA{
		Pix:    rgba,
		Stride: width * 4,
		Rect:   image.Rect(0, 0, width, height),
	}

	var buffer bytes.Buffer
	if err := webp.Encode(&buffer, img, &webp.Options{Lossless: true}); err != nil {
		return nil, err
	}
	return buffer.Bytes(), nil
}

func readUint32(r *bytes.Reader) (uint32, error) {
	var value uint32
	if err := binary.Read(r, binary.LittleEndian, &value); err != nil {
		return 0, err
	}
	return value, nil
}

func readBytes(r *bytes.Reader, size int) ([]byte, error) {
	if size < 0 {
		return nil, fmt.Errorf("negative size: %d", size)
	}
	data := make([]byte, size)
	if _, err := r.Read(data); err != nil {
		return nil, err
	}
	return data, nil
}

func readCString(r *bytes.Reader, maxLen int) (string, error) {
	var buf []byte
	for {
		b, err := r.ReadByte()
		if err != nil {
			return "", err
		}
		if b == 0 {
			break
		}
		buf = append(buf, b)
		if maxLen > 0 && len(buf) >= maxLen {
			break
		}
	}
	return string(buf), nil
}

func lz4DecompressBlock(src []byte, decompressedSize int) ([]byte, error) {
	dst := make([]byte, decompressedSize)
	n, err := lz4.UncompressBlock(src, dst)
	if err != nil {
		return nil, fmt.Errorf("lz4 uncompress failed: %w", err)
	}
	if n != decompressedSize {
		return nil, fmt.Errorf("lz4 uncompress: expected %d bytes, got %d", decompressedSize, n)
	}
	return dst, nil
}

type dxtFlags int

const (
	dxtFlagDXT1 dxtFlags = 1 << 0
	dxtFlagDXT3 dxtFlags = 1 << 1
	dxtFlagDXT5 dxtFlags = 1 << 2
)

func decompressDXT(width, height int, data []byte, flags dxtFlags) []byte {
	rgba := make([]byte, width*height*4)
	sourceBlockPos := 0
	bytesPerBlock := 16
	if flags&dxtFlagDXT1 != 0 {
		bytesPerBlock = 8
	}
	targetRGBA := make([]byte, 16*4)

	for blockY := 0; blockY < height; blockY += 4 {
		for blockX := 0; blockX < width; blockX += 4 {
			if sourceBlockPos >= len(data) {
				continue
			}
			decompressDXTBlock(targetRGBA, data, sourceBlockPos, flags)
			sourceBlockPos += bytesPerBlock

			targetPixelPos := 0
			for py := range 4 {
				for px := range 4 {
					x := blockX + px
					y := blockY + py
					if x < width && y < height {
						dstIndex := 4 * (width*y + x)
						copy(rgba[dstIndex:dstIndex+4], targetRGBA[targetPixelPos:targetPixelPos+4])
					}
					targetPixelPos += 4
				}
			}
		}
	}

	return rgba
}

func decompressDXTBlock(rgba, block []byte, blockIndex int, flags dxtFlags) {
	colorBlockIndex := blockIndex
	if flags&(dxtFlagDXT3|dxtFlagDXT5) != 0 {
		colorBlockIndex += 8
	}

	decompressDXTColor(rgba, block, colorBlockIndex, flags&dxtFlagDXT1 != 0)
	if flags&dxtFlagDXT3 != 0 {
		decompressDXTAlphaDxt3(rgba, block, blockIndex)
	} else if flags&dxtFlagDXT5 != 0 {
		decompressDXTAlphaDxt5(rgba, block, blockIndex)
	}
}

func decompressDXTAlphaDxt3(rgba, block []byte, blockIndex int) {
	for i := range 8 {
		quant := block[blockIndex+i]
		lo := quant & 0x0F
		hi := quant & 0xF0
		rgba[8*i+3] = lo | (lo << 4)
		rgba[8*i+7] = hi | (hi >> 4)
	}
}

func decompressDXTAlphaDxt5(rgba, block []byte, blockIndex int) {
	alpha0 := block[blockIndex+0]
	alpha1 := block[blockIndex+1]

	var codes [8]byte
	codes[0] = alpha0
	codes[1] = alpha1

	if alpha0 <= alpha1 {
		for i := 1; i < 5; i++ {
			codes[1+i] = byte(((5-int32(i))*int32(alpha0) + int32(i)*int32(alpha1)) / 5)
		}
		codes[6] = 0
		codes[7] = 255
	} else {
		for i := 1; i < 7; i++ {
			codes[i+1] = byte(((7-int32(i))*int32(alpha0) + int32(i)*int32(alpha1)) / 7)
		}
	}

	var indices [16]byte
	blockSrcPos := 2
	indicesPos := 0

	for range 2 {
		value := 0
		for j := range 3 {
			b := int(block[blockIndex+blockSrcPos])
			blockSrcPos++
			value |= b << (8 * j)
		}
		for j := range 8 {
			index := (value >> (3 * j)) & 0x07
			indices[indicesPos] = byte(index)
			indicesPos++
		}
	}

	for i := range 16 {
		rgba[4*i+3] = codes[indices[i]]
	}
}

func decompressDXTColor(rgba, block []byte, blockIndex int, isDXT1 bool) {
	var codes [16]byte

	a := unpack565(block, blockIndex+0, codes[0:])
	b := unpack565(block, blockIndex+2, codes[4:])

	for i := range 3 {
		c := int(codes[i])
		d := int(codes[4+i])
		if isDXT1 && a <= b {
			codes[8+i] = byte((c + d) / 2)
			codes[12+i] = 0
		} else {
			codes[8+i] = byte((2*c + d) / 3)
			codes[12+i] = byte((c + 2*d) / 3)
		}
	}

	codes[8+3] = 255
	if isDXT1 && a <= b {
		codes[12+3] = 0
	} else {
		codes[12+3] = 255
	}

	var indices [16]byte
	for row := range 4 {
		packed := block[blockIndex+4+row]
		indices[row*4+0] = packed & 0x3
		indices[row*4+1] = (packed >> 2) & 0x3
		indices[row*4+2] = (packed >> 4) & 0x3
		indices[row*4+3] = (packed >> 6) & 0x3
	}

	for i := range 16 {
		offset := int(indices[i]) * 4
		dst := 4 * i
		rgba[dst+0] = codes[offset+0]
		rgba[dst+1] = codes[offset+1]
		rgba[dst+2] = codes[offset+2]
		rgba[dst+3] = codes[offset+3]
	}
}

func unpack565(block []byte, offset int, color []byte) int {
	value := int(block[offset]) | (int(block[offset+1]) << 8)
	red := byte((value >> 11) & 0x1F)
	green := byte((value >> 5) & 0x3F)
	blue := byte(value & 0x1F)

	color[0] = (red << 3) | (red >> 2)
	color[1] = (green << 2) | (green >> 4)
	color[2] = (blue << 3) | (blue >> 2)
	color[3] = 255

	return value
}
