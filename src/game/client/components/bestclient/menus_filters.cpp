/* ChainsawClient tab: filters block (bc_flt_*, 08.11; the same settings as the "Filters" page of the Right Shift menu).
   One table: a switch per filter, its sliders and color under it while it is on. */
#include <engine/font_icons.h>
#include <engine/shared/config.h>

#include <game/client/components/bestclient/fx/cc_postfx.h>
#include <game/client/components/bestclient/ui_theme/style.h>
#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/localization.h>

#include <algorithm>
#include <vector>

namespace
{
constexpr float LINE_SIZE = 20.0f;
constexpr float MARGIN_SMALL = 5.0f;
constexpr float HEADLINE_FONT_SIZE = 20.0f;
constexpr float MARGIN_BETWEEN_VIEWS = 30.0f;
constexpr float ROW = MARGIN_SMALL + LINE_SIZE;

// min / max of the config variables (like CcCfg in chainsaw_gui.cpp)
namespace FltCfg
{
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) [[maybe_unused]] constexpr int Name##_Min = Min; [[maybe_unused]] constexpr int Name##_Max = Max;
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc)
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Flags, Desc)
#include <engine/shared/config_variables_bestclient.h>
#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_COL
#undef MACRO_CONFIG_STR
}

struct SFltSlider
{
	int *m_pValue;
	int m_Default;
	const char *m_pLabel;
	int m_Min, m_Max;
	const char *m_pSuffix;
};

struct SFltColor
{
	unsigned *m_pValue;
	unsigned m_Default;
	const char *m_pLabel;
};

struct SFlt
{
	int *m_pOn;
	const char *m_pName;
	std::vector<SFltSlider> m_vSliders;
	SFltColor m_Color = {nullptr, 0, nullptr};
};

#define FLT_S(Var, Label, Suffix) SFltSlider{&g_Config.m_##Var, DefaultConfig::Var, Label, FltCfg::Var##_Min, FltCfg::Var##_Max, Suffix}
}

