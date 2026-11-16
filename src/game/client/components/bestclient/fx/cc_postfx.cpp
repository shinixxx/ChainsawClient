/* ChainsawClient: "shaders", see cc_postfx.h. */
#include "cc_postfx.h"

#include <base/io.h>
#include <base/str.h>

#include <engine/bc_postfx_params.h>
#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>

#include <cmath>
#include <iterator>
#include <string>
#include <vector>

static bool BaseEnabled()
{
	return g_Config.m_BcPostFxColor || g_Config.m_BcPostFxBloom || g_Config.m_BcPostFxVignette || g_Config.m_BcPostFxSharpen || g_Config.m_BcPostFxGrain || g_Config.m_BcPostFxChroma;
}

// the filters (08.11): on, and how they fill the 4 numbers and the color of their shader
static void FilterOrder(int *pOrder);

static int BuildFilters(SBcFilter *pOut)
{
	SBcFilter aById[NUM_BC_FILTERS];
	bool aOn[NUM_BC_FILTERS] = {};
	const auto Add = [&](int On, int Id, float P0, float P1 = 0.0f, float P2 = 0.0f, unsigned Color = 0) {
		if(!On)
			return;
		aOn[Id] = true;
		SBcFilter &F = aById[Id];
		F.m_Id = Id;
		F.m_aP[0] = P0;
		F.m_aP[1] = P1;
		F.m_aP[2] = P2;
		const ColorRGBA c = color_cast<ColorRGBA>(ColorHSLA(Color));
		F.m_aColor[0] = c.r;
		F.m_aColor[1] = c.g;
		F.m_aColor[2] = c.b;
	};
	// in the order of BC_FILTER_* (the order they are applied)
	Add(g_Config.m_BcFltGamma, BC_FILTER_GAMMA, g_Config.m_BcFltGammaValue / 100.0f);
	Add(g_Config.m_BcFltHue, BC_FILTER_HUE, g_Config.m_BcFltHueShift / 360.0f);
	Add(g_Config.m_BcFltBw, BC_FILTER_BW, g_Config.m_BcFltBwAmount / 100.0f, g_Config.m_BcFltBwContrast / 100.0f);
	Add(g_Config.m_BcFltSepia, BC_FILTER_SEPIA, g_Config.m_BcFltSepiaAmount / 100.0f);
	Add(g_Config.m_BcFltInvert, BC_FILTER_INVERT, g_Config.m_BcFltInvertAmount / 100.0f);
	Add(g_Config.m_BcFltThermal, BC_FILTER_THERMAL, g_Config.m_BcFltThermalAmount / 100.0f);
	Add(g_Config.m_BcFltNight, BC_FILTER_NIGHTVISION, g_Config.m_BcFltNightAmount / 100.0f, g_Config.m_BcFltNightNoise / 100.0f);
	Add(g_Config.m_BcFltSplit, BC_FILTER_SPLIT, g_Config.m_BcFltSplitShadow / 360.0f, g_Config.m_BcFltSplitLight / 360.0f, g_Config.m_BcFltSplitAmount / 100.0f);
	Add(g_Config.m_BcFltHorror, BC_FILTER_HORROR, g_Config.m_BcFltHorrorAmount / 100.0f, g_Config.m_BcFltHorrorPulse / 100.0f);
	Add(g_Config.m_BcFltWaves, BC_FILTER_WAVES, g_Config.m_BcFltWavesStrength / 50.0f, g_Config.m_BcFltWavesSpeed / 100.0f, g_Config.m_BcFltWavesSize / 100.0f);
	Add(g_Config.m_BcFltFisheye, BC_FILTER_FISHEYE, g_Config.m_BcFltFisheyeStrength / 100.0f);
	Add(g_Config.m_BcFltDrunk, BC_FILTER_DRUNK, g_Config.m_BcFltDrunkStrength / 50.0f);
	Add(g_Config.m_BcFltSwirl, BC_FILTER_SWIRL, g_Config.m_BcFltSwirlStrength / 100.0f, g_Config.m_BcFltSwirlRadius / 100.0f * 0.7f);
	Add(g_Config.m_BcFltBlur, BC_FILTER_BLUR, g_Config.m_BcFltBlurRadius / 100.0f);
	Add(g_Config.m_BcFltTiltshift, BC_FILTER_TILTSHIFT, g_Config.m_BcFltTiltshiftBlur / 100.0f * 2.5f, 1.0f - g_Config.m_BcFltTiltshiftFocus / 100.0f, g_Config.m_BcFltTiltshiftBand / 100.0f);
	Add(g_Config.m_BcFltDream, BC_FILTER_DREAM, g_Config.m_BcFltDreamAmount / 100.0f);
	Add(g_Config.m_BcFltPosterize, BC_FILTER_POSTERIZE, (float)g_Config.m_BcFltPosterizeLevels);
	Add(g_Config.m_BcFltOil, BC_FILTER_OIL, (float)g_Config.m_BcFltOilRadius);
	Add(g_Config.m_BcFltSketch, BC_FILTER_SKETCH, g_Config.m_BcFltSketchLines / 100.0f, g_Config.m_BcFltSketchHatch / 100.0f);
	Add(g_Config.m_BcFltWatercolor, BC_FILTER_WATERCOLOR, g_Config.m_BcFltWatercolorAmount / 100.0f);
	Add(g_Config.m_BcFltCartoon, BC_FILTER_CARTOON, (float)g_Config.m_BcFltCartoonLevels, g_Config.m_BcFltCartoonOutline / 100.0f * 2.5f, g_Config.m_BcFltCartoonWidth / 100.0f, g_Config.m_BcFltCartoonColor);
	Add(g_Config.m_BcFltEdges, BC_FILTER_EDGES, g_Config.m_BcFltEdgesStrength / 100.0f * 2.0f, g_Config.m_BcFltEdgesWidth / 100.0f, g_Config.m_BcFltEdgesOnly / 100.0f, g_Config.m_BcFltEdgesColor);
	Add(g_Config.m_BcFltPixel, BC_FILTER_PIXEL, (float)g_Config.m_BcFltPixelSize);
	Add(g_Config.m_BcFltHex, BC_FILTER_HEX, (float)g_Config.m_BcFltHexSize, g_Config.m_BcFltHexEdges / 100.0f);
	Add(g_Config.m_BcFltStained, BC_FILTER_STAINED, (float)g_Config.m_BcFltStainedSize, g_Config.m_BcFltStainedLines / 100.0f);
	Add(g_Config.m_BcFltHalftone, BC_FILTER_HALFTONE, (float)g_Config.m_BcFltHalftoneSize, g_Config.m_BcFltHalftoneAmount / 100.0f);
	Add(g_Config.m_BcFltAscii, BC_FILTER_ASCII, (float)g_Config.m_BcFltAsciiSize, g_Config.m_BcFltAsciiColor / 100.0f);
	Add(g_Config.m_BcFltGameboy, BC_FILTER_GAMEBOY, g_Config.m_BcFltGameboyAmount / 100.0f, (float)g_Config.m_BcFltGameboyPixel);
	Add(g_Config.m_BcFltDither, BC_FILTER_DITHER, (float)g_Config.m_BcFltDitherLevels, (float)g_Config.m_BcFltDitherPixel);
	Add(g_Config.m_BcFltGlitch, BC_FILTER_GLITCH, g_Config.m_BcFltGlitchStrength / 100.0f, g_Config.m_BcFltGlitchOften / 100.0f);
	Add(g_Config.m_BcFltVhs, BC_FILTER_VHS, g_Config.m_BcFltVhsStrength / 100.0f, g_Config.m_BcFltVhsNoise / 100.0f);
	Add(g_Config.m_BcFltCrt, BC_FILTER_CRT, g_Config.m_BcFltCrtScanlines / 100.0f, g_Config.m_BcFltCrtCurve / 100.0f, g_Config.m_BcFltCrtMask / 100.0f);
	// in the order the player set (bc_flt_order), the rest in the order of the list
	int aOrder[NUM_BC_FILTERS];
	FilterOrder(aOrder);
	int Num = 0;
	for(int Id : aOrder)
		if(aOn[Id])
			pOut[Num++] = aById[Id];
	return Num;
}

