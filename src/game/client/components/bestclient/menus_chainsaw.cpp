/* ChainsawClient: own settings tab, separate from the BestClient pages */
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/components/bestclient/ui_theme/style.h>
#include <game/client/components/menus.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

// The settings page number is limited (ui_settings_page max 15) and every number is taken,
// so the tab keeps its own state. While it is open the page is parked on SETTINGS_LANGUAGE,
// which has no tab button, so no other tab looks selected; any other tab click closes it.
static bool s_ChainsawOpen = false;

bool CMenus::ChainsawTabOpen() const
{
	return s_ChainsawOpen && !BestClientUiTheme::IsNewTabOption();
}

void CMenus::RenderChainsawTabButton(CUIRect &TabBar)
{
	CUIRect Button;
	TabBar.HSplitTop(10.0f, nullptr, &TabBar);
	TabBar.HSplitTop(26.0f, &Button, &TabBar);

	const char *pName = "ChainsawClient";
	const float FontSize = (Button.h - 4.0f) * CUi::ms_FontmodHeight;
	if(TextRender()->TextWidth(FontSize, pName) > Button.w - 8.0f)
		pName = "CC";

	static CButtonContainer s_Button;
	static SUIAnimator s_Animator;
	const int Corners = BestClientUiTheme::IsSettingsNavOnLeft() ? IGraphics::CORNER_L : IGraphics::CORNER_R;
	if(DoButton_MenuTab(&s_Button, pName, s_ChainsawOpen, &Button, Corners, &s_Animator))
		s_ChainsawOpen = true;
	GameClient()->m_Tooltips.DoToolTip(&s_Button, &Button, "ChainsawClient");

	if(s_ChainsawOpen)
		g_Config.m_UiSettingsPage = SETTINGS_LANGUAGE;
}

void CMenus::RenderChainsawTabAfter()
{
	// another tab was clicked
	if(s_ChainsawOpen && g_Config.m_UiSettingsPage != SETTINGS_LANGUAGE)
		s_ChainsawOpen = false;
}

void CMenus::RenderSettingsChainsaw(CUIRect MainView)
{
	CUIRect Header;
	MainView.HSplitTop(30.0f, &Header, &MainView);
	Ui()->DoLabel(&Header, "ChainsawClient", 24.0f, TEXTALIGN_ML);
	MainView.HSplitTop(10.0f, nullptr, &MainView);

	static CScrollRegion s_ScrollRegion;
	CScrollRegionParams Params;
	Params.m_ScrollUnit = 60.0f;
	Params.m_ScrollbarMargin = 5.0f;
	s_ScrollRegion.Begin(&MainView, &Params);
	MainView.VSplitRight(5.0f, &MainView, nullptr);

	CUIRect LeftView, RightView;
	MainView.VSplitMid(&LeftView, &RightView, 30.0f);
	LeftView.VSplitLeft(10.0f, nullptr, &LeftView);

	RenderSettingsBestClientFireIce(LeftView);
	RenderSettingsChainsawFireChain(LeftView);
	RenderSettingsChainsawFireAxe(LeftView);
	RightView.VSplitLeft(10.0f, nullptr, &RightView);
	RenderSettingsChainsawKagune(RightView);
	// the right column may end lower than the left one
	CUIRect RightEnd;
	RightEnd.x = MainView.x;
	RightEnd.y = RightView.y + 10.0f;
	RightEnd.w = MainView.w;
	RightEnd.h = 0.0f;
	s_ScrollRegion.AddRect(RightEnd);

	CUIRect End;
	End.x = MainView.x;
	End.y = LeftView.y + 10.0f;
	End.w = MainView.w;
	End.h = 0.0f;
	s_ScrollRegion.AddRect(End);
	s_ScrollRegion.End();
}
