#pragma once
#include <algorithm>
#include <cmath>


// Pure chord logic, no Rack dependencies so it can be tested standalone.
namespace chordmath {

const int NUM_SCALES = 7;
const char* const SCALE_NAMES[NUM_SCALES] = {
	"Major", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Minor", "Locrian",
};
const int SCALES[NUM_SCALES][7] = {
	{0, 2, 4, 5, 7, 9, 11},
	{0, 2, 3, 5, 7, 9, 10},
	{0, 1, 3, 5, 7, 8, 10},
	{0, 2, 4, 6, 7, 9, 11},
	{0, 2, 4, 5, 7, 9, 10},
	{0, 2, 3, 5, 7, 8, 10},
	{0, 1, 3, 5, 6, 8, 10},
};

const char* const NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
const char* const DEGREE_NAMES[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};

struct ChordType {
	const char* name;
	const char* suffix;
	int count;
	int intervals[5];
};

// Index 0 is Auto: a diatonic triad derived from the scale and degree.
const int NUM_CHORDS = 14;
const ChordType CHORDS[NUM_CHORDS] = {
	{"Auto", "", 0, {}},
	{"Major", "", 3, {0, 4, 7}},
	{"Minor", "m", 3, {0, 3, 7}},
	{"Diminished", "dim", 3, {0, 3, 6}},
	{"Augmented", "aug", 3, {0, 4, 8}},
	{"Sus2", "sus2", 3, {0, 2, 7}},
	{"Sus4", "sus4", 3, {0, 5, 7}},
	{"Major 7", "maj7", 4, {0, 4, 7, 11}},
	{"Minor 7", "m7", 4, {0, 3, 7, 10}},
	{"Dominant 7", "7", 4, {0, 4, 7, 10}},
	{"Diminished 7", "dim7", 4, {0, 3, 6, 9}},
	{"Major 6", "6", 4, {0, 4, 7, 9}},
	{"Minor 6", "m6", 4, {0, 3, 7, 9}},
	{"Dominant 9", "9", 5, {0, 4, 7, 10, 14}},
};

struct Chord {
	int notes[5];		// semitones relative to C4, ascending
	int count;
	int root;			// semitones relative to C4, before inversion
	int pitchClass;		// 0-11
	const char* suffix;
};

struct KeyNote {
	int semitone;	// relative to C4
	int degree;		// 0-6
};

// Rounds a note (semitones relative to C4) down to the closest note in the key.
inline KeyNote quantizeToKey(int note, int tonic, int scale) {
	int rel = note - tonic;
	int octave = (int) std::floor(rel / 12.0);
	int pc = rel - 12 * octave;
	int degree = 0;
	for (int d = 1; d < 7; d++) {
		if (SCALES[scale][d] <= pc)
			degree = d;
	}
	return {tonic + 12 * octave + SCALES[scale][degree], degree};
}

// root is the chord root in semitones relative to C4, degree (0-6) is its position in the scale
// (only used by Auto), inversion is clamped to count - 1, octave shifts the whole chord.
inline Chord buildChord(int root, int scale, int degree, int chordIdx, int inversion, int octave) {
	Chord c;
	int degreeOffset = SCALES[scale][degree];
	c.pitchClass = ((root % 12) + 12) % 12;
	c.root = root + 12 * octave;

	int intervals[5];
	if (chordIdx == 0) {
		// Stack thirds from the scale
		c.count = 3;
		for (int k = 0; k < 3; k++) {
			int idx = degree + 2 * k;
			intervals[k] = SCALES[scale][idx % 7] + 12 * (idx / 7) - degreeOffset;
		}
		c.suffix = "";
		for (int t = 1; t < NUM_CHORDS; t++) {
			const ChordType& ct = CHORDS[t];
			if (ct.count == 3 && std::equal(intervals, intervals + 3, ct.intervals))
				c.suffix = ct.suffix;
		}
	}
	else {
		const ChordType& ct = CHORDS[chordIdx];
		c.count = ct.count;
		std::copy(ct.intervals, ct.intervals + ct.count, intervals);
		c.suffix = ct.suffix;
	}

	for (int i = 0; i < c.count; i++)
		c.notes[i] = c.root + intervals[i];

	int inv = std::min(inversion, c.count - 1);
	for (int i = 0; i < inv; i++) {
		c.notes[0] += 12;
		std::sort(c.notes, c.notes + c.count);
	}
	return c;
}

} // namespace chordmath
