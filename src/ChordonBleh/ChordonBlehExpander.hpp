#pragma once


// Messages between ChordonBleh and the ChordonBlehExt expander on its right.

// Expander to base: the selected option (see FOURTH_NOTES and FIFTH_NOTES in ChordMath.hpp) for each scale degree
struct ExtensionsMessage {
	int fourth[7] = {};
	int fifth[7] = {};
};

// Base to expander: the scale degree (0-6) of the chord being played
struct DegreeMessage {
	int degree = 0;
};
