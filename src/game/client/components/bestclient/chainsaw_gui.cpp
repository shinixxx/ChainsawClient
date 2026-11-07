/* ChainsawClient: in-game menu with all our visuals. See chainsaw_gui.h. */
#include "chainsaw_gui.h"

#include "bestclient.h"
#include "chainsaw_gui_bg.h"
#include "chainsaw_perf.h"
#include "fx/cc_fx_common.h"
#include "media_decoder.h"

#include <base/math.h>
#include <base/mem.h>
#include <base/str.h>

#include <engine/client/keyboard.h>
#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/image.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/sound.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/components/sounds.h>
#include <game/client/gameclient.h>

#include <algorithm>
#include <cmath>
#include <cstring>

// min / max / console name of every bestclient config variable, so the menu never repeats them by hand
namespace CcCfg
{
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) \
	[[maybe_unused]] constexpr int Name##_Min = Min; \
	[[maybe_unused]] constexpr int Name##_Max = Max; \
	[[maybe_unused]] constexpr const char *Name##_Cmd = #ScriptName;
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc) \
	[[maybe_unused]] constexpr const char *Name##_Cmd = #ScriptName;
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Flags, Desc) \
	[[maybe_unused]] constexpr const char *Name##_Cmd = #ScriptName;
#include <engine/shared/config_variables_bestclient.h>
#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_COL
#undef MACRO_CONFIG_STR
} // namespace CcCfg

using SOpt = CChainsawGui::SOpt;
using SFeature = CChainsawGui::SFeature;
using SCategory = CChainsawGui::SCategory;

namespace
{
SOpt MakeInt(CChainsawGui::EOptType Type, int *pValue, int Default, int Min, int Max, const char *pCommand, const char *pLabel)
{
	SOpt Opt;
	Opt.m_Type = Type;
	Opt.m_pInt = pValue;
	Opt.m_DefaultInt = Default;
	Opt.m_Min = Min;
	Opt.m_Max = Max;
	Opt.m_pCommand = pCommand;
	Opt.m_pLabel = pLabel;
	Opt.m_pSuffix = Type == CChainsawGui::OPT_SLIDER ? "%" : "";
	return Opt;
}

SOpt MakeChoice(int *pValue, int Default, const char *pCommand, const char *pLabel, std::initializer_list<const char *> Choices)
{
	SOpt Opt = MakeInt(CChainsawGui::OPT_CHOICE, pValue, Default, 0, (int)Choices.size() - 1, pCommand, pLabel);
	Opt.m_vChoices = Choices;
	return Opt;
}

SOpt MakeColor(unsigned *pValue, unsigned Default, const char *pCommand, const char *pLabel)
{
	SOpt Opt;
	Opt.m_Type = CChainsawGui::OPT_COLOR;
	Opt.m_pColor = pValue;
	Opt.m_DefaultColor = Default;
	Opt.m_pCommand = pCommand;
	Opt.m_pLabel = pLabel;
	return Opt;
}

SOpt MakeGroup(const char *pLabel)
{
	SOpt Opt;
	Opt.m_Type = CChainsawGui::OPT_GROUP;
	Opt.m_pLabel = pLabel;
	return Opt;
}

SOpt MakeButton(const char *pLabel, int Action, bool NeedsGame = true)
{
	SOpt Opt;
	Opt.m_Type = CChainsawGui::OPT_BUTTON;
	Opt.m_pLabel = pLabel;
	Opt.m_Action = Action;
	Opt.m_NeedsGame = NeedsGame;
	return Opt;
}

SOpt MakeSpecial(CChainsawGui::EOptType Type, const char *pLabel)
{
	SOpt Opt;
	Opt.m_Type = Type;
	Opt.m_pLabel = pLabel;
	return Opt;
}

SFeature MakeFeature(const char *pName, const char *pDesc, int *pEnable, const char *pCommand, std::initializer_list<SOpt> Opts)
{
	SFeature Feature;
	Feature.m_pName = pName;
	Feature.m_pDesc = pDesc;
	Feature.m_pEnable = pEnable;
	Feature.m_pCommand = pCommand;
	Feature.m_vOpts = Opts;
	return Feature;
}

struct STheme
{
	const char *m_pName;
	ColorRGBA m_A1;
	ColorRGBA m_A2;
	ColorRGBA m_Bg1;
	ColorRGBA m_Bg2;
};

constexpr ColorRGBA Rgb(int r, int g, int b) { return ColorRGBA(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f); }

const STheme s_aThemes[] = {
	{"Тлеющий уголь", Rgb(255, 61, 61), Rgb(255, 164, 61), Rgb(31, 16, 22), Rgb(12, 10, 14)},
	{"Кровь гуля", Rgb(255, 31, 75), Rgb(155, 69, 255), Rgb(27, 14, 28), Rgb(11, 10, 16)},
	{"Синий огонь", Rgb(54, 214, 255), Rgb(106, 92, 255), Rgb(15, 21, 34), Rgb(9, 10, 16)},
	{"Бенимару", Rgb(255, 226, 122), Rgb(255, 122, 46), Rgb(29, 20, 12), Rgb(13, 11, 9)},
};
constexpr int NUM_THEMES = (int)std::size(s_aThemes);

// looks of the color grading: saturation, vibrance, contrast, brightness, warmth
struct SLook
{
	const char *m_pName;
	int m_Saturation, m_Vibrance, m_Contrast, m_Brightness, m_Warmth;
};
const SLook s_aLooks[] = {
	{"Чистый", 100, 15, 104, 100, 0},
	{"Ярко", 125, 40, 110, 103, 5},
	{"Кино", 90, 10, 118, 97, 12},
	{"Тёплый вечер", 110, 20, 105, 102, 45},
	{"Холодная ночь", 85, 0, 112, 92, -45},
};
const std::vector<const char *> s_vLookNames = {"Чистый", "Ярко", "Кино", "Тёплый вечер", "Холодная ночь"};

unsigned PackRgb(ColorRGBA Color) { return color_cast<ColorHSLA>(Color).Pack(false); }

ColorRGBA Cfg(unsigned Packed) { return color_cast<ColorRGBA>(ColorHSLA(Packed)).WithAlpha(1.0f); }

ColorRGBA Mix(ColorRGBA a, ColorRGBA b, float t)
{
	return ColorRGBA(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t);
}

float Ease(float t)
{
	t = std::clamp(t, 0.0f, 1.0f);
	return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
}

float Approach(float Value, float Target, float Speed)
{
	return Value < Target ? std::min(Target, Value + Speed) : std::max(Target, Value - Speed);
}

const ColorRGBA INK(0.953f, 0.918f, 0.925f, 1.0f);
const ColorRGBA MUTED(0.659f, 0.588f, 0.620f, 1.0f);
const ColorRGBA DIM(0.459f, 0.400f, 0.431f, 1.0f);
const ColorRGBA LINE(1.0f, 0.886f, 0.918f, 0.08f);
const ColorRGBA LINE2(1.0f, 0.886f, 0.918f, 0.15f);
const ColorRGBA TIPBG(0.043f, 0.035f, 0.051f, 0.97f);
const ColorRGBA DARKTEXT(0.09f, 0.04f, 0.04f, 1.0f);

const char *NiceKeyName(const char *pKey)
{
	static const char *const s_aaNames[][2] = {
		{"rshift", "Right Shift"}, {"lshift", "Left Shift"}, {"rctrl", "Right Ctrl"}, {"lctrl", "Left Ctrl"},
		{"ralt", "Right Alt"}, {"lalt", "Left Alt"}, {"insert", "Insert"}, {"delete", "Delete"}, {"home", "Home"},
		{"end", "End"}, {"tab", "Tab"}, {"capslock", "Caps Lock"}, {"mouse3", "Колесо мыши"}, {"mouse4", "Mouse 4"}, {"mouse5", "Mouse 5"}};
	for(const auto &aName : s_aaNames)
		if(str_comp(pKey, aName[0]) == 0)
			return aName[1];
	return pKey;
}

uint64_t HashText(const char *pText, float Size, float LineWidth, const std::string &FontKey)
{
	uint64_t h = 1469598103934665603ULL;
	const auto Mixin = [&](uint8_t b) {
		h ^= b;
		h *= 1099511628211ULL;
	};
	for(const char *p = pText; *p; p++)
		Mixin((uint8_t)*p);
	Mixin(0);
	const int aNums[2] = {(int)std::lround(Size * 100.0f), (int)std::lround(LineWidth * 10.0f)};
	for(int n : aNums)
		for(int i = 0; i < 4; i++)
			Mixin((uint8_t)(n >> (i * 8)));
	for(char c : FontKey)
		Mixin((uint8_t)c);
	return h;
}

const char *const s_apSoundFiles[CChainsawGui::NUM_SOUNDS] = {
	"chainsaw/sounds/gui_open.opus", "chainsaw/sounds/gui_close.opus", "chainsaw/sounds/gui_on.opus", "chainsaw/sounds/gui_off.opus",
	"chainsaw/sounds/gui_click.opus", "chainsaw/sounds/gui_hover.opus", "chainsaw/sounds/gui_slide.opus"};

constexpr const char *BG_FOLDER = "chainsaw/menu_backgrounds";
} // namespace

#define CC_CHECK(Var, Label) MakeInt(CChainsawGui::OPT_CHECK, &g_Config.m_##Var, DefaultConfig::Var, 0, 1, CcCfg::Var##_Cmd, Label)
#define CC_SLIDER(Var, Label) MakeInt(CChainsawGui::OPT_SLIDER, &g_Config.m_##Var, DefaultConfig::Var, CcCfg::Var##_Min, CcCfg::Var##_Max, CcCfg::Var##_Cmd, Label)
#define CC_CHOICE(Var, Label, ...) MakeChoice(&g_Config.m_##Var, DefaultConfig::Var, CcCfg::Var##_Cmd, Label, {__VA_ARGS__})
#define CC_COLOR(Var, Label) MakeColor(&g_Config.m_##Var, DefaultConfig::Var, CcCfg::Var##_Cmd, Label)
#define CC_FEATURE(Name, Desc, Var, ...) MakeFeature(Name, Desc, &g_Config.m_##Var, CcCfg::Var##_Cmd, {__VA_ARGS__})

static SOpt StrOpt(CChainsawGui::EOptType Type, char *pStr, int Size, const char *pDefault, const char *pCommand, const char *pLabel)
{
	SOpt Opt = MakeSpecial(Type, pLabel);
	Opt.m_pStr = pStr;
	Opt.m_StrSize = Size;
	Opt.m_pDefaultStr = pDefault;
	Opt.m_pCommand = pCommand;
	return Opt;
}

CChainsawGui::CChainsawGui()
{
	m_Win = {0.0f, 0.0f, 0.0f, 0.0f};
	for(int &Sound : m_aSounds)
		Sound = -1;
}

