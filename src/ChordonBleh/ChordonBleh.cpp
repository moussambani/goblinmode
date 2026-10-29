#include "plugin.hpp"
#include "ChordMath.hpp"
#include "ChordonBlehExpander.hpp"
#include "PanelText.hpp"

using namespace chordmath;


struct ChordonBleh : Module {
	enum ParamId {
		TONIC_PARAM,
		KEY_PARAM,
		CHORD_PARAM,
		INVERSION_PARAM,
		OCTAVE_PARAM,
		STRUM_PARAM,
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
		STRUM_INPUT,
		CLOCK_INPUT,	// last so cables in older patches keep their ports
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
	enum StrumMode {
		STRUM_UP,
		STRUM_DOWN,
		STRUM_UP_DOWN,
		STRUM_RANDOM,
		NUM_STRUM_MODES
	};

	// Shows divisions instead of milliseconds while a clock is patched
	struct StrumQuantity : ParamQuantity {
		bool clocked() {
			return module && module->inputs[CLOCK_INPUT].isConnected();
		}
		std::string getDisplayValueString() override {
			return clocked() ? DIVISIONS[divisionIndex(getValue())].name : ParamQuantity::getDisplayValueString();
		}
		std::string getUnit() override {
			return clocked() ? "" : ParamQuantity::getUnit();
		}
		// Edits snap to a division while a clock is patched; the value is left alone until then
		void setValue(float value) override {
			if (clocked())
				value = divisionValue(divisionIndex(value));
			ParamQuantity::setValue(value);
		}
	};

	Chord chord;
	int lastState[7] = {};
	int lastOctave = 0;
	bool built = false;
	int lastSettings[6] = {};
	bool lastGate = false;
	float strumTime = 0.f;	// seconds since the last gate rising edge
	int strumMode = STRUM_UP;
	int clockPulse = 0;	// index into CLOCK_PULSES: what one pulse of the clock input stands for
	dsp::SchmittTrigger clockTrigger;
	float clockTime = 0.f;		// seconds since the last clock pulse
	float clockPeriod = 0.f;	// seconds between the last two pulses, 0 until measured
	bool clockSeen = false;
	// Per channel, set on each gate rising edge: the strum step it opens on, and the step it re-opens on (-1 for none)
	int strumSlot[5] = {0, 1, 2, 3, 4};
	int strumRetrigger[5] = {-1, -1, -1, -1, -1};
	float settingTimer = 0.f;
	bool clearPending = false;

	// Read by the display widget
	int dispPitchClass = 0;
	const char* dispSuffix = "";
	int dispInversion = 0;
	int dispSetting = -1;	// index into SETTING_LABELS of the setting shown instead of the chord, or -1
	char dispSettingText[32] = "";

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
		configParam<StrumQuantity>(STRUM_PARAM, 0.f, 500.f, 0.f, "Strum", " ms");

		configInput(TONIC_INPUT, "Tonic (V/oct)");
		configInput(KEY_INPUT, "Key (0-10V)");
		configInput(NOTE_INPUT, "Note (V/oct), rounded down to the key (tonic when unpatched)");
		configInput(CHORD_INPUT, "Chord (0-10V)");
		configInput(INVERSION_INPUT, "Inversion (-5 to 5V)");
		configInput(OCTAVE_INPUT, "Octave (0-10V)");
		configInput(GATE_INPUT, "Gate (held high when unpatched)");
		configInput(STRUM_INPUT, "Strum (0-10V)");
		configInput(CLOCK_INPUT, "Clock (strum steps are note values of its tempo when patched)");

		configOutput(CHORD_OUTPUT, "Chord (polyphonic V/oct)");
		configOutput(GATE_OUTPUT, "Gate (polyphonic)");

		rightExpander.producerMessage = new ExtensionsMessage();
		rightExpander.consumerMessage = new ExtensionsMessage();
	}

	~ChordonBleh() {
		delete (ExtensionsMessage*) rightExpander.producerMessage;
		delete (ExtensionsMessage*) rightExpander.consumerMessage;
	}

