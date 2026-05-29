package main

import (
	"encoding/binary"
	"fmt"
	"strconv"
	"strings"
)

const (
	mdlFlagNormal     = 0x00000002
	mdlFlagTangent    = 0x00000004
	mdlFlagUV         = 0x00000008
	mdlFlagUV2        = 0x00000020
	mdlFlagExtra4     = 0x00010000
	mdlFlagSkinBlend  = 0x00800000
	mdlFlagSkinWeight = 0x01000000
)

type PuppetMetadata struct {
	Path      string
	BoneCount int
	HasMesh   bool
}

type mdlReader struct {
	data []byte
	off  int
}

func (reader *mdlReader) tell() int {
	return reader.off
}

func (reader *mdlReader) seek(offset int) error {
	if offset < 0 || offset > len(reader.data) {
		return fmt.Errorf("seek outside mdl file: %d", offset)
	}
	reader.off = offset
	return nil
}

func (reader *mdlReader) skip(size int) error {
	return reader.seek(reader.off + size)
}

func (reader *mdlReader) readBytes(size int) ([]byte, error) {
	if size < 0 || reader.off+size > len(reader.data) {
		return nil, fmt.Errorf("unexpected end of mdl file at 0x%x", reader.off)
	}
	data := reader.data[reader.off : reader.off+size]
	reader.off += size
	return data, nil
}

func (reader *mdlReader) readUint8() (uint8, error) {
	data, err := reader.readBytes(1)
	if err != nil {
		return 0, err
	}
	return data[0], nil
}

func (reader *mdlReader) readUint16() (uint16, error) {
	data, err := reader.readBytes(2)
	if err != nil {
		return 0, err
	}
	return binary.LittleEndian.Uint16(data), nil
}

func (reader *mdlReader) readUint32() (uint32, error) {
	data, err := reader.readBytes(4)
	if err != nil {
		return 0, err
	}
	return binary.LittleEndian.Uint32(data), nil
}

func (reader *mdlReader) readCString() (string, error) {
	start := reader.off
	for reader.off < len(reader.data) && reader.data[reader.off] != 0 {
		reader.off++
	}
	if reader.off >= len(reader.data) {
		return "", fmt.Errorf("unterminated mdl string at 0x%x", start)
	}
	value := string(reader.data[start:reader.off])
	reader.off++
	return value, nil
}

func (reader *mdlReader) readVersion(prefix string) (int, string, error) {
	data, err := reader.readBytes(9)
	if err != nil {
		return 0, "", err
	}
	tag := strings.TrimRight(string(data), "\x00")
	if !strings.HasPrefix(tag, prefix) || len(tag) < 8 {
		return 0, tag, fmt.Errorf("invalid mdl version tag %q", tag)
	}
	version, err := strconv.Atoi(tag[4:8])
	if err != nil {
		return 0, tag, fmt.Errorf("invalid mdl version tag %q: %w", tag, err)
	}
	return version, tag, nil
}

func (reader *mdlReader) peekMagic(magic string) bool {
	return len(magic) == 4 && reader.off+4 <= len(reader.data) && string(reader.data[reader.off:reader.off+4]) == magic
}

func parsePuppetMetadata(path string, data []byte) (*PuppetMetadata, error) {
	reader := mdlReader{data: data}
	version, _, err := reader.readVersion("MDL")
	if err != nil {
		return nil, err
	}
	headerFlag, err := reader.readUint32()
	if err != nil {
		return nil, err
	}
	alwaysOne, err := reader.readUint32()
	if err != nil {
		return nil, err
	}
	if alwaysOne != 1 {
		return nil, fmt.Errorf("unexpected mdl header marker %d", alwaysOne)
	}
	meshCount, err := reader.readUint32()
	if err != nil {
		return nil, err
	}

	metadata := &PuppetMetadata{Path: path}
	for meshIdx := uint32(0); meshIdx < meshCount; meshIdx++ {
		hasMesh, err := skipPuppetMesh(&reader, version, headerFlag, path)
		if err != nil {
			return nil, err
		}
		metadata.HasMesh = metadata.HasMesh || hasMesh
	}

	for reader.tell()+4 <= len(reader.data) {
		switch {
		case reader.peekMagic("MDLS"):
			boneCount, err := parsePuppetBoneCount(&reader)
			if err != nil {
				return nil, err
			}
			metadata.BoneCount = boneCount
		case reader.peekMagic("MDAT"):
			if err := skipPuppetEndOffsetBlock(&reader); err != nil {
				return nil, err
			}
		case reader.peekMagic("MDLA"):
			if err := skipPuppetAnimationBlock(&reader); err != nil {
				return nil, err
			}
		case reader.peekMagic("MDMP"):
			if err := skipPuppetEndOffsetBlock(&reader); err != nil {
				return nil, err
			}
		case reader.peekMagic("MDLE"):
			if err := skipPuppetEndOffsetBlock(&reader); err != nil {
				return nil, err
			}
		default:
			return metadata, nil
		}
	}

	return metadata, nil
}