void CChainsawGui::BuildItems()
{
	m_vCategories.clear();

	SCategory Fire;
	Fire.m_pName = "Огонь и лёд";
	Fire.m_Icon = ICON_FIRE;
	Fire.m_vFeatures = {
		CC_FEATURE("Огненное кольцо", "Кольцо огня с бусинами и языками пламени за твоим тее, как у Бенимару.", BcFireAura,
			CC_CHECK(BcFireAuraOthers, "Показывать у других игроков"),
			MakeGroup("ВИД"),
			CC_SLIDER(BcFireAuraSize, "Размер кольца"),
			CC_SLIDER(BcFireAuraFlames, "Языки пламени").Desc("Сколько языков огня поднимается над кольцом. Чем больше, тем ровнее огонь по всему кругу."),
			CC_SLIDER(BcFireAuraBeads, "Бусины").Suffix("").New().Desc("Сколько светящихся бусин бежит по кольцу."),
			CC_SLIDER(BcFireAuraSpin, "Скорость вращения"),
			CC_SLIDER(BcFireAuraGlow, "Свечение").New().Desc("Мягкий свет вокруг кольца и языков огня."),
			CC_COLOR(BcFireAuraColor, "Цвет огня").Desc("Этим же цветом горят пылающий молот и золотой топор.")),
		CC_FEATURE("Пылающий молот", "Молот горит, от ударов летят искры. Сам молот выглядит как золотой топор.", BcFireHammer,
			CC_CHECK(BcFireHammerOthers, "Пылающий молот у других"),
			CC_SLIDER(BcFireHammerSparks, "Искры от ударов"),
			CC_SLIDER(BcFireHammerFlames, "Языки огня").New().Desc("Сколько огня стекает с головы молота."),
			CC_SLIDER(BcFireHammerGlow, "Свечение").New().Desc("Свет вокруг головы молота."),
			MakeGroup("ЗОЛОТОЙ ТОПОР"),
			CC_SLIDER(BcFireAxeSize, "Размер топора"),
			CC_SLIDER(BcFireAxeGlow, "Свечение топора"),
			CC_CHECK(BcFireAxeSummon, "Появляется из огня и света").Desc("Когда включаешь пылающий молот с молотом в руке."),
			CC_CHECK(BcFireAxeSummonSwitch, "И каждый раз, когда берёшь молот").ShowIf(&g_Config.m_BcFireAxeSummon),
			CC_SLIDER(BcFireAxeSummonTime, "Время появления").Tenths().ShowIf(&g_Config.m_BcFireAxeSummon),
			CC_CHECK(BcFireAxeHeat, "Разгорается с каждым ударом").Desc("Каждое попадание по тее делает топор ярче, и он горит."),
			CC_SLIDER(BcFireAxeHeatTime, "Горит после удара").Tenths().ShowIf(&g_Config.m_BcFireAxeHeat),
			MakeButton("Тест появления", ACT_AXE_SUMMON),
			MakeButton("Тест удара", ACT_AXE_HIT)),
		CC_FEATURE("Ударная волна", "Волна пыли и огня, когда приземляешься с высоты или бьёшь молотом по тее.", BcLandShockwave,
			CC_CHECK(BcLandShockwaveOthers, "Волна у других игроков"),
			CC_SLIDER(BcLandShockwaveSens, "Чувствительность").New().Desc("Как легко приземление даёт волну: 0 — только большие падения, 100 — даже маленькие прыжки."),
			CC_SLIDER(BcLandShockwavePower, "Сила волны"),
			CC_SLIDER(BcLandShockwaveDust, "Пыль").New().Desc("Сколько пыли разлетается в стороны при приземлении."),
			CC_CHECK(BcLandShockwaveFire, "Огненная, когда горит кольцо").New().Desc("Пока включено огненное кольцо, волна окрашивается в цвет огня."),
			CC_COLOR(BcLandShockwaveColor, "Цвет волны").New().Desc("Цвет волны без огня и волны от удара молотом."),
			MakeButton("Тест волны", ACT_SHOCKWAVE)),
		CC_FEATURE("Лёд при заморозке", "Иней на тее и осколки льда, когда тее замерзает, и лёд разлетается, когда оттаивает.", BcFreezeIce,
			CC_CHECK(BcFreezeIceOthers, "Лёд у других игроков"),
			CC_SLIDER(BcFreezeIceAmount, "Осколки льда"),
			CC_SLIDER(BcFreezeIceFrost, "Иней на тее").New().Desc("Яркость инея и морозных звёздочек на замёрзшем тее."),
			CC_SLIDER(BcFreezeIceGlints, "Блёстки").New().Desc("Сколько ледяных искорок летает вокруг замёрзшего тее."),
			CC_COLOR(BcFreezeIceColor, "Цвет льда"),
			MakeButton("Тест льда", ACT_ICE)),
	};
	m_vCategories.push_back(Fire);

	SCategory Chain;
	Chain.m_pName = "Цепь и удары";
	Chain.m_Icon = ICON_CHAIN;
	Chain.m_vFeatures = {
		CC_FEATURE("Горящая цепь крюка", "Цепь крюка горит: огненный жгут с гарпуном на конце или раскалённые звенья.", BcFireChain,
			CC_CHOICE(BcFireChainStyle, "Вид", "Звенья", "Огненный жгут"),
			CC_CHECK(BcFireChainOthers, "Цепь у других игроков"),
			CC_SLIDER(BcFireChainFlames, "Пламя на цепи"),
			CC_SLIDER(BcFireChainWidth, "Толщина жгута").New().ShowIf(&g_Config.m_BcFireChainStyle, 1),
			CC_SLIDER(BcFireChainGlow, "Свечение").New().Desc("Свет вокруг цепи и наконечника."),
			CC_COLOR(BcFireChainColor, "Цвет огня").Desc("Этим же цветом горят ожоги и обычная вспышка удара.")),
		CC_FEATURE("Ожоги", "Чёрный след с огоньком на тее, по которому попал огненный крюк или удар.", BcFireBurn,
			CC_SLIDER(BcFireBurnTime, "Сколько держится ожог").Tenths()),
		CC_FEATURE("Вспышка удара", "Огненная вспышка в точке удара молотом.", BcFireStrike,
			CC_CHECK(BcFireStrikeOthers, "Вспышка у других игроков"),
			CC_SLIDER(BcFireStrikeSize, "Размер вспышки"),
			CC_SLIDER(BcFireStrikeShake, "Тряска экрана").New().Desc("Экран вздрагивает от твоих ударов. От мега-удара сильнее. 0 — без тряски."),
			MakeGroup("МЕГА-УДАР"),
			CC_CHECK(BcFireStrikeMega, "Огромный взрыв огня"),
			CC_CHECK(BcFireStrikeMegaHitsOnly, "Только при попадании по тее").ShowIf(&g_Config.m_BcFireStrikeMega),
			CC_COLOR(BcFireStrikeColor, "Цвет мега-удара").ShowIf(&g_Config.m_BcFireStrikeMega),
			MakeButton("Тест вспышки", ACT_STRIKE)),
	};
	m_vCategories.push_back(Chain);

	SCategory Kagune;
	Kagune.m_pName = "Кагуне";
	Kagune.m_Icon = ICON_KAGUNE;
	Kagune.m_vFeatures = {
		CC_FEATURE("Кагуне вместо молота", "Щупальца гуля вместо молота. При каждом ударе бьют туда, куда целишься, и хватаются вместо крюка.", BcKagune,
			CC_CHECK(BcKaguneOthers, "Кагуне у других игроков"),
			CC_CHECK(BcKaguneGun, "Оставлять кагуне с пистолетом"),
			MakeGroup("ВИД"),
			CC_CHOICE(BcKaguneStyle, "Вид кагуне", "Ринкаку", "Какуджа", "Укаку", "Кость").Desc("Ринкаку — алые ленты с венами. Какуджа — чёрно-фиолетовая броня с красными швами. Укаку — крыло из кристаллов, стреляет осколками. Кость — белые сегменты с красными глазами."),
			CC_SLIDER(BcKaguneCount, "Щупальца").Suffix(""),
			CC_SLIDER(BcKaguneLength, "Длина"),
			CC_SLIDER(BcKaguneWidth, "Толщина"),
			CC_SLIDER(BcKaguneShimmer, "Бегущий свет"),
			CC_SLIDER(BcKaguneParticles, "Красные клетки"),
			CC_SLIDER(BcKaguneGlow, "Свечение").New().Desc("Мягкий свет вокруг щупалец и на спине."),
			CC_COLOR(BcKaguneColor, "Цвет кагуне"),
			MakeGroup("АТАКИ"),
			CC_CHOICE(BcKaguneAttack, "Удар", "Комбо", "Одно", "Два", "Все как одно", "Случайно").Desc("Комбо: одно, одно, два, одно, два, потом все как одно. Пауза начинает заново."),
			CC_CHECK(BcKaguneBlood, "Кровь при попадании"),
			CC_SLIDER(BcKaguneBloodAmount, "Количество крови").ShowIf(&g_Config.m_BcKaguneBlood)),
	};
	m_vCategories.push_back(Kagune);

	SCategory Atm;
	Atm.m_pName = "Атмосфера";
	Atm.m_Icon = ICON_STAR;
	Atm.m_vFeatures = {
		CC_FEATURE("Светлячки", "Тёплые огоньки летают вокруг и мерцают.", BcAtmFireflies,
			CC_SLIDER(BcAtmFirefliesAmount, "Количество"),
			CC_SLIDER(BcAtmFirefliesSize, "Размер"),
			CC_SLIDER(BcAtmFirefliesGlow, "Свечение"),
			CC_SLIDER(BcAtmFirefliesSpeed, "Скорость"),
			CC_CHECK(BcAtmFirefliesBlink, "Мерцают"),
			CC_COLOR(BcAtmFirefliesColor, "Цвет")),
		CC_FEATURE("Падающие звёзды", "По небу иногда пролетают падающие звёзды с хвостом.", BcAtmStars,
			CC_SLIDER(BcAtmStarsRate, "Сколько в минуту").Suffix(""),
			CC_SLIDER(BcAtmStarsLength, "Длина хвоста"),
			CC_SLIDER(BcAtmStarsBright, "Яркость"),
			CC_COLOR(BcAtmStarsColor, "Цвет")),
		CC_FEATURE("Гроза", "Дождь, молнии в небе и вспышки, мир становится темнее. Гром — со звуком.", BcAtmStorm,
			MakeGroup("ДОЖДЬ"),
			CC_SLIDER(BcAtmRainAmount, "Сила дождя"),
			CC_SLIDER(BcAtmRainLength, "Длина капель"),
			CC_SLIDER(BcAtmRainOpacity, "Заметность"),
			CC_SLIDER(BcAtmStormDark, "Темнее мир"),
			CC_CHECK(BcAtmRainPhysics, "Капли падают на блоки").New().Desc("Капли рядом с тобой разбиваются о блоки на мелкие брызги и собираются в маленькие лужи. Под крышей и в пещерах не капает. Дальние капли летят позади, как раньше."),
			CC_SLIDER(BcAtmRainSplash, "Брызги").New().ShowIf(&g_Config.m_BcAtmRainPhysics),
			CC_SLIDER(BcAtmRainPuddles, "Лужи").New().Desc("Как быстро набираются лужи. 0 — без луж. Без дождя лужи высыхают.").ShowIf(&g_Config.m_BcAtmRainPhysics),
			MakeGroup("МОЛНИИ"),
			CC_SLIDER(BcAtmLightning, "Как часто").Desc("0 — молний нет, 100 — бьют каждые несколько секунд."),
			CC_SLIDER(BcAtmLightningFlash, "Яркость вспышки"),
			CC_CHECK(BcAtmLightningBolts, "Видно сами молнии в небе"),
			MakeButton("Тест молнии", ACT_TEST_LIGHTNING)),
		CC_FEATURE("Ветер", "Полосы воздуха и летящие листья. Ветер ещё сносит дождь, снег, туман и угли.", BcAtmWind,
			CC_SLIDER(BcAtmWindStrength, "Сила ветра"),
			CC_CHOICE(BcAtmWindDir, "Дует", "Влево", "Вправо"),
			CC_SLIDER(BcAtmWindStreaks, "Полосы воздуха"),
			CC_SLIDER(BcAtmWindLeaves, "Листья"),
			CC_CHOICE(BcAtmWindLeafKind, "Листья", "Осень", "Лето", "Сакура")),
		CC_FEATURE("Снег", "Падают снежинки.", BcAtmSnow,
			CC_SLIDER(BcAtmSnowAmount, "Количество"),
			CC_SLIDER(BcAtmSnowSize, "Размер"),
			CC_SLIDER(BcAtmSnowSpeed, "Скорость"),
			CC_COLOR(BcAtmSnowColor, "Цвет"),
			CC_CHECK(BcAtmSnowPhysics, "Снег копится на блоках").New().Desc("Снежинки рядом с тобой ложатся на блоки и на уже лежащий снег, вырастают маленькие сугробы. С краёв снег осыпается."),
			CC_SLIDER(BcAtmSnowPile, "Высота сугробов").New().ShowIf(&g_Config.m_BcAtmSnowPhysics),
			CC_SLIDER(BcAtmSnowMelt, "Как быстро тает").New().Desc("Пока идёт снег. 0 — не тает. Когда снег выключен, сугробы тают быстрее.").ShowIf(&g_Config.m_BcAtmSnowPhysics)),
		CC_FEATURE("Туман", "Мягкий туман плывёт над землёй или по всему экрану.", BcAtmFog,
			CC_SLIDER(BcAtmFogDensity, "Густота"),
			CC_CHECK(BcAtmFogLow, "Только у земли"),
			CC_SLIDER(BcAtmFogSpeed, "Скорость"),
			CC_COLOR(BcAtmFogColor, "Цвет")),
		CC_FEATURE("Угли и пепел", "Горящие угольки поднимаются вверх и мерцают.", BcAtmEmbers,
			CC_SLIDER(BcAtmEmbersAmount, "Количество"),
			CC_SLIDER(BcAtmEmbersSize, "Размер"),
			CC_SLIDER(BcAtmEmbersSpeed, "Скорость"),
			CC_COLOR(BcAtmEmbersColor, "Цвет")),
		CC_FEATURE("Звуки погоды", "Шум дождя, гром и ветер.", BcAtmSounds,
			CC_SLIDER(BcAtmSoundVolume, "Громкость")),
	};
	m_vCategories.push_back(Atm);

	SCategory Shaders;
	Shaders.m_pName = "Шейдеры";
	Shaders.m_Icon = ICON_IMAGE;
	SOpt Renderer = MakeSpecial(OPT_RENDERER, "");
	SOpt Looks = MakeSpecial(OPT_PRESETS, "Готовые").Desc("Ставит все ползунки ниже разом. Потом их можно подвинуть.");
	Shaders.m_vFeatures = {
		CC_FEATURE("Цвет и свет", "Насыщенность, контраст, яркость и тёплый или холодный свет картинки игры. Интерфейс не меняется.", BcPostFxColor,
			Renderer,
			Looks,
			CC_SLIDER(BcPostFxSaturation, "Насыщенность"),
			CC_SLIDER(BcPostFxVibrance, "Сочность").Desc("Добавляет цвета только блёклым местам, яркие не пересвечивает."),
			CC_SLIDER(BcPostFxContrast, "Контраст"),
			CC_SLIDER(BcPostFxBrightness, "Яркость"),
			CC_SLIDER(BcPostFxWarmth, "Тёплый / холодный").Suffix(""),
			CC_COLOR(BcPostFxTint, "Цвет света").Desc("Белый — без оттенка.")),
		CC_FEATURE("Свечение (bloom)", "Яркие места светятся: огонь, молнии, лазеры, светлячки.", BcPostFxBloom,
			Renderer,
			CC_SLIDER(BcPostFxBloomAmount, "Сила"),
			CC_SLIDER(BcPostFxBloomThreshold, "Порог").Desc("Насколько ярким должно быть место, чтобы светиться."),
			CC_SLIDER(BcPostFxBloomRadius, "Размер")),
		CC_FEATURE("Виньетка", "Края экрана темнее, взгляд в центр.", BcPostFxVignette,
			Renderer,
			CC_SLIDER(BcPostFxVignetteAmount, "Сила")),
		CC_FEATURE("Резкость", "Картинка чётче.", BcPostFxSharpen,
			Renderer,
			CC_SLIDER(BcPostFxSharpenAmount, "Сила")),
		CC_FEATURE("Зерно плёнки", "Лёгкий шум, как на плёнке.", BcPostFxGrain,
			Renderer,
			CC_SLIDER(BcPostFxGrainAmount, "Сила")),
		CC_FEATURE("Цветные края", "Хроматическая аберрация: по краям экрана цвета чуть расходятся.", BcPostFxChroma,
			Renderer,
			CC_SLIDER(BcPostFxChromaAmount, "Сила")),
	};
	m_vCategories.push_back(Shaders);

	SCategory Perf;
	Perf.m_pName = "Оптимизация";
	Perf.m_Icon = ICON_BOLT;
	SOpt PerfStatus = MakeSpecial(OPT_PERF_STATUS, "");
	Perf.m_vFeatures = {
		CC_FEATURE("Максимальная оптимизация", "Выключает всё тяжёлое разом: больше FPS и ничего лишнего. Выключишь режим — твои настройки вернутся как были.", BcPerfMode,
			PerfStatus,
			MakeGroup("ЧТО ВЫКЛЮЧАТЬ"),
			CC_CHECK(BcPerfOwn, "Эффекты ChainsawClient").Desc("Огненное кольцо, пылающий молот и топор, ударная волна, лёд, горящая цепь, ожоги, вспышка удара, кагуне."),
			CC_CHECK(BcPerfAtm, "Атмосфера и шейдеры").Desc("Светлячки, звёзды, гроза, ветер, снег, туман, угли, все шейдеры, погода BestClient и видео вместо фона карты."),
			CC_CHECK(BcPerfOthers, "Эффекты BestClient и TClient").Desc("3D-частицы, motion blur, желе-тее, след курсора, свечение оружия, ядерный взрыв, градиент и полёт ников, стили крюка, кольцо прыжка, взмах молота, энергия, нити холода, молния при заморозке, следы тее, питомец, радуга."),
			CC_CHECK(BcPerfMap, "Карта попроще").Desc("Без слоёв высокой детализации, без квадов (картинок и фона карты) и без обводок тайлов."),
			CC_CHECK(BcPerfParticles, "Без частиц").Desc("Совсем без частиц: дым, искры, звёздочки и следы снарядов. Через оптимизатор BestClient."),
			CC_CHECK(BcPerfSync, "Без V-Sync и сглаживания").Desc("Вертикальная синхронизация выключается сразу: FPS не упирается в частоту монитора, меньше задержка. Сглаживание (FSAA) — после перезапуска игры."),
			CC_CHECK(BcPerfHud, "Ничего лишнего на экране").Desc("Музыкальный плеер, картинки и гифки в чате, облачка над игроками, клавиши на экране, анимации интерфейса, видео в главном меню. Это меню — в лёгком режиме."),
			CC_CHECK(BcPerfSystem, "Система").Desc("Игре — высокий приоритет процесса. Статус в Discord выключается (после перезапуска игры).")),
	};
	Perf.m_vFeatures[0].m_Open = true; // the groups are the point of it: shown right away
	m_vCategories.push_back(Perf);

	SCategory Menu;
	Menu.m_pName = "Меню";
	Menu.m_Icon = ICON_MENU;
	SFeature Look;
	Look.m_pName = "Вид меню";
	Look.m_pDesc = "Цвета градиента, прозрачность и размер этого окна.";
	Look.m_vOpts = {
		MakeSpecial(OPT_THEMES, "Готовые градиенты").Desc("Нажми, чтобы поставить все четыре цвета разом. Потом их можно поменять ниже."),
		CC_COLOR(BcGuiColor1, "Цвет градиента 1"),
		CC_COLOR(BcGuiColor2, "Цвет градиента 2"),
		CC_COLOR(BcGuiBg1, "Фон сверху"),
		CC_COLOR(BcGuiBg2, "Фон снизу"),
		CC_SLIDER(BcGuiOpacity, "Непрозрачность фона"),
		CC_SLIDER(BcGuiScale, "Размер меню"),
		CC_SLIDER(BcGuiTooltipDelay, "Задержка подсказки").Suffix(" мс"),
	};
	SFeature Back;
	Back.m_pName = "Картинка за меню";
	Back.m_pDesc = "Пейзаж или своя картинка за окном меню. Свои картинки (png, jpg, webp) клади в папку chainsaw/menu_backgrounds.";
	Back.m_vOpts = {
		MakeSpecial(OPT_BG_PRESETS, "Готовые"),
		MakeSpecial(OPT_BG_FILES, "Свои картинки"),
		CC_SLIDER(BcGuiBgDim, "Затемнение").Desc("Насколько цвета меню закрывают картинку."),
		CC_SLIDER(BcGuiBgBlur, "Размытие"),
		MakeButton("Открыть папку", ACT_OPEN_BG_FOLDER, false),
		MakeButton("Обновить список", ACT_RESCAN_BG, false),
	};
	Back.m_vOpts[0].m_pInt = &g_Config.m_BcGuiBgPreset;
	Back.m_vOpts[0].m_DefaultInt = DefaultConfig::BcGuiBgPreset;
	Back.m_vOpts[0].m_pCommand = CcCfg::BcGuiBgPreset_Cmd;
	Back.m_vOpts[1].m_pStr = g_Config.m_BcGuiBgFile;
	Back.m_vOpts[1].m_StrSize = sizeof(g_Config.m_BcGuiBgFile);
	Back.m_vOpts[1].m_pDefaultStr = DefaultConfig::BcGuiBgFile;
	Back.m_vOpts[1].m_pCommand = CcCfg::BcGuiBgFile_Cmd;
	SFeature Font;
	Font.m_pName = "Шрифт меню";
	Font.m_pDesc = "Шрифт и размер текста только в этом меню, остальная игра не меняется. Свои шрифты (ttf, otf) клади в папку tclient/fonts.";
	Font.m_vOpts = {
		StrOpt(OPT_FONT, g_Config.m_BcGuiFont, sizeof(g_Config.m_BcGuiFont), DefaultConfig::BcGuiFont, CcCfg::BcGuiFont_Cmd, "Шрифт"),
		CC_SLIDER(BcGuiFontSize, "Размер текста"),
	};
	SFeature Anim;
	Anim.m_pName = "Анимации и звуки";
	Anim.m_pDesc = "Как появляется меню, звуки, и лёгкий режим для слабых ПК.";
	Anim.m_vOpts = {
		CC_CHECK(BcGuiLight, "Лёгкий режим для слабых ПК").Desc("Без тени, свечения, картинки и анимаций. Меню почти не тратит FPS."),
		CC_CHECK(BcGuiAnim, "Анимации").ShowIf(&g_Config.m_BcGuiLight, 0),
		CC_CHOICE(BcGuiAnimStyle, "Появление", "Плавно", "Сверху", "Сбоку", "Прозрачность").ShowIf(&g_Config.m_BcGuiAnim),
		MakeGroup("ЗВУКИ"),
		CC_CHECK(BcGuiSounds, "Звуки меню"),
		CC_SLIDER(BcGuiSoundVolume, "Громкость").ShowIf(&g_Config.m_BcGuiSounds),
		CC_CHECK(BcGuiSoundHover, "Тихий щелчок при наведении").ShowIf(&g_Config.m_BcGuiSounds),
	};
	SFeature Keys;
	Keys.m_pName = "Клавиша меню";
	Keys.m_pDesc = "Какой клавишей открывать и закрывать это меню.";
	Keys.m_vOpts = {
		StrOpt(OPT_KEY, g_Config.m_BcGuiKey, sizeof(g_Config.m_BcGuiKey), DefaultConfig::BcGuiKey, CcCfg::BcGuiKey_Cmd, "Открывать по"),
		CC_CHECK(BcGuiEsc, "Закрывать по Esc"),
		MakeSpecial(OPT_NOTE, "Можно и консолью: toggle_chainsaw_gui"),
	};
	Menu.m_vFeatures = {Look, Back, Font, Anim, Keys};
	m_vCategories.push_back(Menu);

	for(int c = 0; c < (int)m_vCategories.size(); c++)
		for(SFeature &Feature : m_vCategories[c].m_vFeatures)
			Feature.m_Category = c;
	m_Built = true;
}