	// Knob plus CV, where 10V of CV spans the whole range of the knob (1V per step for inversion's -5 to 5V).
	int stepped(int paramId, int inputId) {
		ParamQuantity* pq = paramQuantities[paramId];
		float v = params[paramId].getValue();
		if (inputs[inputId].isConnected())
			v += inputs[inputId].getVoltage() * (pq->maxValue - pq->minValue) / 10.f;
		return clamp((int) std::round(v), (int) pq->minValue, (int) pq->maxValue);
	}

	void startStrum(int count) {
		for (int i = 0; i < 5; i++) {
			strumSlot[i] = i;
			strumRetrigger[i] = -1;
		}
		if (strumMode == STRUM_DOWN) {
			for (int i = 0; i < count; i++)
				strumSlot[i] = count - 1 - i;
		}
		else if (strumMode == STRUM_UP_DOWN) {
			for (int i = 0; i < count - 1; i++)
				strumRetrigger[i] = 2 * count - 2 - i;
		}
		else if (strumMode == STRUM_RANDOM) {
			for (int i = count - 1; i > 0; i--)
				std::swap(strumSlot[i], strumSlot[random::u32() % (i + 1)]);
		}
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "strumMode", json_integer(strumMode));
		json_object_set_new(rootJ, "clockPulse", json_integer(clockPulse));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* modeJ = json_object_get(rootJ, "strumMode");
		if (modeJ)
			strumMode = clamp((int) json_integer_value(modeJ), 0, NUM_STRUM_MODES - 1);
		json_t* pulseJ = json_object_get(rootJ, "clockPulse");
		if (pulseJ)
			clockPulse = clamp((int) json_integer_value(pulseJ), 0, NUM_CLOCK_PULSES - 1);
	}

	void showSetting(int which, int value) {
		static const char* const SETTING_LABELS[6] = {"Tonic", "Key", "Chord", "Inv", "Oct", "Strum"};
		if (which == 0)
			std::snprintf(dispSettingText, sizeof(dispSettingText), "%s: %s", SETTING_LABELS[which], NOTE_NAMES[value]);
		else if (which == 1)
			std::snprintf(dispSettingText, sizeof(dispSettingText), "%s: %s", SETTING_LABELS[which], SCALE_NAMES[value]);
		else if (which == 2)
			std::snprintf(dispSettingText, sizeof(dispSettingText), "%s: %s", SETTING_LABELS[which], CHORDS[value].name);
		else if (which == 5 && value < 0)
			std::snprintf(dispSettingText, sizeof(dispSettingText), "%s: %s", SETTING_LABELS[which], DIVISIONS[-1 - value].name);
		else if (which == 5)
			std::snprintf(dispSettingText, sizeof(dispSettingText), "%s: %d ms", SETTING_LABELS[which], value);
		else
			std::snprintf(dispSettingText, sizeof(dispSettingText), "%s: %d", SETTING_LABELS[which], value);
		dispSetting = which;
		settingTimer = 2.f;
		clearPending = false;
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

		// The expander on the right picks the optional 4th and 5th note per degree, and shows which degree is playing
		Extensions ext = {0, 0};
		if (rightExpander.module && rightExpander.module->model == modelChordonBlehExt) {
			const ExtensionsMessage* in = (const ExtensionsMessage*) rightExpander.consumerMessage;
			ext = {in->fourth[keyNote.degree], in->fifth[keyNote.degree]};
			DegreeMessage* out = (DegreeMessage*) rightExpander.module->leftExpander.producerMessage;
			out->degree = keyNote.degree;
			rightExpander.module->leftExpander.messageFlipRequested = true;
		}

		int state[7] = {
			keyNote.semitone,
			key,
			keyNote.degree,
			stepped(CHORD_PARAM, CHORD_INPUT),
			stepped(INVERSION_PARAM, INVERSION_INPUT),
			ext.fourth,
			ext.fifth,
		};
		int octave = stepped(OCTAVE_PARAM, OCTAVE_INPUT);
		// Each gate opens one strum delay after the previous one; 10V of CV spans the whole 0-500 ms range
		float strumMs = params[STRUM_PARAM].getValue();
		if (inputs[STRUM_INPUT].isConnected())
			strumMs += inputs[STRUM_INPUT].getVoltage() * 50.f;
		strumMs = clamp(strumMs, 0.f, 500.f);
		float strumDelay = strumMs / 1000.f;
		int division = divisionIndex(strumMs);
		bool clocked = inputs[CLOCK_INPUT].isConnected();
		if (clocked) {
			// With a clock the knob picks a note value. The tempo is the time between the last two pulses, kept when the clock stops.
			if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 2.f)) {
				if (clockSeen && clockTime >= 0.005f && clockTime <= 4.f)
					clockPeriod = clockTime;
				clockSeen = true;
				clockTime = 0.f;
			}
			clockTime = std::min(clockTime + args.sampleTime, 10.f);
			strumDelay = strumDelaySeconds(division, clockPeriod, CLOCK_PULSES[clockPulse].quarters);
		}
		else {
			clockSeen = false;
		}
		bool gate = !inputs[GATE_INPUT].isConnected() || inputs[GATE_INPUT].getVoltage() >= 1.f;

		// A changed setting is shown until the next note, but for at least 2 seconds
		int settings[6] = {tonic, key, state[3], state[4], octave, clocked ? -1 - division : (int) std::round(strumMs)};
		bool noteEvent = (gate && !lastGate) || keyNote.semitone != lastState[0];
		if (gate && !lastGate) {
			strumTime = 0.f;
			startStrum(chord.count);
		}
		lastGate = gate;
		if (built) {
			int changed = 0;
			while (changed < 6 && settings[changed] == lastSettings[changed])
				changed++;
			if (changed < 6)
				showSetting(changed, settings[changed]);
			else if (noteEvent && dispSetting >= 0)
				clearPending = true;
		}
		std::copy(settings, settings + 6, lastSettings);
		if (clearPending && settingTimer <= 0.f) {
			dispSetting = -1;
			clearPending = false;
		}
		settingTimer -= args.sampleTime;

		if (!built || !std::equal(state, state + 7, lastState) || octave != lastOctave) {
			std::copy(state, state + 7, lastState);
			lastOctave = octave;
			built = true;
			chord = buildChord(state[0], state[1], state[2], state[3], state[4], octave, ext);
			dispPitchClass = chord.pitchClass;
			dispSuffix = chord.suffix;
			dispInversion = state[4];
		}

		strumTime = std::min(strumTime + args.sampleTime, 10.f);

		outputs[CHORD_OUTPUT].setChannels(chord.count);
		outputs[GATE_OUTPUT].setChannels(chord.count);
		for (int i = 0; i < chord.count; i++) {
			outputs[CHORD_OUTPUT].setVoltage(chord.notes[i] / 12.f, i);
			bool open = gate && strumTime >= strumSlot[i] * strumDelay;
			// Re-opening needs the gate to drop first; below 2 ms the gap does not fit between steps
			if (strumRetrigger[i] >= 0 && strumDelay >= 0.002f) {
				float t = strumTime - strumRetrigger[i] * strumDelay;
				if (t >= 0.f && t < 0.001f)
					open = false;
			}
			outputs[GATE_OUTPUT].setVoltage(open ? 10.f : 0.f, i);
		}
	}
};


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
			if (module && module->dispSetting >= 0)
				std::snprintf(text, sizeof(text), "%s", module->dispSettingText);
			else if (module)
				std::snprintf(text, sizeof(text), "%s%s", NOTE_NAMES[module->dispPitchClass], module->dispSuffix);
			else
				std::snprintf(text, sizeof(text), "Cmaj7");
			if (module && module->dispSetting < 0 && module->dispInversion != 0) {
				size_t n = std::strlen(text);
				std::snprintf(text + n, sizeof(text) - n, "/%d", module->dispInversion);
			}

			std::shared_ptr<window::Font> font = loadPanelFont();
			if (font) {
				nvgFontFaceId(args.vg, font->handle);
				// Long readouts such as "Chord: Diminished 7" shrink to fit the display
				nvgFontSize(args.vg, std::min(22.f, box.size.x * 0.95f / (std::strlen(text) * 0.55f)));
				nvgFillColor(args.vg, nvgRGB(0xff, 0xb0, 0x30));
				nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
				nvgText(args.vg, box.size.x / 2.f, box.size.y / 2.f, text, NULL);
			}
		}
		TransparentWidget::drawLayer(args, layer);
	}
};


