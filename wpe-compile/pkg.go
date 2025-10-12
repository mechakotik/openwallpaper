package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"io"
)

func extractPkg(data []byte) (map[string][]byte, error) {
	r := bytes.NewReader(data)

	if _, err := readStringI32(r, 32); err != nil {
		return nil, fmt.Errorf("read magic: %w", err)
	}

	entryCount, err := readInt32(r)
	if err != nil {
		return nil, fmt.Errorf("read entry count: %w", err)
	}
	if entryCount < 0 {
		return nil, fmt.Errorf("negative entry count: %d", entryCount)
	}

	type hdr struct {
		path   string
		offset int32
		length int32
	}

	headers := make([]hdr, 0, entryCount)
	for i := 0; i < int(entryCount); i++ {
		path, err := readStringI32(r, 255)
		if err != nil {
			return nil, fmt.Errorf("read entry[%d] path: %w", i, err)
		}
		off, err := readInt32(r)
		if err != nil {
			return nil, fmt.Errorf("read entry[%d] offset: %w", i, err)
		}
		ln, err := readInt32(r)
		if err != nil {
			return nil, fmt.Errorf("read entry[%d] length: %w", i, err)
		}
		if off < 0 || ln < 0 {
			return nil, fmt.Errorf("invalid header for entry[%d]: offset=%d length=%d", i, off, ln)
		}
		headers = append(headers, hdr{path: path, offset: off, length: ln})
	}

	dataStart, err := r.Seek(0, io.SeekCurrent)
	if err != nil {
		return nil, fmt.Errorf("get dataStart: %w", err)
	}

	out := make(map[string][]byte, len(headers))
	for _, h := range headers {
		start := dataStart + int64(h.offset)
		end := start + int64(h.length)

		if start < 0 || end < 0 || start > end || end > int64(len(data)) {
			return nil, fmt.Errorf("invalid data range for %q: [%d,%d) of %d",
				h.path, start, end, len(data))
		}

		chunk := make([]byte, h.length)
		copy(chunk, data[int(start):int(end)])
		out[h.path] = chunk
	}

	return out, nil
}

func readInt32(r io.Reader) (int32, error) {
	var v int32
	if err := binary.Read(r, binary.LittleEndian, &v); err != nil {
		return 0, err
	}
	return v, nil
}

func readStringI32(r io.Reader, max int) (string, error) {
	n, err := readInt32(r)
	if err != nil {
		return "", err
	}
	if n < 0 {
		return "", fmt.Errorf("negative string length: %d", n)
	}
	if max > 0 && int(n) > max {
		return "", fmt.Errorf("string length %d exceeds limit %d", n, max)
	}
	b := make([]byte, int(n))
	if _, err := io.ReadFull(r, b); err != nil {
		return "", err
	}
	return string(b), nil
}