bool CCcPostFx::AnyEnabled()
{
	SBcFilter aFilters[NUM_BC_FILTERS];
	return BaseEnabled() || BuildFilters(aFilters) > 0;
}

bool CCcPostFx::RendererSupported()
{
	return str_comp_nocase(g_Config.m_GfxBackend, "OpenGL") == 0;
}

void CCcPostFx::OnRender()
{
	if(!AnyEnabled() || g_Config.m_BcHeadless)
		return;
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	SBcPostFxParams Params;
	if(g_Config.m_BcPostFxColor)
	{
		Params.m_Saturation = g_Config.m_BcPostFxSaturation / 100.0f;
		Params.m_Vibrance = g_Config.m_BcPostFxVibrance / 100.0f;
		Params.m_Contrast = g_Config.m_BcPostFxContrast / 100.0f;
		Params.m_Brightness = g_Config.m_BcPostFxBrightness / 100.0f;
		Params.m_Warmth = g_Config.m_BcPostFxWarmth / 100.0f;
		const ColorRGBA Tint = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_BcPostFxTint));
		Params.m_aTint[0] = Tint.r;
		Params.m_aTint[1] = Tint.g;
		Params.m_aTint[2] = Tint.b;
	}
	if(g_Config.m_BcPostFxBloom)
	{
		Params.m_Bloom = g_Config.m_BcPostFxBloomAmount / 100.0f;
		Params.m_BloomThreshold = 0.3f + 0.65f * g_Config.m_BcPostFxBloomThreshold / 100.0f;
		Params.m_BloomRadius = g_Config.m_BcPostFxBloomRadius / 100.0f * 2.0f;
	}
	if(g_Config.m_BcPostFxVignette)
		Params.m_Vignette = g_Config.m_BcPostFxVignetteAmount / 100.0f * 0.8f;
	if(g_Config.m_BcPostFxSharpen)
		Params.m_Sharpen = g_Config.m_BcPostFxSharpenAmount / 100.0f;
	if(g_Config.m_BcPostFxGrain)
		Params.m_Grain = g_Config.m_BcPostFxGrainAmount / 100.0f;
	if(g_Config.m_BcPostFxChroma)
		Params.m_Chroma = g_Config.m_BcPostFxChromaAmount / 100.0f;
	Params.m_Time = (float)std::fmod(Client()->LocalTime(), 1000.0);
	Params.m_Base = BaseEnabled();
	Params.m_NumFilters = BuildFilters(Params.m_aFilters);
	Graphics()->BcPostFx(Params);
}