void CMenus::RenderSettingsChainsawFilters(CUIRect &Column)
{
	static const std::vector<SFlt> s_vFilters = {
		{&g_Config.m_BcFltGamma, "Gamma", {FLT_S(BcFltGammaValue, "Gamma", "%")}},
		{&g_Config.m_BcFltHue, "Hue shift", {FLT_S(BcFltHueShift, "Turn", "°")}},
		{&g_Config.m_BcFltBw, "Black and white", {FLT_S(BcFltBwAmount, "Amount", "%"), FLT_S(BcFltBwContrast, "Contrast", "%")}},
		{&g_Config.m_BcFltSepia, "Sepia", {FLT_S(BcFltSepiaAmount, "Amount", "%")}},
		{&g_Config.m_BcFltInvert, "Negative", {FLT_S(BcFltInvertAmount, "Amount", "%")}},
		{&g_Config.m_BcFltThermal, "Thermal vision", {FLT_S(BcFltThermalAmount, "Amount", "%")}},
		{&g_Config.m_BcFltNight, "Night vision", {FLT_S(BcFltNightAmount, "Amount", "%"), FLT_S(BcFltNightNoise, "Noise", "%")}},
		{&g_Config.m_BcFltSplit, "Split toning", {FLT_S(BcFltSplitShadow, "Shadow hue", "°"), FLT_S(BcFltSplitLight, "Light hue", "°"), FLT_S(BcFltSplitAmount, "Amount", "%")}},
		{&g_Config.m_BcFltHorror, "Horror", {FLT_S(BcFltHorrorAmount, "Amount", "%"), FLT_S(BcFltHorrorPulse, "Heartbeat", "%")}},
		{&g_Config.m_BcFltWaves, "Waves", {FLT_S(BcFltWavesStrength, "Amount", "%"), FLT_S(BcFltWavesSpeed, "Speed", "%"), FLT_S(BcFltWavesSize, "Density", "%")}},
		{&g_Config.m_BcFltFisheye, "Fisheye", {FLT_S(BcFltFisheyeStrength, "Amount", "%")}},
		{&g_Config.m_BcFltDrunk, "Drunk", {FLT_S(BcFltDrunkStrength, "Amount", "%")}},
		{&g_Config.m_BcFltSwirl, "Swirl", {FLT_S(BcFltSwirlStrength, "Amount", "%"), FLT_S(BcFltSwirlRadius, "Size", "%")}},
		{&g_Config.m_BcFltBlur, "Blur", {FLT_S(BcFltBlurRadius, "Radius", "%")}},
		{&g_Config.m_BcFltTiltshift, "Tilt-shift", {FLT_S(BcFltTiltshiftBlur, "Blur", "%"), FLT_S(BcFltTiltshiftFocus, "Sharp line height", "%"), FLT_S(BcFltTiltshiftBand, "Sharp band", "%")}},
		{&g_Config.m_BcFltDream, "Dream", {FLT_S(BcFltDreamAmount, "Amount", "%")}},
		{&g_Config.m_BcFltPosterize, "Posterize", {FLT_S(BcFltPosterizeLevels, "Shades", "")}},
		{&g_Config.m_BcFltOil, "Oil painting", {FLT_S(BcFltOilRadius, "Brush size", "")}},
		{&g_Config.m_BcFltSketch, "Pencil sketch", {FLT_S(BcFltSketchLines, "Outlines", "%"), FLT_S(BcFltSketchHatch, "Hatching", "%")}},
		{&g_Config.m_BcFltWatercolor, "Watercolor", {FLT_S(BcFltWatercolorAmount, "Amount", "%")}},
		{&g_Config.m_BcFltCartoon, "Cartoon", {FLT_S(BcFltCartoonLevels, "Shades", ""), FLT_S(BcFltCartoonOutline, "Outlines", "%"), FLT_S(BcFltCartoonWidth, "Outline width", "%")},
			{&g_Config.m_BcFltCartoonColor, DefaultConfig::BcFltCartoonColor, "Outline color"}},
		{&g_Config.m_BcFltEdges, "Outlines", {FLT_S(BcFltEdgesStrength, "Amount", "%"), FLT_S(BcFltEdgesWidth, "Width", "%"), FLT_S(BcFltEdgesOnly, "Only the lines", "%")},
			{&g_Config.m_BcFltEdgesColor, DefaultConfig::BcFltEdgesColor, "Line color"}},
		{&g_Config.m_BcFltPixel, "Pixel art", {FLT_S(BcFltPixelSize, "Pixel size", "")}},
		{&g_Config.m_BcFltHex, "Hexagons", {FLT_S(BcFltHexSize, "Size", ""), FLT_S(BcFltHexEdges, "Dark edges", "%")}},
		{&g_Config.m_BcFltStained, "Stained glass", {FLT_S(BcFltStainedSize, "Piece size", ""), FLT_S(BcFltStainedLines, "Lead lines", "%")}},
		{&g_Config.m_BcFltHalftone, "Comic dots", {FLT_S(BcFltHalftoneSize, "Dot size", ""), FLT_S(BcFltHalftoneAmount, "Amount", "%")}},
		{&g_Config.m_BcFltAscii, "ASCII", {FLT_S(BcFltAsciiSize, "Letter size", ""), FLT_S(BcFltAsciiColor, "Picture colors", "%")}},
		{&g_Config.m_BcFltGameboy, "Game Boy", {FLT_S(BcFltGameboyAmount, "Amount", "%"), FLT_S(BcFltGameboyPixel, "Pixel size", "")}},
		{&g_Config.m_BcFltDither, "8-bit", {FLT_S(BcFltDitherLevels, "Shades", ""), FLT_S(BcFltDitherPixel, "Pixel size", "")}},
		{&g_Config.m_BcFltGlitch, "Glitch", {FLT_S(BcFltGlitchStrength, "Amount", "%"), FLT_S(BcFltGlitchOften, "How often", "%")}},
		{&g_Config.m_BcFltVhs, "VHS tape", {FLT_S(BcFltVhsStrength, "Amount", "%"), FLT_S(BcFltVhsNoise, "Noise", "%")}},
		{&g_Config.m_BcFltCrt, "Old TV", {FLT_S(BcFltCrtScanlines, "Scanlines", "%"), FLT_S(BcFltCrtCurve, "Screen curve", "%"), FLT_S(BcFltCrtMask, "Color dots", "%")}},
	};
	static CButtonContainer s_aColor[32];

	int Rows = 5; // hint, two rows of ready sets, own sets, load
	for(const SFlt &F : s_vFilters)
		Rows += 1 + (*F.m_pOn ? (int)F.m_vSliders.size() + (F.m_Color.m_pValue ? 1 : 0) : 0);

	const float BlockPadding = MARGIN_BETWEEN_VIEWS * 0.6666f;
	Column.HSplitTop(MARGIN_BETWEEN_VIEWS, nullptr, &Column);
	CUIRect Block;
	Column.HSplitTop(LINE_SIZE + Rows * ROW, &Block, &Column);
	CUIRect Bg = Block;
	Bg.w += BlockPadding;
	Bg.h += BlockPadding;
	Bg.x -= BlockPadding * 0.5f;
	Bg.y -= BlockPadding * 0.5f;
	BestClientUiTheme::DrawUiBlock(&Bg, IGraphics::CORNER_ALL, 10.0f);

	CUIRect Label, Title, ResetButton;
	Block.HSplitTop(LINE_SIZE, &Label, &Block);
	Label.VSplitRight(LINE_SIZE + 8.0f, &Title, &ResetButton);
	static CButtonContainer s_Reset;
	if(Ui()->DoButton_FontIcon(&s_Reset, FontIcon::ARROW_ROTATE_LEFT, 0, &ResetButton, BUTTONFLAG_LEFT))
	{
		// all filters off, their settings back
		for(const SFlt &F : s_vFilters)
		{
			*F.m_pOn = 0;
			for(const SFltSlider &S : F.m_vSliders)
				*S.m_pValue = S.m_Default;
			if(F.m_Color.m_pValue)
				*F.m_Color.m_pValue = F.m_Color.m_Default;
		}
	}
	GameClient()->m_Tooltips.DoToolTip(&s_Reset, &ResetButton, BcLocalize("Reset to defaults"));
	Ui()->DoLabel(&Title, BcLocalize("Filters"), HEADLINE_FONT_SIZE, TEXTALIGN_ML);

	{
		Block.HSplitTop(MARGIN_SMALL, nullptr, &Block);
		CUIRect Hint;
		Block.HSplitTop(LINE_SIZE, &Hint, &Block);
		Ui()->DoLabel(&Hint, BcLocalize("The game picture only, OpenGL renderer; applied top to bottom"), 12.0f, TEXTALIGN_ML);
	}
	// ready sets (the same as in the Right Shift menu)
	{
		static CButtonContainer s_aPreset[9];
		const char *apPreset[9] = {"Cinema", "Neon", "Retro", "Horror", "Cartoon", "Oil painting", "Old game", "Sketch", "All off"};
		const int aPresetId[9] = {CCcPostFx::FILTER_PRESET_CINEMA, CCcPostFx::FILTER_PRESET_NEON, CCcPostFx::FILTER_PRESET_RETRO, CCcPostFx::FILTER_PRESET_HORROR,
			CCcPostFx::FILTER_PRESET_CARTOON, CCcPostFx::FILTER_PRESET_PAINTING, CCcPostFx::FILTER_PRESET_OLDGAME, CCcPostFx::FILTER_PRESET_SKETCH, CCcPostFx::FILTER_PRESET_OFF};
		for(int Row = 0; Row < 2; Row++)
		{
			Block.HSplitTop(MARGIN_SMALL, nullptr, &Block);
			CUIRect Line;
			Block.HSplitTop(LINE_SIZE, &Line, &Block);
			const int First = Row == 0 ? 0 : 5, Num = Row == 0 ? 5 : 4;
			const float W = Line.w / Num;
			for(int i = First; i < First + Num; i++)
			{
				CUIRect Button;
				Line.VSplitLeft(W, &Button, &Line);
				Button.VMargin(1.0f, &Button);
				if(DoButton_Menu(&s_aPreset[i], BcLocalize(apPreset[i]), 0, &Button))
					CCcPostFx::ApplyFilterPreset(aPresetId[i]);
			}
		}
	}
	// own sets (09.11): slot 1..3 save / load, copy as text for a friend, paste a friend's
	{
		Block.HSplitTop(MARGIN_SMALL, nullptr, &Block);
		CUIRect Line;
		Block.HSplitTop(LINE_SIZE, &Line, &Block);
		static CButtonContainer s_aOwn[8];
		const char *apOwn[8] = {"Save 1", "Load 1", "Save 2", "Load 2", "Save 3", "Load 3", "Copy", "Paste"};
		const float W = Line.w / 8.0f;
		for(int i = 0; i < 8; i++)
		{
			CUIRect Button;
			Line.VSplitLeft(W, &Button, &Line);
			Button.VMargin(1.0f, &Button);
			if(!DoButton_Menu(&s_aOwn[i], BcLocalize(apOwn[i]), 0, &Button))
				continue;
			if(i < 6)
			{
				if(i % 2 == 0)
					CCcPostFx::SaveSlot(Storage(), i / 2 + 1);
				else
					CCcPostFx::LoadSlot(Storage(), i / 2 + 1);
			}
			else if(i == 6)
				Input()->SetClipboardText(CCcPostFx::ExportFilters().c_str());
			else
				CCcPostFx::ImportFilters(Input()->GetClipboardText().c_str());
		}
	}
	// the rough load of everything that is on
	{
		Block.HSplitTop(MARGIN_SMALL, nullptr, &Block);
		CUIRect Line;
		Block.HSplitTop(LINE_SIZE, &Line, &Block);
		const int Cost = CCcPostFx::TotalCost();
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "%s: %s", BcLocalize("Load on the video card"),
			Cost == 0 ? BcLocalize("none") : Cost <= 4 ? BcLocalize("light") : Cost <= 9 ? BcLocalize("medium") : BcLocalize("heavy, FPS may drop"));
		TextRender()->TextColor(Cost > 9 ? ColorRGBA(1.0f, 0.55f, 0.4f, 1.0f) : TextRender()->DefaultTextColor());
		Ui()->DoLabel(&Line, aBuf, 12.0f, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	int ColorIndex = 0;
	for(const SFlt &F : s_vFilters)
	{
		Block.HSplitTop(MARGIN_SMALL, nullptr, &Block);
		CUIRect Line;
		Block.HSplitTop(LINE_SIZE, &Line, &Block);
		const int Id = CCcPostFx::FilterIdOf(F.m_pOn);
		if(*F.m_pOn && Id >= 0)
		{
			// the place in the chain and arrows to move it earlier / later
			CUIRect Up, Down, Place;
			Line.VSplitRight(LINE_SIZE, &Line, &Down);
			Line.VSplitRight(LINE_SIZE + 2.0f, &Line, &Up);
			Line.VSplitRight(36.0f, &Line, &Place);
			static CButtonContainer s_aUp[64], s_aDown[64];
			if(Ui()->DoButton_FontIcon(&s_aUp[Id], FontIcon::CHEVRON_UP, 0, &Up, BUTTONFLAG_LEFT))
				CCcPostFx::MoveFilter(Id, -1);
			if(Ui()->DoButton_FontIcon(&s_aDown[Id], FontIcon::CHEVRON_DOWN, 0, &Down, BUTTONFLAG_LEFT))
				CCcPostFx::MoveFilter(Id, 1);
			char aPlace[16];
			str_format(aPlace, sizeof(aPlace), "#%d", CCcPostFx::FilterPosition(Id));
			Ui()->DoLabel(&Place, aPlace, 12.0f, TEXTALIGN_MR);
		}
		DoButton_CheckBoxAutoVMarginAndSet(F.m_pOn, BcLocalize(F.m_pName), F.m_pOn, &Line, LINE_SIZE);
		if(!*F.m_pOn)
			continue;
		for(const SFltSlider &S : F.m_vSliders)
		{
			Block.HSplitTop(MARGIN_SMALL, nullptr, &Block);
			CUIRect SliderLine;
			Block.HSplitTop(LINE_SIZE, &SliderLine, &Block);
			SliderLine.VSplitLeft(20.0f, nullptr, &SliderLine);
			Ui()->DoScrollbarOption(S.m_pValue, S.m_pValue, &SliderLine, BcLocalize(S.m_pLabel), S.m_Min, S.m_Max, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_DELAYUPDATE, S.m_pSuffix);
		}
		if(F.m_Color.m_pValue && ColorIndex < (int)std::size(s_aColor))
		{
			Block.HSplitTop(MARGIN_SMALL, nullptr, &Block);
			DoLine_ColorPicker(&s_aColor[ColorIndex++], LINE_SIZE, 13.0f, 0.0f, &Block, BcLocalize(F.m_Color.m_pLabel), F.m_Color.m_pValue,
				color_cast<ColorRGBA>(ColorHSLA(F.m_Color.m_Default)), false);
		}
	}
}

// own map background (09.11): switch, file dialog, the pictures of the folder as a list, parallax, brightness
void CMenus::RenderSettingsChainsawMapBg(CUIRect &Column)
{
	CCcMapBg &MapBg = GameClient()->m_CcMapBg;
	const std::vector<std::string> &vFiles = MapBg.Files();
	const int Shown = std::min((int)vFiles.size(), 12);
	const bool On = g_Config.m_BcMapBg != 0;
	const int Rows = 3 + (On ? 3 + std::max(Shown, 1) : 0); // switch, buttons, hint | status, parallax, brightness, list

	const float BlockPadding = MARGIN_BETWEEN_VIEWS * 0.6666f;
	Column.HSplitTop(MARGIN_BETWEEN_VIEWS, nullptr, &Column);
	CUIRect Block;
	Column.HSplitTop(LINE_SIZE + Rows * ROW, &Block, &Column);
	CUIRect Bg = Block;
	Bg.w += BlockPadding;
	Bg.h += BlockPadding;
	Bg.x -= BlockPadding * 0.5f;
	Bg.y -= BlockPadding * 0.5f;
	BestClientUiTheme::DrawUiBlock(&Bg, IGraphics::CORNER_ALL, 10.0f);
	CUIRect Title;
	Block.HSplitTop(LINE_SIZE, &Title, &Block);
	Ui()->DoLabel(&Title, BcLocalize("Map background"), HEADLINE_FONT_SIZE, TEXTALIGN_ML);

	const auto Row = [&]() {
		Block.HSplitTop(MARGIN_SMALL, nullptr, &Block);
		CUIRect Line;
		Block.HSplitTop(LINE_SIZE, &Line, &Block);
		return Line;
	};
	CUIRect Line = Row();
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_BcMapBg, BcLocalize("Own picture instead of the map background"), &g_Config.m_BcMapBg, &Line, LINE_SIZE);
	Line = Row();
	{
		static CButtonContainer s_Pick, s_Folder, s_Rescan;
		CUIRect A, B, C;
		Line.VSplitLeft(Line.w / 3.0f, &A, &Line);
		Line.VSplitMid(&B, &C, 0.0f);
		A.VMargin(1.0f, &A);
		B.VMargin(1.0f, &B);
		C.VMargin(1.0f, &C);
		if(DoButton_Menu(&s_Pick, BcLocalize("Choose a file"), 0, &A))
			MapBg.PickFile();
		if(DoButton_Menu(&s_Folder, BcLocalize("Open the folder"), 0, &B))
			MapBg.OpenFolder();
		if(DoButton_Menu(&s_Rescan, BcLocalize("Refresh the list"), 0, &C))
			MapBg.Rescan();
	}
	Line = Row();
	Ui()->DoLabel(&Line, BcLocalize("Put pictures into chainsaw/map_backgrounds and pick one below"), 12.0f, TEXTALIGN_ML);
	if(!On)
		return;
	Line = Row();
	MapBg.Replaces(); // loads the picture now, so the status is fresh
	Ui()->DoLabel(&Line, MapBg.Status(), 12.0f, TEXTALIGN_ML);
	Line = Row();
	Ui()->DoScrollbarOption(&g_Config.m_BcMapBgParallax, &g_Config.m_BcMapBgParallax, &Line, BcLocalize("Moves with the camera"), 0, 100, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_DELAYUPDATE, "%");
	Line = Row();
	Ui()->DoScrollbarOption(&g_Config.m_BcMapBgBrightness, &g_Config.m_BcMapBgBrightness, &Line, BcLocalize("Brightness"), 20, 150, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_DELAYUPDATE, "%");
	if(vFiles.empty())
	{
		Line = Row();
		Ui()->DoLabel(&Line, BcLocalize("The folder is empty"), 12.0f, TEXTALIGN_ML);
		return;
	}
	static CButtonContainer s_aFile[12];
	for(int i = 0; i < Shown; i++)
	{
		Line = Row();
		const bool Selected = str_comp(vFiles[i].c_str(), g_Config.m_BcMapBgFile) == 0;
		if(DoButton_Menu(&s_aFile[i], vFiles[i].c_str(), Selected, &Line))
			MapBg.Select(i);
	}
}

