#include "plugin.hpp"
#include "ChordMath.hpp"

using namespace chordmath;


struct ChordonBleh : Module {
	enum ParamId {
		TONIC_PARAM,
		KEY_PARAM,
		CHORD_PARAM,
		INVERSION_PARAM,
		OCTAVE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		TONIC_INPUT,
		KEY_INPUT,
		NOTE_INPUT,
		CHORD_INPUT,
		INVERSION_INPUT,
		OCTAVE_INPUT,
		GATE_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		CHORD_OUTPUT,
		GATE_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	Chord chord;
	int lastState[5] = {};
	int lastOctave = 0;
	bool built = false;

	// Read by the display widget
	int dispPitchClass = 0;
	const char* dispSuffix = "";
	int dispInversion = 0;

	ChordonBleh() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		std::vector<std::string> rootLabels(NOTE_NAMES, NOTE_NAMES + 12);
		std::vector<std::string> keyLabels(SCALE_NAMES, SCALE_NAMES + NUM_SCALES);
		std::vector<std::string> chordLabels;
		for (const ChordType& ct : CHORDS)
			chordLabels.push_back(ct.name);

		configSwitch(TONIC_PARAM, 0.f, 11.f, 0.f, "Tonic", rootLabels);
		configSwitch(KEY_PARAM, 0.f, NUM_SCALES - 1, 0.f, "Key", keyLabels);
		configSwitch(CHORD_PARAM, 0.f, NUM_CHORDS - 1, 0.f, "Chord", chordLabels);
		configParam(INVERSION_PARAM, -5.f, 5.f, 0.f, "Inversion");
		paramQuantities[INVERSION_PARAM]->snapEnabled = true;
		configParam(OCTAVE_PARAM, -2.f, 2.f, 0.f, "Octave");
		paramQuantities[OCTAVE_PARAM]->snapEnabled = true;

		configInput(TONIC_INPUT, "Tonic (V/oct)");
		configInput(KEY_INPUT, "Key (0-10V)");
		configInput(NOTE_INPUT, "Note (V/oct), rounded down to the key (tonic when unpatched)");
		configInput(CHORD_INPUT, "Chord (0-10V)");
		configInput(INVERSION_INPUT, "Inversion (-5 to 5V)");
		configInput(OCTAVE_INPUT, "Octave (0-10V)");
		configInput(GATE_INPUT, "Gate (held high when unpatched)");

		configOutput(CHORD_OUTPUT, "Chord (polyphonic V/oct)");
		configOutput(GATE_OUTPUT, "Gate (polyphonic)");
	}

	// Knob plus CV, where 10V of CV spans the whole range of the knob (1V per step for inversion's -5 to 5V).
	int stepped(int paramId, int inputId) {
		ParamQuantity* pq = paramQuantities[paramId];
		float v = params[paramId].getValue();
		if (inputs[inputId].isConnected())
			v += inputs[inputId].getVoltage() * (pq->maxValue - pq->minValue) / 10.f;
		return clamp((int) std::round(v), (int) pq->minValue, (int) pq->maxValue);
	}

	void process(const ProcessArgs& args) override {
		int tonic = (int) std::round(params[TONIC_PARAM].getValue());
		if (inputs[TONIC_INPUT].isConnected())
			tonic += (int) std::round(inputs[TONIC_INPUT].getVoltage() * 12.f);
		tonic = ((tonic % 12) + 12) % 12;

		int key = stepped(KEY_PARAM, KEY_INPUT);

		// Epsilon keeps exact semitone voltages from flooring to the note below
		int note = tonic;
		if (inputs[NOTE_INPUT].isConnected())
			note = (int) std::floor(inputs[NOTE_INPUT].getVoltage() * 12.f + 1e-3f);
		KeyNote keyNote = quantizeToKey(note, tonic, key);

		int state[5] = {
			keyNote.semitone,
			key,
			keyNote.degree,
			stepped(CHORD_PARAM, CHORD_INPUT),
			stepped(INVERSION_PARAM, INVERSION_INPUT),
		};
		int octave = stepped(OCTAVE_PARAM, OCTAVE_INPUT);

		if (!built || !std::equal(state, state + 5, lastState) || octave != lastOctave) {
			std::copy(state, state + 5, lastState);
			lastOctave = octave;
			built = true;
			chord = buildChord(state[0], state[1], state[2], state[3], state[4], octave);
			dispPitchClass = chord.pitchClass;
			dispSuffix = chord.suffix;
			dispInversion = state[4];
		}

		bool gate = !inputs[GATE_INPUT].isConnected() || inputs[GATE_INPUT].getVoltage() >= 1.f;

		outputs[CHORD_OUTPUT].setChannels(chord.count);
		outputs[GATE_OUTPUT].setChannels(chord.count);
		for (int i = 0; i < chord.count; i++) {
			outputs[CHORD_OUTPUT].setVoltage(chord.notes[i] / 12.f, i);
			outputs[GATE_OUTPUT].setVoltage(gate ? 10.f : 0.f, i);
		}
	}
};