// ready sets (08.11): every set turns all filters off first, then switches on its own ones with their settings
static int *const *FilterToggles()
{
	static int *const s_apToggles[] = {&g_Config.m_BcFltGamma, &g_Config.m_BcFltHue, &g_Config.m_BcFltBw, &g_Config.m_BcFltSepia, &g_Config.m_BcFltInvert,
		&g_Config.m_BcFltThermal, &g_Config.m_BcFltNight, &g_Config.m_BcFltSplit, &g_Config.m_BcFltHorror, &g_Config.m_BcFltWaves,
		&g_Config.m_BcFltFisheye, &g_Config.m_BcFltDrunk, &g_Config.m_BcFltSwirl, &g_Config.m_BcFltBlur, &g_Config.m_BcFltTiltshift,
		&g_Config.m_BcFltDream, &g_Config.m_BcFltPosterize, &g_Config.m_BcFltOil, &g_Config.m_BcFltSketch, &g_Config.m_BcFltWatercolor,
		&g_Config.m_BcFltCartoon, &g_Config.m_BcFltEdges, &g_Config.m_BcFltPixel, &g_Config.m_BcFltHex, &g_Config.m_BcFltStained,
		&g_Config.m_BcFltHalftone, &g_Config.m_BcFltAscii, &g_Config.m_BcFltGameboy, &g_Config.m_BcFltDither, &g_Config.m_BcFltGlitch,
		&g_Config.m_BcFltVhs, &g_Config.m_BcFltCrt};
	static_assert(std::size(s_apToggles) == NUM_BC_FILTERS, "every filter in the list, in the order of BC_FILTER_*");
	return s_apToggles;
}