// our own sound files: read directly, because the sound pack logic of LoadOpus skips files that are not in the pack
int CChainsawGui::LoadOwnOpus(const char *pPath)
{
	void *pData = nullptr;
	unsigned DataSize = 0;
	if(!Sound()->IsSoundEnabled() || !Storage()->ReadFile(pPath, IStorage::TYPE_ALL, &pData, &DataSize))
		return -1;
	const int Id = Sound()->LoadOpusFromMem(pData, DataSize, false, pPath);
	free(pData);
	return Id;
}

void CChainsawGui::OnConsoleInit()
{
	Console()->Register("toggle_chainsaw_gui", "", CFGFLAG_CLIENT, ConToggle, this, "Open or close the ChainsawClient menu");
}

void CChainsawGui::OnInit()
{
	if(!m_Built)
		BuildItems();
	m_SearchInput.SetEmptyText("Найти функцию…");
	for(int i = 0; i < NUM_SOUNDS; i++)
		m_aSounds[i] = LoadOwnOpus(s_apSoundFiles[i]);
	Storage()->CreateFolder("chainsaw", IStorage::TYPE_SAVE);
	Storage()->CreateFolder(BG_FOLDER, IStorage::TYPE_SAVE);
}

void CChainsawGui::OnShutdown()
{
	ClearTextCache();
}

void CChainsawGui::OnWindowResize()
{
	ClearTextCache();
}

void CChainsawGui::OnReset()
{
	if(m_Active)
		SetActive(false);
}

void CChainsawGui::OnRelease()
{
	SetActive(false);
}

void CChainsawGui::ConToggle(IConsole::IResult *pResult, void *pUserData)
{
	CChainsawGui *pSelf = static_cast<CChainsawGui *>(pUserData);
	if(pSelf->m_Active || pSelf->CanOpen())
		pSelf->SetActive(!pSelf->m_Active);
}

bool CChainsawGui::CanOpen() const
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return false;
	return !GameClient()->m_Menus.IsActive();
}

bool CChainsawGui::Light() const
{
	return g_Config.m_BcGuiLight != 0;
}

void CChainsawGui::PlaySound(int SoundId, float Volume)
{
	if(!g_Config.m_BcGuiSounds || SoundId < 0 || SoundId >= NUM_SOUNDS || m_aSounds[SoundId] < 0)
		return;
	Sound()->Play(CSounds::CHN_GLOBAL, m_aSounds[SoundId], 0, std::min(1.0f, Volume * g_Config.m_BcGuiSoundVolume / 100.0f));
}

void CChainsawGui::SetUiMousePos(vec2 Pos)
{
	const vec2 WindowSize = vec2(Graphics()->WindowWidth(), Graphics()->WindowHeight());
	const CUIRect *pScreen = Ui()->Screen();
	const vec2 UpdatedMousePos = Ui()->UpdatedMousePos();
	Pos = Pos / vec2(pScreen->w, pScreen->h) * WindowSize;
	Ui()->OnCursorMove(Pos.x - UpdatedMousePos.x, Pos.y - UpdatedMousePos.y);
}

void CChainsawGui::SetActive(bool Active)
{
	if(m_Active == Active)
		return;
	m_Active = Active;
	if(m_Active)
	{
		if(!m_Built)
			BuildItems();
		if(!m_Closing)
			m_Anim = 0.0f;
		m_Closing = false;
		m_ListTime = 0.0f;
		m_MouseUnlocked = true;
		m_LastGameMousePos = Ui()->MousePos();
		SetUiMousePos(m_LastMenuMousePos.value_or(Ui()->Screen()->Center()));
		m_pTipId = nullptr;
		m_BgScanned = false;
		PlaySound(SND_OPEN);
	}
	else
	{
		m_Closing = g_Config.m_BcGuiAnim && !Light() && m_Anim > 0.0f;
		PlaySound(SND_CLOSE);
		if(m_MouseUnlocked)
		{
			Ui()->ClosePopupMenus();
			Ui()->ClearHotkeys();
			Ui()->SetActiveItem(nullptr);
			m_SearchInput.Deactivate();
			m_WaitingKey = false;
			m_MouseUnlocked = false;
			m_LastMenuMousePos = Ui()->MousePos();
			if(m_LastGameMousePos.has_value())
				SetUiMousePos(m_LastGameMousePos.value());
		}
	}
}

bool CChainsawGui::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(!m_Active || !m_MouseUnlocked)
		return false;
	Ui()->ConvertMouseMove(&x, &y, CursorType);
	Ui()->OnCursorMove(x, y);
	return true;
}

bool CChainsawGui::OnInput(const IInput::CEvent &Event)
{
	const bool Press = (Event.m_Flags & IInput::FLAG_PRESS) != 0;
	const bool Repeat = (Event.m_Flags & IInput::FLAG_REPEAT) != 0;
	const int ToggleKey = Input()->FindKeyByName(g_Config.m_BcGuiKey);

	if(!m_Active)
	{
		if(Press && !Repeat && ToggleKey > KEY_FIRST && Event.m_Key == ToggleKey && CanOpen())
		{
			SetActive(true);
			return true;
		}
		return false;
	}

	if(m_WaitingKey && Press && Event.m_Key > KEY_FIRST && Event.m_Key < KEY_LAST)
	{
		// left click or escape cancels, any other key (mouse buttons too) becomes the menu key
		if(Event.m_Key != KEY_ESCAPE && Event.m_Key != KEY_MOUSE_1)
		{
			str_copy(g_Config.m_BcGuiKey, KeyName(Event.m_Key), sizeof(g_Config.m_BcGuiKey));
			PlaySound(SND_ON);
		}
		m_WaitingKey = false;
		return true;
	}

	if(Press && Event.m_Key == KEY_ESCAPE)
	{
		if(Ui()->IsPopupOpen())
		{
			Ui()->ClosePopupMenus();
			return true;
		}
		if(g_Config.m_BcGuiEsc)
		{
			SetActive(false);
			return true;
		}
	}
	// while typing in the search the key is a letter or shift for capitals
	if(Press && !Repeat && ToggleKey > KEY_FIRST && Event.m_Key == ToggleKey && !m_SearchInput.IsActive())
	{
		SetActive(false);
		return true;
	}

	Ui()->OnInput(Event);
	// releases go on, so keys held when the menu opened do not get stuck
	return Press || (Event.m_Flags & IInput::FLAG_TEXT) != 0;
}

// ---------------------------------------------------------------- text cache

float CChainsawGui::F(float Size) const
{
	return Size * m_K * g_Config.m_BcGuiFontSize / 100.0f;
}

const CChainsawGui::SCachedText &CChainsawGui::Text(const char *pText, float Size, float LineWidth, bool Icon)
{
	const uint64_t Key = HashText(pText, Size, LineWidth, m_FontKey) ^ (Icon ? 0x9e3779b97f4a7c15ULL : 0ULL);
	auto It = m_TextCache.find(Key);
	if(It != m_TextCache.end())
	{
		It->second.m_LastFrame = m_Frame;
		return It->second;
	}
	SCachedText &Entry = m_TextCache[Key];
	Entry.m_LastFrame = m_Frame;
	if(pText[0] == '\0')
		return Entry;
	CTextCursor Cursor;
	Cursor.m_FontSize = Size;
	Cursor.m_LineWidth = LineWidth;
	Cursor.m_Flags = TEXTFLAG_RENDER;
	// the color comes when drawing, so the quads keep the default one
	const ColorRGBA OldColor = TextRender()->GetTextColor();
	const ColorRGBA OldOutline = TextRender()->GetTextOutlineColor();
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
	if(Icon)
	{
		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
	}
	TextRender()->CreateTextContainer(Entry.m_Index, &Cursor, pText);
	if(Icon)
	{
		TextRender()->SetRenderFlags(0);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	}
	TextRender()->TextColor(OldColor);
	TextRender()->TextOutlineColor(OldOutline);
	const STextBoundingBox Box = Cursor.BoundingBox();
	Entry.m_W = Box.m_W;
	Entry.m_H = Box.m_H;
	Entry.m_MaxCharH = Cursor.m_MaxCharacterHeight;
	return Entry;
}

void CChainsawGui::ClearLogo()
{
	if(m_LogoName.Valid())
		TextRender()->DeleteTextContainer(m_LogoName);
	if(m_LogoSub.Valid())
		TextRender()->DeleteTextContainer(m_LogoSub);
	m_LogoKey.clear();
}

void CChainsawGui::ClearTextCache()
{
	for(auto &[Key, Entry] : m_TextCache)
		if(Entry.m_Index.Valid())
			TextRender()->DeleteTextContainer(Entry.m_Index);
	m_TextCache.clear();
	ClearLogo();
	m_TextGeneration++;
}

// ---------------------------------------------------------------- fonts

void CChainsawGui::ApplyMenuFont()
{
	char aKey[96];
	str_format(aKey, sizeof(aKey), "%s|%d", g_Config.m_BcGuiFont, g_Config.m_BcGuiFontSize);
	if(m_FontKey != aKey)
	{
		ClearTextCache();
		m_FontKey = aKey;
	}
	if(g_Config.m_BcGuiFont[0] != '\0')
		TextRender()->SetCustomFace(g_Config.m_BcGuiFont);
}

void CChainsawGui::RestoreFont()
{
	if(g_Config.m_BcGuiFont[0] != '\0')
		BestClientApplyMenuFont(TextRender());
}

// ---------------------------------------------------------------- batched drawing

ColorRGBA CChainsawGui::Fade(ColorRGBA Color) const
{
	Color.a *= m_Alpha;
	return Color;
}

void CChainsawGui::Rect4(const CUIRect &R, ColorRGBA TL, ColorRGBA TR, ColorRGBA BL, ColorRGBA BR, float Rounding, int Corners)
{
	if(R.w <= 0.0f || R.h <= 0.0f)
		return;
	TL = Fade(TL);
	TR = Fade(TR);
	BL = Fade(BL);
	BR = Fade(BR);
	if(TL.a <= 0.002f && TR.a <= 0.002f && BL.a <= 0.002f && BR.a <= 0.002f)
		return;
	SRectCmd Cmd;
	Cmd.m_X = R.x;
	Cmd.m_Y = R.y;
	Cmd.m_W = R.w;
	Cmd.m_H = R.h;
	Cmd.m_Round = std::min(Rounding, std::min(R.w, R.h) * 0.5f);
	Cmd.m_Corners = Cmd.m_Round > 0.0f ? Corners : IGraphics::CORNER_NONE;
	Cmd.m_aColors[0] = TL;
	Cmd.m_aColors[1] = TR;
	Cmd.m_aColors[2] = BL;
	Cmd.m_aColors[3] = BR;
	m_vRects.push_back(Cmd);
}

void CChainsawGui::Rect(const CUIRect &R, ColorRGBA Color, float Rounding, int Corners)
{
	Rect4(R, Color, Color, Color, Color, Rounding, Corners);
}

void CChainsawGui::RectH(const CUIRect &R, ColorRGBA Left, ColorRGBA Right, float Rounding, int Corners)
{
	Rect4(R, Left, Right, Left, Right, Rounding, Corners);
}

void CChainsawGui::Glow(vec2 Pos, float Size, ColorRGBA Color)
{
	if(Light())
		return;
	Color = Fade(Color);
	if(Color.a <= 0.002f)
		return;
	m_vGlows.push_back({Pos, Size, Color});
}

void CChainsawGui::TextAt(float x, float y, const char *pText, float Size, ColorRGBA Color, float LineWidth)
{
	const SCachedText &T = Text(pText, Size, LineWidth);
	if(!T.m_Index.Valid())
		return;
	Color = Fade(Color);
	m_vTexts.push_back({T.m_Index, x, y, Color, ColorRGBA(0.0f, 0.0f, 0.0f, 0.18f * Color.a)});
}

void CChainsawGui::Label(const CUIRect &R, const char *pText, float Size, int Align, ColorRGBA Color)
{
	const SCachedText &T = Text(pText, Size);
	if(!T.m_Index.Valid())
		return;
	const vec2 Pos = CUi::CalcAlignedCursorPos(&R, vec2(T.m_W, T.m_H), Align, &T.m_MaxCharH);
	Color = Fade(Color);
	m_vTexts.push_back({T.m_Index, Pos.x, Pos.y, Color, ColorRGBA(0.0f, 0.0f, 0.0f, 0.18f * Color.a)});
}

void CChainsawGui::IconLabel(const CUIRect &R, const char *pIcon, float Size, ColorRGBA Color)
{
	// a glyph of the icon font, cached like every other label
	const SCachedText &T = Text(pIcon, Size, -1.0f, true);
	if(!T.m_Index.Valid())
		return;
	const vec2 Pos = CUi::CalcAlignedCursorPos(&R, vec2(T.m_W, T.m_H), TEXTALIGN_MC, &T.m_MaxCharH);
	m_vTexts.push_back({T.m_Index, Pos.x, Pos.y, Fade(Color), ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f)});
}

