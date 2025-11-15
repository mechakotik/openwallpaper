package main

import (
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
	"image"
	"image/draw"
	"image/gif"
	"image/jpeg"
	"image/png"
	"io"

	"github.com/chai2010/webp"
	"github.com/dblezek/tga"
	"github.com/pierrec/lz4/v4"
	"golang.org/x/image/bmp"
	"golang.org/x/image/tiff"
)

type texFormat int32

const (
	texRGBA8888 texFormat = 0
	texDXT5     texFormat = 4
	texDXT3     texFormat = 6
	texDXT1     texFormat = 7
	texRG88     texFormat = 8
	texR8       texFormat = 9
)

type texFlags uint32

const (
	flagNoInterp texFlags = 1
	flagClampUVs texFlags = 2
	flagIsGif    texFlags = 4
	flagIsVideo  texFlags = 32
)

type freeImageFormat int32

const (
	fifUNKNOWN freeImageFormat = -1
	fifBMP     freeImageFormat = 0
	fifICO     freeImageFormat = 1
	fifJPEG    freeImageFormat = 2
	fifJNG     freeImageFormat = 3
	fifKOALA   freeImageFormat = 4
	fifLBM     freeImageFormat = 5
	fifMNG     freeImageFormat = 6
	fifPBM     freeImageFormat = 7
	fifPBMRAW  freeImageFormat = 8
	fifPCD     freeImageFormat = 9
	fifPCX     freeImageFormat = 10
	fifPGM     freeImageFormat = 11
	fifPGMRAW  freeImageFormat = 12
	fifPNG     freeImageFormat = 13
	fifPPM     freeImageFormat = 14
	fifPPMRAW  freeImageFormat = 15
	fifRAS     freeImageFormat = 16
	fifTARGA   freeImageFormat = 17
	fifTIFF    freeImageFormat = 18
	fifWBMP    freeImageFormat = 19
	fifPSD     freeImageFormat = 20
	fifCUT     freeImageFormat = 21
	fifXBM     freeImageFormat = 22
	fifXPM     freeImageFormat = 23
	fifDDS     freeImageFormat = 24
	fifGIF     freeImageFormat = 25
	fifHDR     freeImageFormat = 26
	fifFAXG3   freeImageFormat = 27
	fifSGI     freeImageFormat = 28
	fifEXR     freeImageFormat = 29
	fifJ2K     freeImageFormat = 30
	fifJP2     freeImageFormat = 31
	fifPFM     freeImageFormat = 32
	fifPICT    freeImageFormat = 33
	fifRAW     freeImageFormat = 34
	fifMP4     freeImageFormat = 35
)

type texHeader struct {
	Format   texFormat
	Flags    texFlags
	TextureW int32
	TextureH int32
	ImageW   int32
	ImageH   int32
	UnkInt0  uint32
}

type mipmap struct {
	W, H    int32
	LZ4     bool
	DecSize int32
	Data    []byte
}

type imageContainer struct {
	Magic       string
	Version     int
	ImageFormat freeImageFormat
	Images      [][]mipmap
}

type frameInfo struct {
	ImageID   int32
	Frametime float32
	X, Y      float32
	Width     float32
	WidthY    float32
	HeightX   float32
	Height    float32
}

type frameInfoContainer struct {
	Magic  string
	W, H   int32
	Frames []frameInfo
}

type tex struct {
	Header texHeader
	IC     imageContainer
	FIC    *frameInfoContainer
}

type WebPResult struct {
	Data        []byte
	Width       int
	Height      int
	PixelFormat string
}

func texToWEBP(texBytes []byte) (WebPResult, error) {
	r := bytes.NewReader(texBytes)
	t, err := readTEX(r)
	if err != nil {
		return WebPResult{}, err
	}
	if (t.Header.Flags & flagIsVideo) != 0 {
		return WebPResult{}, errors.New("video TEX is not supported")
	}
	if (t.Header.Flags & flagIsGif) != 0 {
		return WebPResult{}, errors.New("animated TEX is not supported")
	}
	if len(t.IC.Images) == 0 || len(t.IC.Images[0]) == 0 {
		return WebPResult{}, errors.New("empty TEX")
	}
	srcImg, err := decodeMipmapToImage(t.Header, t.IC, t.IC.Images[0][0])
	if err != nil {
		return WebPResult{}, err
	}
	if srcImg.Bounds().Dx() != int(t.Header.ImageW) || srcImg.Bounds().Dy() != int(t.Header.ImageH) {
		srcImg = crop(srcImg, image.Rect(0, 0, int(t.Header.ImageW), int(t.Header.ImageH)))
	}
	w := srcImg.Bounds().Dx()
	h := srcImg.Bounds().Dy()
	pf := pixelFormatOf(srcImg)
	var buf bytes.Buffer
	if err := webp.Encode(&buf, srcImg, &webp.Options{Lossless: true}); err != nil {
		return WebPResult{}, fmt.Errorf("webp encode: %w", err)
	}
	return WebPResult{Data: buf.Bytes(), Width: w, Height: h, PixelFormat: pf}, nil
}