void CCcPostFx::ApplyFilterPreset(int Preset)
{
	for(int i = 0; i < NUM_BC_FILTERS; i++)
		*FilterToggles()[i] = 0;
	switch(Preset)
	{
	case FILTER_PRESET_CINEMA: // teal shadows, orange lights, dark edges, a little film grain
		g_Config.m_BcFltSplit = 1;
		g_Config.m_BcFltSplitShadow = 190;
		g_Config.m_BcFltSplitLight = 30;
		g_Config.m_BcFltSplitAmount = 45;
		g_Config.m_BcPostFxVignette = 1;
		g_Config.m_BcPostFxVignetteAmount = 45;
		g_Config.m_BcPostFxGrain = 1;
		g_Config.m_BcPostFxGrainAmount = 20;
		break;
	case FILTER_PRESET_NEON: // glowing cyan outlines, purple shadows
		g_Config.m_BcFltEdges = 1;
		g_Config.m_BcFltEdgesStrength = 160;
		g_Config.m_BcFltEdgesWidth = 100;
		g_Config.m_BcFltEdgesOnly = 0;
		g_Config.m_BcFltEdgesColor = (127 << 16) | (255 << 8) | 140;
		g_Config.m_BcFltSplit = 1;
		g_Config.m_BcFltSplitShadow = 270;
		g_Config.m_BcFltSplitLight = 190;
		g_Config.m_BcFltSplitAmount = 60;
		g_Config.m_BcFltDream = 1;
		g_Config.m_BcFltDreamAmount = 50;
		break;
	case FILTER_PRESET_RETRO: // a VHS tape on an old TV
		g_Config.m_BcFltVhs = 1;
		g_Config.m_BcFltVhsStrength = 50;
		g_Config.m_BcFltVhsNoise = 35;
		g_Config.m_BcFltCrt = 1;
		g_Config.m_BcFltCrtScanlines = 55;
		g_Config.m_BcFltCrtCurve = 45;
		g_Config.m_BcFltCrtMask = 35;
		break;
	case FILTER_PRESET_HORROR:
		g_Config.m_BcFltHorror = 1;
		g_Config.m_BcFltHorrorAmount = 85;
		g_Config.m_BcFltHorrorPulse = 60;
		g_Config.m_BcFltGlitch = 1;
		g_Config.m_BcFltGlitchStrength = 30;
		g_Config.m_BcFltGlitchOften = 12;
		break;
	case FILTER_PRESET_CARTOON:
		g_Config.m_BcFltCartoon = 1;
		g_Config.m_BcFltCartoonLevels = 5;
		g_Config.m_BcFltCartoonOutline = 110;
		g_Config.m_BcFltCartoonWidth = 100;
		break;
	case FILTER_PRESET_PAINTING:
		g_Config.m_BcFltOil = 1;
		g_Config.m_BcFltOilRadius = 3;
		g_Config.m_BcFltSplit = 1;
		g_Config.m_BcFltSplitShadow = 230;
		g_Config.m_BcFltSplitLight = 45;
		g_Config.m_BcFltSplitAmount = 30;
		break;
	case FILTER_PRESET_OLDGAME:
		g_Config.m_BcFltGameboy = 1;
		g_Config.m_BcFltGameboyAmount = 100;
		g_Config.m_BcFltGameboyPixel = 3;
		break;
	case FILTER_PRESET_SKETCH:
		g_Config.m_BcFltSketch = 1;
		g_Config.m_BcFltSketchLines = 110;
		g_Config.m_BcFltSketchHatch = 90;
		break;
	default:
		break;
	}
}

// ---- 09.11: the order of the chain, own sets (3 slots, copy / paste as text), the load of the filters ----

