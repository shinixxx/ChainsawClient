/* ChainsawClient: painted backgrounds for the in-game menu (no engine code, so they can be previewed offline). */
#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_CHAINSAW_GUI_BG_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_CHAINSAW_GUI_BG_H

#include <cstdint>
#include <vector>

namespace CcMenuBg
{
enum
{
	BG_NONE = 0,
	BG_SUNSET, // mountains at sunset
	BG_FOREST, // night forest under the moon
	BG_SAKURA, // sakura branch, moon
	BG_STARS, // milky way over the hills
	BG_NEON, // synthwave city
	BG_EMBERS, // ash and embers
	NUM_PRESETS,
};

const char *PresetName(int Preset);

// paints a preset into RGBA8 pixels (W * H * 4)
void Paint(int Preset, int W, int H, std::vector<uint8_t> &vRgba);

// box blur three times, close to a gaussian; Radius in pixels
void Blur(std::vector<uint8_t> &vRgba, int W, int H, int Radius);

// scales RGBA8 down (box filter) so that it covers at most MaxW x MaxH
void Fit(std::vector<uint8_t> &vRgba, int &W, int &H, int MaxW, int MaxH);
} // namespace CcMenuBg

#endif
