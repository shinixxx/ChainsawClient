/* ChainsawClient tab: fire chain block */
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

// a block with a title and a reset button, Rows rows below; returns the inner rect, Reset is set when it was clicked
static CUIRect BeginChainsawBlock(CGameClient *pGameClient, CUi *pUi, CUIRect &Column, int Rows, const char *pTitle, CButtonContainer *pResetId, bool &Reset)
{
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
	Reset = pUi->DoButton_FontIcon(pResetId, FontIcon::ARROW_ROTATE_LEFT, 0, &ResetButton, BUTTONFLAG_LEFT) != 0;
	pGameClient->m_Tooltips.DoToolTip(pResetId, &ResetButton, BcLocalize("Reset to defaults"));
	pUi->DoLabel(&Title, pTitle, HEADLINE_FONT_SIZE, TEXTALIGN_ML);
	return Block;
}

void CMenus::RenderSettingsChainsawFireChain(CUIRect &Column)
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

	const bool Chain = g_Config.m_BcFireChain != 0;
	const bool Burn = g_Config.m_BcFireBurn != 0;
	const bool Strike = g_Config.m_BcFireStrike != 0;
	const bool InGame = GameClient()->m_Snap.m_pLocalCharacter != nullptr;
	int Rows = 4; // three switches and the color
	if(Chain)
		Rows += 3;
	if(Burn)
		Rows += 1;
	if(Strike)
		Rows += g_Config.m_BcFireStrikeMega ? 6 : 4; // + mega switch, only on hits, its color

	static CButtonContainer s_Reset;
	bool Reset = false;
	CUIRect Block = BeginChainsawBlock(GameClient(), Ui(), Column, Rows, BcLocalize("Fire chain"), &s_Reset, Reset);
	if(Reset)
	{
		g_Config.m_BcFireChainOthers = DefaultConfig::BcFireChainOthers;
		g_Config.m_BcFireChainStyle = DefaultConfig::BcFireChainStyle;
		g_Config.m_BcFireChainFlames = DefaultConfig::BcFireChainFlames;
		g_Config.m_BcFireBurnTime = DefaultConfig::BcFireBurnTime;
		g_Config.m_BcFireStrikeOthers = DefaultConfig::BcFireStrikeOthers;
		g_Config.m_BcFireStrikeSize = DefaultConfig::BcFireStrikeSize;
		g_Config.m_BcFireChainColor = DefaultConfig::BcFireChainColor;
		g_Config.m_BcFireStrikeMega = DefaultConfig::BcFireStrikeMega;
		g_Config.m_BcFireStrikeColor = DefaultConfig::BcFireStrikeColor;
		g_Config.m_BcFireStrikeMegaHitsOnly = DefaultConfig::BcFireStrikeMegaHitsOnly;
	}

	RowCheck(Block, &g_Config.m_BcFireChain, BcLocalize("Burning hook chain"));
	if(Chain)
	{
		RowCheck(Block, &g_Config.m_BcFireChainStyle, BcLocalize("Burning rope instead of links"));
		RowCheck(Block, &g_Config.m_BcFireChainOthers, BcLocalize("Fire chain for other players"));
		RowSlider(Block, &g_Config.m_BcFireChainFlames, BcLocalize("Chain flames"), 25, 300, "%");
	}
	RowCheck(Block, &g_Config.m_BcFireBurn, BcLocalize("Burn marks on hit players"));
	if(Burn)
		RowSlider(Block, &g_Config.m_BcFireBurnTime, BcLocalize("Burn mark time (0.1 s)"), 10, 100, "");
	RowCheck(Block, &g_Config.m_BcFireStrike, BcLocalize("Fire flash on hammer strikes"));
	if(Strike)
	{
		RowCheck(Block, &g_Config.m_BcFireStrikeOthers, BcLocalize("Fire flash for other players"));
		RowSlider(Block, &g_Config.m_BcFireStrikeSize, BcLocalize("Flash size"), 50, 200, "%");
		RowCheck(Block, &g_Config.m_BcFireStrikeMega, BcLocalize("Mega strike: huge orange blast"));
		if(g_Config.m_BcFireStrikeMega)
		{
			RowCheck(Block, &g_Config.m_BcFireStrikeMegaHitsOnly, BcLocalize("Mega strike only when the hammer hits a tee"));
			static CButtonContainer s_StrikeColor;
			Block.HSplitTop(MARGIN_SMALL, nullptr, &Block);
			DoLine_ColorPicker(&s_StrikeColor, LINE_SIZE, 13.0f, 0.0f, &Block, BcLocalize("Mega strike color"), &g_Config.m_BcFireStrikeColor,
				color_cast<ColorRGBA>(ColorHSLA(DefaultConfig::BcFireStrikeColor)), false);
		}
		Block.HSplitTop(MARGIN_SMALL, nullptr, &Block);
		CUIRect Line;
		Block.HSplitTop(LINE_SIZE, &Line, &Block);
		if(!InGame)
			Ui()->DoLabel(&Line, BcLocalize("Join a server to preview"), 13.0f, TEXTALIGN_ML);
		else
		{
			Line.VSplitLeft(160.0f, &Line, nullptr);
			static CButtonContainer s_StrikeTest;
			if(DoButton_Menu(&s_StrikeTest, BcLocalize("Test flash"), 0, &Line))
				GameClient()->m_BcFireChainFx.PreviewStrike(GameClient()->m_LocalCharacterPos);
		}
	}
	static CButtonContainer s_Color;
	Block.HSplitTop(MARGIN_SMALL, nullptr, &Block);
	DoLine_ColorPicker(&s_Color, LINE_SIZE, 13.0f, 0.0f, &Block, BcLocalize("Fire whip color"), &g_Config.m_BcFireChainColor,
		color_cast<ColorRGBA>(ColorHSLA(DefaultConfig::BcFireChainColor)), false);
}