// bc_flt_order: the ids in the order the player set; ids it does not name follow in the order of the list
static void FilterOrder(int *pOrder)
{
	bool aUsed[NUM_BC_FILTERS] = {};
	int Num = 0;
	const char *p = g_Config.m_BcFltOrder;
	while(*p && Num < NUM_BC_FILTERS)
	{
		while(*p == ',' || *p == ' ')
			p++;
		if(*p < '0' || *p > '9')
			break;
		int Id = 0;
		while(*p >= '0' && *p <= '9')
			Id = Id * 10 + (*p++ - '0');
		if(Id < NUM_BC_FILTERS && !aUsed[Id])
		{
			aUsed[Id] = true;
			pOrder[Num++] = Id;
		}
	}
	for(int Id = 0; Id < NUM_BC_FILTERS; Id++)
		if(!aUsed[Id])
			pOrder[Num++] = Id;
}

static void WriteOrder(const int *pOrder)
{
	char aBuf[sizeof(g_Config.m_BcFltOrder)] = "";
	for(int i = 0; i < NUM_BC_FILTERS; i++)
	{
		char aNum[8];
		str_format(aNum, sizeof(aNum), i ? ",%d" : "%d", pOrder[i]);
		str_append(aBuf, aNum);
	}
	str_copy(g_Config.m_BcFltOrder, aBuf);
}

int CCcPostFx::FilterPosition(int Id)
{
	// the place among the filters that are on (1 = applied first), 0 = off
	if(Id < 0 || Id >= NUM_BC_FILTERS || !*FilterToggles()[Id])
		return 0;
	int aOrder[NUM_BC_FILTERS];
	FilterOrder(aOrder);
	int Pos = 0;
	for(int i : aOrder)
	{
		if(*FilterToggles()[i])
			Pos++;
		if(i == Id)
			return Pos;
	}
	return 0;
}

void CCcPostFx::MoveFilter(int Id, int Dir)
{
	if(Id < 0 || Id >= NUM_BC_FILTERS || Dir == 0)
		return;
	int aOrder[NUM_BC_FILTERS];
	FilterOrder(aOrder);
	int At = 0;
	while(At < NUM_BC_FILTERS && aOrder[At] != Id)
		At++;
	// past the next filter that is on (moving past ones that are off changes nothing you can see)
	int To = At + Dir;
	while(To >= 0 && To < NUM_BC_FILTERS && !*FilterToggles()[aOrder[To]])
		To += Dir;
	if(To < 0 || To >= NUM_BC_FILTERS)
		return;
	const int Moved = aOrder[At];
	for(int i = At; i != To; i += Dir)
		aOrder[i] = aOrder[i + Dir];
	aOrder[To] = Moved;
	WriteOrder(aOrder);
}

