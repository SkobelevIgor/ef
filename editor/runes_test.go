package editor

import "testing"

func TestInsertRunes(t *testing.T) {
	line := []rune("hello")
	result := insertRunes(line, 2, []rune("XY"))
	if string(result) != "heXYllo" {
		t.Errorf("got %q", string(result))
	}

	// Insert at start
	result = insertRunes(line, 0, []rune("A"))
	if string(result) != "Ahello" {
		t.Errorf("got %q", string(result))
	}

	// Insert at end
	result = insertRunes(line, 5, []rune("!"))
	if string(result) != "hello!" {
		t.Errorf("got %q", string(result))
	}
}

func TestRemoveRunes(t *testing.T) {
	line := []rune("hello")
	result := removeRunes(line, 1, 3)
	if string(result) != "hlo" {
		t.Errorf("got %q", string(result))
	}

	// Remove single char
	result = removeRunes([]rune("abc"), 0, 1)
	if string(result) != "bc" {
		t.Errorf("got %q", string(result))
	}
}

func TestSpliceLines(t *testing.T) {
	lines := [][]rune{[]rune("a"), []rune("b"), []rune("c")}

	// Delete middle line
	result := spliceLines(lines, 1, 1, nil)
	if len(result) != 2 || string(result[0]) != "a" || string(result[1]) != "c" {
		t.Errorf("delete: %v", result)
	}

	// Insert line
	result = spliceLines(lines, 1, 0, [][]rune{[]rune("X")})
	if len(result) != 4 || string(result[1]) != "X" {
		t.Errorf("insert: %v", result)
	}

	// Replace line
	result = spliceLines(lines, 1, 1, [][]rune{[]rune("Y")})
	if len(result) != 3 || string(result[1]) != "Y" {
		t.Errorf("replace: %v", result)
	}
}