// While a clock is patched the strum knob handles its own drag: the distance dragged is added up and moves one division
// per DRAG_PIXELS, however fast the mouse moves. Rack's drag adds each mouse event to the already snapped value instead,
// so slow movements are lost and fast ones overshoot.
struct ChordonBlehKnob : RoundSmallBlackKnob {
	static constexpr float DRAG_PIXELS = 20.f;
	float dragPosition = 0.f;	// in divisions

	ChordonBleh::StrumQuantity* clockedStrum() {
		ChordonBleh::StrumQuantity* strum = dynamic_cast<ChordonBleh::StrumQuantity*>(getParamQuantity());
		return strum && strum->clocked() ? strum : NULL;
	}

	void onDragStart(const DragStartEvent& e) override {
		ChordonBleh::StrumQuantity* strum = clockedStrum();
		if (strum)
			dragPosition = divisionIndex(strum->getValue());
		RoundSmallBlackKnob::onDragStart(e);
	}

	void onDragMove(const DragMoveEvent& e) override {
		ChordonBleh::StrumQuantity* strum = clockedStrum();
		if (!strum) {
			RoundSmallBlackKnob::onDragMove(e);
			return;
		}
		dragPosition = clamp(dragPosition - e.mouseDelta.y / DRAG_PIXELS, 0.f, (float) (NUM_DIVISIONS - 1));
		strum->setValue(divisionValue((int) std::round(dragPosition)));
	}
};


