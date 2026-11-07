/* ChainsawClient tab: kagune block (bc_kagune*) */
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

void CMenus::RenderSettingsChainsawKagune(CUIRect &Column)
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

	const bool On = g_Config.m_BcKagune != 0;
	const int Rows = On ? 16 : 2; // switch, others, pistol, kind title + buttons, 5 sliders, color, attacks title + buttons, blood, blood amount, hint | switch, hint

	// the same block as the other ChainsawClient blocks: a title with a reset button
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
		g_Config.m_BcKaguneOthers = DefaultConfig::BcKaguneOthers;
		g_Config.m_BcKaguneCount = DefaultConfig::BcKaguneCount;
		g_Config.m_BcKaguneLength = DefaultConfig::BcKaguneLength;
		g_Config.m_BcKaguneWidth = DefaultConfig::BcKaguneWidth;
		g_Config.m_BcKaguneShimmer = DefaultConfig::BcKaguneShimmer;
		g_Config.m_BcKaguneParticles = DefaultConfig::BcKaguneParticles;
		g_Config.m_BcKaguneColor = DefaultConfig::BcKaguneColor;
		g_Config.m_BcKaguneAttack = DefaultConfig::BcKaguneAttack;
		g_Config.m_BcKaguneStyle = DefaultConfig::BcKaguneStyle;
		g_Config.m_BcKaguneGun = DefaultConfig::BcKaguneGun;
		g_Config.m_BcKaguneBlood = DefaultConfig::BcKaguneBlood;
		g_Config.m_BcKaguneBloodAmount = DefaultConfig::BcKaguneBloodAmount;
	}
	GameClient()->m_Tooltips.DoToolTip(&s_Reset, &ResetButton, BcLocalize("Reset to defaults"));
	Ui()->DoLabel(&Title, BcLocalize("Kagune"), HEADLINE_FONT_SIZE, TEXTALIGN_ML);

	RowCheck(Block, &g_Config.m_BcKagune, BcLocalize("Kagune instead of the hammer"));
	if(On)
	{
		RowCheck(Block, &g_Config.m_BcKaguneOthers, BcLocalize("Kagune for other players"));
		RowCheck(Block, &g_Config.m_BcKaguneGun, BcLocalize("Keep the kagune with the pistol"));

		// the kind of kagune (examples-of-kagune)
		{
			Block.HSplitTop(MARGIN_SMALL, nullptr, &Block);
			CUIRect StyleLabel;
			Block.HSplitTop(LINE_SIZE, &StyleLabel, &Block);
			Ui()->DoLabel(&StyleLabel, BcLocalize("Kind of kagune"), 14.0f, TEXTALIGN_ML);
			Block.HSplitTop(MARGIN_SMALL, nullptr, &Block);
			CUIRect StyleButtons;
			Block.HSplitTop(LINE_SIZE, &StyleButtons, &Block);
			static CButtonContainer s_aStyle[BcKagune::NUM_STYLES];
			const char *apStyle[BcKagune::NUM_STYLES] = {BcLocalize("Rinkaku"), BcLocalize("Kakuja"), BcLocalize("Ukaku"), BcLocalize("Bone")};
			const char *apStyleTip[BcKagune::NUM_STYLES] = {
				BcLocalize("Crimson ribbons with glowing veins and scales"),
				BcLocalize("Black violet armour plates, red glowing seams, claws and red tendrils"),
				BcLocalize("A wing of crystal shards with teeth and streamers, shoots shards"),
				BcLocalize("White bone segments with red eyes, red joints and spikes")};
			const float StyleW = StyleButtons.w / (float)BcKagune::NUM_STYLES;
			for(int Kind = 0; Kind < BcKagune::NUM_STYLES; Kind++)
			{
				CUIRect Button;
				StyleButtons.VSplitLeft(StyleW, &Button, &StyleButtons);
				Button.VMargin(1.0f, &Button);
				const int Corners = Kind == 0 ? IGraphics::CORNER_L : (Kind == BcKagune::NUM_STYLES - 1 ? IGraphics::CORNER_R : IGraphics::CORNER_NONE);
				if(DoButton_Menu(&s_aStyle[Kind], apStyle[Kind], g_Config.m_BcKaguneStyle == Kind, &Button, BUTTONFLAG_LEFT, nullptr, Corners, 5.0f, 0.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f)))
					g_Config.m_BcKaguneStyle = Kind;
				GameClient()->m_Tooltips.DoToolTip(&s_aStyle[Kind], &Button, apStyleTip[Kind]);
			}
		}

		RowSlider(Block, &g_Config.m_BcKaguneCount, BcLocalize("Tentacles"), 1, 8, "");
		RowSlider(Block, &g_Config.m_BcKaguneLength, BcLocalize("Kagune length"), 50, 200, "%");
		RowSlider(Block, &g_Config.m_BcKaguneWidth, BcLocalize("Kagune thickness"), 50, 200, "%");
		RowSlider(Block, &g_Config.m_BcKaguneShimmer, BcLocalize("Flowing light"), 0, 200, "%");
		RowSlider(Block, &g_Config.m_BcKaguneParticles, BcLocalize("Red cells (particles)"), 0, 200, "%");
		static CButtonContainer s_Color;
		Block.HSplitTop(MARGIN_SMALL, nullptr, &Block);
		DoLine_ColorPicker(&s_Color, LINE_SIZE, 13.0f, 0.0f, &Block, BcLocalize("Kagune color"), &g_Config.m_BcKaguneColor,
			color_cast<ColorRGBA>(ColorHSLA(DefaultConfig::BcKaguneColor)), false);

		// the kind of strike on a hammer hit
		Block.HSplitTop(MARGIN_SMALL, nullptr, &Block);
		CUIRect AttackLabel;
		Block.HSplitTop(LINE_SIZE, &AttackLabel, &Block);
		Ui()->DoLabel(&AttackLabel, BcLocalize("Kagune attacks"), 14.0f, TEXTALIGN_ML);
		Block.HSplitTop(MARGIN_SMALL, nullptr, &Block);
		CUIRect Buttons;
		Block.HSplitTop(LINE_SIZE, &Buttons, &Block);
		static CButtonContainer s_aAttack[BcKagune::NUM_ATTACK_MODES];
		const char *apAttack[BcKagune::NUM_ATTACK_MODES] = {BcLocalize("Combo"), BcLocalize("One"), BcLocalize("Two"), BcLocalize("All as one"), BcLocalize("Random")};
		const char *apAttackTip[BcKagune::NUM_ATTACK_MODES] = {
			BcLocalize("One, one, two, one, two, then all as one; a pause starts over"),
			BcLocalize("One tentacle stabs"),
			BcLocalize("Two tentacles stab together, crossing"),
			BcLocalize("All tentacles press together and strike as one spear"),
			BcLocalize("A random kind on every hit")};
		const float ButtonW = Buttons.w / (float)BcKagune::NUM_ATTACK_MODES;
		for(int Mode = 0; Mode < BcKagune::NUM_ATTACK_MODES; Mode++)
		{
			CUIRect Button;
			Buttons.VSplitLeft(ButtonW, &Button, &Buttons);
			Button.VMargin(1.0f, &Button);
			const int Corners = Mode == 0 ? IGraphics::CORNER_L : (Mode == BcKagune::NUM_ATTACK_MODES - 1 ? IGraphics::CORNER_R : IGraphics::CORNER_NONE);
			if(DoButton_Menu(&s_aAttack[Mode], apAttack[Mode], g_Config.m_BcKaguneAttack == Mode, &Button, BUTTONFLAG_LEFT, nullptr, Corners, 5.0f, 0.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f)))
				g_Config.m_BcKaguneAttack = Mode;
			GameClient()->m_Tooltips.DoToolTip(&s_aAttack[Mode], &Button, apAttackTip[Mode]);
		}
		RowCheck(Block, &g_Config.m_BcKaguneBlood, BcLocalize("Blood when a strike hits a tee"));
		RowSlider(Block, &g_Config.m_BcKaguneBloodAmount, BcLocalize("Amount of blood"), 0, 300, "%");
	}
	Block.HSplitTop(MARGIN_SMALL, nullptr, &Block);
	CUIRect Hint;
	Block.HSplitTop(LINE_SIZE, &Hint, &Block);
	Ui()->DoLabel(&Hint, BcLocalize("Stabs where you aim on every hammer hit"), 12.0f, TEXTALIGN_ML);
}