void CChainsawGui::Flush()
{
	if(!m_vRects.empty())
	{
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		for(const SRectCmd &Cmd : m_vRects)
			Graphics()->DrawRectExt4(Cmd.m_X, Cmd.m_Y, Cmd.m_W, Cmd.m_H, Cmd.m_aColors[0], Cmd.m_aColors[1], Cmd.m_aColors[2], Cmd.m_aColors[3], Cmd.m_Round, Cmd.m_Corners);
		Graphics()->QuadsEnd();
		m_vRects.clear();
	}
	if(!m_vShapes.empty() || !m_vGlows.empty())
	{
		const CcFx::CAtlas &Atlas = CcFx::Atlas(Graphics());
		if(!m_vShapes.empty())
		{
			CcFx::Begin(Graphics(), Atlas.m_Texture, false);
			for(const auto &Shape : m_vShapes)
				Shape();
			CcFx::End(Graphics());
			m_vShapes.clear();
		}
		if(!m_vGlows.empty())
		{
			CcFx::Begin(Graphics(), Atlas.m_Texture, true);
			for(const SGlowCmd &Cmd : m_vGlows)
				CcFx::QuadAxis(Graphics(), Cmd.m_Pos, Cmd.m_Size, Cmd.m_Size, Atlas.m_Glow, Cmd.m_Color);
			CcFx::End(Graphics());
			m_vGlows.clear();
		}
	}
	for(const STextCmd &Cmd : m_vTexts)
		TextRender()->RenderTextContainer(Cmd.m_Index, Cmd.m_Color, Cmd.m_Outline, Cmd.m_X, Cmd.m_Y);
	m_vTexts.clear();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void CChainsawGui::ClipEnable(const CUIRect *pRect)
{
	Flush();
	Ui()->ClipEnable(pRect);
}

void CChainsawGui::ClipDisable()
{
	Flush();
	Ui()->ClipDisable();
}

static void Stroke(IGraphics *pGraphics, vec2 a, vec2 b, float Width, const ColorRGBA &Color)
{
	const vec2 d = b - a;
	const float Len = length(d);
	if(Len < 0.0001f)
		return;
	const vec2 n = vec2(-d.y, d.x) / Len * (Width * 0.5f);
	CcFx::Shape(pGraphics, a - n, a + n, b - n, b + n, Color);
}

void CChainsawGui::DrawIcon(int Icon, const CUIRect &R, ColorRGBA Main, ColorRGBA Second)
{
	const vec2 C = R.Center();
	const float S = R.h;
	Main = Fade(Main);
	Second = Fade(Second);
	IGraphics *pGraphics = Graphics();
	if(Icon == ICON_FIRE)
	{
		const ColorRGBA Core = Fade(ColorRGBA(1.0f, 0.97f, 0.9f, 1.0f));
		m_vShapes.emplace_back([=]() {
			const CcFx::CAtlas &Atlas = CcFx::Atlas(pGraphics);
			vec2 Center, Right, Up;
			CcFx::TongueQuad(C + vec2(0.0f, S * 0.5f), vec2(0.0f, -1.0f), S * 0.5f, Center, Right, Up);
			const ColorRGBA aColors[3] = {Main, Second, Core};
			for(int Layer = 0; Layer < std::min(3, (int)BcFireShapes::LAYERS); Layer++)
				CcFx::Quad(pGraphics, Center, Right, Up, Atlas.m_aaFlame[1][Layer], aColors[Layer]);
		});
	}
	else if(Icon == ICON_CHAIN)
	{
		m_vShapes.emplace_back([=]() {
			for(int Link = 0; Link < 2; Link++)
			{
				const vec2 At = C + vec2(Link ? S * 0.17f : -S * 0.17f, Link ? -S * 0.17f : S * 0.17f);
				const vec2 Ax = normalize(vec2(1.0f, -1.0f)), Ay = vec2(-Ax.y, Ax.x);
				constexpr int SEGMENTS = 16;
				vec2 Last;
				for(int i = 0; i <= SEGMENTS; i++)
				{
					const float a = i * 2.0f * pi / SEGMENTS;
					const vec2 P = At + Ax * (std::cos(a) * S * 0.3f) + Ay * (std::sin(a) * S * 0.17f);
					if(i > 0)
						Stroke(pGraphics, Last, P, S * 0.11f, Link ? Second : Main);
					Last = P;
				}
			}
		});
	}
	else if(Icon == ICON_KAGUNE)
	{
		m_vShapes.emplace_back([=]() {
			const vec2 Root = C + vec2(0.0f, S * 0.42f);
			const vec2 aCtrl[3] = {vec2(-0.55f, 0.05f), vec2(0.1f, -0.15f), vec2(0.55f, 0.05f)};
			const vec2 aTip[3] = {vec2(-0.3f, -0.45f), vec2(-0.05f, -0.5f), vec2(0.42f, -0.4f)};
			for(int t = 0; t < 3; t++)
			{
				constexpr int SEGMENTS = 9;
				vec2 Last = Root;
				for(int i = 1; i <= SEGMENTS; i++)
				{
					const float u = i / (float)SEGMENTS;
					const vec2 Ctrl = C + aCtrl[t] * S, Tip = C + aTip[t] * S;
					const vec2 P = Root * ((1 - u) * (1 - u)) + Ctrl * (2 * u * (1 - u)) + Tip * (u * u);
					Stroke(pGraphics, Last, P, S * (0.17f - 0.12f * u), Mix(Main, Second, u));
					Last = P;
				}
			}
		});
	}
	else if(Icon == ICON_STAR)
		IconLabel(R, FontIcon::STAR, S * 0.8f, Main);
	else if(Icon == ICON_IMAGE)
		IconLabel(R, FontIcon::IMAGE, S * 0.8f, Main);
	else if(Icon == ICON_BOLT)
	{
		// a lightning bolt: top right, down to the middle left, a step right, down to the bottom left
		m_vShapes.emplace_back([=]() {
			const vec2 aPoints[] = {C + vec2(0.2f, -0.5f) * S, C + vec2(-0.2f, 0.06f) * S, C + vec2(0.16f, 0.0f) * S, C + vec2(-0.16f, 0.5f) * S};
			constexpr int NUM = (int)std::size(aPoints);
			for(int i = 0; i + 1 < NUM; i++)
				Stroke(pGraphics, aPoints[i], aPoints[i + 1], S * 0.13f, Mix(Main, Second, (i + 0.5f) / (NUM - 1)));
		});
	}
	else
		IconLabel(R, FontIcon::GEAR, S * 0.85f, Main);
}

void CChainsawGui::DrawLogo(const CUIRect &R)
{
	// a chainsaw drawn as an outline: engine body with the top handle, the guide bar with teeth on it;
	// the line takes the menu gradient from left to right
	const ColorRGBA A1 = Fade(Cfg(g_Config.m_BcGuiColor1)), A2 = Fade(Cfg(g_Config.m_BcGuiColor2));
	const float x0 = R.x, y0 = R.y, w = R.w, h = R.h;
	const float Width = std::max(1.0f, h * 0.085f);
	IGraphics *pGraphics = Graphics();
	m_vShapes.emplace_back([=]() {
		const auto P = [&](float u, float v) { return vec2(x0 + u * w, y0 + v * h); };
		const auto Col = [&](vec2 p) { return Mix(A1, A2, std::clamp((p.x - x0) / w, 0.0f, 1.0f)); };
		const auto Seg = [&](vec2 a, vec2 b) { Stroke(pGraphics, a, b, Width, Col((a + b) * 0.5f)); };
		// a rounded rectangle outline in design units
		const auto RoundRect = [&](float u0, float v0, float u1, float v1, float rad) {
			const float rx = rad * h / w, ry = rad;
			const vec2 aCenters[4] = {vec2(u1 - rx, v0 + ry), vec2(u1 - rx, v1 - ry), vec2(u0 + rx, v1 - ry), vec2(u0 + rx, v0 + ry)};
			vec2 Last = P(u0 + rx, v0);
			for(int c = 0; c < 4; c++)
			{
				for(int i = 0; i <= 4; i++)
				{
					const float a = -pi * 0.5f + (c + i / 4.0f) * pi * 0.5f;
					const vec2 Cur = P(aCenters[c].x + std::cos(a) * rx, aCenters[c].y + std::sin(a) * ry);
					Seg(Last, Cur);
					Last = Cur;
				}
			}
			Seg(Last, P(u0 + rx, v0));
		};
		RoundRect(0.03f, 0.36f, 0.42f, 0.94f, 0.14f); // engine
		// top handle: an arch over the engine
		{
			vec2 Last = P(0.09f, 0.36f);
			for(int i = 1; i <= 8; i++)
			{
				const float a = pi - i * pi / 8.0f;
				const vec2 Cur = P(0.22f + std::cos(a) * 0.13f, 0.34f - std::sin(a) * 0.28f);
				Seg(Last, Cur);
				Last = Cur;
			}
		}
		RoundRect(0.42f, 0.5f, 0.98f, 0.84f, 0.17f); // guide bar
		// teeth along the top of the bar
		for(float u = 0.47f; u < 0.93f; u += 0.075f)
		{
			Seg(P(u, 0.5f), P(u + 0.03f, 0.38f));
			Seg(P(u + 0.03f, 0.38f), P(u + 0.05f, 0.5f));
		}
		// starter grip and the bar bolt
		Seg(P(0.12f, 0.65f), P(0.3f, 0.65f));
		Seg(P(0.5f, 0.67f), P(0.53f, 0.67f));
	});
	Glow(R.Center(), R.w * 1.4f, Cfg(g_Config.m_BcGuiColor1).WithAlpha(0.12f));
}

void CChainsawGui::DrawSwitch(const CUIRect &R, float On)
{
	const float K = m_K;
	const ColorRGBA A1 = Cfg(g_Config.m_BcGuiColor1), A2 = Cfg(g_Config.m_BcGuiColor2);
	Rect(R, ColorRGBA(1.0f, 1.0f, 1.0f, 0.1f * (1.0f - On)), R.h * 0.5f);
	if(On > 0.0f)
	{
		RectH(R, A1.WithAlpha(On), A2.WithAlpha(On), R.h * 0.5f);
		Glow(R.Center(), R.w * 1.5f, A2.WithAlpha(0.22f * On));
	}
	const float Knob = R.h - 4.0f * K;
	CUIRect KnobRect = {R.x + 2.0f * K + On * (R.w - Knob - 4.0f * K), R.y + 2.0f * K, Knob, Knob};
	Rect(KnobRect, Mix(ColorRGBA(0.61f, 0.55f, 0.58f, 1.0f), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), On), Knob * 0.5f);
}

void CChainsawGui::DrawNewBadge(CUIRect *pRect, float FontSize)
{
	const float K = m_K;
	const ColorRGBA A2 = Cfg(g_Config.m_BcGuiColor2);
	const float W = TextW("+", FontSize) + 6.0f * K;
	CUIRect Badge = {pRect->x, pRect->y + (pRect->h - FontSize - 3.0f * K) * 0.5f, W, FontSize + 3.0f * K};
	Rect(Badge, A2.WithAlpha(0.16f), 3.0f * K);
	Label(Badge, "+", FontSize, TEXTALIGN_MC, A2);
	pRect->x += W;
	pRect->w -= W;
}

// ---------------------------------------------------------------- input helpers

int CChainsawGui::Button(const void *pId, const CUIRect *pRect, unsigned Flags)
{
	if(!m_Interactive)
		return 0;
	return Ui()->DoButtonLogic(pId, 0, pRect, Flags);
}

bool CChainsawGui::Hovered(const CUIRect *pRect) const
{
	return m_Interactive && Ui()->MouseHovered(pRect);
}

bool CChainsawGui::Hot(const void *pId) const
{
	return m_Interactive && Ui()->HotItem() == pId;
}

void CChainsawGui::Tip(const void *pId, const CUIRect &R, const char *pTitle, const char *pText, const char *pCommand, bool New)
{
	if(!Hovered(&R))
		return;
	m_pTipCandidate = pId;
	m_pCandTitle = pTitle;
	m_pCandText = pText;
	m_pCandCommand = pCommand;
	m_CandNew = New;
}

// ---------------------------------------------------------------- backgrounds

int CChainsawGui::BackgroundListCallback(const char *pName, int IsDir, int StorageType, void *pUser)
{
	CChainsawGui *pSelf = static_cast<CChainsawGui *>(pUser);
	if(IsDir || pName[0] == '.')
		return 0;
	const char *apExt[] = {".png", ".jpg", ".jpeg", ".webp", ".bmp", ".gif"};
	for(const char *pExt : apExt)
		if(str_endswith_nocase(pName, pExt))
		{
			if(std::find(pSelf->m_vBgFiles.begin(), pSelf->m_vBgFiles.end(), pName) == pSelf->m_vBgFiles.end())
				pSelf->m_vBgFiles.emplace_back(pName);
			break;
		}
	return 0;
}

void CChainsawGui::ScanBackgrounds()
{
	m_vBgFiles.clear();
	Storage()->ListDirectory(IStorage::TYPE_ALL, BG_FOLDER, BackgroundListCallback, this);
	std::sort(m_vBgFiles.begin(), m_vBgFiles.end());
	m_vpBgFileNames.clear();
	for(const std::string &File : m_vBgFiles)
		m_vpBgFileNames.push_back(File.c_str());
	m_BgScanned = true;
}

void CChainsawGui::MakeThumbnails()
{
	m_ThumbsMade = true;
	for(int p = 1; p < CcMenuBg::NUM_PRESETS && p < 16; p++)
	{
		std::vector<uint8_t> vPixels;
		const int W = 192, H = 108;
		CcMenuBg::Paint(p, W, H, vPixels);
		CImageInfo Image;
		Image.m_Width = W;
		Image.m_Height = H;
		Image.m_Format = CImageInfo::FORMAT_RGBA;
		Image.Allocate();
		mem_copy(Image.m_pData, vPixels.data(), vPixels.size());
		m_aThumbs[p] = Graphics()->LoadTextureRawMove(Image, 0, "cc_menu_thumb");
	}
}

void CChainsawGui::UpdateBackground()
{
	const bool Want = !Light() && (g_Config.m_BcGuiBgFile[0] != '\0' || g_Config.m_BcGuiBgPreset > 0);
	if(!Want)
	{
		if(m_BgTexture.IsValid())
			Graphics()->UnloadTexture(&m_BgTexture);
		m_BgKey.clear();
		return;
	}
	// wait until the blur slider is let go, a new picture takes a moment
	if(m_Interactive && Ui()->ActiveItem() != nullptr && Ui()->MouseButton(0))
		return;
	char aKey[256];
	str_format(aKey, sizeof(aKey), "%s|%d|%d", g_Config.m_BcGuiBgFile, g_Config.m_BcGuiBgPreset, g_Config.m_BcGuiBgBlur);
	if(m_BgKey == aKey)
		return;
	m_BgKey = aKey;

	std::vector<uint8_t> vPixels;
	int W = 0, H = 0;
	bool Ok = false;
	if(g_Config.m_BcGuiBgFile[0] != '\0')
	{
		char aPath[IO_MAX_PATH_LENGTH];
		str_format(aPath, sizeof(aPath), "%s/%s", BG_FOLDER, g_Config.m_BcGuiBgFile);
		void *pData = nullptr;
		unsigned DataSize = 0;
		if(Storage()->ReadFile(aPath, IStorage::TYPE_ALL, &pData, &DataSize))
		{
			CImageInfo Image;
			if(MediaDecoder::DecodeImageToRgba(Graphics(), static_cast<unsigned char *>(pData), DataSize, aPath, Image) && Image.m_Width > 0 && Image.m_Height > 0)
			{
				W = (int)Image.m_Width;
				H = (int)Image.m_Height;
				vPixels.resize((size_t)W * H * 4);
				if(Image.m_Format == CImageInfo::FORMAT_RGBA)
					mem_copy(vPixels.data(), Image.m_pData, vPixels.size());
				else
					for(size_t i = 0; i < (size_t)W * H; i++)
					{
						const ColorRGBA c = Image.PixelColor(i % W, i / W);
						vPixels[i * 4 + 0] = (uint8_t)round_to_int(c.r * 255.0f);
						vPixels[i * 4 + 1] = (uint8_t)round_to_int(c.g * 255.0f);
						vPixels[i * 4 + 2] = (uint8_t)round_to_int(c.b * 255.0f);
						vPixels[i * 4 + 3] = 255;
					}
				CcMenuBg::Fit(vPixels, W, H, 1280, 720);
				Ok = true;
			}
			free(pData);
		}
	}
	if(!Ok)
	{
		if(g_Config.m_BcGuiBgPreset <= 0)
		{
			if(m_BgTexture.IsValid())
				Graphics()->UnloadTexture(&m_BgTexture);
			return;
		}
		W = 960;
		H = 540;
		CcMenuBg::Paint(std::clamp(g_Config.m_BcGuiBgPreset, 1, CcMenuBg::NUM_PRESETS - 1), W, H, vPixels);
	}
	CcMenuBg::Blur(vPixels, W, H, (int)std::round(g_Config.m_BcGuiBgBlur / 100.0f * 12.0f * W / 960.0f));
	CImageInfo Image;
	Image.m_Width = W;
	Image.m_Height = H;
	Image.m_Format = CImageInfo::FORMAT_RGBA;
	Image.Allocate();
	mem_copy(Image.m_pData, vPixels.data(), vPixels.size());
	if(m_BgTexture.IsValid())
		Graphics()->UnloadTexture(&m_BgTexture);
	m_BgTexture = Graphics()->LoadTextureRawMove(Image, 0, "cc_menu_bg");
	m_BgW = W;
	m_BgH = H;
}

void CChainsawGui::RenderBackgroundImage(const CUIRect &Win, float Round, float Alpha)
{
	// the picture covers the window (cropped to its shape) and follows the rounded corners with thin strips
	Flush();
	const float Aspect = Win.w / Win.h, ImgAspect = m_BgW / (float)std::max(1, m_BgH);
	float u0 = 0.0f, u1 = 1.0f, v0 = 0.0f, v1 = 1.0f;
	if(ImgAspect > Aspect)
	{
		const float Part = Aspect / ImgAspect;
		u0 = (1.0f - Part) * 0.5f;
		u1 = u0 + Part;
	}
	else
	{
		const float Part = ImgAspect / Aspect;
		v0 = (1.0f - Part) * 0.5f;
		v1 = v0 + Part;
	}
	Graphics()->TextureSet(m_BgTexture);
	Graphics()->QuadsBegin();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha * m_Alpha);
	const auto Strip = [&](float y, float h, float Inset) {
		const float x = Win.x + Inset, w = Win.w - Inset * 2.0f;
		if(w <= 0.0f || h <= 0.0f)
			return;
		const float su0 = u0 + (u1 - u0) * (x - Win.x) / Win.w, su1 = u0 + (u1 - u0) * (x + w - Win.x) / Win.w;
		const float sv0 = v0 + (v1 - v0) * (y - Win.y) / Win.h, sv1 = v0 + (v1 - v0) * (y + h - Win.y) / Win.h;
		Graphics()->QuadsSetSubset(su0, sv0, su1, sv1);
		IGraphics::CQuadItem Item(x, y, w, h);
		Graphics()->QuadsDrawTL(&Item, 1);
	};
	constexpr int STRIPS = 6;
	const float Step = Round / STRIPS;
	for(int i = 0; i < STRIPS; i++)
	{
		const float Mid = Round - (i + 0.5f) * Step;
		const float Inset = Round - std::sqrt(std::max(0.0f, Round * Round - Mid * Mid));
		Strip(Win.y + i * Step, Step, Inset);
		Strip(Win.y + Win.h - (i + 1) * Step, Step, Inset);
	}
	Strip(Win.y + Round, Win.h - Round * 2.0f, 0.0f);
	Graphics()->QuadsEnd();
	Graphics()->QuadsSetSubset(0.0f, 0.0f, 1.0f, 1.0f);
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}

// ---------------------------------------------------------------- window