struct ChordonBlehWidget : ModuleWidget {
	void addLabel(Vec posMm, const std::string& text, float fontSize = 8.f, NVGcolor color = nvgRGB(0xd8, 0xd8, 0xe0)) {
		PanelLabel* label = new PanelLabel;
		label->text = text;
		label->fontSize = fontSize;
		label->color = color;
		label->box.pos = mm2px(posMm);
		addChild(label);
	}

	void appendContextMenu(Menu* menu) override {
		ChordonBleh* module = dynamic_cast<ChordonBleh*>(this->module);
		if (!module)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Strum direction",
			{"Up", "Down", "Up & down", "Random"},
			[=]() { return (size_t) module->strumMode; },
			[=](size_t mode) { module->strumMode = (int) mode; }
		));
		std::vector<std::string> pulseNames;
		for (const Division& d : CLOCK_PULSES)
			pulseNames.push_back(d.name);
		menu->addChild(createIndexSubmenuItem("Clock pulse length",
			pulseNames,
			[=]() { return (size_t) module->clockPulse; },
			[=](size_t pulse) { module->clockPulse = (int) pulse; }
		));
	}

	ChordonBlehWidget(ChordonBleh* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/ChordonBleh.svg")));

		addLabel(Vec(30.48f, 5.f), "CHORDONBLEH", 10.f);

		ChordDisplay* display = new ChordDisplay;
		display->module = module;
		display->box.pos = mm2px(Vec(5.08f, 10.f));
		display->box.size = mm2px(Vec(50.8f, 12.f));
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
			{"STRUM", ChordonBleh::STRUM_PARAM, ChordonBleh::STRUM_INPUT},
		};
		float y = 32.f;
		for (const Row& row : rows) {
			addParam(createParamCentered<ChordonBlehKnob>(mm2px(Vec(12.7f, y)), module, row.param));
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(48.26f, y)), module, row.input));
			addLabel(Vec(30.48f, y), row.label);
			y += 12.5f;
		}

		const float portY = 111.f;
		const char* const portLabels[5] = {"NOTE", "GATE IN", "CLOCK", "CHORD", "GATE OUT"};
		for (int i = 0; i < 5; i++)
			addLabel(Vec(6.35f + i * 12.065f, 104.5f), portLabels[i], 6.5f, i < 3 ? nvgRGB(0xd8, 0xd8, 0xe0) : nvgRGB(0x26, 0x26, 0x2e));
		addLabel(Vec(30.48f, 123.5f), "goblin mode", 8.f);
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.35f, portY)), module, ChordonBleh::NOTE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.415f, portY)), module, ChordonBleh::GATE_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(30.48f, portY)), module, ChordonBleh::CLOCK_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(42.545f, portY)), module, ChordonBleh::CHORD_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(54.61f, portY)), module, ChordonBleh::GATE_OUTPUT));
	}
};


Model* modelChordonBleh = createModel<ChordonBleh, ChordonBlehWidget>("ChordonBleh");
