#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>


// Pure chord logic, no Rack dependencies so it can be tested standalone.
namespace chordmath {

const int NUM_SCALES = 12;
const char* const SCALE_NAMES[NUM_SCALES] = {
	"Major", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Minor", "Locrian",
	"Harmonic Minor", "Melodic Minor", "Harmonic Major", "Phrygian Dominant", "Double Harmonic",
};
const int SCALES[NUM_SCALES][7] = {
	{0, 2, 4, 5, 7, 9, 11},
	{0, 2, 3, 5, 7, 9, 10},
	{0, 1, 3, 5, 7, 8, 10},
	{0, 2, 4, 6, 7, 9, 11},
	{0, 2, 4, 5, 7, 9, 10},
	{0, 2, 3, 5, 7, 8, 10},
	{0, 1, 3, 5, 6, 8, 10},
	{0, 2, 3, 5, 7, 8, 11},
	{0, 2, 3, 5, 7, 9, 11},
	{0, 2, 4, 5, 7, 8, 11},
	{0, 1, 4, 5, 7, 8, 10},
	{0, 1, 4, 5, 7, 8, 11},
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

// Optional 4th and 5th notes that Auto can add on top of the triad. Option 0 is Off and option 1 is Diatonic
// (the next third stacked from the scale); the others are chromatic, with the interval in semitones above the root.
struct Extension {
	const char* label;
	int interval;
};

const int NUM_FOURTH_NOTES = 5;
const Extension FOURTH_NOTES[NUM_FOURTH_NOTES] = {
	{"Off", 0}, {"Dia", 0}, {"6", 9}, {"m7", 10}, {"M7", 11},
};

const int NUM_FIFTH_NOTES = 7;
const Extension FIFTH_NOTES[NUM_FIFTH_NOTES] = {
	{"Off", 0}, {"Dia", 0}, {"b9", 13}, {"9", 14}, {"#9", 15}, {"11", 17}, {"#11", 18},
};

// Selected options for the degree being built
struct Extensions {
	int fourth;
	int fifth;
};

struct Chord {
	int notes[5];		// semitones relative to C4, ascending
	int count;
	int root;			// semitones relative to C4, before inversion
	int pitchClass;		// 0-11
	char suffix[16];
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

inline const char* findSuffix(const int* intervals, int count) {
	for (int t = 1; t < NUM_CHORDS; t++) {
		const ChordType& ct = CHORDS[t];
		if (ct.count == count && std::equal(intervals, intervals + count, ct.intervals))
			return ct.suffix;
	}
	return NULL;
}

// Semitones above the chord root of an option's note, or -1 for Off. thirds is how many thirds are stacked
// from the scale for Diatonic (3 for the 7th, 4 for the 9th).
inline int extensionInterval(const Extension* table, int numOptions, int option, int scale, int degree, int thirds) {
	option = std::min(std::max(option, 0), numOptions - 1);
	if (option == 0)
		return -1;
	if (option > 1)
		return table[option].interval;
	int idx = degree + 2 * thirds;
	return SCALES[scale][idx % 7] + 12 * (idx / 7) - SCALES[scale][degree];
}

inline const char* extensionLabel(const Extension* table, int numOptions, int interval) {
	for (int i = 2; i < numOptions; i++) {
		if (table[i].interval == interval)
			return table[i].label;
	}
	return "?";
}

// root is the chord root in semitones relative to C4, degree (0-6) is its position in the scale
// (only used by Auto, which also adds the optional 4th and 5th notes from ext), a positive inversion moves the lowest
// note up an octave per step, a negative one moves the highest note down (steps beyond count - 1 keep going into the
// next octave), octave shifts the whole chord.
inline Chord buildChord(int root, int scale, int degree, int chordIdx, int inversion, int octave, Extensions ext = {0, 0}) {
	Chord c = {};
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
		const char* triadSuffix = findSuffix(intervals, 3);
		if (!triadSuffix)
			triadSuffix = "";

		const char* labels[2];
		int numLabels = 0;
		int fourth = extensionInterval(FOURTH_NOTES, NUM_FOURTH_NOTES, ext.fourth, scale, degree, 3);
		int fifth = extensionInterval(FIFTH_NOTES, NUM_FIFTH_NOTES, ext.fifth, scale, degree, 4);
		if (fourth >= 0) {
			intervals[c.count++] = fourth;
			labels[numLabels++] = extensionLabel(FOURTH_NOTES, NUM_FOURTH_NOTES, fourth);
		}
		if (fifth >= 0) {
			intervals[c.count++] = fifth;
			labels[numLabels++] = extensionLabel(FIFTH_NOTES, NUM_FIFTH_NOTES, fifth);
		}

		// A known chord such as m7 gets its own name, anything else is the triad with the added notes in brackets
		const char* known = numLabels > 0 ? findSuffix(intervals, c.count) : triadSuffix;
		if (known)
			std::snprintf(c.suffix, sizeof(c.suffix), "%s", known);
		else if (numLabels == 1)
			std::snprintf(c.suffix, sizeof(c.suffix), "%s(%s)", triadSuffix, labels[0]);
		else
			std::snprintf(c.suffix, sizeof(c.suffix), "%s(%s,%s)", triadSuffix, labels[0], labels[1]);
	}
	else {
		const ChordType& ct = CHORDS[chordIdx];
		c.count = ct.count;
		std::copy(ct.intervals, ct.intervals + ct.count, intervals);
		std::snprintf(c.suffix, sizeof(c.suffix), "%s", ct.suffix);
	}

	for (int i = 0; i < c.count; i++)
		c.notes[i] = c.root + intervals[i];

	for (int i = 0; i < inversion; i++) {
		c.notes[0] += 12;
		std::sort(c.notes, c.notes + c.count);
	}
	for (int i = 0; i > inversion; i--) {
		c.notes[c.count - 1] -= 12;
		std::sort(c.notes, c.notes + c.count);
	}
	return c;
}

} // namespace chordmath
