/* ChainsawClient: own map background (bc_map_bg, 09.11). A picture from chainsaw/map_backgrounds takes the place of
   the background layers of the map (everything the map draws behind the game layer): CMapLayers of the background
   type asks Replaces() and calls Draw() instead of drawing its layers. Any picture Windows can open (WIC), cover-fit to
   the screen, moves a little with the camera (bc_map_bg_parallax). */
#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_CC_MAP_BG_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_CC_MAP_BG_H

#include <engine/graphics.h>

#include <game/client/component.h>

#include <string>
#include <vector>

class CCcMapBg : public CComponent
{
public:
	static constexpr const char *FOLDER = "chainsaw/map_backgrounds";

	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnShutdown() override;

	bool Replaces(); // the own background is on and loaded
	void Draw();

	// the pictures in the folder (names), scanned again on Rescan()
	const std::vector<std::string> &Files() const { return m_vFiles; }
	void Rescan();
	void Select(int Index); // turns it on with that picture
	void Step(int Dir); // the next / previous picture of the folder
	void PickFile(); // a Windows file dialog: the picture is copied into the folder and chosen
	void OpenFolder();
	const char *Status() const { return m_aStatus; }

private:
	IGraphics::CTextureHandle m_Texture;
	int m_Width = 0;
	int m_Height = 0;
	char m_aLoaded[128] = "";
	bool m_Failed = false;
	char m_aStatus[160] = "";
	std::vector<std::string> m_vFiles;

	void Load();
};

#endif
