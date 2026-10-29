# ChordonBleh – design notes

## Purpose

ChordonBleh is a VCV Rack 2 module (plugin `goblinmode`) that turns a single note into a chord. It takes a V/Oct note, a key (tonic + scale), a chord type, an inversion and an octave shift, and outputs the chord as polyphonic V/Oct CV plus a matching polyphonic gate. Patch a sequencer or keyboard into `NOTE`, send `CHORD` into a polyphonic oscillator, and the module plays the chord for each note.

## Signal flow

```
NOTE (V/Oct) ──► round down to the closest note in key ──► chord root + scale degree
TONIC + KEY ───► defines the key ───────────────────────────┘
CHORD, INV, OCT ──► chord quality, inversion, octave shift ──► CHORD (poly V/Oct)
GATE IN ─────────────────────────────────────────────────────► GATE OUT (poly)
```

## Controls

Each knob has a matching CV input. CV is added to the knob and the result is rounded to a step and clamped.

| Control | Range | Notes |
|---|---|---|
| TONIC | C..B (12 steps) | Key tonic. CV is V/Oct (1 V = 12 semitones). |
| KEY | Major, Dorian, Phrygian, Lydian, Mixolydian, Minor, Locrian, Harmonic Minor, Melodic Minor, Harmonic Major, Phrygian Dominant, Double Harmonic | CV 0–10 V sweeps the full range. |
| CHORD | Auto + 13 types | See below. CV 0–10 V sweeps the full range. |
| INV | 0–3 | Clamped to (note count − 1). CV 0–10 V sweeps the full range. |
| OCT | −2..+2 | Shifts the whole chord. CV 0–10 V sweeps the full range. |
| STRUM | 0–500 ms | Continuous. Delay between consecutive gates. CV 0–10 V sweeps the full range. With a clock patched it picks a note value instead (see Strum). |

Inputs and outputs, in the bottom section: `NOTE` (V/Oct in), `GATE IN`, `CLOCK` (strum tempo, optional), `CHORD` (poly V/Oct out), `GATE OUT` (poly gate out).

Chord types: Auto, Major, Minor, Diminished, Augmented, Sus2, Sus4, Major 7, Minor 7, Dominant 7, Diminished 7, Major 6, Minor 6, Dominant 9. Chords have 3–5 notes, and the output channel count follows the note count. In `Auto`, the optional expander adds a 4th and a 5th note per scale degree (see Extension expander).

## Design decisions

- **Note input instead of scale degree.** The chord root comes from a V/Oct `NOTE` input, not a degree knob. The note is rounded down to the closest note in the key set by TONIC and KEY. An out-of-key C♯ in C major gives a C chord.
- **Octave carries through.** The chord sits in the octave of the incoming note, and OCT shifts it further. Notes below the tonic wrap down correctly (the key repeats every octave).
- **Unpatched `NOTE` uses the tonic.** The module always produces a chord, even with nothing patched.
- **Auto chord quality.** `Auto` builds the diatonic triad for the quantized note by stacking thirds from the scale (in C major: C, Dm, Em, F, G, Am, Bdim). The suffix is looked up from the chord table. Explicit chord types ignore the scale for quality and are built on the quantized note as their root.
- **Auto is triads unless the expander adds notes.** Without a `ChordonBlehExt` on its right, `Auto` never produces more than three notes.
- **Rounding tolerance.** The note voltage is floored after adding 1e-3 semitones, so exact semitone voltages are not pushed to the note below by float error. Slightly flat pitches still round down.
- **Inversion.** Range -5 to 5, default 0, CV -5 to 5 V (1 V per step). Positive steps move the lowest note up an octave, negative steps move the highest note down an octave, keeping the notes sorted. Steps beyond the chord size continue into the next octave rather than clamping.
- **Gate.** Polyphonic, one channel per chord note, 10 V while `GATE IN` is at or above 1 V. It is held high when `GATE IN` is unpatched.
- **Strum.** On a `GATE IN` rising edge the gates open one STRUM delay apart; all gates close together when `GATE IN` falls. Only gates are delayed, pitches are not. Changing the note while the gate is held does not re-strum. The direction is set in the context menu (saved with the patch): **Up** (default, lowest voice first), **Down** (highest first), **Up & down** (up, then back down in one strum; each voice except the top is re-triggered on the way down, with the gate low for 1 ms, skipped when the delay is under 2 ms), **Random** (new voice order on every gate).
- **Strum clock.** With `CLOCK` patched the STRUM knob (and its CV) picks the delay between consecutive notes as a note value by position: Off, 1/64, 1/32T, 1/32, 1/16T, 1/16, 1/8T, 1/8, so every voice lands on the grid whatever the chord size. The tempo is the time between the last two rising edges (5 ms to 4 s) and is kept when the clock stops; until two edges have been seen the delay is 0. What one clock pulse stands for is set in the context menu (**Clock pulse length**: 1/4 default, 1/8, 1/16, 1/32, saved with the patch). While a clock is patched, editing the knob snaps it to the nearest division (dragging adds up the distance moved, one division per 20 px, so it does not depend on mouse speed); the value is not touched when a clock is patched or unpatched, so the ms setting survives until the knob is moved. The tooltip and the changed-setting readout show the division, such as `Strum: 1/16`. `CLOCK` is the last input so cables in older patches keep their ports.
- **No root output.** A mono root output existed in an earlier version and was removed.
- **Naming.** The key-tonic knob is called TONIC so it is not confused with the chord root.
- **Chord recomputation.** The chord is rebuilt only when a quantized input changes, not every sample, and the audio path does not allocate.
- **Chord display.** A small display shows the chord name, such as `Cmaj7` or `Dm/1` or `Dm/-1` (inversion after the slash, hidden at 0). When a setting changes, by knob or CV, the display shows it instead, such as `Tonic: C#` or `Strum: 120 ms`, until another setting changes or a note is played (gate rising edge or quantized note change). It stays for at least 2 seconds, and a note arriving earlier clears it once the 2 seconds are up. The text shrinks to fit long values.
- **Brand.** Both panels end with a separator line and the text `goblin mode`. The base module's port row sits above it.
- **Panel labels are drawn in code.** Rack does not render `<text>` in panel SVGs, so labels are drawn by the widget. The SVG is a plain 12HP (60.96 mm) dark background.