func readTEX(r *bytes.Reader) (*tex, error) {
	m1, err := readNString(r, 16)
	if err != nil || m1 != "TEXV0005" {
		return nil, fmt.Errorf("invalid TEXV magic: %q", m1)
	}
	m2, err := readNString(r, 16)
	if err != nil || m2 != "TEXI0001" {
		return nil, fmt.Errorf("invalid TEXI magic: %q", m2)
	}
	var h texHeader
	if err := readHeader(r, &h); err != nil {
		return nil, err
	}
	ic, err := readImageContainer(r, h.Format)
	if err != nil {
		return nil, err
	}
	var fic *frameInfoContainer
	if (h.Flags & flagIsGif) != 0 {
		f, err := readFrameInfoContainer(r)
		if err != nil {
			return nil, err
		}
		fic = &f
	}
	return &tex{Header: h, IC: ic, FIC: fic}, nil
}

func readHeader(r io.Reader, h *texHeader) error {
	var tmp [7]int32
	if err := binary.Read(r, binary.LittleEndian, &tmp); err != nil {
		return err
	}
	h.Format = texFormat(tmp[0])
	h.Flags = texFlags(tmp[1])
	h.TextureW, h.TextureH = tmp[2], tmp[3]
	h.ImageW, h.ImageH = tmp[4], tmp[5]
	if err := binary.Read(r, binary.LittleEndian, &h.UnkInt0); err != nil {
		return err
	}
	return nil
}

func readImageContainer(r *bytes.Reader, _ texFormat) (imageContainer, error) {
	magic, err := readNString(r, 16)
	if err != nil {
		return imageContainer{}, err
	}
	var count int32
	if err := binary.Read(r, binary.LittleEndian, &count); err != nil {
		return imageContainer{}, err
	}
	ic := imageContainer{Magic: "TEXB" + magic, Version: mustAtoi(magic)}
	switch ic.Magic {
	case "TEXB0001", "TEXB0002":
		ic.ImageFormat = fifUNKNOWN
	case "TEXB0003":
		var f int32
		if err := binary.Read(r, binary.LittleEndian, &f); err != nil {
			return ic, err
		}
		ic.ImageFormat = freeImageFormat(f)
	case "TEXB0004":
		var f, isMp4 int32
		if err := binary.Read(r, binary.LittleEndian, &f); err != nil {
			return ic, err
		}
		if err := binary.Read(r, binary.LittleEndian, &isMp4); err != nil {
			return ic, err
		}
		if freeImageFormat(f) == fifUNKNOWN && isMp4 == 1 {
			ic.ImageFormat = fifMP4
		} else {
			ic.ImageFormat = freeImageFormat(f)
		}
	default:
		return ic, fmt.Errorf("unsupported TEXB: %s", ic.Magic)
	}
	if ic.Version == 4 && ic.ImageFormat != fifMP4 {
		ic.Version = 3
	}
	ic.Images = make([][]mipmap, count)
	for i := 0; i < int(count); i++ {
		mc, err := readMipmaps(r, ic.Version)
		if err != nil {
			return ic, err
		}
		ic.Images[i] = mc
	}
	return ic, nil
}