static std::shared_ptr<window::Font> loadPanelFont() {
	return APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
}


struct ChordDisplay : TransparentWidget {
	ChordonBleh* module = NULL;

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 3.f);
		nvgFillColor(args.vg, nvgRGB(0x10, 0x10, 0x14));
		nvgFill(args.vg);
		TransparentWidget::draw(args);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			char text[32];
			if (module)
				std::snprintf(text, sizeof(text), "%s%s", NOTE_NAMES[module->dispPitchClass], module->dispSuffix);
			else
				std::snprintf(text, sizeof(text), "Cmaj7");
			if (module && module->dispInversion != 0) {
				size_t n = std::strlen(text);
				std::snprintf(text + n, sizeof(text) - n, "/%d", module->dispInversion);
			}

			std::shared_ptr<window::Font> font = loadPanelFont();
			if (font) {
				nvgFontFaceId(args.vg, font->handle);
				nvgFontSize(args.vg, 22.f);
				nvgFillColor(args.vg, nvgRGB(0xff, 0xb0, 0x30));
				nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
				nvgText(args.vg, box.size.x / 2.f, box.size.y / 2.f, text, NULL);
			}
		}
		TransparentWidget::drawLayer(args, layer);
	}
};


// Rack does not render <text> in panel SVGs, so labels are drawn by the widget.
struct PanelLabel : TransparentWidget {
	std::string text;
	float fontSize = 8.f;

	void draw(const DrawArgs& args) override {
		std::shared_ptr<window::Font> font = loadPanelFont();
		if (!font)
			return;
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, fontSize);
		nvgFillColor(args.vg, nvgRGB(0xd8, 0xd8, 0xe0));
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgText(args.vg, 0.f, 0.f, text.c_str(), NULL);
	}
};


struct ChordonBlehWidget : ModuleWidget {
	void addLabel(Vec posMm, const std::string& text, float fontSize = 8.f) {
		PanelLabel* label = new PanelLabel;
		label->text = text;
		label->fontSize = fontSize;
		label->box.pos = mm2px(posMm);
		addChild(label);
	}

	ChordonBlehWidget(ChordonBleh* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/ChordonBleh.svg")));

		addLabel(Vec(25.4f, 5.f), "CHORDONBLEH", 10.f);

		ChordDisplay* display = new ChordDisplay;
		display->module = module;
		display->box.pos = mm2px(Vec(5.08f, 10.f));
		display->box.size = mm2px(Vec(40.64f, 12.f));
		addChild(display);

		struct Row {
			const char* label;
			ChordonBleh::ParamId param;
			ChordonBleh::InputId input;
		};
		const Row rows[] = {
			{"TONIC", ChordonBleh::TONIC_PARAM, ChordonBleh::TONIC_INPUT},
			{"KEY", ChordonBleh::KEY_PARAM, ChordonBleh::KEY_INPUT},
			{"CHORD", ChordonBleh::CHORD_PARAM, ChordonBleh::CHORD_INPUT},
			{"INV", ChordonBleh::INVERSION_PARAM, ChordonBleh::INVERSION_INPUT},
			{"OCT", ChordonBleh::OCTAVE_PARAM, ChordonBleh::OCTAVE_INPUT},
		};
		float y = 36.f;
		for (const Row& row : rows) {
			addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(12.7f, y)), module, row.param));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(38.1f, y)), module, row.input));
			addLabel(Vec(25.4f, y), row.label);
			y += 14.f;
		}

		const float portY = 115.f;
		addLabel(Vec(6.35f, 108.f), "NOTE", 6.5f);
		addLabel(Vec(19.05f, 108.f), "GATE IN", 6.5f);
		addLabel(Vec(31.75f, 108.f), "CHORD", 6.5f);
		addLabel(Vec(44.45f, 108.f), "GATE OUT", 6.5f);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.35f, portY)), module, ChordonBleh::NOTE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(19.05f, portY)), module, ChordonBleh::GATE_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(31.75f, portY)), module, ChordonBleh::CHORD_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(44.45f, portY)), module, ChordonBleh::GATE_OUTPUT));
	}
};


Model* modelChordonBleh = createModel<ChordonBleh, ChordonBlehWidget>("ChordonBleh");
