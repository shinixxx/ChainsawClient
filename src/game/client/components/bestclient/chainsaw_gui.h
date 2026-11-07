/* ChainsawClient: in-game menu with all our visuals (opened with bc_gui_key, right shift by default).
   Every function is switched on and off with a toggle, a right click unfolds all of its settings,
   holding the mouse on a function shows what it does.
   Drawing: every label is a cached text container, rectangles / glows / shapes are queued and drawn in a few
   batches (flushed when the clip area changes), so the open menu costs little. */
#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_CHAINSAW_GUI_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_CHAINSAW_GUI_H

#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/textrender.h>

#include <game/client/component.h>
#include <game/client/lineinput.h>
#include <game/client/ui.h>
#include <game/client/ui_scrollregion.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class CChainsawGui : public CComponent
{
public:
	enum EOptType
	{
		OPT_CHECK = 0,
		OPT_SLIDER,
		OPT_COLOR,
		OPT_CHOICE,
		OPT_GROUP,
		OPT_BUTTON,
		OPT_NOTE,
		OPT_KEY,
		OPT_THEMES,
		OPT_FONT,
		OPT_BG_PRESETS,
		OPT_BG_FILES,
		OPT_PRESETS, // chips that set several sliders at once (shader looks)
		OPT_RENDERER, // note: do the shaders work with the current renderer
		OPT_PERF_STATUS, // note: what the optimization mode has turned off right now
		OPT_BG_STATUS, // note: the own picture behind the menu could not be opened
	};

	enum EFormat
	{
		FMT_PLAIN = 0,
		FMT_TENTHS, // tenths of a second, shown as "0,8 с"
	};

	enum EAction
	{
		ACT_NONE = 0,
		ACT_AXE_SUMMON,
		ACT_AXE_HIT,
		ACT_SHOCKWAVE,
		ACT_ICE,
		ACT_STRIKE,
		ACT_OPEN_BG_FOLDER,
		ACT_RESCAN_BG,
		ACT_TEST_LIGHTNING,
	};

	enum EIcon
	{
		ICON_FIRE = 0,
		ICON_CHAIN,
		ICON_KAGUNE,
		ICON_MENU,
		ICON_STAR,
		ICON_IMAGE,
		ICON_BOLT,
	};

	enum ESound
	{
		SND_OPEN = 0,
		SND_CLOSE,
		SND_ON,
		SND_OFF,
		SND_CLICK,
		SND_HOVER,
		SND_SLIDE,
		NUM_SOUNDS,
	};

	struct SOpt
	{
		EOptType m_Type = OPT_NOTE;
		const char *m_pLabel = "";
		const char *m_pDesc = nullptr;
		const char *m_pCommand = nullptr;
		int *m_pInt = nullptr;
		unsigned *m_pColor = nullptr;
		char *m_pStr = nullptr; // string settings: reset only
		int m_StrSize = 0;
		const char *m_pDefaultStr = nullptr;
		int m_Min = 0;
		int m_Max = 1;
		int m_DefaultInt = 0;
		unsigned m_DefaultColor = 0;
		bool m_Alpha = false;
		const char *m_pSuffix = "";
		int m_Format = FMT_PLAIN;
		std::vector<const char *> m_vChoices;
		int m_Action = ACT_NONE;
		int m_PresetSet = 0;
		bool m_NeedsGame = false; // test buttons need your tee on a server
		bool m_New = false; // "+": a setting made for this menu
		int *m_pShowIf = nullptr;
		int m_ShowIfValue = -1; // -1: shown while *m_pShowIf != 0

		SOpt &Desc(const char *pDesc)
		{
			m_pDesc = pDesc;
			return *this;
		}
		SOpt &New()
		{
			m_New = true;
			return *this;
		}
		SOpt &Tenths()
		{
			m_Format = FMT_TENTHS;
			m_pSuffix = "";
			return *this;
		}
		SOpt &Suffix(const char *pSuffix)
		{
			m_pSuffix = pSuffix;
			return *this;
		}
		SOpt &ShowIf(int *pValue, int Value = -1)
		{
			m_pShowIf = pValue;
			m_ShowIfValue = Value;
			return *this;
		}
		bool Visible() const
		{
			if(!m_pShowIf)
				return true;
			return m_ShowIfValue < 0 ? *m_pShowIf != 0 : *m_pShowIf == m_ShowIfValue;
		}

		// ui ids
		char m_aIds[16] = {};
		float m_Anim = 0.0f;
	};

	struct SFeature
	{
		const char *m_pName = "";
		const char *m_pDesc = "";
		const char *m_pCommand = nullptr;
		int *m_pEnable = nullptr; // nullptr: always on (menu settings), always unfolded
		std::vector<SOpt> m_vOpts;
		int m_Category = 0;

		bool m_Open = false;
		float m_OpenAnim = 0.0f;
		float m_SwitchAnim = 0.0f;
		float m_HoverAnim = 0.0f;
		char m_RowId = 0;
		char m_MoreId = 0;
		char m_ResetId = 0;
	};

	struct SCategory
	{
		const char *m_pName = "";
		int m_Icon = ICON_FIRE;
		std::vector<SFeature> m_vFeatures;
		char m_Id = 0;
		float m_Anim = 0.0f;
	};

	CChainsawGui();
	int Sizeof() const override { return sizeof(*this); }

	void OnConsoleInit() override;
	void OnInit() override;
	void OnReset() override;
	void OnRelease() override;
	void OnShutdown() override;
	void OnWindowResize() override;
	void OnRender() override;
	bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;
	bool OnInput(const IInput::CEvent &Event) override;

	bool IsActive() const { return m_Active; }
	void SetActive(bool Active);

private:
	static void ConToggle(IConsole::IResult *pResult, void *pUserData);

	void BuildItems();
	void SetUiMousePos(vec2 Pos);
	bool CanOpen() const;
	bool Light() const;
	void PlaySound(int Sound, float Volume = 1.0f);
	int LoadOwnOpus(const char *pPath);

	// ---- cached text
	struct SCachedText
	{
		STextContainerIndex m_Index;
		float m_W = 0.0f;
		float m_H = 0.0f;
		float m_MaxCharH = 0.0f;
		int m_LastFrame = 0;
	};
	const SCachedText &Text(const char *pText, float Size, float LineWidth = -1.0f, bool Icon = false);
	float TextW(const char *pText, float Size) { return Text(pText, Size).m_W; }
	void ClearTextCache();
	void ClearLogo();
	STextContainerIndex m_LogoName;
	STextContainerIndex m_LogoSub;
	std::string m_LogoKey;
	float m_LogoNameW = 0.0f;
	float m_LogoNameH = 0.0f;
	float m_LogoNameMaxH = 0.0f;
	float m_LogoSubW = 0.0f;
	float m_LogoSubH = 0.0f;
	int m_TextGeneration = 0; // grows when all text containers are dropped (window resize)
	static constexpr const char *LOGO_FONT = "Montserrat SemiBold";
	float F(float Size) const; // font size in ui units (scale and bc_gui_font_size)

	// ---- batched drawing (colors are multiplied by the current fade when queued)
	struct SRectCmd
	{
		float m_X, m_Y, m_W, m_H, m_Round;
		int m_Corners;
		ColorRGBA m_aColors[4];
	};
	struct SGlowCmd
	{
		vec2 m_Pos;
		float m_Size;
		ColorRGBA m_Color;
	};
	struct STextCmd
	{
		STextContainerIndex m_Index;
		float m_X, m_Y;
		ColorRGBA m_Color;
		ColorRGBA m_Outline;
	};
	std::vector<SRectCmd> m_vRects;
	std::vector<SGlowCmd> m_vGlows;
	std::vector<std::function<void()>> m_vShapes; // drawn inside a CcFx batch with the fx atlas
	std::vector<STextCmd> m_vTexts;
	void Flush();
	void ClipEnable(const CUIRect *pRect);
	void ClipDisable();

	ColorRGBA Fade(ColorRGBA Color) const;
	void Rect(const CUIRect &Rect, ColorRGBA Color, float Rounding, int Corners = IGraphics::CORNER_ALL);
	void RectH(const CUIRect &Rect, ColorRGBA Left, ColorRGBA Right, float Rounding, int Corners = IGraphics::CORNER_ALL);
	void Rect4(const CUIRect &Rect, ColorRGBA TL, ColorRGBA TR, ColorRGBA BL, ColorRGBA BR, float Rounding, int Corners = IGraphics::CORNER_ALL);
	void Label(const CUIRect &Rect, const char *pText, float Size, int Align, ColorRGBA Color);
	void TextAt(float x, float y, const char *pText, float Size, ColorRGBA Color, float LineWidth = -1.0f);
	void IconLabel(const CUIRect &Rect, const char *pIcon, float Size, ColorRGBA Color);
	void Glow(vec2 Pos, float Size, ColorRGBA Color);
	void DrawIcon(int Icon, const CUIRect &Rect, ColorRGBA Main, ColorRGBA Second);
	void DrawLogo(const CUIRect &Rect);
	void DrawSwitch(const CUIRect &Rect, float On);
	void DrawNewBadge(CUIRect *pRect, float FontSize);

	// ---- input helpers (do nothing while the window is closing)
	int Button(const void *pId, const CUIRect *pRect, unsigned Flags = BUTTONFLAG_LEFT);
	bool Hovered(const CUIRect *pRect) const;
	bool Hot(const void *pId) const;
	void Tip(const void *pId, const CUIRect &Rect, const char *pTitle, const char *pText, const char *pCommand, bool New);

	// ---- window parts
	void RenderWindow(const CUIRect &Screen);
	void RenderBar(CUIRect Bar);
	void RenderRail(CUIRect Rail);
	void RenderList(CUIRect List);
	void RenderFooter(CUIRect Footer);
	void RenderTooltip(const CUIRect &Screen);
	void RenderBackgroundImage(const CUIRect &Win, float Round, float Alpha);

	float FeatureHeight(SFeature &Feature, float Width);
	float OptsHeight(SFeature &Feature, float Width);
	float OptHeight(SOpt &Opt, float Width);
	float WrapHeight(const std::vector<const char *> &vItems, float Size, float Width, int *pLines);
	void RenderFeature(SFeature &Feature, CUIRect Rect, bool ShowCategory);
	void RenderOpt(SFeature &Feature, SOpt &Opt, CUIRect Row);
	void ResetFeature(SFeature &Feature);
	void RunAction(int Action);
	bool Matches(const SFeature &Feature, const char *pQuery) const;
	void FormatValue(const SOpt &Opt, int Value, char *pBuf, int BufSize) const;
	int EnabledCount(const SCategory &Category) const;
	int TotalCount(const SCategory &Category) const;

	// ---- menu backgrounds
	void ScanBackgrounds();
	void UpdateBackground();
	void MakeThumbnails();
	static int BackgroundListCallback(const char *pName, int IsDir, int StorageType, void *pUser);
	IGraphics::CTextureHandle m_BgTexture;
	int m_BgW = 0;
	int m_BgH = 0;
	std::string m_BgKey; // what the texture shows: preset / file + blur
	std::vector<std::string> m_vBgFiles;
	std::vector<const char *> m_vpBgFileNames;
	bool m_BgScanned = false;
	IGraphics::CTextureHandle m_aThumbs[16];
	bool m_ThumbsMade = false;

	// ---- fonts
	std::vector<const char *> m_vpFonts; // "" = game font, then the faces the text renderer knows
	void ApplyMenuFont();
	void RestoreFont();
	std::string m_FontKey;

	std::vector<SCategory> m_vCategories;
	bool m_Built = false;

	bool m_Active = false;
	bool m_Closing = false;
	bool m_Interactive = false;
	float m_Anim = 0.0f; // 0 closed .. 1 open
	float m_Alpha = 1.0f;
	float m_ListTime = 0.0f; // for the cards coming in one after another
	float m_K = 1.0f; // ui units per design pixel
	float m_Dt = 0.0f;
	int m_Frame = 0;
	bool m_MouseUnlocked = false;
	std::optional<vec2> m_LastGameMousePos;
	std::optional<vec2> m_LastMenuMousePos;

	vec2 m_WinPos = vec2(0.0f, 0.0f);
	float m_LastScreenW = -1.0f;
	float m_LastScreenH = -1.0f;
	CUIRect m_Win;
	char m_DragId = 0;
	vec2 m_DragOffset = vec2(0.0f, 0.0f);
	char m_CloseId = 0;

	CLineInputBuffered<64> m_SearchInput;
	CScrollRegion m_ScrollRegion;
	CUi::SColorPickerPopupContext m_ColorPicker;
	bool m_WaitingKey = false;
	int m_LastCategory = -1;

	std::unordered_map<uint64_t, SCachedText> m_TextCache;
	char m_aPerfStatus[256] = {};
	int m_BgFailed = 0; // the chosen own picture could not be opened (shows m_aBgError)
	char m_aBgError[256] = {};

	int m_aSounds[NUM_SOUNDS];
	float m_LastSlideSound = 0.0f;
	const void *m_pLastHoverRow = nullptr;

	// tooltip
	const void *m_pTipCandidate = nullptr;
	const void *m_pTipId = nullptr;
	float m_TipTime = 0.0f;
	const char *m_pTipTitle = nullptr;
	const char *m_pTipText = nullptr;
	const char *m_pTipCommand = nullptr;
	bool m_TipNew = false;
	const char *m_pCandTitle = nullptr;
	const char *m_pCandText = nullptr;
	const char *m_pCandCommand = nullptr;
	bool m_CandNew = false;
};

#endif
