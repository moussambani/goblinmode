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
	check("inversion clamps", buildChord(0, 0, 0, 1, 5, 0), {7, 12, 16}, "");
	check("octave up", buildChord(0, 0, 0, 1, 0, 1), {12, 16, 19}, "");
	check("auto C major II = Dm", buildChord(2, 0, 1, 0, 0, 0), {2, 5, 9}, "m");
	check("auto C major VII = Bdim", buildChord(11, 0, 6, 0, 0, 0), {11, 14, 17}, "dim");
	check("auto A minor I = Am", buildChord(9, 5, 0, 0, 0, 0), {9, 12, 16}, "m");
	check("C dom9 1st inv stays sorted", buildChord(0, 0, 0, 13, 1, 0), {4, 7, 10, 12, 14}, "9");
	check("root above first octave", buildChord(18, 0, 4, 1, 0, 0), {18, 22, 25}, "");
	check("negative root", buildChord(-1, 0, 6, 1, 0, 0), {-1, 3, 6}, "");

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
