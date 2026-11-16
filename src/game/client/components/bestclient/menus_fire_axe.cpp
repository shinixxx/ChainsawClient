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
	const bool Anim = g_Config.m_BcFireAxeAnim != 0;
	Rows += Anim ? 9 : 1; // animations: switch | + swing, trail, hit, sway, spin, speed, strike, strike size
	if(Anim && InGame)
		Rows += 1; // test buttons of the animations
	if(Anim)
		Rows += 2; // kind of strike: title + buttons
	const bool AxeSound = g_Config.m_BcFireAxeSound != 0;
	Rows += AxeSound ? 4 : 1; // sounds: switch | + volume, others, instead of the hammer

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
			g_Config.m_BcFireAxeAnim = DefaultConfig::BcFireAxeAnim;
			g_Config.m_BcFireAxeAnimSwing = DefaultConfig::BcFireAxeAnimSwing;
			g_Config.m_BcFireAxeAnimTrail = DefaultConfig::BcFireAxeAnimTrail;
			g_Config.m_BcFireAxeAnimImpact = DefaultConfig::BcFireAxeAnimImpact;
			g_Config.m_BcFireAxeAnimIdle = DefaultConfig::BcFireAxeAnimIdle;
			g_Config.m_BcFireAxeAnimSpin = DefaultConfig::BcFireAxeAnimSpin;
			g_Config.m_BcFireAxeAnimSpeed = DefaultConfig::BcFireAxeAnimSpeed;
			g_Config.m_BcFireAxeAnimSlash = DefaultConfig::BcFireAxeAnimSlash;
			g_Config.m_BcFireAxeAnimSlashSize = DefaultConfig::BcFireAxeAnimSlashSize;
			g_Config.m_BcFireAxeAnimAttack = DefaultConfig::BcFireAxeAnimAttack;
			g_Config.m_BcFireAxeSound = DefaultConfig::BcFireAxeSound;
			g_Config.m_BcFireAxeSoundVolume = DefaultConfig::BcFireAxeSoundVolume;
			g_Config.m_BcFireAxeSoundOthers = DefaultConfig::BcFireAxeSoundOthers;
			g_Config.m_BcFireAxeSoundReplace = DefaultConfig::BcFireAxeSoundReplace;
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

	// animations (08.11)
	RowCheck(Block, &g_Config.m_BcFireAxeAnim, BcLocalize("Axe animations"));
	if(Anim)
	{
		RowCheck(Block, &g_Config.m_BcFireAxeAnimSwing, BcLocalize("Swing on every hit"));
		RowCheck(Block, &g_Config.m_BcFireAxeAnimTrail, BcLocalize("Golden trail behind the blade"));
		RowCheck(Block, &g_Config.m_BcFireAxeAnimImpact, BcLocalize("Kicks back when it hits a tee"));
		RowCheck(Block, &g_Config.m_BcFireAxeAnimIdle, BcLocalize("Sways in the hand, a glint runs over it"));
		RowCheck(Block, &g_Config.m_BcFireAxeAnimSpin, BcLocalize("Spins in the hand"));
		RowCheck(Block, &g_Config.m_BcFireAxeAnimSlash, BcLocalize("Elegant strike: golden crescent and a cross of light"));
		RowSlider(Block, &g_Config.m_BcFireAxeAnimSlashSize, BcLocalize("Crescent size"), 50, 200, "%");
		RowSlider(Block, &g_Config.m_BcFireAxeAnimSpeed, BcLocalize("Animation speed"), 50, 200, "%");
		// the kind of strike
		{
			Block.HSplitTop(MARGIN_SMALL, nullptr, &Block);
			CUIRect KindLabel;
			Block.HSplitTop(LINE_SIZE, &KindLabel, &Block);
			Ui()->DoLabel(&KindLabel, BcLocalize("Axe strikes"), 14.0f, TEXTALIGN_ML);
			Block.HSplitTop(MARGIN_SMALL, nullptr, &Block);
			CUIRect KindButtons;
			Block.HSplitTop(LINE_SIZE, &KindButtons, &Block);
			constexpr int NUM_MODES = 5;
			static CButtonContainer s_aKind[NUM_MODES];
			const char *apKind[NUM_MODES] = {BcLocalize("Combo"), BcLocalize("Diagonal"), BcLocalize("Horizontal"), BcLocalize("Head bash"), BcLocalize("Random")};
			const char *apKindTip[NUM_MODES] = {
				BcLocalize("Diagonal, diagonal, horizontal, diagonal, head bash; a pause starts over"),
				BcLocalize("A golden crescent from above"),
				BcLocalize("A wide flat sweep round the tee"),
				BcLocalize("The axe head thrusts forward"),
				BcLocalize("A random kind on every hit")};
			const float KindW = KindButtons.w / (float)NUM_MODES;
			for(int Mode = 0; Mode < NUM_MODES; Mode++)
			{
				CUIRect Button;
				KindButtons.VSplitLeft(KindW, &Button, &KindButtons);
				Button.VMargin(1.0f, &Button);
				const int Corners = Mode == 0 ? IGraphics::CORNER_L : (Mode == NUM_MODES - 1 ? IGraphics::CORNER_R : IGraphics::CORNER_NONE);
				if(DoButton_Menu(&s_aKind[Mode], apKind[Mode], g_Config.m_BcFireAxeAnimAttack == Mode, &Button, BUTTONFLAG_LEFT, nullptr, Corners, 5.0f, 0.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f)))
					g_Config.m_BcFireAxeAnimAttack = Mode;
				GameClient()->m_Tooltips.DoToolTip(&s_aKind[Mode], &Button, apKindTip[Mode]);
			}
		}
		if(InGame)
		{
			Block.HSplitTop(MARGIN_SMALL, nullptr, &Block);
			CUIRect AnimLine, AnimLeft, AnimRight;
			Block.HSplitTop(LINE_SIZE, &AnimLine, &Block);
			AnimLine.VSplitMid(&AnimLeft, &AnimRight, 10.0f);
			static CButtonContainer s_TestSwing, s_TestSpin;
			if(DoButton_Menu(&s_TestSwing, BcLocalize("Test swing"), 0, &AnimLeft))
				GameClient()->m_BcFireAxeFx.PreviewSwing();
			if(DoButton_Menu(&s_TestSpin, BcLocalize("Test spin"), 0, &AnimRight))
				GameClient()->m_BcFireAxeFx.PreviewSpin();
		}
	}

	// sounds (08.11)
	RowCheck(Block, &g_Config.m_BcFireAxeSound, BcLocalize("Axe sounds: swing and hit"));
	if(AxeSound)
	{
		RowSlider(Block, &g_Config.m_BcFireAxeSoundVolume, BcLocalize("Axe sound volume"), 0, 100, "%");
		RowCheck(Block, &g_Config.m_BcFireAxeSoundOthers, BcLocalize("Axe sounds of other players"));
		RowCheck(Block, &g_Config.m_BcFireAxeSoundReplace, BcLocalize("Instead of the hammer sounds"));
	}

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
