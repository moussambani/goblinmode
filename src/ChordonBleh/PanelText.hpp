#pragma once
#include "plugin.hpp"


static std::shared_ptr<window::Font> loadPanelFont() {
	return APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
}


// Rack does not render <text> in panel SVGs, so labels are drawn by the widget.
struct PanelLabel : TransparentWidget {
	std::string text;
	float fontSize = 8.f;
	NVGcolor color = nvgRGB(0xd8, 0xd8, 0xe0);

	void draw(const DrawArgs& args) override {
		std::shared_ptr<window::Font> font = loadPanelFont();
		if (!font)
			return;
		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, fontSize);
		nvgFillColor(args.vg, color);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgText(args.vg, 0.f, 0.f, text.c_str(), NULL);
	}
};
