/* ChainsawClient: light and shadows over the map (bc_light, 10.11). The world gets dark and is lit by lamps: the tees
   carry one, light falls in from the sky, and the player can hang own lamps (saved per map in chainsaw/lights).
   Every lamp throws real shadows of the blocks and makes the dusty air glow. The drawing itself is done by the
   OpenGL 3.3 renderer (engine/cc_light_params.h); this component only collects what lights the screen. */
#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_CC_LIGHT_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_CC_LIGHT_H

#include <base/vmath.h>

#include <engine/console.h>
#include <engine/graphics.h>

#include <game/client/component.h>

#include <vector>

class CCcLight : public CComponent
{
public:
	static constexpr const char *FOLDER = "chainsaw/lights";

	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void OnMapLoad() override;
	void OnShutdown() override;
	void OnRender() override;

	void PlaceLamp(bool AtCursor); // hangs an own lamp at the cursor or at the tee
	void RemoveNearest(); // takes down the own lamp closest to the tee
	void ClearLamps(); // all own lamps of this map
	int NumOwnLamps() const { return (int)m_vOwn.size(); }

private:
	IGraphics::CTextureHandle m_MapTexture;
	int m_MapWidth = 0;
	int m_MapHeight = 0;
	std::vector<vec2> m_vOwn; // own lamps of this map, in blocks

	vec2 OwnPos() const; // where the own tee (or the camera) is, in world units
	void LampFile(char *pBuf, int BufSize) const;
	void LoadLamps();
	void SaveLamps();

	static void ConPlace(IConsole::IResult *pResult, void *pUserData);
	static void ConRemove(IConsole::IResult *pResult, void *pUserData);
	static void ConClear(IConsole::IResult *pResult, void *pUserData);
};

#endif