namespace
{
struct SFltVar
{
	const char *m_pName;
	int *m_pInt;
	unsigned *m_pCol;
	int m_Min, m_Max;
};

// every setting of the shaders and filters (bc_postfx_*, bc_flt_*): what a set saves and loads
const std::vector<SFltVar> &FltVars()
{
	static const std::vector<SFltVar> s_vVars = [] {
		std::vector<SFltVar> vAll;
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) vAll.push_back({#ScriptName, &g_Config.m_##Name, nullptr, Min, Max});
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc) vAll.push_back({#ScriptName, nullptr, &g_Config.m_##Name, 0, 0});
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Flags, Desc)
#include <engine/shared/config_variables_bestclient.h>
#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_COL
#undef MACRO_CONFIG_STR
		std::vector<SFltVar> vOut;
		for(const SFltVar &Var : vAll)
			if(str_startswith(Var.m_pName, "bc_flt_") || str_startswith(Var.m_pName, "bc_postfx_"))
				vOut.push_back(Var);
		return vOut;
	}();
	return s_vVars;
}
}

std::string CCcPostFx::ExportFilters()
{
	std::string Out = "chainsaw-filters 1;";
	char aBuf[64];
	for(const SFltVar &Var : FltVars())
	{
		str_format(aBuf, sizeof(aBuf), " %s %u;", Var.m_pName, Var.m_pInt ? (unsigned)*Var.m_pInt : *Var.m_pCol);
		Out += aBuf;
	}
	Out += " bc_flt_order ";
	Out += g_Config.m_BcFltOrder[0] ? g_Config.m_BcFltOrder : "-";
	Out += ";";
	return Out;
}

int CCcPostFx::ImportFilters(const char *pText)
{
	// only our own settings are read (never other console commands): "name value;" pairs
	if(!pText || !str_find(pText, "chainsaw-filters"))
		return 0;
	int Num = 0;
	const char *p = pText;
	char aName[64], aValue[160];
	while(*p)
	{
		while(*p == ' ' || *p == ';' || *p == '\n' || *p == '\r' || *p == '\t')
			p++;
		int n = 0;
		while(*p && *p != ' ' && *p != ';' && n < (int)sizeof(aName) - 1)
			aName[n++] = *p++;
		aName[n] = 0;
		while(*p == ' ')
			p++;
		n = 0;
		while(*p && *p != ';' && n < (int)sizeof(aValue) - 1)
			aValue[n++] = *p++;
		aValue[n] = 0;
		if(!aName[0])
			break;
		if(str_comp(aName, "bc_flt_order") == 0)
		{
			str_copy(g_Config.m_BcFltOrder, str_comp(aValue, "-") == 0 ? "" : aValue);
			Num++;
			continue;
		}
		for(const SFltVar &Var : FltVars())
		{
			if(str_comp(Var.m_pName, aName) != 0)
				continue;
			if(Var.m_pInt)
				*Var.m_pInt = std::clamp(str_toint(aValue), Var.m_Min, Var.m_Max);
			else
				*Var.m_pCol = (unsigned)str_toulong_base(aValue, 10);
			Num++;
			break;
		}
	}
	return Num;
}

static void SlotPath(int Slot, char *pBuf, int Size)
{
	str_format(pBuf, Size, "chainsaw/filters_%d.txt", Slot);
}

bool CCcPostFx::SaveSlot(IStorage *pStorage, int Slot)
{
	char aPath[64];
	SlotPath(Slot, aPath, sizeof(aPath));
	pStorage->CreateFolder("chainsaw", IStorage::TYPE_SAVE);
	IOHANDLE File = pStorage->OpenFile(aPath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return false;
	const std::string Text = ExportFilters();
	io_write(File, Text.c_str(), Text.size());
	io_close(File);
	return true;
}

bool CCcPostFx::LoadSlot(IStorage *pStorage, int Slot)
{
	char aPath[64];
	SlotPath(Slot, aPath, sizeof(aPath));
	char *pText = pStorage->ReadFileStr(aPath, IStorage::TYPE_SAVE);
	if(!pText)
		return false;
	const int Num = ImportFilters(pText);
	free(pText);
	return Num > 0;
}

// a rough load of the filters that are on: every filter copies the picture once (1) plus its own work
int CCcPostFx::FilterCost(int Id)
{
	switch(Id)
	{
	case BC_FILTER_OIL: return 2 + 2 * std::clamp(g_Config.m_BcFltOilRadius, 1, 6);
	case BC_FILTER_CARTOON:
	case BC_FILTER_EDGES:
	case BC_FILTER_SKETCH:
	case BC_FILTER_WATERCOLOR:
	case BC_FILTER_STAINED:
	case BC_FILTER_BLUR:
	case BC_FILTER_TILTSHIFT:
	case BC_FILTER_DREAM: return 3;
	default: return 1;
	}
}

int CCcPostFx::TotalCost()
{
	int Sum = BaseEnabled() ? 2 : 0;
	for(int Id = 0; Id < NUM_BC_FILTERS; Id++)
		if(*FilterToggles()[Id])
			Sum += FilterCost(Id);
	return Sum;
}

int CCcPostFx::FilterIdOf(const int *pToggle)
{
	for(int Id = 0; Id < NUM_BC_FILTERS; Id++)
		if(FilterToggles()[Id] == pToggle)
			return Id;
	return -1;
}
