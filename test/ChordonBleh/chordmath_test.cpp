// Build and run: g++ -std=c++11 -I../src chordmath_test.cpp -o /tmp/chordmath_test && /tmp/chordmath_test
#include "ChordMath.hpp"
#include <cstdio>
#include <cstring>
#include <initializer_list>

using namespace chordmath;

static int failures = 0;

static void check(const char* label, const Chord& c, std::initializer_list<int> expected, const char* suffix) {
	bool ok = c.count == (int) expected.size() && std::strcmp(c.suffix, suffix) == 0;
	int i = 0;
	for (int e : expected) {
		if (ok && c.notes[i] != e)
			ok = false;
		i++;
	}
	if (!ok) {
		failures++;
		std::printf("FAIL %s: got count=%d suffix='%s' notes=", label, c.count, c.suffix);
		for (int j = 0; j < c.count; j++)
			std::printf("%d ", c.notes[j]);
		std::printf("\n");
	}
}

static void checkQuant(const char* label, KeyNote k, int semitone, int degree) {
	if (k.semitone != semitone || k.degree != degree) {
		failures++;
		std::printf("FAIL %s: got semitone=%d degree=%d\n", label, k.semitone, k.degree);
	}
}

int main() {
	check("C major", buildChord(0, 0, 0, 1, 0, 0), {0, 4, 7}, "");
	check("C major 1st inv", buildChord(0, 0, 0, 1, 1, 0), {4, 7, 12}, "");
	check("inversion past chord size", buildChord(0, 0, 0, 1, 5, 0), {19, 24, 28}, "");
	check("harmonic minor V = G major", buildChord(7, 7, 4, 0, 0, 0), {7, 11, 14}, "");
	check("harmonic minor III = Eb aug", buildChord(3, 7, 2, 0, 0, 0), {3, 7, 11}, "aug");
	check("octave up", buildChord(0, 0, 0, 1, 0, 1), {12, 16, 19}, "");
	check("auto C major II = Dm", buildChord(2, 0, 1, 0, 0, 0), {2, 5, 9}, "m");
	check("auto C major VII = Bdim", buildChord(11, 0, 6, 0, 0, 0), {11, 14, 17}, "dim");
	check("auto A minor I = Am", buildChord(9, 5, 0, 0, 0, 0), {9, 12, 16}, "m");
	check("C dom9 1st inv stays sorted", buildChord(0, 0, 0, 13, 1, 0), {4, 7, 10, 12, 14}, "9");
	check("root above first octave", buildChord(18, 0, 4, 1, 0, 0), {18, 22, 25}, "");
	check("negative root", buildChord(-1, 0, 6, 1, 0, 0), {-1, 3, 6}, "");

	// Extensions: Auto only, fourth then fifth. Options: 0 Off, 1 Dia, fourth 2 = 6, 3 = m7, 4 = M7, fifth 2 = b9, 3 = 9, 4 = #9, 5 = 11, 6 = #11
	check("C major VI + M7", buildChord(9, 0, 5, 0, 0, 0, {4, 0}), {9, 12, 16, 20}, "m(M7)");
	check("C major VI + M7 + b9", buildChord(9, 0, 5, 0, 0, 0, {4, 2}), {9, 12, 16, 20, 22}, "m(M7,b9)");
	check("C major VI + 9 only", buildChord(9, 0, 5, 0, 0, 0, {0, 3}), {9, 12, 16, 23}, "m(9)");
	check("C major VI diatonic 7th", buildChord(9, 0, 5, 0, 0, 0, {1, 0}), {9, 12, 16, 19}, "m7");
	check("C major VI diatonic 7th and 9th", buildChord(9, 0, 5, 0, 0, 0, {1, 1}), {9, 12, 16, 19, 23}, "m(m7,9)");
	check("C major I diatonic 7th", buildChord(0, 0, 0, 0, 0, 0, {1, 0}), {0, 4, 7, 11}, "maj7");
	check("C major V diatonic 7th and 9th = G9", buildChord(7, 0, 4, 0, 0, 0, {1, 1}), {7, 11, 14, 17, 21}, "9");
	check("C major V + m7", buildChord(7, 0, 4, 0, 0, 0, {3, 0}), {7, 11, 14, 17}, "7");
	check("C major VII diatonic 7th = Bm7b5 (no name)", buildChord(11, 0, 6, 0, 0, 0, {1, 0}), {11, 14, 17, 21}, "dim(m7)");
	check("C major VII + 6 = dim7", buildChord(11, 0, 6, 0, 0, 0, {2, 0}), {11, 14, 17, 20}, "dim7");
	check("Phrygian Dominant I + #11", buildChord(0, 10, 0, 0, 0, 0, {0, 6}), {0, 4, 7, 18}, "(#11)");
	check("extensions with inversion", buildChord(9, 0, 5, 0, 1, 0, {4, 2}), {12, 16, 20, 21, 22}, "m(M7,b9)");
	check("extensions ignored for explicit chord", buildChord(0, 0, 0, 1, 0, 0, {4, 2}), {0, 4, 7}, "");
	check("out of range options are clamped", buildChord(9, 0, 5, 0, 0, 0, {99, -3}), {9, 12, 16, 20}, "m(M7)");

	checkQuant("in-key note stays", quantizeToKey(4, 0, 0), 4, 2);
	checkQuant("C# rounds down to C in C major", quantizeToKey(1, 0, 0), 0, 0);
	checkQuant("A# rounds down to A in C major", quantizeToKey(10, 0, 0), 9, 5);
	checkQuant("below tonic wraps down an octave", quantizeToKey(-1, 0, 0), -1, 6);
	checkQuant("C# below tonic", quantizeToKey(-11, 0, 0), -12, 0);
	checkQuant("tonic offset: D in D minor", quantizeToKey(2, 2, 5), 2, 0);
	checkQuant("tonic offset: E in D minor stays", quantizeToKey(4, 2, 5), 4, 1);
	checkQuant("tonic offset: Eb rounds down to D", quantizeToKey(3, 2, 5), 2, 0);
	checkQuant("octave up", quantizeToKey(16, 0, 0), 16, 2);

	std::printf(failures ? "%d failure(s)\n" : "all passed\n", failures);
	return failures != 0;
}
