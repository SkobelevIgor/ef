package main

import (
	"fmt"
	"os"
	"strconv"
	"strings"

	"ef/editor"
)

// parseFileArg parses a file argument that may include a line number
// Format: filename or filename:line
// Returns filename and line number (0 if not specified)
func parseFileArg(arg string) (string, int) {
	// Find the last colon that could be a line number separator
	lastColon := strings.LastIndex(arg, ":")
	if lastColon == -1 {
		return arg, 0
	}

	// Check if everything after the colon is a number
	lineStr := arg[lastColon+1:]
	lineNum, err := strconv.Atoi(lineStr)
	if err != nil || lineNum < 1 {
		// Not a valid line number, treat the whole thing as filename
		return arg, 0
	}

	return arg[:lastColon], lineNum
}

func main() {
	args := os.Args[1:]

	// Check for -h flag (horizontal split mode)
	// Default is vertical splits (side-by-side) for modern widescreen displays
	splitMode := editor.SplitVertical
	if len(args) > 0 && args[0] == "-h" {
		splitMode = editor.SplitHorizontal
		args = args[1:]
	}

	// Require at least one file
	if len(args) < 1 {
		fmt.Fprintln(os.Stderr, "Usage: ef [-h] <filename[:line]> [filename2[:line]] ...")
		os.Exit(1)
	}

	var fileInfos []editor.FileInfo
	for _, arg := range args {
		filename, line := parseFileArg(arg)
		fileInfos = append(fileInfos, editor.FileInfo{
			Filename: filename,
			Line:     line,
		})
	}

	ed, err := editor.New(fileInfos, splitMode)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error: %v\n", err)
		os.Exit(1)
	}

	if err := ed.Run(); err != nil {
		fmt.Fprintf(os.Stderr, "Error: %v\n", err)
		os.Exit(1)
	}
}