// light and shadows over the map (10.11): switch, quality, darkness, the three kinds of light, own lamps
void CMenus::RenderSettingsChainsawLight(CUIRect &Column)
{
	CCcLight &Light = GameClient()->m_CcLight;
	const bool On = g_Config.m_BcLight != 0;
	int Rows = 2; // switch, hint
	if(On)
		Rows += 6 + 1 + (g_Config.m_BcLightTee ? 4 : 0) + 1 + (g_Config.m_BcLightSun ? 4 : 0) + 5;

	const float BlockPadding = MARGIN_BETWEEN_VIEWS * 0.6666f;
	Column.HSplitTop(MARGIN_BETWEEN_VIEWS, nullptr, &Column);
	CUIRect Block;
	Column.HSplitTop(LINE_SIZE + Rows * ROW, &Block, &Column);
	CUIRect Bg = Block;
	Bg.w += BlockPadding;
	Bg.h += BlockPadding;
	Bg.x -= BlockPadding * 0.5f;
	Bg.y -= BlockPadding * 0.5f;
	BestClientUiTheme::DrawUiBlock(&Bg, IGraphics::CORNER_ALL, 10.0f);
	CUIRect Title;
	Block.HSplitTop(LINE_SIZE, &Title, &Block);
	Ui()->DoLabel(&Title, BcLocalize("Light and shadows"), HEADLINE_FONT_SIZE, TEXTALIGN_ML);

	const auto Row = [&]() {
		Block.HSplitTop(MARGIN_SMALL, nullptr, &Block);
		CUIRect Line;
		Block.HSplitTop(LINE_SIZE, &Line, &Block);
		return Line;
	};
	const auto Slider = [&](int *pValue, const char *pLabel, int Min, int Max, const char *pSuffix, bool Indent = false) {
		CUIRect Line = Row();
		if(Indent)
			Line.VSplitLeft(20.0f, nullptr, &Line);
		Ui()->DoScrollbarOption(pValue, pValue, &Line, BcLocalize(pLabel), Min, Max, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_DELAYUPDATE, pSuffix);
	};
	const auto Check = [&](int *pValue, const char *pLabel, bool Indent = false) {
		CUIRect Line = Row();
		if(Indent)
			Line.VSplitLeft(20.0f, nullptr, &Line);
		DoButton_CheckBoxAutoVMarginAndSet(pValue, BcLocalize(pLabel), pValue, &Line, LINE_SIZE);
	};
	static CButtonContainer s_TeeColor, s_SunColor, s_LampColor;
	const auto Color = [&](CButtonContainer *pId, unsigned *pValue, const char *pLabel, unsigned Default) {
		Block.HSplitTop(MARGIN_SMALL, nullptr, &Block);
		DoLine_ColorPicker(pId, LINE_SIZE, 13.0f, 0.0f, &Block, BcLocalize(pLabel), pValue, color_cast<ColorRGBA>(ColorHSLA(Default)), false);
	};

	Check(&g_Config.m_BcLight, "The map gets dark, lamps light it, blocks throw shadows");
	CUIRect Line = Row();
	Ui()->DoLabel(&Line, BcLocalize("OpenGL 3.3 only. Heavy: lower the quality on a weak PC"), 12.0f, TEXTALIGN_ML);
	if(!On)
		return;
	Slider(&g_Config.m_BcLightQuality, "Light quality (0 low - 3 ultra)", 0, 3, "");
	Slider(&g_Config.m_BcLightDark, "Darkness", 0, 100, "%");
	Slider(&g_Config.m_BcLightPower, "Brightness of the light", 20, 300, "%");
	Slider(&g_Config.m_BcLightHaze, "Rays in the air", 0, 300, "%");
	Slider(&g_Config.m_BcLightDust, "Specks of dust", 0, 300, "%");
	Slider(&g_Config.m_BcLightVignette, "Dark corners of the screen", 0, 100, "%");

	Check(&g_Config.m_BcLightTee, "Tees carry a light");
	if(g_Config.m_BcLightTee)
	{
		Check(&g_Config.m_BcLightTeeOthers, "Other tees too", true);
		Check(&g_Config.m_BcLightTeeSkin, "Light in the colour of the tee", true);
		Slider(&g_Config.m_BcLightTeeRange, "Reach in blocks", 3, 30, "", true);
		Color(&s_TeeColor, &g_Config.m_BcLightTeeColor, "Colour of the tee light", DefaultConfig::BcLightTeeColor);
	}
	Check(&g_Config.m_BcLightSun, "Light from the sky");
	if(g_Config.m_BcLightSun)
	{
		Slider(&g_Config.m_BcLightSunAngle, "Angle of the rays", -75, 75, "", true);
		Slider(&g_Config.m_BcLightSunPower, "Brightness", 10, 300, "%", true);
		Slider(&g_Config.m_BcLightSunReach, "Free blocks up to the sky", 8, 60, "", true);
		Color(&s_SunColor, &g_Config.m_BcLightSunColor, "Colour of the sky light", DefaultConfig::BcLightSunColor);
	}
	Check(&g_Config.m_BcLightLamps, "Own lamps are lit");
	Line = Row();
	{
		static CButtonContainer s_Place, s_Remove, s_Clear;
		CUIRect A, B, C;
		Line.VSplitLeft(Line.w / 3.0f, &A, &Line);
		Line.VSplitMid(&B, &C, 0.0f);
		A.VMargin(1.0f, &A);
		B.VMargin(1.0f, &B);
		C.VMargin(1.0f, &C);
		if(DoButton_Menu(&s_Place, BcLocalize("Hang a lamp here"), 0, &A))
			Light.PlaceLamp(false);
		if(DoButton_Menu(&s_Remove, BcLocalize("Remove the nearest"), 0, &B))
			Light.RemoveNearest();
		if(DoButton_Menu(&s_Clear, BcLocalize("Remove all"), 0, &C))
			Light.ClearLamps();
	}
	Line = Row();
	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s: %d. bind l cc_light_place", BcLocalize("Lamps on this map"), Light.NumOwnLamps());
	Ui()->DoLabel(&Line, aBuf, 12.0f, TEXTALIGN_ML);
	Slider(&g_Config.m_BcLightLampRange, "Reach of the lamps in blocks", 4, 40, "");
	Color(&s_LampColor, &g_Config.m_BcLightLampColor, "Colour of the lamps", DefaultConfig::BcLightLampColor);
}