func readMipmaps(r *bytes.Reader, ver int) ([]mipmap, error) {
	var mipCount int32
	if err := binary.Read(r, binary.LittleEndian, &mipCount); err != nil {
		return nil, err
	}
	mips := make([]mipmap, mipCount)
	for i := range mips {
		switch ver {
		case 1:
			var w, h, sz int32
			if err := binary.Read(r, binary.LittleEndian, &w); err != nil {
				return nil, err
			}
			if err := binary.Read(r, binary.LittleEndian, &h); err != nil {
				return nil, err
			}
			if err := binary.Read(r, binary.LittleEndian, &sz); err != nil {
				return nil, err
			}
			data := make([]byte, sz)
			if _, err := io.ReadFull(r, data); err != nil {
				return nil, err
			}
			mips[i] = mipmap{W: w, H: h, Data: data}
		case 2, 3:
			var w, h, lz4Flag, dec, sz int32
			if err := binary.Read(r, binary.LittleEndian, &w); err != nil {
				return nil, err
			}
			if err := binary.Read(r, binary.LittleEndian, &h); err != nil {
				return nil, err
			}
			if err := binary.Read(r, binary.LittleEndian, &lz4Flag); err != nil {
				return nil, err
			}
			if err := binary.Read(r, binary.LittleEndian, &dec); err != nil {
				return nil, err
			}
			if err := binary.Read(r, binary.LittleEndian, &sz); err != nil {
				return nil, err
			}
			data := make([]byte, sz)
			if _, err := io.ReadFull(r, data); err != nil {
				return nil, err
			}
			mips[i] = mipmap{W: w, H: h, LZ4: lz4Flag == 1, DecSize: dec, Data: data}
		case 4:
			var p1, p2 int32
			if err := binary.Read(r, binary.LittleEndian, &p1); err != nil {
				return nil, err
			}
			if err := binary.Read(r, binary.LittleEndian, &p2); err != nil {
				return nil, err
			}
			if _, err := readNString(r, -1); err != nil {
				return nil, err
			}
			var p3 int32
			if err := binary.Read(r, binary.LittleEndian, &p3); err != nil {
				return nil, err
			}
			var w, h, lz4Flag, dec, sz int32
			if err := binary.Read(r, binary.LittleEndian, &w); err != nil {
				return nil, err
			}
			if err := binary.Read(r, binary.LittleEndian, &h); err != nil {
				return nil, err
			}
			if err := binary.Read(r, binary.LittleEndian, &lz4Flag); err != nil {
				return nil, err
			}
			if err := binary.Read(r, binary.LittleEndian, &dec); err != nil {
				return nil, err
			}
			if err := binary.Read(r, binary.LittleEndian, &sz); err != nil {
				return nil, err
			}
			data := make([]byte, sz)
			if _, err := io.ReadFull(r, data); err != nil {
				return nil, err
			}
			mips[i] = mipmap{W: w, H: h, LZ4: lz4Flag == 1, DecSize: dec, Data: data}
		default:
			return nil, fmt.Errorf("unsupported container version: %d", ver)
		}
	}
	return mips, nil
}

func readFrameInfoContainer(r *bytes.Reader) (frameInfoContainer, error) {
	magic, err := readNString(r, 16)
	if err != nil {
		return frameInfoContainer{}, err
	}
	var n int32
	if err := binary.Read(r, binary.LittleEndian, &n); err != nil {
		return frameInfoContainer{}, err
	}
	fic := frameInfoContainer{Magic: magic}
	switch magic {
	case "TEXS0001":
		fic.Frames = make([]frameInfo, n)
		for i := 0; i < int(n); i++ {
			var im int32
			var ft float32
			var xi, yi, wi, wyi, hxi, hi int32
			binary.Read(r, binary.LittleEndian, &im)
			binary.Read(r, binary.LittleEndian, &ft)
			binary.Read(r, binary.LittleEndian, &xi)
			binary.Read(r, binary.LittleEndian, &yi)
			binary.Read(r, binary.LittleEndian, &wi)
			binary.Read(r, binary.LittleEndian, &wyi)
			binary.Read(r, binary.LittleEndian, &hxi)
			binary.Read(r, binary.LittleEndian, &hi)
			fic.Frames[i] = frameInfo{
				ImageID: im, Frametime: ft,
				X: float32(xi), Y: float32(yi),
				Width: float32(wi), WidthY: float32(wyi),
				HeightX: float32(hxi), Height: float32(hi),
			}
		}
	case "TEXS0002":
		fic.Frames = make([]frameInfo, n)
		for i := 0; i < int(n); i++ {
			var f frameInfo
			binary.Read(r, binary.LittleEndian, &f.ImageID)
			binary.Read(r, binary.LittleEndian, &f.Frametime)
			binary.Read(r, binary.LittleEndian, &f.X)
			binary.Read(r, binary.LittleEndian, &f.Y)
			binary.Read(r, binary.LittleEndian, &f.Width)
			binary.Read(r, binary.LittleEndian, &f.WidthY)
			binary.Read(r, binary.LittleEndian, &f.HeightX)
			binary.Read(r, binary.LittleEndian, &f.Height)
			fic.Frames[i] = f
		}
	case "TEXS0003":
		binary.Read(r, binary.LittleEndian, &fic.W)
		binary.Read(r, binary.LittleEndian, &fic.H)
		fic.Frames = make([]frameInfo, n)
		for i := 0; i < int(n); i++ {
			var f frameInfo
			binary.Read(r, binary.LittleEndian, &f.ImageID)
			binary.Read(r, binary.LittleEndian, &f.Frametime)
			binary.Read(r, binary.LittleEndian, &f.X)
			binary.Read(r, binary.LittleEndian, &f.Y)
			binary.Read(r, binary.LittleEndian, &f.Width)
			binary.Read(r, binary.LittleEndian, &f.WidthY)
			binary.Read(r, binary.LittleEndian, &f.HeightX)
			binary.Read(r, binary.LittleEndian, &f.Height)
			fic.Frames[i] = f
		}
	default:
		return fic, fmt.Errorf("unknown TEXS: %s", magic)
	}
	if fic.W == 0 || fic.H == 0 {
		if len(fic.Frames) > 0 {
			fic.W = int32(absf(nonzero(fic.Frames[0].Width, fic.Frames[0].HeightX)))
			fic.H = int32(absf(nonzero(fic.Frames[0].Height, fic.Frames[0].WidthY)))
		}
	}
	return fic, nil
}