void CChainsawGui::OnRender()
{
	CcPerf::Update(Console(), Storage()); // optimization mode: applies / gives back the settings when it is switched
	if(!m_Active && !m_Closing)
		return;
	if(m_Active && !CanOpen())
	{
		SetActive(false);
		m_Closing = false;
		return;
	}
	if(!m_Built)
		BuildItems();

	m_Frame++;
	m_Dt = std::clamp(Client()->RenderFrameTime(), 0.0f, 0.1f);
	const bool Animate = g_Config.m_BcGuiAnim && !Light();
	if(m_Active)
		m_Anim = Animate ? std::min(1.0f, m_Anim + m_Dt / 0.2f) : 1.0f;
	else
	{
		m_Anim = Animate ? std::max(0.0f, m_Anim - m_Dt / 0.14f) : 0.0f;
		if(m_Anim <= 0.0f)
		{
			m_Closing = false;
			return;
		}
	}
	m_ListTime += m_Dt;
	m_Alpha = Ease(m_Anim);
	m_Interactive = m_Active;

	// forget texts nobody drew for a while
	if(m_Frame % 300 == 0)
		for(auto It = m_TextCache.begin(); It != m_TextCache.end();)
		{
			if(m_Frame - It->second.m_LastFrame > 600)
			{
				if(It->second.m_Index.Valid())
					TextRender()->DeleteTextContainer(It->second.m_Index);
				It = m_TextCache.erase(It);
			}
			else
				++It;
		}

	if(m_Interactive)
	{
		Ui()->StartCheck();
		Ui()->Update();
	}
	const CUIRect Screen = *Ui()->Screen();
	Ui()->MapScreen();
	ApplyMenuFont();
	if(!m_BgScanned)
		ScanBackgrounds();
	UpdateBackground();

	m_pTipCandidate = nullptr;
	RenderWindow(Screen);
	Flush();

	if(m_Interactive)
	{
		Ui()->RenderPopupMenus();
		const float Now = Client()->LocalTime();
		if(m_pTipCandidate != m_pTipId || Ui()->MouseButton(0) || Ui()->MouseButton(1) || Ui()->IsPopupOpen())
		{
			m_pTipId = Ui()->IsPopupOpen() ? nullptr : m_pTipCandidate;
			m_TipTime = Now;
			m_pTipTitle = m_pCandTitle;
			m_pTipText = m_pCandText;
			m_pTipCommand = m_pCandCommand;
			m_TipNew = m_CandNew;
		}
		RenderTooltip(Screen);
		Flush();
		RenderTools()->RenderCursor(Ui()->MousePos(), 24.0f);
		Ui()->FinishCheck();
		Ui()->ClearHotkeys();
	}
	RestoreFont();
}

void CChainsawGui::RenderWindow(const CUIRect &Screen)
{
	m_K = 0.62f * g_Config.m_BcGuiScale / 100.0f;
	const float K = m_K;
	const float W = std::min(800.0f * K, Screen.w - 16.0f);
	const float H = std::min(540.0f * K, Screen.h - 16.0f);

	if(m_LastScreenW != Screen.w || m_LastScreenH != Screen.h)
	{
		m_WinPos.x = g_Config.m_BcGuiX >= 0 ? Screen.w * g_Config.m_BcGuiX / 1000.0f : (Screen.w - W) * 0.5f;
		m_WinPos.y = g_Config.m_BcGuiY >= 0 ? Screen.h * g_Config.m_BcGuiY / 1000.0f : (Screen.h - H) * 0.5f;
		m_LastScreenW = Screen.w;
		m_LastScreenH = Screen.h;
	}

	// dragging by the title bar
	if(m_Interactive)
	{
		CUIRect BarHit = {m_WinPos.x, m_WinPos.y, W, 52.0f * K};
		if(Ui()->CheckActiveItem(&m_DragId))
		{
			if(Ui()->MouseButton(0))
				m_WinPos = Ui()->MousePos() - m_DragOffset;
			else
			{
				Ui()->SetActiveItem(nullptr);
				g_Config.m_BcGuiX = std::clamp((int)std::round(m_WinPos.x / Screen.w * 1000.0f), 0, 1000);
				g_Config.m_BcGuiY = std::clamp((int)std::round(m_WinPos.y / Screen.h * 1000.0f), 0, 1000);
			}
		}
		else if(Ui()->HotItem() == &m_DragId && Ui()->MouseButtonClicked(0))
		{
			Ui()->SetActiveItem(&m_DragId);
			m_DragOffset = Ui()->MousePos() - m_WinPos;
		}
		if(Ui()->MouseHovered(&BarHit))
			Ui()->SetHotItem(&m_DragId);
	}
	m_WinPos.x = std::clamp(m_WinPos.x, 60.0f - W, Screen.w - 60.0f);
	m_WinPos.y = std::clamp(m_WinPos.y, 0.0f, Screen.h - 40.0f * K);

	// the way it comes in (clip areas follow the real layout, so it moves instead of scaling)
	vec2 Offset(0.0f, 0.0f);
	const float Out = 1.0f - Ease(m_Anim);
	switch(g_Config.m_BcGuiAnimStyle)
	{
	case 0: Offset.y = Out * 14.0f * K; break;
	case 1: Offset.y = -Out * 60.0f * K; break;
	case 2: Offset.x = Out * 80.0f * K; break;
	default: break;
	}
	m_Win = {m_WinPos.x + Offset.x, m_WinPos.y + Offset.y, W, H};
	const float Round = 14.0f * K;
	const float Opacity = g_Config.m_BcGuiOpacity / 100.0f;
	const ColorRGBA A1 = Cfg(g_Config.m_BcGuiColor1), A2 = Cfg(g_Config.m_BcGuiColor2);
	const ColorRGBA Bg1 = Cfg(g_Config.m_BcGuiBg1), Bg2 = Cfg(g_Config.m_BcGuiBg2);

	if(!Light())
	{
		for(int i = 3; i >= 0; i--)
		{
			CUIRect Shadow = m_Win;
			const float Grow = (i + 1) * 6.0f * K;
			Shadow.x -= Grow;
			Shadow.y -= Grow - 8.0f * K;
			Shadow.w += Grow * 2.0f;
			Shadow.h += Grow * 2.0f;
			Rect(Shadow, ColorRGBA(0.0f, 0.0f, 0.0f, 0.09f * Opacity), Round + Grow);
		}
	}
	// the body: a picture (if chosen) under a diagonal gradient of the two background colors, the accent glows from the top left
	const ColorRGBA Mid = Mix(Bg1, Bg2, 0.5f);
	float BodyAlpha = Opacity;
	if(m_BgTexture.IsValid() && !Light())
	{
		RenderBackgroundImage(m_Win, Round, Opacity);
		BodyAlpha = Opacity * g_Config.m_BcGuiBgDim / 100.0f;
	}
	Rect4(m_Win, Bg1.WithAlpha(BodyAlpha), Mid.WithAlpha(BodyAlpha), Mid.WithAlpha(BodyAlpha), Bg2.WithAlpha(BodyAlpha), Round);
	Rect4(m_Win, A1.WithAlpha(0.17f * Opacity), A1.WithAlpha(0.0f), A1.WithAlpha(0.0f), A2.WithAlpha(0.07f * Opacity), Round);
	{
		const float Px = std::max(0.6f, 1.0f * K);
		Rect({m_Win.x + Round, m_Win.y, m_Win.w - Round * 2.0f, Px}, LINE2, 0.0f);
		Rect({m_Win.x + Round, m_Win.y + m_Win.h - Px, m_Win.w - Round * 2.0f, Px}, LINE, 0.0f);
		Rect({m_Win.x, m_Win.y + Round, Px, m_Win.h - Round * 2.0f}, LINE, 0.0f);
		Rect({m_Win.x + m_Win.w - Px, m_Win.y + Round, Px, m_Win.h - Round * 2.0f}, LINE, 0.0f);
	}

	CUIRect Win = m_Win, Bar, Body, Footer, Rail, List;
	Win.HSplitTop(52.0f * K, &Bar, &Body);
	Body.HSplitBottom(32.0f * K, &Body, &Footer);
	Body.VSplitLeft(178.0f * K, &Rail, &List);

	RenderBar(Bar);
	RenderRail(Rail);
	RenderFooter(Footer);
	RenderList(List);
}

void CChainsawGui::RenderBar(CUIRect Bar)
{
	const float K = m_K;
	const ColorRGBA A1 = Cfg(g_Config.m_BcGuiColor1), A2 = Cfg(g_Config.m_BcGuiColor2);

	// the burning line under the title
	{
		CUIRect LineRect = {Bar.x, Bar.y + Bar.h - 2.0f * K, Bar.w * 0.45f, 2.0f * K};
		RectH(LineRect, A1, A2, 0.0f);
		LineRect.x += LineRect.w;
		LineRect.w = Bar.w * 0.4f;
		RectH(LineRect, A2, A2.WithAlpha(0.0f), 0.0f);
	}

	CUIRect Content;
	Bar.VMargin(14.0f * K, &Content);
	Content.HMargin(12.0f * K, &Content);

	// chainsaw logo, then the name: one text with the gradient baked into its letters, always in its own font,
	// so it looks the same whatever font the menu uses
	{
		CUIRect Logo = {Content.x, Content.y + (Content.h - 20.0f * K) * 0.5f, 42.0f * K, 20.0f * K};
		DrawLogo(Logo);
		const float Size = 16.0f * K, SubSize = 9.0f * K;
		char aKey[128];
		str_format(aKey, sizeof(aKey), "%u|%u|%d|%d", g_Config.m_BcGuiColor1, g_Config.m_BcGuiColor2, round_to_int(Size * 100.0f), m_TextGeneration);
		if(m_LogoKey != aKey)
		{
			ClearLogo();
			m_LogoKey = aKey;
			TextRender()->SetCustomFace(LOGO_FONT);
			const ColorRGBA OldColor = TextRender()->GetTextColor();
			TextRender()->TextColor(TextRender()->DefaultTextColor());
			const char *pName = "CHAINSAW";
			CTextCursor Cursor;
			Cursor.m_FontSize = Size;
			Cursor.m_Flags = TEXTFLAG_RENDER;
			const int Len = str_length(pName);
			for(int i = 0; i < Len; i++)
				Cursor.m_vColorSplits.emplace_back(i, 1, Mix(A1, A2, i / (float)(Len - 1)));
			TextRender()->CreateTextContainer(m_LogoName, &Cursor, pName);
			const STextBoundingBox NameBox = Cursor.BoundingBox();
			m_LogoNameW = NameBox.m_W;
			m_LogoNameH = NameBox.m_H;
			m_LogoNameMaxH = Cursor.m_MaxCharacterHeight;
			CTextCursor SubCursor;
			SubCursor.m_FontSize = SubSize;
			SubCursor.m_Flags = TEXTFLAG_RENDER;
			TextRender()->CreateTextContainer(m_LogoSub, &SubCursor, "CLIENT");
			const STextBoundingBox SubBox = SubCursor.BoundingBox();
			m_LogoSubW = SubBox.m_W;
			m_LogoSubH = SubBox.m_H;
			TextRender()->TextColor(OldColor);
			// back to the font of the menu
			if(g_Config.m_BcGuiFont[0] != '\0')
				TextRender()->SetCustomFace(g_Config.m_BcGuiFont);
			else
				BestClientApplyMenuFont(TextRender());
		}
		const float x0 = Logo.x + Logo.w + 9.0f * K;
		const float y = Content.y + (Content.h - m_LogoNameMaxH) * 0.5f - (m_LogoNameH - m_LogoNameMaxH);
		const ColorRGBA White = Fade(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
		if(m_LogoName.Valid())
			m_vTexts.push_back({m_LogoName, x0, y, White, ColorRGBA(0.0f, 0.0f, 0.0f, 0.2f * White.a)});
		const float x = x0 + m_LogoNameW + 7.0f * K;
		if(m_LogoSub.Valid())
		{
			const ColorRGBA Sub = Fade(MUTED);
			m_vTexts.push_back({m_LogoSub, x, y + (m_LogoNameH - m_LogoSubH) - (m_LogoNameH - m_LogoNameMaxH) * 0.15f, Sub, ColorRGBA(0.0f, 0.0f, 0.0f, 0.18f * Sub.a)});
		}
		Content.VSplitLeft(x + m_LogoSubW - Content.x + 14.0f * K, nullptr, &Content);
	}

	// close button
	CUIRect Close;
	Content.VSplitRight(28.0f * K, &Content, &Close);
	Close.HMargin((Close.h - 28.0f * K) * 0.5f, &Close);
	{
		const bool IsHot = Hot(&m_CloseId);
		if(IsHot)
			Rect(Close, ColorRGBA(1.0f, 1.0f, 1.0f, 0.07f), 7.0f * K);
		IconLabel(Close, FontIcon::XMARK, F(11.0f), IsHot ? INK : MUTED);
		if(Button(&m_CloseId, &Close))
			SetActive(false);
	}

	// search on the right side of the bar
	CUIRect Search;
	Content.VSplitRight(10.0f * K, &Content, nullptr);
	const float SearchW = std::min(280.0f * K, Content.w);
	Content.VSplitRight(SearchW, &Content, &Search);
	Search.HMargin((Search.h - 26.0f * K) * 0.5f, &Search);
	{
		CUIRect Icon;
		Search.VSplitLeft(20.0f * K, &Icon, &Search);
		IconLabel(Icon, FontIcon::MAGNIFYING_GLASS, F(9.5f), DIM);
		if(m_Interactive)
		{
			Flush();
			const bool HadText = m_SearchInput.GetLength() > 0;
			Ui()->DoClearableEditBox(&m_SearchInput, &Search, F(10.5f));
			if(HadText != (m_SearchInput.GetLength() > 0))
				m_ScrollRegion.Reset();
		}
		else
		{
			Rect(Search, ColorRGBA(1.0f, 1.0f, 1.0f, 0.06f), 3.0f * K);
			CUIRect Inner;
			Search.VMargin(4.0f * K, &Inner);
			Label(Inner, m_SearchInput.GetLength() > 0 ? m_SearchInput.GetString() : "Найти функцию…", F(10.5f), TEXTALIGN_ML, m_SearchInput.GetLength() > 0 ? INK : DIM);
		}
	}
}

int CChainsawGui::EnabledCount(const SCategory &Category) const
{
	int Num = 0;
	for(const SFeature &Feature : Category.m_vFeatures)
		Num += Feature.m_pEnable && *Feature.m_pEnable ? 1 : 0;
	return Num;
}

int CChainsawGui::TotalCount(const SCategory &Category) const
{
	int Num = 0;
	for(const SFeature &Feature : Category.m_vFeatures)
		Num += Feature.m_pEnable ? 1 : 0;
	return Num;
}

void CChainsawGui::RenderRail(CUIRect Rail)
{
	const float K = m_K;
	const ColorRGBA A1 = Cfg(g_Config.m_BcGuiColor1), A2 = Cfg(g_Config.m_BcGuiColor2);
	Rect({Rail.x + Rail.w - std::max(0.6f, K), Rail.y, std::max(0.6f, K), Rail.h}, LINE, 0.0f);

	CUIRect View;
	Rail.Margin(8.0f * K, &View);
	View.HSplitTop(4.0f * K, nullptr, &View);
	const bool Searching = m_SearchInput.GetLength() > 0;
	const int Current = std::clamp(g_Config.m_BcGuiTab, 0, (int)m_vCategories.size() - 1);
	const float Speed = (g_Config.m_BcGuiAnim && !Light()) ? m_Dt : 1.0f;
	for(int c = 0; c < (int)m_vCategories.size(); c++)
	{
		SCategory &Cat = m_vCategories[c];
		if(c == (int)m_vCategories.size() - 1)
		{
			CUIRect Sep;
			View.HSplitTop(8.0f * K, nullptr, &View);
			View.HSplitTop(std::max(0.6f, K), &Sep, &View);
			Sep.VMargin(6.0f * K, &Sep);
			Rect(Sep, LINE, 0.0f);
			View.HSplitTop(8.0f * K, nullptr, &View);
		}
		CUIRect ButtonRect;
		View.HSplitTop(34.0f * K, &ButtonRect, &View);
		View.HSplitTop(2.0f * K, nullptr, &View);

		const bool Active = c == Current && !Searching;
		if(Button(&Cat.m_Id, &ButtonRect))
		{
			if(g_Config.m_BcGuiTab != c || Searching)
			{
				m_ScrollRegion.Reset();
				m_ListTime = 0.0f;
				PlaySound(SND_CLICK);
			}
			g_Config.m_BcGuiTab = c;
			m_SearchInput.Clear();
		}
		const bool IsHot = Hot(&Cat.m_Id);
		if(IsHot && m_pLastHoverRow != &Cat.m_Id && g_Config.m_BcGuiSoundHover)
			PlaySound(SND_HOVER);
		if(IsHot)
			m_pLastHoverRow = &Cat.m_Id;
		Cat.m_Anim = Approach(Cat.m_Anim, Active ? 1.0f : (IsHot ? 0.4f : 0.0f), Speed * 8.0f);

		if(Cat.m_Anim > 0.0f)
		{
			if(Active || Cat.m_Anim > 0.4f)
				RectH(ButtonRect, A1.WithAlpha(0.24f * Cat.m_Anim), A2.WithAlpha(0.0f), 8.0f * K);
			else
				Rect(ButtonRect, ColorRGBA(1.0f, 1.0f, 1.0f, 0.04f * Cat.m_Anim / 0.4f), 8.0f * K);
		}
		CUIRect Icon, Name, Count;
		ButtonRect.VSplitLeft(10.0f * K, nullptr, &ButtonRect);
		ButtonRect.VSplitLeft(18.0f * K, &Icon, &ButtonRect);
		Icon.HMargin((Icon.h - 18.0f * K) * 0.5f, &Icon);
		DrawIcon(Cat.m_Icon, Icon, Active ? A1 : Mix(MUTED, INK, IsHot ? 0.6f : 0.0f), Active ? A2 : Mix(DIM, MUTED, IsHot ? 0.6f : 0.0f));
		ButtonRect.VSplitLeft(10.0f * K, nullptr, &ButtonRect);
		ButtonRect.VSplitRight(8.0f * K, &ButtonRect, nullptr);
		ButtonRect.VSplitRight(28.0f * K, &Name, &Count);
		Label(Name, Cat.m_pName, F(12.0f), TEXTALIGN_ML, Active || IsHot ? INK : MUTED);
		if(TotalCount(Cat) > 0)
		{
			char aBuf[16];
			str_format(aBuf, sizeof(aBuf), "%d/%d", EnabledCount(Cat), TotalCount(Cat));
			Label(Count, aBuf, F(9.0f), TEXTALIGN_MR, Active ? A2 : DIM);
		}
	}
}

bool CChainsawGui::Matches(const SFeature &Feature, const char *pQuery) const
{
	if(str_utf8_find_nocase(Feature.m_pName, pQuery) || str_utf8_find_nocase(Feature.m_pDesc, pQuery))
		return true;
	if(Feature.m_pCommand && str_utf8_find_nocase(Feature.m_pCommand, pQuery))
		return true;
	for(const SOpt &Opt : Feature.m_vOpts)
	{
		if(Opt.m_pLabel && str_utf8_find_nocase(Opt.m_pLabel, pQuery))
			return true;
		if(Opt.m_pCommand && str_utf8_find_nocase(Opt.m_pCommand, pQuery))
			return true;
	}
	return false;
}

void CChainsawGui::RenderList(CUIRect List)
{
	const float K = m_K;
	CUIRect View;
	List.VMargin(16.0f * K, &View);
	View.HSplitTop(14.0f * K, nullptr, &View);
	View.HSplitBottom(10.0f * K, &View, nullptr);

	const char *pQuery = m_SearchInput.GetString();
	const bool Searching = pQuery[0] != '\0';
	const int Current = std::clamp(g_Config.m_BcGuiTab, 0, (int)m_vCategories.size() - 1);
	if(Current != m_LastCategory)
	{
		m_ScrollRegion.Reset();
		m_LastCategory = Current;
	}

	std::vector<SFeature *> vpShown;
	if(Searching)
	{
		for(SCategory &Cat : m_vCategories)
			for(SFeature &Feature : Cat.m_vFeatures)
				if(Matches(Feature, pQuery))
					vpShown.push_back(&Feature);
	}
	else
		for(SFeature &Feature : m_vCategories[Current].m_vFeatures)
			vpShown.push_back(&Feature);

	CUIRect Header;
	View.HSplitTop(22.0f * K, &Header, &View);
	View.HSplitTop(6.0f * K, nullptr, &View);
	{
		CUIRect Title, Hint;
		Header.VSplitMid(&Title, &Hint);
		Label(Title, Searching ? "Поиск" : m_vCategories[Current].m_pName, F(13.5f), TEXTALIGN_ML, INK);
		char aBuf[64] = "";
		if(Searching)
		{
			if(!vpShown.empty())
				str_format(aBuf, sizeof(aBuf), "найдено: %d", (int)vpShown.size());
		}
		else if(TotalCount(m_vCategories[Current]) > 0)
			str_copy(aBuf, "ПКМ по функции — настройки");
		Label(Hint, aBuf, F(9.5f), TEXTALIGN_MR, DIM);
	}

	if(Searching && vpShown.empty())
	{
		char aBuf[160];
		str_format(aBuf, sizeof(aBuf), "Ничего не нашлось по «%s». Попробуй: огонь, цвет, свечение.", pQuery);
		CUIRect Empty;
		View.HSplitTop(30.0f * K, &Empty, &View);
		Label(Empty, aBuf, F(11.0f), TEXTALIGN_ML, MUTED);
		return;
	}

	CScrollRegionParams Params;
	Params.m_ScrollUnit = 60.0f * K;
	Params.m_ScrollbarThickness = 5.0f * K;
	Params.m_ScrollbarMargin = 1.0f * K;
	Params.m_SliderMinSize = 20.0f * K;
	Params.m_RailBgColor = Fade(ColorRGBA(1.0f, 1.0f, 1.0f, 0.04f));
	Params.m_SliderColor = Fade(ColorRGBA(1.0f, 1.0f, 1.0f, 0.16f));
	Params.m_SliderColorHover = Fade(ColorRGBA(1.0f, 1.0f, 1.0f, 0.26f));
	Params.m_SliderColorGrabbed = Fade(ColorRGBA(1.0f, 1.0f, 1.0f, 0.34f));
	Flush();
	m_ScrollRegion.Begin(&View, &Params);
	View.VSplitRight(4.0f * K, &View, nullptr);

	const bool Stagger = g_Config.m_BcGuiAnim && !Light();
	const float BaseAlpha = m_Alpha;
	int Index = 0;
	for(SFeature *pFeature : vpShown)
	{
		const float Height = FeatureHeight(*pFeature, View.w);
		CUIRect Card;
		View.HSplitTop(Height, &Card, &View);
		if(m_ScrollRegion.AddRect(Card))
		{
			// cards come in one after another
			const float In = Stagger ? Ease((m_ListTime - Index * 0.035f) / 0.2f) : 1.0f;
			m_Alpha = BaseAlpha * In;
			Card.x += (1.0f - In) * 18.0f * K;
			RenderFeature(*pFeature, Card, Searching);
			m_Alpha = BaseAlpha;
			Index++;
		}
		else
		{
			pFeature->m_OpenAnim = (pFeature->m_Open || !pFeature->m_pEnable) ? 1.0f : 0.0f;
			pFeature->m_SwitchAnim = pFeature->m_pEnable && *pFeature->m_pEnable ? 1.0f : 0.0f;
		}
		CUIRect Gap;
		View.HSplitTop(6.0f * K, &Gap, &View);
		m_ScrollRegion.AddRect(Gap);
	}
	Flush();
	m_ScrollRegion.End();
}

void CChainsawGui::RenderFooter(CUIRect Footer)
{
	const float K = m_K;
	Rect({Footer.x, Footer.y, Footer.w, std::max(0.6f, K)}, LINE, 0.0f);
	CUIRect View;
	Footer.VMargin(16.0f * K, &View);

	int On = 0, All = 0;
	for(const SCategory &Cat : m_vCategories)
	{
		On += EnabledCount(Cat);
		All += TotalCount(Cat);
	}
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "Включено %d из %d", On, All);
	const float Size = F(9.5f);
	Label(View, aBuf, Size, TEXTALIGN_ML, MUTED);

	const char *pHelp = "ЛКМ — вкл/выкл · ПКМ — настройки · наведи — подсказка";
	const float HelpW = TextW(pHelp, Size);
	const char *pNew = "новая настройка";
	const float NewW = TextW(pNew, Size) + 16.0f * K;
	const float CountW = TextW(aBuf, Size);
	if(CountW + HelpW + 30.0f * K < View.w)
		Label(View, pHelp, Size, TEXTALIGN_MR, DIM);
	if(CountW + HelpW + NewW + 60.0f * K < View.w)
	{
		CUIRect MidRect = {View.x + CountW + (View.w - CountW - HelpW - NewW) * 0.5f, View.y, NewW, View.h};
		DrawNewBadge(&MidRect, Size);
		MidRect.x += 4.0f * K;
		Label(MidRect, pNew, Size, TEXTALIGN_ML, DIM);
	}
}