void CMenus::RenderSettingsBestClientAtmosphereFilterRealisticBlocks(CUIRect &Column)
{
	const bool On = g_Config.m_BcBlocks;
	CUIRect Block = Column;
	Block.HSplitTop(MARGIN_BETWEEN_VIEWS, nullptr, &Block);
	Block.HSplitTop(LINE_SIZE, nullptr, &Block);
	const float BlockPadding = 10.0f;
	float h = 2 * BlockPadding + MARGIN_SMALL + LINE_SIZE + MARGIN_SMALL + LINE_SIZE;
	if(On)
		h += 7 * (MARGIN_SMALL + LINE_SIZE);
	Block.HSplitTop(h, &Block, nullptr);
	CUIRect Bg = Block;
	Bg.w += BlockPadding;
	Bg.h += BlockPadding;
	Bg.x -= BlockPadding * 0.5f;
	Bg.y -= BlockPadding * 0.5f;
	BestClientUiTheme::DrawUiBlock(&Bg, IGraphics::CORNER_ALL, 10.0f);
	CUIRect Title;
	Block.HSplitTop(LINE_SIZE, &Title, &Block);
	Ui()->DoLabel(&Title, BcLocalize("Realistic blocks"), HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	const auto Row = [&]() {
		Block.HSplitTop(MARGIN_SMALL, nullptr, &Block);
		CUIRect Line;
		Block.HSplitTop(LINE_SIZE, &Line, &Block);
		return Line;
	};
	const auto Slider = [&](int *pValue, const char *pLabel, int Min, int Max, const char *pSuffix) {
		CUIRect Line = Row();
		Ui()->DoScrollbarOption(pValue, pValue, &Line, BcLocalize(pLabel), Min, Max, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_DELAYUPDATE, pSuffix);
	};
	const auto Check = [&](int *pValue, const char *pLabel) {
		CUIRect Line = Row();
		DoButton_CheckBoxAutoVMarginAndSet(pValue, BcLocalize(pLabel), pValue, &Line, LINE_SIZE);
	};
	Check(&g_Config.m_BcBlocks, "Hookable = earth, unhookable = rock, freeze = water");
	CUIRect Line = Row();
	Ui()->DoLabel(&Line, BcLocalize("OpenGL 3.3 only. Photo textures, grass, flowers"), 12.0f, TEXTALIGN_ML);
	if(!On)
		return;
	Slider(&g_Config.m_BcBlocksStyle, "Style (0 photo - 1 pixel art)", 0, 1, "");
	Slider(&g_Config.m_BcBlocksQuality, "Quality (0 low - 3 ultra)", 0, 3, "");
	Check(&g_Config.m_BcBlocksDecor, "Grass and flowers above the blocks");
	Slider(&g_Config.m_BcBlocksVines, "Vines under the ceilings (0 none)", 0, 100, "%");
	Check(&g_Config.m_BcBlocksOutline, "Dark outline around the blocks");
	Slider(&g_Config.m_BcBlocksWaterClear, "Clear water (tees show through)", 0, 100, "%");
	Line = Row();
	Ui()->DoLabel(&Line, BcLocalize("Heavy on ultra: use quality 2 on a weak PC"), 12.0f, TEXTALIGN_ML);
}
