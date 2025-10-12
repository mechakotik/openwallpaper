package main

import (
	"archive/zip"
	"bytes"
	"compress/flate"
	"io"
	"time"
)

func zipBytes(files map[string][]byte) ([]byte, error) {
	buf := new(bytes.Buffer)
	zipWriter := zip.NewWriter(buf)

	zipWriter.RegisterCompressor(zip.Deflate, func(out io.Writer) (io.WriteCloser, error) {
		return flate.NewWriter(out, flate.BestCompression)
	})

	now := time.Now().UTC()

	for name, data := range files {
		header := &zip.FileHeader{
			Name:     name,
			Method:   zip.Deflate,
			Modified: now,
		}
		header.SetMode(0644)

		writer, err := zipWriter.CreateHeader(header)
		if err != nil {
			_ = zipWriter.Close()
			return nil, err
		}
		if _, err := writer.Write(data); err != nil {
			_ = zipWriter.Close()
			return nil, err
		}
	}

	if err := zipWriter.Close(); err != nil {
		return nil, err
	}
	return buf.Bytes(), nil
}