// ---------------------------------------------------------------- functions and their settings

static constexpr float HEAD_H = 40.0f;
static constexpr float ROW_H = 30.0f;

float CChainsawGui::WrapHeight(const std::vector<const char *> &vItems, float Size, float Width, int *pLines)
{
	const float K = m_K;
	int Lines = vItems.empty() ? 0 : 1;
	float x = 0.0f;
	for(const char *pItem : vItems)
	{
		const float w = TextW(pItem, Size) + 18.0f * K;
		if(x > 0.0f && x + w > Width)
		{
			Lines++;
			x = 0.0f;
		}
		x += w + 4.0f * K;
	}
	if(pLines)
		*pLines = Lines;
	return Lines * 26.0f * K;
}

float CChainsawGui::OptHeight(SOpt &Opt, float Width)
{
	const float K = m_K;
	switch(Opt.m_Type)
	{
	case OPT_GROUP: return 28.0f * K;
	case OPT_NOTE: return 22.0f * K;
	case OPT_THEMES: return 34.0f * K;
	case OPT_FONT: return ROW_H * K;
	case OPT_RENDERER: return 34.0f * K;
	case OPT_PERF_STATUS: return 40.0f * K;
	case OPT_PRESETS: return ROW_H * K + WrapHeight(s_vLookNames, F(10.5f), Width - 8.0f * K, nullptr);
	case OPT_BG_PRESETS:
	{
		const float ChipW = 64.0f * K, Gap = 6.0f * K;
		const int PerLine = std::max(1, (int)((Width - 8.0f * K + Gap) / (ChipW + Gap)));
		const int Lines = (CcMenuBg::NUM_PRESETS + PerLine - 1) / PerLine;
		return 22.0f * K + Lines * (36.0f * K + Gap) + 4.0f * K;
	}
	case OPT_BG_FILES:
		if(m_vpBgFileNames.empty())
			return 22.0f * K + 22.0f * K;
		return 22.0f * K + WrapHeight(m_vpBgFileNames, F(10.0f), Width - 8.0f * K, nullptr) + 4.0f * K;
	case OPT_CHOICE:
	{
		const float Size = F(10.5f);
		float Total = 0.0f;
		for(const char *pChoice : Opt.m_vChoices)
			Total += TextW(pChoice, Size) + 22.0f * K;
		const float Right = Width - TextW(Opt.m_pLabel, F(11.5f)) - 24.0f * K;
		if(Total <= Right)
			return ROW_H * K;
		return ROW_H * K + WrapHeight(Opt.m_vChoices, Size, Width - 8.0f * K, nullptr);
	}
	default: return ROW_H * K;
	}
}

float CChainsawGui::OptsHeight(SFeature &Feature, float Width)
{
	const float K = m_K;
	const float Inner = Width - 28.0f * K;
	float Height = 2.0f * K + 12.0f * K;
	bool Any = false;
	for(SOpt &Opt : Feature.m_vOpts)
	{
		if(!Opt.Visible())
			continue;
		Height += OptHeight(Opt, Inner) + 2.0f * K;
		Any = true;
	}
	if(!Any)
		Height += 22.0f * K;
	Height += 40.0f * K; // reset + command line
	return Height;
}

float CChainsawGui::FeatureHeight(SFeature &Feature, float Width)
{
	const float K = m_K;
	const float Open = Feature.m_pEnable ? Ease(Feature.m_OpenAnim) : 1.0f;
	return HEAD_H * K + (Open > 0.0f ? OptsHeight(Feature, Width) * Open : 0.0f);
}

void CChainsawGui::FormatValue(const SOpt &Opt, int Value, char *pBuf, int BufSize) const
{
	if(Opt.m_Format == FMT_TENTHS)
		str_format(pBuf, BufSize, "%d,%d с", Value / 10, Value % 10);
	else
		str_format(pBuf, BufSize, "%d%s", Value, Opt.m_pSuffix);
}

void CChainsawGui::ResetFeature(SFeature &Feature)
{
	for(SOpt &Opt : Feature.m_vOpts)
	{
		if(Opt.m_pInt)
			*Opt.m_pInt = Opt.m_DefaultInt;
		if(Opt.m_pColor)
			*Opt.m_pColor = Opt.m_DefaultColor;
		if(Opt.m_pStr && Opt.m_pDefaultStr)
			str_copy(Opt.m_pStr, Opt.m_pDefaultStr, Opt.m_StrSize);
	}
	// "Вид меню" also puts the window back in the middle
	if(&Feature == &m_vCategories.back().m_vFeatures.front())
	{
		g_Config.m_BcGuiX = DefaultConfig::BcGuiX;
		g_Config.m_BcGuiY = DefaultConfig::BcGuiY;
		m_LastScreenW = -1.0f;
	}
}

void CChainsawGui::RunAction(int Action)
{
	const vec2 Pos = GameClient()->m_LocalCharacterPos;
	switch(Action)
	{
	case ACT_AXE_SUMMON: GameClient()->m_BcFireAxeFx.PreviewSummon(); break;
	case ACT_AXE_HIT: GameClient()->m_BcFireAxeFx.PreviewHit(); break;
	case ACT_SHOCKWAVE: GameClient()->m_BcFireIceFx.PreviewShockwave(Pos); break;
	case ACT_ICE: GameClient()->m_BcFireIceFx.PreviewIce(Pos); break;
	case ACT_STRIKE: GameClient()->m_BcFireChainFx.PreviewStrike(Pos); break;
	case ACT_OPEN_BG_FOLDER:
	{
		char aPath[IO_MAX_PATH_LENGTH];
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, BG_FOLDER, aPath, sizeof(aPath));
		Client()->ViewFile(aPath);
		break;
	}
	case ACT_RESCAN_BG:
		ScanBackgrounds();
		m_BgKey.clear();
		break;
	case ACT_TEST_LIGHTNING: GameClient()->m_CcAtmosphere.TestLightning(); break;
	default: break;
	}
}