## Extension expander (ChordonBlehExt)

A 7HP module placed directly to the right of ChordonBleh. It only picks values; the music logic stays in the base module (`buildChord`). Without it the base behaves as before.

- **Controls.** For each of the 7 scale degrees, two cycle buttons: a 4th note and a 5th note. A click steps to the next value and wraps after the last; the button face shows the current value. The 5th note does not need the 4th. Values are saved with the patch (they are stepped params). Vertical layout: one row per degree, with a light for the playing degree, then the 4th note column, then the 5th note column.
- **4th note options:** Off, Dia (the scale's 7th), 6, m7, M7.
- **5th note options:** Off, Dia (the scale's 9th), b9, 9, #9, 11, #11. No 13ths.
- **Chromatic on purpose.** M7 on degree VI in C major gives Am(M7), with G♯, not the diatonic Am7. `Dia` stacks thirds from the scale and so works in every key.
- **Auto only.** Explicit chord types ignore the extensions. A rebuild happens when the selected values for the current degree change.
- **Names.** A full interval set that matches the chord table gets its table name (`m7`, `maj7`, `9`, `dim7`), anything else shows as the triad plus the added notes, such as `Am(M7,b9)`. The base display is not used to show extension changes.
- **Messages.** Rack's expander message API, one sample of latency: the expander writes 14 ints (`ExtensionsMessage`) into the base's `rightExpander`, the base writes the current degree (`DegreeMessage`) into the expander's `leftExpander`. Both are in `ChordonBlehExpander.hpp`.

## Code layout

| File | Role |
|---|---|
| `src/ChordonBleh/ChordMath.hpp` | Pure chord logic with no Rack dependencies: scale and chord tables, `quantizeToKey`, `buildChord`. |
| `src/ChordonBleh/ChordonBleh.cpp` | Module (params, CV handling, `process()`), chord display widget, panel labels, module widget. |
| `src/ChordonBleh/ChordonBlehExt.cpp` | Expander module, the `CycleButton` widget and the expander widget. |
| `src/ChordonBleh/ChordonBlehExpander.hpp` | Message structs shared by the module and the expander. |
| `src/ChordonBleh/PanelText.hpp` | Panel font loader and `PanelLabel`, shared by both modules. |
| `res/ChordonBleh.svg`, `res/ChordonBlehExt.svg` | Panel backgrounds. |
| `test/ChordonBleh/chordmath_test.cpp` | Standalone test of the chord logic. |

Plugin-level files (`plugin.json`, `src/plugin.cpp`, `Makefile`) are described in the root `design.md`.

## Testing

- Chord logic test: `cd test/ChordonBleh && g++ -std=c++11 -I../../src/ChordonBleh chordmath_test.cpp -o "$TMPDIR/chordmath_test" && "$TMPDIR/chordmath_test"`.
- For building and installing, see the root `design.md`.

## Status

- Builds cleanly, installs, and loads in Rack without log warnings. The chord logic test passes.
- Not yet checked by ear or by eye: panel layout, the display, and audio output with a poly oscillator. The expander builds but has not been loaded in Rack yet.

## Possible next steps

- Drop-2 / open voicings.
- Per-note mono outputs through an expander.
- A designed panel (currently a plain background) and panel text converted to paths before release.
