/* bestclient: settings block for fire ring, flaming hammer, landing shockwave and freeze ice */
#include <engine/font_icons.h>
#include <engine/shared/config.h>

#include <game/client/components/bestclient/ui_theme/style.h>
#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/localization.h>

void CMenus::RenderSettingsBestClientFireIce(CUIRect &Column)
{
	const float LineSize = 20.0f;
	const float MarginSmall = 5.0f;
	const float HeadlineFontSize = 20.0f;
	const float MarginBetweenViews = 30.0f;
	const float BlockPadding = MarginBetweenViews * 0.6666f;
	const float Row = MarginSmall + LineSize;

	const auto RowCheck = [&](CUIRect &View, int *pValue, const char *pText) {
		View.HSplitTop(MarginSmall, nullptr, &View);
		CUIRect Line;
		View.HSplitTop(LineSize, &Line, &View);
		DoButton_CheckBoxAutoVMarginAndSet(pValue, pText, pValue, &Line, LineSize);
	};
	const auto RowSlider = [&](CUIRect &View, int *pValue, const char *pText, int Min, int Max) {
		View.HSplitTop(MarginSmall, nullptr, &View);
		CUIRect Line;
		View.HSplitTop(LineSize, &Line, &View);
		Ui()->DoScrollbarOption(pValue, pValue, &Line, pText, Min, Max, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_DELAYUPDATE, "%");
	};
	const auto RowColor = [&](CUIRect &View, CButtonContainer *pResetId, const char *pText, unsigned *pColor, unsigned DefaultColor) {
		View.HSplitTop(MarginSmall, nullptr, &View);
		DoLine_ColorPicker(pResetId, LineSize, 13.0f, 0.0f, &View, pText, pColor, color_cast<ColorRGBA>(ColorHSLA(DefaultColor)), false);
	};
	const bool InGame = GameClient()->m_Snap.m_pLocalCharacter != nullptr;
	const auto RowButton = [&](CUIRect &View, CButtonContainer *pId, const char *pText) {
		View.HSplitTop(MarginSmall, nullptr, &View);
		CUIRect Line;
		View.HSplitTop(LineSize, &Line, &View);
		if(!InGame)
		{
			Ui()->DoLabel(&Line, BcLocalize("Join a server to preview"), 13.0f, TEXTALIGN_ML);
			return false;
		}
		Line.VSplitLeft(160.0f, &Line, nullptr);
		return DoButton_Menu(pId, pText, 0, &Line) != 0;
	};

	const bool Aura = g_Config.m_BcFireAura != 0;
	const bool Hammer = g_Config.m_BcFireHammer != 0;
	const bool Shock = g_Config.m_BcLandShockwave != 0;
	const bool Ice = g_Config.m_BcFreezeIce != 0;
	int Rows = 5;
	if(Aura)
		Rows += 4;
	if(Aura || Hammer)
		Rows += 1;
	if(Hammer)
		Rows += 2;
	if(Shock)
		Rows += 3;
	if(Ice)
		Rows += 4;

	Column.HSplitTop(MarginBetweenViews, nullptr, &Column);
	CUIRect Block;
	Column.HSplitTop(LineSize + Rows * Row, &Block, &Column);
	CUIRect Bg = Block;
	Bg.w += BlockPadding;
	Bg.h += BlockPadding;
	Bg.x -= BlockPadding * 0.5f;
	Bg.y -= BlockPadding * 0.5f;
	BestClientUiTheme::DrawUiBlock(&Bg, IGraphics::CORNER_ALL, 10.0f);

	CUIRect Label, Title, ResetButton;
	Block.HSplitTop(LineSize, &Label, &Block);
	Label.VSplitRight(LineSize + 8.0f, &Title, &ResetButton);
	static CButtonContainer s_Reset;
	if(Ui()->DoButton_FontIcon(&s_Reset, FontIcon::ARROW_ROTATE_LEFT, 0, &ResetButton, BUTTONFLAG_LEFT))
	{
		g_Config.m_BcFireAuraOthers = DefaultConfig::BcFireAuraOthers;
		g_Config.m_BcFireAuraSize = DefaultConfig::BcFireAuraSize;
		g_Config.m_BcFireAuraFlames = DefaultConfig::BcFireAuraFlames;
		g_Config.m_BcFireAuraSpin = DefaultConfig::BcFireAuraSpin;
		g_Config.m_BcFireAuraColor = DefaultConfig::BcFireAuraColor;
		g_Config.m_BcFireHammerOthers = DefaultConfig::BcFireHammerOthers;
		g_Config.m_BcFireHammerSparks = DefaultConfig::BcFireHammerSparks;
		g_Config.m_BcLandShockwaveOthers = DefaultConfig::BcLandShockwaveOthers;
		g_Config.m_BcLandShockwavePower = DefaultConfig::BcLandShockwavePower;
		g_Config.m_BcFreezeIceOthers = DefaultConfig::BcFreezeIceOthers;
		g_Config.m_BcFreezeIceAmount = DefaultConfig::BcFreezeIceAmount;
		g_Config.m_BcFreezeIceColor = DefaultConfig::BcFreezeIceColor;
	}
	GameClient()->m_Tooltips.DoToolTip(&s_Reset, &ResetButton, BcLocalize("Reset to defaults"));
	Ui()->DoLabel(&Title, BcLocalize("Fire & Ice"), HeadlineFontSize, TEXTALIGN_ML);

	RowCheck(Block, &g_Config.m_BcFireAura, BcLocalize("Fire ring"));
	if(Aura)
	{
		RowCheck(Block, &g_Config.m_BcFireAuraOthers, BcLocalize("Show on other players"));
		RowSlider(Block, &g_Config.m_BcFireAuraSize, BcLocalize("Ring size"), 50, 200);
		RowSlider(Block, &g_Config.m_BcFireAuraFlames, BcLocalize("Flames"), 25, 300);
		RowSlider(Block, &g_Config.m_BcFireAuraSpin, BcLocalize("Spin speed"), 0, 300);
	}
	RowCheck(Block, &g_Config.m_BcFireHammer, BcLocalize("Flaming hammer"));
	if(Hammer)
	{
		RowCheck(Block, &g_Config.m_BcFireHammerOthers, BcLocalize("Flaming hammer for other players"));
		RowSlider(Block, &g_Config.m_BcFireHammerSparks, BcLocalize("Fire sparks"), 0, 300);
	}
	if(Aura || Hammer)
	{
		static CButtonContainer s_FireColor;
		RowColor(Block, &s_FireColor, BcLocalize("Fire color"), &g_Config.m_BcFireAuraColor, DefaultConfig::BcFireAuraColor);
	}

	RowCheck(Block, &g_Config.m_BcLandShockwave, BcLocalize("Landing shockwave"));
	if(Shock)
	{
		RowCheck(Block, &g_Config.m_BcLandShockwaveOthers, BcLocalize("Shockwave for other players"));
		RowSlider(Block, &g_Config.m_BcLandShockwavePower, BcLocalize("Shockwave power"), 25, 300);
		static CButtonContainer s_ShockTest;
		if(RowButton(Block, &s_ShockTest, BcLocalize("Test shockwave")))
			GameClient()->m_BcFireIceFx.PreviewShockwave(GameClient()->m_LocalCharacterPos);
	}

	RowCheck(Block, &g_Config.m_BcFreezeIce, BcLocalize("Freeze ice"));
	if(Ice)
	{
		RowCheck(Block, &g_Config.m_BcFreezeIceOthers, BcLocalize("Ice for other players"));
		RowSlider(Block, &g_Config.m_BcFreezeIceAmount, BcLocalize("Ice shards"), 25, 300);
		static CButtonContainer s_IceColor;
		RowColor(Block, &s_IceColor, BcLocalize("Ice color"), &g_Config.m_BcFreezeIceColor, DefaultConfig::BcFreezeIceColor);
		static CButtonContainer s_IceTest;
		if(RowButton(Block, &s_IceTest, BcLocalize("Test ice")))
			GameClient()->m_BcFireIceFx.PreviewIce(GameClient()->m_LocalCharacterPos);
	}

	// the hint about what is shown; one row is always reserved for it
	Block.HSplitTop(MarginSmall, nullptr, &Block);
	CUIRect Hint;
	Block.HSplitTop(LineSize, &Hint, &Block);
	Ui()->DoLabel(&Hint, BcLocalize("Effects turn down by themselves when FPS drops"), 12.0f, TEXTALIGN_ML);
}