void CChainsawGui::RenderFeature(SFeature &Feature, CUIRect Card, bool ShowCategory)
{
	const float K = m_K;
	const ColorRGBA A1 = Cfg(g_Config.m_BcGuiColor1);
	const bool Fixed = Feature.m_pEnable == nullptr;
	const bool On = Fixed || *Feature.m_pEnable;
	const bool HasOpts = !Feature.m_vOpts.empty();

	CUIRect Head;
	Card.HSplitTop(HEAD_H * K, &Head, nullptr);

	CUIRect Right = Head, More, Switch;
	Right.VSplitRight(12.0f * K, &Right, nullptr);
	if(!Fixed)
	{
		Right.VSplitRight(34.0f * K, &Right, &Switch);
		Switch.HMargin((Switch.h - 18.0f * K) * 0.5f, &Switch);
		Right.VSplitRight(8.0f * K, &Right, nullptr);
		if(HasOpts)
		{
			Right.VSplitRight(24.0f * K, &Right, &More);
			More.HMargin((More.h - 24.0f * K) * 0.5f, &More);
		}
	}
	if(!Fixed)
	{
		const int Click = Button(&Feature.m_RowId, &Head, BUTTONFLAG_LEFT | BUTTONFLAG_RIGHT);
		if(Click == 1)
		{
			*Feature.m_pEnable ^= 1;
			PlaySound(*Feature.m_pEnable ? SND_ON : SND_OFF);
		}
		else if(Click == 2 && HasOpts)
		{
			Feature.m_Open = !Feature.m_Open;
			PlaySound(SND_CLICK, 0.7f);
		}
		if(HasOpts && Button(&Feature.m_MoreId, &More))
		{
			Feature.m_Open = !Feature.m_Open;
			PlaySound(SND_CLICK, 0.7f);
		}
	}
	Tip(&Feature.m_RowId, Head, Feature.m_pName, Feature.m_pDesc, Feature.m_pCommand, false);

	const bool IsHot = Hovered(&Head) && !Fixed;
	if(IsHot && m_pLastHoverRow != &Feature.m_RowId && g_Config.m_BcGuiSoundHover)
		PlaySound(SND_HOVER);
	if(IsHot)
		m_pLastHoverRow = &Feature.m_RowId;
	const float Speed = (g_Config.m_BcGuiAnim && !Light()) ? m_Dt : 1.0f;
	Feature.m_HoverAnim = Approach(Feature.m_HoverAnim, IsHot ? 1.0f : 0.0f, Speed * 8.0f);
	Feature.m_SwitchAnim = Approach(Feature.m_SwitchAnim, On ? 1.0f : 0.0f, Speed * 7.0f);
	Feature.m_OpenAnim = Approach(Feature.m_OpenAnim, Fixed || Feature.m_Open ? 1.0f : 0.0f, Speed * 6.0f);

	const float Round = 10.0f * K;
	Rect(Card, ColorRGBA(1.0f, 1.0f, 1.0f, 0.035f + 0.015f * Ease(Feature.m_OpenAnim) * (Fixed ? 0.0f : 1.0f)), Round);
	if(!Fixed && Feature.m_SwitchAnim > 0.0f)
		RectH(Card, A1.WithAlpha(0.09f * Feature.m_SwitchAnim), A1.WithAlpha(0.02f * Feature.m_SwitchAnim), Round);
	if(Feature.m_HoverAnim > 0.0f)
		Rect(Head, ColorRGBA(1.0f, 1.0f, 1.0f, 0.035f * Feature.m_HoverAnim), Round, Feature.m_OpenAnim > 0.01f ? IGraphics::CORNER_T : IGraphics::CORNER_ALL);

	{
		CUIRect Name = Right;
		Name.VSplitLeft(12.0f * K, nullptr, &Name);
		const float Size = F(12.5f);
		Label(Name, Feature.m_pName, Size, TEXTALIGN_ML, On ? INK : Mix(MUTED, INK, Feature.m_HoverAnim * 0.5f));
		if(ShowCategory)
		{
			const char *pCat = m_vCategories[Feature.m_Category].m_pName;
			const float CatSize = F(9.0f);
			CUIRect Chip = {Name.x + TextW(Feature.m_pName, Size) + 8.0f * K, Name.y + (Name.h - 15.0f * K) * 0.5f, TextW(pCat, CatSize) + 12.0f * K, 15.0f * K};
			if(Chip.x + Chip.w < Name.x + Name.w)
			{
				Rect(Chip, ColorRGBA(1.0f, 1.0f, 1.0f, 0.06f), 4.0f * K);
				Label(Chip, pCat, CatSize, TEXTALIGN_MC, DIM);
			}
		}
	}
	if(!Fixed)
	{
		DrawSwitch(Switch, Ease(Feature.m_SwitchAnim));
		if(HasOpts)
		{
			const bool MoreHot = Hot(&Feature.m_MoreId);
			if(MoreHot)
				Rect(More, ColorRGBA(1.0f, 1.0f, 1.0f, 0.07f), 6.0f * K);
			IconLabel(More, Feature.m_OpenAnim > 0.5f ? FontIcon::CHEVRON_DOWN : FontIcon::CHEVRON_RIGHT, F(8.0f), MoreHot ? INK : DIM);
		}
	}

	const float Open = Fixed ? 1.0f : Ease(Feature.m_OpenAnim);
	if(Open <= 0.001f)
		return;
	const float Full = OptsHeight(Feature, Card.w);
	CUIRect Clip = {Card.x, Head.y + Head.h, Card.w, Full * Open};
	const bool NeedClip = Open < 0.999f;
	if(NeedClip)
		ClipEnable(&Clip);
	const CUIRect *pVisible = Ui()->IsClipped() ? Ui()->ClipArea() : nullptr;
	CUIRect View = {Card.x + 14.0f * K, Clip.y + 2.0f * K, Card.w - 28.0f * K, Full};
	bool Any = false;
	for(SOpt &Opt : Feature.m_vOpts)
	{
		if(!Opt.Visible())
			continue;
		Any = true;
		CUIRect Row;
		View.HSplitTop(OptHeight(Opt, View.w), &Row, &View);
		View.HSplitTop(2.0f * K, nullptr, &View);
		// rows scrolled out of the list or still folded away are not drawn
		const bool InView = !pVisible || (Row.y < pVisible->y + pVisible->h && Row.y + Row.h > pVisible->y);
		if(InView && (!NeedClip || Row.y < Clip.y + Clip.h))
			RenderOpt(Feature, Opt, Row);
	}
	if(!Any)
	{
		CUIRect Note;
		View.HSplitTop(22.0f * K, &Note, &View);
		Label(Note, "Дополнительных настроек нет.", F(11.0f), TEXTALIGN_ML, DIM);
	}
	{
		CUIRect Foot;
		View.HSplitTop(10.0f * K, nullptr, &View);
		View.HSplitTop(30.0f * K, &Foot, &View);
		Rect({Foot.x, Foot.y, Foot.w, std::max(0.6f, K)}, LINE, 0.0f);
		CUIRect Reset, Cmd;
		Foot.VSplitMid(&Reset, &Cmd);
		const char *pReset = "Сбросить к стандартным";
		Reset.w = TextW(pReset, F(10.5f)) + 4.0f * K;
		const bool ResetHot = Hot(&Feature.m_ResetId);
		Label(Reset, pReset, F(10.5f), TEXTALIGN_ML, ResetHot ? INK : MUTED);
		Rect({Reset.x, Reset.y + Reset.h * 0.5f + 6.5f * K, Reset.w - 4.0f * K, std::max(0.5f, 0.8f * K)}, ResetHot ? MUTED.WithAlpha(0.6f) : LINE2, 0.0f);
		if(Button(&Feature.m_ResetId, &Reset))
		{
			ResetFeature(Feature);
			PlaySound(SND_CLICK);
		}
		if(Feature.m_pCommand)
			Label(Cmd, Feature.m_pCommand, F(9.5f), TEXTALIGN_MR, DIM);
	}
	if(NeedClip)
		ClipDisable();
}