func skipPuppetMesh(reader *mdlReader, mdlVersion int, headerFlag uint32, path string) (bool, error) {
	if _, err := reader.readCString(); err != nil {
		return false, err
	}
	flagA, err := reader.readUint32()
	if err != nil {
		return false, err
	}
	if flagA == 2 {
		if _, err := reader.readUint32(); err != nil {
			return false, err
		}
	}
	if mdlVersion >= 17 {
		if err := reader.skip(24); err != nil {
			return false, err
		}
	}

	meshFlag := headerFlag
	if mdlVersion > 14 {
		meshFlag, err = reader.readUint32()
		if err != nil {
			return false, err
		}
	}

	vertexBytes, err := reader.readUint32()
	if err != nil {
		return false, err
	}
	stride := puppetVertexStride(meshFlag)
	if stride == 0 || vertexBytes%stride != 0 {
		return false, fmt.Errorf("unsupported mdl vertex size %d (flag=0x%x stride=%d) in %s", vertexBytes, meshFlag, stride, path)
	}
	vertexCount := int(vertexBytes / stride)
	if err := reader.skip(int(vertexBytes)); err != nil {
		return false, err
	}

	indexBytes, err := reader.readUint32()
	if err != nil {
		return false, err
	}
	if indexBytes%6 != 0 {
		return false, fmt.Errorf("unsupported mdl index size %d in %s", indexBytes, path)
	}
	if err := reader.skip(int(indexBytes)); err != nil {
		return false, err
	}

	if mdlVersion >= 21 {
		if err := skipPuppetMeshParts(reader, mdlVersion, vertexCount); err != nil {
			return false, err
		}
		if mdlVersion > 21 {
			if err := skipPuppetMeshMasks(reader); err != nil {
				return false, err
			}
		}
	}

	return vertexBytes > 0 && indexBytes > 0, nil
}

func puppetVertexStride(flag uint32) uint32 {
	stride := uint32(12)
	if flag&mdlFlagNormal != 0 {
		stride += 12
	}
	if flag&mdlFlagTangent != 0 {
		stride += 16
	}
	if flag&mdlFlagExtra4 != 0 {
		stride += 4
	}
	if flag&mdlFlagSkinBlend != 0 {
		stride += 16
	}
	if flag&mdlFlagSkinWeight != 0 {
		stride += 16
	}
	if flag&(mdlFlagUV|mdlFlagUV2) != 0 {
		stride += 8
	}
	if flag&mdlFlagUV2 != 0 {
		stride += 8
	}
	return stride
}

func skipPuppetMeshParts(reader *mdlReader, mdlVersion int, vertexCount int) error {
	unkA, err := reader.readUint8()
	if err != nil {
		return err
	}
	if unkA == 1 {
		unkB, err := reader.readUint8()
		if err != nil {
			return err
		}
		if unkB != 0 {
			if err := reader.skip(3); err != nil {
				return err
			}
			payloadSize, err := reader.readUint32()
			if err != nil {
				return err
			}
			if payloadSize != uint32(12*vertexCount) {
				return fmt.Errorf("mdlv%d parts payload size %d != 12*%d", mdlVersion, payloadSize, vertexCount)
			}
			if err := reader.skip(int(payloadSize)); err != nil {
				return err
			}
		}
	} else if unkA != 0 {
		return fmt.Errorf("mdlv%d unsupported parts marker %d", mdlVersion, unkA)
	}

	hasParts, err := reader.readUint8()
	if err != nil {
		return err
	}
	if hasParts == 0 {
		return nil
	}
	partsBytes, err := reader.readUint32()
	if err != nil {
		return err
	}
	if partsBytes%16 != 0 {
		return fmt.Errorf("mdlv%d parts byte count %d is not divisible by 16", mdlVersion, partsBytes)
	}
	return reader.skip(int(partsBytes))
}

func skipPuppetMeshMasks(reader *mdlReader) error {
	maskCount, err := reader.readUint32()
	if err != nil {
		return err
	}
	for maskIdx := uint32(0); maskIdx < maskCount; maskIdx++ {
		if err := reader.skip(8); err != nil {
			return err
		}
		if _, err := reader.readCString(); err != nil {
			return err
		}
		if err := reader.skip(4); err != nil {
			return err
		}
		aCount, err := reader.readUint32()
		if err != nil {
			return err
		}
		if err := reader.skip(int(aCount) * 4); err != nil {
			return err
		}
		bCount, err := reader.readUint32()
		if err != nil {
			return err
		}
		if err := reader.skip(int(bCount) * 4); err != nil {
			return err
		}
	}
	return nil
}

func parsePuppetBoneCount(reader *mdlReader) (int, error) {
	if _, _, err := reader.readVersion("MDL"); err != nil {
		return 0, err
	}
	endOffset, err := reader.readUint32()
	if err != nil {
		return 0, err
	}
	boneCount, err := reader.readUint16()
	if err != nil {
		return 0, err
	}
	if err := reader.seek(int(endOffset)); err != nil {
		return 0, err
	}
	return int(boneCount), nil
}

func skipPuppetEndOffsetBlock(reader *mdlReader) error {
	if _, err := reader.readBytes(9); err != nil {
		return err
	}
	endOffset, err := reader.readUint32()
	if err != nil {
		return err
	}
	return reader.seek(int(endOffset))
}

func skipPuppetAnimationBlock(reader *mdlReader) error {
	version, _, err := reader.readVersion("MDL")
	if err != nil {
		return err
	}
	if version == 0 {
		return nil
	}
	endOffset, err := reader.readUint32()
	if err != nil {
		return err
	}
	return reader.seek(int(endOffset))
}