func decodeMipmapToImage(h texHeader, ic imageContainer, m mipmap) (image.Image, error) {
	if ic.ImageFormat != fifUNKNOWN && ic.ImageFormat != fifMP4 {
		img, err := decodeEmbeddedImage(ic.ImageFormat, m.Data)
		if err != nil {
			return nil, err
		}
		return img, nil
	}
	data := m.Data
	if m.LZ4 {
		dst := make([]byte, m.DecSize)
		if _, err := lz4.UncompressBlock(m.Data, dst); err != nil {
			return nil, fmt.Errorf("lz4: %w", err)
		}
		data = dst
	}
	switch h.Format {
	case texRGBA8888:
		return rgbaFromBytes(data, int(m.W), int(m.H))
	case texR8:
		return r8FromBytes(data, int(m.W), int(m.H))
	case texRG88:
		return rg88FromBytes(data, int(m.W), int(m.H))
	case texDXT1:
		rgba := decompressDXT(int(m.W), int(m.H), data, dxt1)
		return rgbaFromBytes(rgba, int(m.W), int(m.H))
	case texDXT3:
		rgba := decompressDXT(int(m.W), int(m.H), data, dxt3)
		return rgbaFromBytes(rgba, int(m.W), int(m.H))
	case texDXT5:
		rgba := decompressDXT(int(m.W), int(m.H), data, dxt5)
		return rgbaFromBytes(rgba, int(m.W), int(m.H))
	default:
		return nil, fmt.Errorf("unsupported texFormat: %d", h.Format)
	}
}

func decodeEmbeddedImage(f freeImageFormat, b []byte) (image.Image, error) {
	br := bytes.NewReader(b)
	switch f {
	case fifPNG:
		return png.Decode(br)
	case fifJPEG:
		return jpeg.Decode(br)
	case fifGIF:
		return gif.Decode(br)
	case fifBMP:
		return bmp.Decode(br)
	case fifTIFF:
		return tiff.Decode(br)
	case fifTARGA:
		return tga.Decode(br)
	default:
		return nil, fmt.Errorf("embedded format %d is not supported by the pure Go decoder", f)
	}
}

func rgbaFromBytes(pix []byte, w, h int) (image.Image, error) {
	if len(pix) != w*h*4 {
		return nil, fmt.Errorf("RGBA size mismatch: %d != %d", len(pix), w*h*4)
	}
	img := image.NewRGBA(image.Rect(0, 0, w, h))
	copy(img.Pix, pix)
	return img, nil
}

func r8FromBytes(pix []byte, w, h int) (image.Image, error) {
	if len(pix) != w*h {
		return nil, fmt.Errorf("R8 size mismatch: %d != %d", len(pix), w*h)
	}
	img := image.NewRGBA(image.Rect(0, 0, w, h))
	for i := 0; i < w*h; i++ {
		v := pix[i]
		base := i * 4
		img.Pix[base+0] = v
		img.Pix[base+1] = v
		img.Pix[base+2] = v
		img.Pix[base+3] = 255
	}
	return img, nil
}