void CChainsawGui::RenderOpt(SFeature &Feature, SOpt &Opt, CUIRect Row)
{
	const float K = m_K;
	const ColorRGBA A1 = Cfg(g_Config.m_BcGuiColor1), A2 = Cfg(g_Config.m_BcGuiColor2);
	const float LabelSize = F(11.5f);
	const float ValueW = 58.0f * K;
	const bool RowHovered = Hovered(&Row);
	const float Speed = (g_Config.m_BcGuiAnim && !Light()) ? m_Dt : 1.0f;
	Opt.m_Anim = Approach(Opt.m_Anim, RowHovered ? 1.0f : 0.0f, Speed * 10.0f);

	if(Opt.m_Type == OPT_GROUP)
	{
		CUIRect Title;
		Row.HSplitTop(10.0f * K, nullptr, &Title);
		const float Size = F(9.0f);
		Label(Title, Opt.m_pLabel, Size, TEXTALIGN_ML, DIM);
		const float TextWidth = TextW(Opt.m_pLabel, Size) + 8.0f * K;
		Rect({Title.x + TextWidth, Title.y + Title.h * 0.5f, Title.w - TextWidth, std::max(0.6f, K)}, LINE, 0.0f);
		return;
	}
	if(Opt.m_Type == OPT_NOTE)
	{
		Label(Row, Opt.m_pLabel, F(10.5f), TEXTALIGN_ML, DIM);
		return;
	}
	if(Opt.m_Type == OPT_RENDERER)
	{
		const bool Ok = CCcPostFx::RendererSupported();
		const char *pText = Ok ? "Работает: в игре рендер OpenGL. Интерфейс и чат шейдеры не трогают." :
					 "Сейчас рендер не OpenGL, шейдеры не видны. Поставь OpenGL: Настройки → Графика → Рендер, потом перезапусти игру.";
		CUIRect Box;
		Row.HMargin(2.0f * K, &Box);
		Rect(Box, Ok ? A2.WithAlpha(0.08f) : ColorRGBA(1.0f, 0.3f, 0.25f, 0.14f), 6.0f * K);
		const float Size = F(9.5f);
		const SCachedText &T = Text(pText, Size, Box.w - 16.0f * K);
		TextAt(Box.x + 8.0f * K, Box.y + (Box.h - T.m_H) * 0.5f, pText, Size, Ok ? MUTED : INK, Box.w - 16.0f * K);
		return;
	}
	if(Opt.m_Type == OPT_PERF_STATUS)
	{
		const int Changed = CcPerf::NumChanged();
		if(!g_Config.m_BcPerfMode)
			str_copy(m_aPerfStatus, "Режим выключен. Включишь — запомнит, что меняет, и при выключении вернёт всё как было.");
		else if(Changed == 0)
			str_copy(m_aPerfStatus, "Режим включён: всё из списка ниже и так было выключено.");
		else
			str_format(m_aPerfStatus, sizeof(m_aPerfStatus), "Режим включён, изменено настроек: %d. Что включишь обратно руками, так и останется и после выключения режима.", Changed);
		CUIRect Box;
		Row.HMargin(2.0f * K, &Box);
		Rect(Box, g_Config.m_BcPerfMode ? A2.WithAlpha(0.1f) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.04f), 6.0f * K);
		const float Size = F(9.5f);
		const SCachedText &T = Text(m_aPerfStatus, Size, Box.w - 16.0f * K);
		TextAt(Box.x + 8.0f * K, Box.y + (Box.h - T.m_H) * 0.5f, m_aPerfStatus, Size, g_Config.m_BcPerfMode ? INK : MUTED, Box.w - 16.0f * K);
		return;
	}

	if(Opt.m_Anim > 0.0f && Opt.m_Type != OPT_BUTTON)
		Rect(Row, ColorRGBA(1.0f, 1.0f, 1.0f, 0.03f * Opt.m_Anim), 6.0f * K);

	CUIRect Line;
	Row.HSplitTop(Opt.m_Type == OPT_BG_PRESETS || Opt.m_Type == OPT_BG_FILES ? 22.0f * K : ROW_H * K, &Line, nullptr);
	CUIRect Content;
	Line.VMargin(4.0f * K, &Content);

	const bool TwoColumns = Opt.m_Type == OPT_SLIDER || Opt.m_Type == OPT_COLOR || Opt.m_Type == OPT_FONT;
	CUIRect LabelRect, Control;
	if(TwoColumns)
		Content.VSplitLeft(Content.w * 0.44f, &LabelRect, &Control);
	else
	{
		LabelRect = Content;
		Control = Content;
	}
	if(Opt.m_Type != OPT_BUTTON)
	{
		Label(LabelRect, Opt.m_pLabel, LabelSize, TEXTALIGN_ML, Mix(MUTED, INK, Opt.m_Anim * 0.7f));
		if(Opt.m_New)
		{
			CUIRect Badge = LabelRect;
			Badge.x += TextW(Opt.m_pLabel, LabelSize) + 5.0f * K;
			DrawNewBadge(&Badge, F(8.5f));
		}
		Tip(&Opt.m_aIds[15], Line, Opt.m_pLabel, Opt.m_pDesc, Opt.m_pCommand, Opt.m_New);
	}

	switch(Opt.m_Type)
	{
	case OPT_CHECK:
	{
		const bool Checked = *Opt.m_pInt != 0;
		if(Button(&Opt.m_aIds[0], &Line))
		{
			*Opt.m_pInt = Checked ? 0 : 1;
			PlaySound(*Opt.m_pInt ? SND_ON : SND_OFF, 0.8f);
		}
		CUIRect Box;
		Content.VSplitRight(16.0f * K, nullptr, &Box);
		Box.HMargin((Box.h - 16.0f * K) * 0.5f, &Box);
		if(*Opt.m_pInt)
		{
			RectH(Box, A1, A2, 4.0f * K);
			Glow(Box.Center(), 30.0f * K, A2.WithAlpha(0.18f));
			const ColorRGBA Mark = Fade(DARKTEXT);
			const vec2 Center = Box.Center();
			IGraphics *pGraphics = Graphics();
			m_vShapes.emplace_back([=]() {
				const vec2 a = Center + vec2(-4.2f, 0.4f) * K, b = Center + vec2(-1.2f, 3.4f) * K, c = Center + vec2(4.4f, -3.6f) * K;
				Stroke(pGraphics, a, b, 2.2f * K, Mark);
				Stroke(pGraphics, b - normalize(c - b) * 0.8f * K, c, 2.2f * K, Mark);
			});
		}
		else
		{
			Rect(Box, ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f + 0.15f * Opt.m_Anim), 4.0f * K);
			CUIRect Inner;
			Box.Margin(1.5f * K, &Inner);
			Rect(Inner, ColorRGBA(0.07f, 0.055f, 0.07f, 1.0f), 3.0f * K);
		}
		break;
	}
	case OPT_SLIDER:
	{
		CUIRect Track, Value;
		Control.VSplitRight(ValueW, &Track, &Value);
		Track.VSplitRight(12.0f * K, &Track, nullptr);
		Track.VMargin(6.0f * K, &Track);
		const void *pId = &Opt.m_aIds[0];
		CUIRect Hit = Track;
		Hit.x -= 6.0f * K;
		Hit.w += 12.0f * K;
		const bool HitHover = Hovered(&Hit);
		const int Before = *Opt.m_pInt;
		if(m_Interactive)
		{
			if(Ui()->CheckActiveItem(pId))
			{
				if(!Ui()->MouseButton(0))
					Ui()->SetActiveItem(nullptr);
			}
			else if(Ui()->HotItem() == pId && Ui()->MouseButtonClicked(0))
				Ui()->SetActiveItem(pId);
			if(HitHover && !Ui()->MouseButton(0))
				Ui()->SetHotItem(pId);
			if(Ui()->CheckActiveItem(pId) && Track.w > 0.0f)
			{
				const float t = std::clamp((Ui()->MouseX() - Track.x) / Track.w, 0.0f, 1.0f);
				*Opt.m_pInt = std::clamp((int)std::round(Opt.m_Min + t * (Opt.m_Max - Opt.m_Min)), Opt.m_Min, Opt.m_Max);
			}
			// right click on the slider: back to the default
			if(HitHover && Ui()->MouseButtonClicked(1))
			{
				*Opt.m_pInt = Opt.m_DefaultInt;
				PlaySound(SND_CLICK, 0.6f);
			}
		}
		if(*Opt.m_pInt != Before && Client()->LocalTime() - m_LastSlideSound > 0.035f)
		{
			PlaySound(SND_SLIDE, 0.6f);
			m_LastSlideSound = Client()->LocalTime();
		}

		const float t = Opt.m_Max > Opt.m_Min ? std::clamp((*Opt.m_pInt - Opt.m_Min) / (float)(Opt.m_Max - Opt.m_Min), 0.0f, 1.0f) : 0.0f;
		const bool Grabbed = m_Interactive && Ui()->CheckActiveItem(pId);
		CUIRect Bar = {Track.x, Track.y + Track.h * 0.5f - 2.0f * K, Track.w, 4.0f * K};
		Rect(Bar, ColorRGBA(1.0f, 1.0f, 1.0f, 0.1f), 2.0f * K);
		CUIRect Fill = Bar;
		Fill.w = Bar.w * t;
		RectH(Fill, A1, Mix(A1, A2, t), 2.0f * K);
		const float Thumb = (Grabbed ? 14.0f : 12.0f) * K;
		const vec2 ThumbAt = vec2(Bar.x + Bar.w * t, Bar.y + Bar.h * 0.5f);
		Glow(ThumbAt, (Grabbed ? 40.0f : 30.0f) * K, A2.WithAlpha(HitHover || Grabbed ? 0.6f : 0.4f));
		Rect({ThumbAt.x - Thumb * 0.5f, ThumbAt.y - Thumb * 0.5f, Thumb, Thumb}, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), Thumb * 0.5f);
		char aBuf[32];
		FormatValue(Opt, *Opt.m_pInt, aBuf, sizeof(aBuf));
		Label(Value, aBuf, F(10.0f), TEXTALIGN_MR, INK);
		break;
	}
	case OPT_COLOR:
	{
		CUIRect Swatch, Value;
		Control.VSplitRight(ValueW, &Swatch, &Value);
		Swatch.VSplitRight(12.0f * K, &Swatch, nullptr);
		Swatch.HMargin((Swatch.h - 20.0f * K) * 0.5f, &Swatch);
		const ColorHSLA Hsla(*Opt.m_pColor, Opt.m_Alpha);
		const ColorRGBA Rgba = color_cast<ColorRGBA>(Hsla);
		Rect(Swatch, ColorRGBA(1.0f, 1.0f, 1.0f, Hot(Opt.m_pColor) ? 0.45f : 0.2f), 5.0f * K);
		CUIRect Inner;
		Swatch.Margin(1.5f * K, &Inner);
		Rect(Inner, Rgba.WithAlpha(1.0f), 4.0f * K);
		if(Button(Opt.m_pColor, &Swatch))
		{
			m_ColorPicker.m_pHslaColor = Opt.m_pColor;
			m_ColorPicker.m_HslaColor = Hsla;
			m_ColorPicker.m_HsvaColor = color_cast<ColorHSVA>(Hsla);
			m_ColorPicker.m_RgbaColor = color_cast<ColorRGBA>(m_ColorPicker.m_HsvaColor);
			m_ColorPicker.m_Alpha = Opt.m_Alpha;
			m_ColorPicker.m_DefaultColor = color_cast<ColorRGBA>(ColorHSLA(Opt.m_DefaultColor, Opt.m_Alpha));
			m_ColorPicker.m_HasDefault = true;
			Ui()->ShowPopupColorPicker(Ui()->MouseX(), Ui()->MouseY(), &m_ColorPicker);
			PlaySound(SND_CLICK);
		}
		char aBuf[16];
		str_format(aBuf, sizeof(aBuf), "#%02X%02X%02X", round_to_int(Rgba.r * 255.0f), round_to_int(Rgba.g * 255.0f), round_to_int(Rgba.b * 255.0f));
		Label(Value, aBuf, F(9.5f), TEXTALIGN_MR, MUTED);
		break;
	}
	case OPT_CHOICE:
	{
		const float Size = F(10.5f);
		float Total = 0.0f;
		for(const char *pChoice : Opt.m_vChoices)
			Total += TextW(pChoice, Size) + 22.0f * K;
		const bool Inline = Row.h <= ROW_H * K + 0.5f;
		float x = Inline ? Content.x + Content.w - Total + 4.0f * K : Row.x + 4.0f * K;
		float y = Inline ? Content.y + (Content.h - 22.0f * K) * 0.5f : Row.y + ROW_H * K;
		for(int i = 0; i < (int)Opt.m_vChoices.size() && i < 15; i++)
		{
			const char *pChoice = Opt.m_vChoices[i];
			const float w = TextW(pChoice, Size) + 18.0f * K;
			if(!Inline && x > Row.x + 4.0f * K && x + w > Row.x + Row.w)
			{
				x = Row.x + 4.0f * K;
				y += 26.0f * K;
			}
			CUIRect Seg = {x, y, w, 22.0f * K};
			const bool Selected = *Opt.m_pInt == i;
			if(Button(&Opt.m_aIds[i], &Seg) && !Selected)
			{
				*Opt.m_pInt = i;
				PlaySound(SND_CLICK);
			}
			const bool SegHot = Hot(&Opt.m_aIds[i]);
			if(Selected)
			{
				RectH(Seg, A1.WithAlpha(0.9f), A2.WithAlpha(0.9f), 6.0f * K);
				Glow(Seg.Center(), Seg.w * 1.3f, A2.WithAlpha(0.15f));
			}
			else
				Rect(Seg, ColorRGBA(1.0f, 1.0f, 1.0f, SegHot ? 0.1f : 0.06f), 6.0f * K);
			Label(Seg, pChoice, Size, TEXTALIGN_MC, Selected ? DARKTEXT : (SegHot ? INK : MUTED));
			x += w + 4.0f * K;
		}
		break;
	}
	case OPT_BUTTON:
	{
		const bool InGame = GameClient()->m_Snap.m_pLocalCharacter != nullptr;
		if(Opt.m_NeedsGame && !InGame)
		{
			Label(Content, "Зайди за тее на сервере, чтобы проверить", F(10.5f), TEXTALIGN_ML, DIM);
			break;
		}
		const float w = TextW(Opt.m_pLabel, F(11.0f)) + 26.0f * K;
		CUIRect ButtonRect = {Content.x, Content.y + (Content.h - 24.0f * K) * 0.5f, w, 24.0f * K};
		const bool ButtonHot = Hot(&Opt.m_aIds[0]);
		Rect(ButtonRect, A2.WithAlpha(ButtonHot ? 0.55f : 0.35f), 7.0f * K);
		CUIRect Inner;
		ButtonRect.Margin(1.0f * K, &Inner);
		Rect(Inner, Mix(Cfg(g_Config.m_BcGuiBg1), A2, ButtonHot ? 0.18f : 0.08f), 6.0f * K);
		Label(ButtonRect, Opt.m_pLabel, F(11.0f), TEXTALIGN_MC, INK);
		if(Button(&Opt.m_aIds[0], &ButtonRect))
		{
			RunAction(Opt.m_Action);
			PlaySound(SND_CLICK);
		}
		break;
	}
	case OPT_KEY:
	{
		const char *pText = m_WaitingKey ? "Нажми клавишу…" : NiceKeyName(g_Config.m_BcGuiKey);
		const float w = TextW(pText, F(10.5f)) + 22.0f * K;
		CUIRect ButtonRect = {Content.x + Content.w - w, Content.y + (Content.h - 22.0f * K) * 0.5f, w, 22.0f * K};
		const bool ButtonHot = Hot(&Opt.m_aIds[0]);
		Rect(ButtonRect, m_WaitingKey ? A2.WithAlpha(0.6f) : ColorRGBA(1.0f, 1.0f, 1.0f, ButtonHot ? 0.22f : 0.14f), 6.0f * K);
		CUIRect Inner;
		ButtonRect.Margin(1.0f * K, &Inner);
		Rect(Inner, ColorRGBA(0.08f, 0.065f, 0.08f, 1.0f), 5.0f * K);
		Label(ButtonRect, pText, F(10.5f), TEXTALIGN_MC, m_WaitingKey ? A2 : INK);
		if(Button(&Opt.m_aIds[0], &ButtonRect))
		{
			m_WaitingKey = !m_WaitingKey;
			PlaySound(SND_CLICK);
		}
		break;
	}
	case OPT_FONT:
	{
		// "< name >": the game font, then every face the text renderer knows
		m_vpFonts.clear();
		m_vpFonts.push_back("");
		for(const std::string &Face : *TextRender()->GetCustomFaces())
			m_vpFonts.push_back(Face.c_str());
		int Current = 0;
		for(int i = 1; i < (int)m_vpFonts.size(); i++)
			if(str_comp(m_vpFonts[i], g_Config.m_BcGuiFont) == 0)
				Current = i;
		CUIRect Prev, Next, Name;
		Control.VSplitLeft(24.0f * K, &Prev, &Name);
		Name.VSplitRight(24.0f * K, &Name, &Next);
		Prev.HMargin((Prev.h - 22.0f * K) * 0.5f, &Prev);
		Next.HMargin((Next.h - 22.0f * K) * 0.5f, &Next);
		Rect(Prev, ColorRGBA(1.0f, 1.0f, 1.0f, Hot(&Opt.m_aIds[0]) ? 0.14f : 0.07f), 6.0f * K);
		Rect(Next, ColorRGBA(1.0f, 1.0f, 1.0f, Hot(&Opt.m_aIds[1]) ? 0.14f : 0.07f), 6.0f * K);
		IconLabel(Prev, FontIcon::CHEVRON_LEFT, F(8.0f), INK);
		IconLabel(Next, FontIcon::CHEVRON_RIGHT, F(8.0f), INK);
		Label(Name, Current == 0 ? "Как в игре" : m_vpFonts[Current], F(11.0f), TEXTALIGN_MC, INK);
		int Step = 0;
		if(Button(&Opt.m_aIds[0], &Prev))
			Step = -1;
		if(Button(&Opt.m_aIds[1], &Next))
			Step = 1;
		if(Step != 0)
		{
			const int Num = (int)m_vpFonts.size();
			Current = ((Current + Step) % Num + Num) % Num;
			str_copy(g_Config.m_BcGuiFont, m_vpFonts[Current], sizeof(g_Config.m_BcGuiFont));
			PlaySound(SND_CLICK);
		}
		break;
	}
	case OPT_PRESETS:
	{
		const float Size = F(10.5f);
		float x = Row.x + 4.0f * K, y = Row.y + ROW_H * K;
		for(int i = 0; i < (int)std::size(s_aLooks) && i < 15; i++)
		{
			const SLook &Look = s_aLooks[i];
			const float w = TextW(Look.m_pName, Size) + 18.0f * K;
			if(x > Row.x + 4.0f * K && x + w > Row.x + Row.w - 4.0f * K)
			{
				x = Row.x + 4.0f * K;
				y += 26.0f * K;
			}
			CUIRect Seg = {x, y, w, 22.0f * K};
			const bool Selected = g_Config.m_BcPostFxSaturation == Look.m_Saturation && g_Config.m_BcPostFxVibrance == Look.m_Vibrance &&
					      g_Config.m_BcPostFxContrast == Look.m_Contrast && g_Config.m_BcPostFxBrightness == Look.m_Brightness && g_Config.m_BcPostFxWarmth == Look.m_Warmth;
			if(Button(&Opt.m_aIds[i], &Seg))
			{
				g_Config.m_BcPostFxSaturation = Look.m_Saturation;
				g_Config.m_BcPostFxVibrance = Look.m_Vibrance;
				g_Config.m_BcPostFxContrast = Look.m_Contrast;
				g_Config.m_BcPostFxBrightness = Look.m_Brightness;
				g_Config.m_BcPostFxWarmth = Look.m_Warmth;
				PlaySound(SND_CLICK);
			}
			const bool SegHot = Hot(&Opt.m_aIds[i]);
			if(Selected)
				RectH(Seg, A1.WithAlpha(0.9f), A2.WithAlpha(0.9f), 6.0f * K);
			else
				Rect(Seg, ColorRGBA(1.0f, 1.0f, 1.0f, SegHot ? 0.1f : 0.06f), 6.0f * K);
			Label(Seg, Look.m_pName, Size, TEXTALIGN_MC, Selected ? DARKTEXT : (SegHot ? INK : MUTED));
			x += w + 4.0f * K;
		}
		break;
	}
	case OPT_THEMES:
	{
		const float ChipW = 46.0f * K, ChipH = 20.0f * K, Gap = 6.0f * K;
		float x = Content.x + Content.w - NUM_THEMES * (ChipW + Gap) + Gap;
		for(int i = 0; i < NUM_THEMES && i < 15; i++)
		{
			const STheme &Theme = s_aThemes[i];
			CUIRect Chip = {x, Content.y + (Content.h - ChipH) * 0.5f, ChipW, ChipH};
			const bool Selected = g_Config.m_BcGuiColor1 == PackRgb(Theme.m_A1) && g_Config.m_BcGuiColor2 == PackRgb(Theme.m_A2) &&
					      g_Config.m_BcGuiBg1 == PackRgb(Theme.m_Bg1) && g_Config.m_BcGuiBg2 == PackRgb(Theme.m_Bg2);
			const bool ChipHot = Hot(&Opt.m_aIds[i]);
			Rect(Chip, ColorRGBA(1.0f, 1.0f, 1.0f, Selected ? 0.85f : (ChipHot ? 0.4f : 0.15f)), 6.0f * K);
			CUIRect Inner;
			Chip.Margin((Selected ? 1.6f : 1.0f) * K, &Inner);
			Rect4(Inner, Theme.m_A1, Theme.m_A2, Mix(Theme.m_A1, Theme.m_Bg1, 0.6f), Mix(Theme.m_A2, Theme.m_Bg2, 0.6f), 5.0f * K);
			if(Button(&Opt.m_aIds[i], &Chip))
			{
				g_Config.m_BcGuiColor1 = PackRgb(Theme.m_A1);
				g_Config.m_BcGuiColor2 = PackRgb(Theme.m_A2);
				g_Config.m_BcGuiBg1 = PackRgb(Theme.m_Bg1);
				g_Config.m_BcGuiBg2 = PackRgb(Theme.m_Bg2);
				PlaySound(SND_CLICK);
			}
			Tip(&Opt.m_aIds[i], Chip, Theme.m_pName, "Поставить этот градиент.", nullptr, false);
			x += ChipW + Gap;
		}
		break;
	}
	case OPT_BG_PRESETS:
	{
		if(!m_ThumbsMade)
			MakeThumbnails();
		const float ChipW = 64.0f * K, ChipH = 36.0f * K, Gap = 6.0f * K;
		const int PerLine = std::max(1, (int)((Row.w - 8.0f * K + Gap) / (ChipW + Gap)));
		const bool FileChosen = g_Config.m_BcGuiBgFile[0] != '\0';
		for(int p = 0; p < CcMenuBg::NUM_PRESETS && p < 15; p++)
		{
			CUIRect Chip = {Row.x + 4.0f * K + (p % PerLine) * (ChipW + Gap), Row.y + 22.0f * K + (p / PerLine) * (ChipH + Gap), ChipW, ChipH};
			const bool Selected = !FileChosen && g_Config.m_BcGuiBgPreset == p;
			const bool ChipHot = Hot(&Opt.m_aIds[p]);
			Rect(Chip, Selected ? A2 : ColorRGBA(1.0f, 1.0f, 1.0f, ChipHot ? 0.4f : 0.14f), 6.0f * K);
			CUIRect Inner;
			Chip.Margin((Selected ? 1.8f : 1.0f) * K, &Inner);
			if(p == 0)
			{
				Rect(Inner, ColorRGBA(0.07f, 0.055f, 0.07f, 1.0f), 5.0f * K);
				Label(Inner, "Нет", F(9.5f), TEXTALIGN_MC, MUTED);
			}
			else if(m_aThumbs[p].IsValid())
			{
				// the thumbnail goes over the frame: drawn now, in the order of the queue
				Flush();
				Graphics()->TextureSet(m_aThumbs[p]);
				Graphics()->QuadsBegin();
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, m_Alpha);
				Graphics()->QuadsSetSubset(0.0f, 0.0f, 1.0f, 1.0f);
				IGraphics::CQuadItem Item(Inner.x, Inner.y, Inner.w, Inner.h);
				Graphics()->QuadsDrawTL(&Item, 1);
				Graphics()->QuadsEnd();
			}
			if(Button(&Opt.m_aIds[p], &Chip))
			{
				g_Config.m_BcGuiBgPreset = p;
				g_Config.m_BcGuiBgFile[0] = '\0';
				PlaySound(SND_CLICK);
			}
			Tip(&Opt.m_aIds[p], Chip, CcMenuBg::PresetName(p), nullptr, nullptr, false);
		}
		break;
	}
	case OPT_BG_FILES:
	{
		if(m_vpBgFileNames.empty())
		{
			CUIRect Note = {Row.x + 4.0f * K, Row.y + 22.0f * K, Row.w - 8.0f * K, 22.0f * K};
			Label(Note, "Папка пуста. Нажми «Открыть папку» и положи туда png или jpg.", F(10.0f), TEXTALIGN_ML, DIM);
			break;
		}
		const float Size = F(10.0f);
		float x = Row.x + 4.0f * K, y = Row.y + 22.0f * K;
		for(int i = 0; i < (int)m_vpBgFileNames.size(); i++)
		{
			const char *pName = m_vpBgFileNames[i];
			const float w = TextW(pName, Size) + 18.0f * K;
			if(x > Row.x + 4.0f * K && x + w > Row.x + Row.w - 4.0f * K)
			{
				x = Row.x + 4.0f * K;
				y += 26.0f * K;
			}
			CUIRect Seg = {x, y, w, 22.0f * K};
			const bool Selected = str_comp(g_Config.m_BcGuiBgFile, pName) == 0;
			const void *pId = &m_vBgFiles[i];
			if(Button(pId, &Seg))
			{
				if(Selected)
					g_Config.m_BcGuiBgFile[0] = '\0';
				else
					str_copy(g_Config.m_BcGuiBgFile, pName, sizeof(g_Config.m_BcGuiBgFile));
				PlaySound(SND_CLICK);
			}
			if(Selected)
				RectH(Seg, A1.WithAlpha(0.9f), A2.WithAlpha(0.9f), 6.0f * K);
			else
				Rect(Seg, ColorRGBA(1.0f, 1.0f, 1.0f, Hot(pId) ? 0.1f : 0.06f), 6.0f * K);
			Label(Seg, pName, Size, TEXTALIGN_MC, Selected ? DARKTEXT : MUTED);
			x += w + 4.0f * K;
		}
		break;
	}
	default: break;
	}
	(void)Feature;
}

void CChainsawGui::RenderTooltip(const CUIRect &Screen)
{
	if(!m_pTipId || (!m_pTipText && !m_TipNew && !m_pTipCommand))
		return;
	const float Shown = Client()->LocalTime() - m_TipTime - g_Config.m_BcGuiTooltipDelay / 1000.0f;
	if(Shown <= 0.0f)
		return;
	const float K = m_K;
	const float SavedAlpha = m_Alpha;
	m_Alpha = (Light() ? 1.0f : std::min(1.0f, Shown / 0.12f)) * SavedAlpha;

	const float MaxW = 270.0f * K;
	const float Pad = 10.0f * K;
	const float TitleSize = F(11.5f), TextSize = F(11.0f), CmdSize = F(9.5f);
	const char *pNewText = "Новая настройка, сделана для этого меню.";
	const float LineW = MaxW - Pad * 2.0f;
	float TextWidth = 0.0f, Height = 0.0f;
	const auto Measure = [&](const char *p, float Size, float LineWidth) {
		const SCachedText &T = Text(p, Size, LineWidth);
		TextWidth = std::max(TextWidth, T.m_W);
		Height += T.m_H + 4.0f * K;
	};
	if(m_pTipTitle)
		Measure(m_pTipTitle, TitleSize, LineW);
	if(m_pTipText)
		Measure(m_pTipText, TextSize, LineW);
	if(m_TipNew)
		Measure(pNewText, TextSize, LineW);
	if(m_pTipCommand)
		Measure(m_pTipCommand, CmdSize, -1.0f);
	const float W = std::min(MaxW, TextWidth + Pad * 2.0f);
	const float H = Height + Pad * 2.0f - 4.0f * K;
	float x = Ui()->MouseX() + 14.0f * K, y = Ui()->MouseY() + 18.0f * K;
	if(x + W > Screen.w - 6.0f)
		x = Screen.w - W - 6.0f;
	if(y + H > Screen.h - 6.0f)
		y = Ui()->MouseY() - H - 10.0f * K;

	if(!Light())
		Rect({x - 4.0f * K, y - 1.0f * K, W + 8.0f * K, H + 10.0f * K}, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), 12.0f * K);
	Rect({x - 1.0f * K, y - 1.0f * K, W + 2.0f * K, H + 2.0f * K}, LINE2.WithAlpha(0.22f), 10.0f * K);
	Rect({x, y, W, H}, TIPBG, 9.0f * K);

	float ty = y + Pad;
	const float tx = x + Pad;
	const auto Put = [&](const char *p, float Size, float LineWidth, ColorRGBA Color) {
		TextAt(tx, ty, p, Size, Color, LineWidth);
		ty += Text(p, Size, LineWidth).m_H + 4.0f * K;
	};
	if(m_pTipTitle)
		Put(m_pTipTitle, TitleSize, LineW, INK);
	if(m_pTipText)
		Put(m_pTipText, TextSize, LineW, MUTED);
	if(m_TipNew)
		Put(pNewText, TextSize, LineW, Cfg(g_Config.m_BcGuiColor2));
	if(m_pTipCommand)
		Put(m_pTipCommand, CmdSize, -1.0f, DIM);
	m_Alpha = SavedAlpha;
}
