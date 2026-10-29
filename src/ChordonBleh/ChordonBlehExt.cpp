#include "plugin.hpp"
#include "ChordMath.hpp"
#include "ChordonBlehExpander.hpp"
#include "PanelText.hpp"

using namespace chordmath;


struct ChordonBlehExt : Module {
	enum ParamId {
		FOURTH_PARAM,	// 7 params, one per scale degree
		FIFTH_PARAM = FOURTH_PARAM + 7,
		PARAMS_LEN = FIFTH_PARAM + 7
	};
	enum InputId {
		INPUTS_LEN
	};
	enum OutputId {
		OUTPUTS_LEN
	};
	enum LightId {
		DEGREE_LIGHT,	// 7 lights, one per scale degree
		LIGHTS_LEN = DEGREE_LIGHT + 7
	};

	ChordonBlehExt() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		std::vector<std::string> fourthLabels;
		for (const Extension& e : FOURTH_NOTES)
			fourthLabels.push_back(e.label);
		std::vector<std::string> fifthLabels;
		for (const Extension& e : FIFTH_NOTES)
			fifthLabels.push_back(e.label);
		for (int d = 0; d < 7; d++) {
			configSwitch(FOURTH_PARAM + d, 0.f, NUM_FOURTH_NOTES - 1, 0.f, std::string(DEGREE_NAMES[d]) + " 4th note", fourthLabels);
			configSwitch(FIFTH_PARAM + d, 0.f, NUM_FIFTH_NOTES - 1, 0.f, std::string(DEGREE_NAMES[d]) + " 5th note", fifthLabels);
			configLight(DEGREE_LIGHT + d, std::string(DEGREE_NAMES[d]) + " playing");
		}

		leftExpander.producerMessage = new DegreeMessage();
		leftExpander.consumerMessage = new DegreeMessage();
	}

	~ChordonBlehExt() {
		delete (DegreeMessage*) leftExpander.producerMessage;
		delete (DegreeMessage*) leftExpander.consumerMessage;
	}

	void process(const ProcessArgs& args) override {
		bool attached = leftExpander.module && leftExpander.module->model == modelChordonBleh;
		int degree = attached ? ((const DegreeMessage*) leftExpander.consumerMessage)->degree : -1;
		for (int d = 0; d < 7; d++)
			lights[DEGREE_LIGHT + d].setBrightness(d == degree ? 1.f : 0.f);
		if (!attached)
			return;

		ExtensionsMessage* out = (ExtensionsMessage*) leftExpander.module->rightExpander.producerMessage;
		for (int d = 0; d < 7; d++) {
			out->fourth[d] = (int) std::round(params[FOURTH_PARAM + d].getValue());
			out->fifth[d] = (int) std::round(params[FIFTH_PARAM + d].getValue());
		}
		leftExpander.module->rightExpander.messageFlipRequested = true;
	}
};


// Steps through the values on each click and shows the current one on its face
struct CycleButton : app::Switch {
	void draw(const DrawArgs& args) override {
		ParamQuantity* pq = getParamQuantity();
		std::string text = pq ? pq->getDisplayValueString() : "Off";
		bool off = !pq || pq->getValue() < 0.5f;

		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 3.f);
		nvgFillColor(args.vg, nvgRGB(0x10, 0x10, 0x14));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, nvgRGB(0x3a, 0x3a, 0x46));
		nvgStrokeWidth(args.vg, 1.f);
		nvgStroke(args.vg);

		std::shared_ptr<window::Font> font = loadPanelFont();
		if (font) {
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, 11.f);
			nvgFillColor(args.vg, off ? nvgRGB(0x60, 0x60, 0x6c) : nvgRGB(0xff, 0xb0, 0x30));
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgText(args.vg, box.size.x / 2.f, box.size.y / 2.f, text.c_str(), NULL);
		}
	}
};


struct ChordonBlehExtWidget : ModuleWidget {
	void addLabel(Vec posMm, const std::string& text, float fontSize = 8.f) {
		PanelLabel* label = new PanelLabel;
		label->text = text;
		label->fontSize = fontSize;
		label->box.pos = mm2px(posMm);
		addChild(label);
	}

	ChordonBlehExtWidget(ChordonBlehExt* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/ChordonBlehExt.svg")));

		const float centerX = 17.78f;
		addLabel(Vec(centerX, 5.f), "CHORDONBLEH", 8.f);
		addLabel(Vec(centerX, 10.f), "EXT", 8.f);

		// Columns: playing degree, 4th note, 5th note
		const float degreeX = 6.f;
		const float columnX[2] = {17.5f, 28.5f};
		const Vec buttonSize = Vec(9.f, 7.f);
		addLabel(Vec(degreeX, 20.f), "DEG", 6.5f);
		addLabel(Vec(columnX[0], 20.f), "4TH", 6.5f);
		addLabel(Vec(columnX[1], 20.f), "5TH", 6.5f);

		for (int d = 0; d < 7; d++) {
			float y = 30.f + d * 12.5f;
			addLabel(Vec(degreeX, y - 2.5f), DEGREE_NAMES[d], 7.f);
			addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(degreeX, y + 3.f)), module, ChordonBlehExt::DEGREE_LIGHT + d));
			for (int col = 0; col < 2; col++) {
				CycleButton* button = createParam<CycleButton>(mm2px(Vec(columnX[col], y) - buttonSize / 2.f), module, (col ? ChordonBlehExt::FIFTH_PARAM : ChordonBlehExt::FOURTH_PARAM) + d);
				button->box.size = mm2px(buttonSize);
				addParam(button);
			}
		}

		addLabel(Vec(centerX, 123.5f), "goblin mode", 8.f);
	}
};


Model* modelChordonBlehExt = createModel<ChordonBlehExt, ChordonBlehExtWidget>("ChordonBlehExt");