func rg88FromBytes(pix []byte, w, h int) (image.Image, error) {
	if len(pix) != w*h*2 {
		return nil, fmt.Errorf("RG88 size mismatch: %d != %d", len(pix), w*h*2)
	}
	img := image.NewRGBA(image.Rect(0, 0, w, h))
	for i := 0; i < w*h; i++ {
		r := pix[i*2+0]
		g := pix[i*2+1]
		base := i * 4
		img.Pix[base+0] = g
		img.Pix[base+1] = g
		img.Pix[base+2] = g
		img.Pix[base+3] = r
	}
	return img, nil
}

func crop(src image.Image, r image.Rectangle) image.Image {
	r = r.Intersect(src.Bounds())
	dst := image.NewRGBA(image.Rect(0, 0, r.Dx(), r.Dy()))
	draw.Draw(dst, dst.Bounds(), src, r.Min, draw.Src)
	return dst
}

func rotate90(img image.Image, deg int) image.Image {
	deg = ((deg % 360) + 360) % 360
	b := img.Bounds()
	switch deg {
	case 0:
		return crop(img, b)
	case 90:
		dst := image.NewRGBA(image.Rect(0, 0, b.Dy(), b.Dx()))
		for y := b.Min.Y; y < b.Max.Y; y++ {
			for x := b.Min.X; x < b.Max.X; x++ {
				c := img.At(x, y)
				dst.Set(b.Max.Y-1-y, x-b.Min.X, c)
			}
		}
		return dst
	case 180:
		dst := image.NewRGBA(image.Rect(0, 0, b.Dx(), b.Dy()))
		for y := b.Min.Y; y < b.Max.Y; y++ {
			for x := b.Min.X; x < b.Max.X; x++ {
				c := img.At(x, y)
				dst.Set(b.Max.X-1-x, b.Max.Y-1-y, c)
			}
		}
		return dst
	case 270:
		dst := image.NewRGBA(image.Rect(0, 0, b.Dy(), b.Dx()))
		for y := b.Min.Y; y < b.Max.Y; y++ {
			for x := b.Min.X; x < b.Max.X; x++ {
				c := img.At(x, y)
				dst.Set(y-b.Min.Y, b.Max.X-1-x, c)
			}
		}
		return dst
	default:
		return crop(img, b)
	}
}

func rotationFromSigns(ws, hs float32) int {
	switch {
	case ws > 0 && hs > 0:
		return 0
	case ws < 0 && hs > 0:
		return 270
	case ws < 0 && hs < 0:
		return 180
	case ws > 0 && hs < 0:
		return 90
	default:
		return 0
	}
}

func nonzero(a, b float32) float32 {
	if a != 0 {
		return a
	}
	return b
}

func absf(v float32) float32 {
	if v < 0 {
		return -v
	}
	return v
}

type dxtFlags int

const (
	dxt1 dxtFlags = iota + 1
	dxt3
	dxt5
)

func decompressDXT(w, h int, data []byte, f dxtFlags) []byte {
	rgba := make([]byte, w*h*4)
	var blockSize int
	if f == dxt1 {
		blockSize = 8
	} else {
		blockSize = 16
	}
	src := 0
	tmp := make([]byte, 16*4)
	for by := 0; by < h; by += 4 {
		for bx := 0; bx < w; bx += 4 {
			if src >= len(data) {
				break
			}
			switch f {
			case dxt1:
				decompressBlockColor(tmp, data, src, true)
			case dxt3:
				decompressBlockColor(tmp, data, src+8, false)
				decompressBlockAlphaDxt3(tmp, data, src)
			case dxt5:
				decompressBlockColor(tmp, data, src+8, false)
				decompressBlockAlphaDxt5(tmp, data, src)
			}
			off := 0
			for py := range 4 {
				for px := range 4 {
					x := bx + px
					y := by + py
					if x < w && y < h {
						dst := 4 * (y*w + x)
						copy(rgba[dst:dst+4], tmp[off:off+4])
					}
					off += 4
				}
			}
			src += blockSize
		}
	}
	return rgba
}

func decompressBlockAlphaDxt3(dst, src []byte, si int) {
	for i := range 8 {
		quant := src[si+i]
		lo := quant & 0x0F
		hi := (quant & 0xF0) >> 4
		dst[8*i+3] = lo | (lo << 4)
		dst[8*i+7] = hi | (hi << 4)
	}
}

