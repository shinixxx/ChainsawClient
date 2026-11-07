/* ChainsawClient tab: golden axe block (fx/fire_axe.cpp) */
#include <engine/font_icons.h>
#include <engine/shared/config.h>

#include <game/client/components/bestclient/ui_theme/style.h>
#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/localization.h>

namespace
{
constexpr float LINE_SIZE = 20.0f;
constexpr float MARGIN_SMALL = 5.0f;
constexpr float HEADLINE_FONT_SIZE = 20.0f;
constexpr float MARGIN_BETWEEN_VIEWS = 30.0f;
constexpr float ROW = MARGIN_SMALL + LINE_SIZE;
}

void CMenus::RenderSettingsChainsawFireAxe(CUIRect &Column)
{
	const auto RowCheck = [&](CUIRect &View, int *pValue, const char *pText) {
		View.HSplitTop(MARGIN_SMALL, nullptr, &View);
		CUIRect Line;
		View.HSplitTop(LINE_SIZE, &Line, &View);
		DoButton_CheckBoxAutoVMarginAndSet(pValue, pText, pValue, &Line, LINE_SIZE);
	};
	const auto RowSlider = [&](CUIRect &View, int *pValue, const char *pText, int Min, int Max, const char *pSuffix) {
		View.HSplitTop(MARGIN_SMALL, nullptr, &View);
		CUIRect Line;
		View.HSplitTop(LINE_SIZE, &Line, &View);
		Ui()->DoScrollbarOption(pValue, pValue, &Line, pText, Min, Max, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_DELAYUPDATE, pSuffix);
	};

	const bool Summon = g_Config.m_BcFireAxeSummon != 0;
	const bool Heat = g_Config.m_BcFireAxeHeat != 0;
	const bool InGame = GameClient()->m_Snap.m_pLocalCharacter != nullptr;
	int Rows = 6; // hint, size, glow, summon, heat, test buttons
	if(Summon)
		Rows += 2;
	if(Heat)
		Rows += 1;

	// the block, like the other ChainsawClient blocks
	static CButtonContainer s_Reset;
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
	{
		CUIRect Label, Title, ResetButton;
		Block.HSplitTop(LINE_SIZE, &Label, &Block);
		Label.VSplitRight(LINE_SIZE + 8.0f, &Title, &ResetButton);
		if(Ui()->DoButton_FontIcon(&s_Reset, FontIcon::ARROW_ROTATE_LEFT, 0, &ResetButton, BUTTONFLAG_LEFT))
		{
			g_Config.m_BcFireAxeSize = DefaultConfig::BcFireAxeSize;
			g_Config.m_BcFireAxeGlow = DefaultConfig::BcFireAxeGlow;
			g_Config.m_BcFireAxeSummon = DefaultConfig::BcFireAxeSummon;
			g_Config.m_BcFireAxeSummonSwitch = DefaultConfig::BcFireAxeSummonSwitch;
			g_Config.m_BcFireAxeSummonTime = DefaultConfig::BcFireAxeSummonTime;
			g_Config.m_BcFireAxeHeat = DefaultConfig::BcFireAxeHeat;
			g_Config.m_BcFireAxeHeatTime = DefaultConfig::BcFireAxeHeatTime;
		}
		GameClient()->m_Tooltips.DoToolTip(&s_Reset, &ResetButton, BcLocalize("Reset to defaults"));
		Ui()->DoLabel(&Title, BcLocalize("Golden axe"), HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	}

	{
		Block.HSplitTop(MARGIN_SMALL, nullptr, &Block);
		CUIRect Hint;
		Block.HSplitTop(LINE_SIZE, &Hint, &Block);
		Ui()->DoLabel(&Hint, g_Config.m_BcFireHammer ? BcLocalize("Works while the flaming hammer is on") : BcLocalize("Turn on the flaming hammer (Fire & Ice) to see it"), 12.0f, TEXTALIGN_ML);
	}
	RowSlider(Block, &g_Config.m_BcFireAxeSize, BcLocalize("Axe size"), 60, 160, "%");
	RowSlider(Block, &g_Config.m_BcFireAxeGlow, BcLocalize("Axe glow"), 0, 200, "%");
	RowCheck(Block, &g_Config.m_BcFireAxeSummon, BcLocalize("Comes out of fire and light when turned on"));
	if(Summon)
	{
		RowCheck(Block, &g_Config.m_BcFireAxeSummonSwitch, BcLocalize("Also every time you take the hammer"));
		RowSlider(Block, &g_Config.m_BcFireAxeSummonTime, BcLocalize("Appear time (0.1 s)"), 3, 25, "");
	}
	RowCheck(Block, &g_Config.m_BcFireAxeHeat, BcLocalize("Burns brighter with every hit"));
	if(Heat)
		RowSlider(Block, &g_Config.m_BcFireAxeHeatTime, BcLocalize("Burn time after a hit (0.1 s)"), 10, 150, "");

	Block.HSplitTop(MARGIN_SMALL, nullptr, &Block);
	CUIRect Line;
	Block.HSplitTop(LINE_SIZE, &Line, &Block);
	if(!InGame)
		Ui()->DoLabel(&Line, BcLocalize("Join a server to preview"), 13.0f, TEXTALIGN_ML);
	else
	{
		CUIRect Left, Right;
		Line.VSplitMid(&Left, &Right, 10.0f);
		static CButtonContainer s_TestSummon, s_TestHit;
		if(DoButton_Menu(&s_TestSummon, BcLocalize("Test appear"), 0, &Left))
			GameClient()->m_BcFireAxeFx.PreviewSummon();
		if(DoButton_Menu(&s_TestHit, BcLocalize("Test hit"), 0, &Right))
			GameClient()->m_BcFireAxeFx.PreviewHit();
	}
}