func decompressBlockAlphaDxt5(dst, src []byte, si int) {
	a0 := src[si+0]
	a1 := src[si+1]
	var codes [8]byte
	codes[0], codes[1] = a0, a1
	if a0 <= a1 {
		for i := 1; i < 5; i++ {
			codes[1+i] = byte(((5-i)*int(a0) + i*int(a1)) / 5)
		}
		codes[6], codes[7] = 0, 255
	} else {
		for i := 1; i < 7; i++ {
			codes[i+1] = byte(((7-i)*int(a0) + i*int(a1)) / 7)
		}
	}
	var idx [16]byte
	sp := si + 2
	for k := range 2 {
		val := int(src[sp]) | int(src[sp+1])<<8 | int(src[sp+2])<<16
		sp += 3
		for j := range 8 {
			idx[k*8+j] = byte((val >> (3 * j)) & 0x7)
		}
	}
	for i := range 16 {
		dst[4*i+3] = codes[idx[i]]
	}
}

func decompressBlockColor(dst, src []byte, si int, isDXT1 bool) {
	codes := make([]byte, 16)
	a := unpack565(src, si+0, codes[0:4])
	b := unpack565(src, si+2, codes[4:8])
	for i := range 3 {
		c := codes[i]
		d := codes[4+i]
		if isDXT1 && a <= b {
			codes[8+i] = byte((int(c) + int(d)) / 2)
			codes[12+i] = 0
		} else {
			codes[8+i] = byte((2*int(c) + int(d)) / 3)
			codes[12+i] = byte((int(c) + 2*int(d)) / 3)
		}
	}
	codes[8+3] = 255
	if isDXT1 && a <= b {
		codes[12+3] = 0
	} else {
		codes[12+3] = 255
	}
	indices := make([]byte, 16)
	for i := range 4 {
		packed := src[si+4+i]
		indices[i*4+0] = packed & 0x03
		indices[i*4+1] = (packed >> 2) & 0x03
		indices[i*4+2] = (packed >> 4) & 0x03
		indices[i*4+3] = (packed >> 6) & 0x03
	}
	for i := range 16 {
		o := 4 * indices[i]
		copy(dst[4*i:4*i+3], codes[o:o+3])
		dst[4*i+3] = codes[o+3]
	}
}

func unpack565(src []byte, off int, out []byte) int {
	val := int(src[off]) | int(src[off+1])<<8
	red := byte((val >> 11) & 0x1F)
	green := byte((val >> 5) & 0x3F)
	blue := byte(val & 0x1F)
	out[0] = (red << 3) | (red >> 2)
	out[1] = (green << 2) | (green >> 4)
	out[2] = (blue << 3) | (blue >> 2)
	out[3] = 255
	return val
}

func readNString(r *bytes.Reader, max int) (string, error) {
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
		if max > 0 && len(buf) >= max {
			break
		}
	}
	return string(buf), nil
}

func mustAtoi(s string) int {
	x := 0
	for _, c := range s {
		if c >= '0' && c <= '9' {
			x = x*10 + int(c-'0')
		}
	}
	return x
}

func pixelFormatOf(img image.Image) string {
	switch im := img.(type) {
	case *image.RGBA:
		return "RGBA8888"
	case *image.NRGBA:
		return "NRGBA8888"
	case *image.RGBA64:
		return "RGBA64"
	case *image.NRGBA64:
		return "NRGBA64"
	case *image.Gray:
		return "GRAY8"
	case *image.Gray16:
		return "GRAY16"
	case *image.CMYK:
		return "CMYK"
	case *image.YCbCr:
		var sr string
		switch im.SubsampleRatio {
		case image.YCbCrSubsampleRatio444:
			sr = "4:4:4"
		case image.YCbCrSubsampleRatio422:
			sr = "4:2:2"
		case image.YCbCrSubsampleRatio420:
			sr = "4:2:0"
		case image.YCbCrSubsampleRatio440:
			sr = "4:4:0"
		case image.YCbCrSubsampleRatio411:
			sr = "4:1:1"
		default:
			sr = "unknown"
		}
		return "YCbCr " + sr
	case *image.Paletted:
		return "PAL8"
	case *image.Alpha:
		return "ALPHA8"
	case *image.Alpha16:
		return "ALPHA16"
	default:
		return fmt.Sprintf("%T", img)
	}
}
