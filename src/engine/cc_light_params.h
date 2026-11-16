/* ChainsawClient: light and shadows over the map, sent to the renderer once per frame (fx/cc_light.cpp).
   The picture drawn so far gets dark and is lit only by the lamps: every lamp throws shadows of the blocks,
   the air in its light glows (rays, dust). Positions are in blocks of the map. */
#ifndef ENGINE_CC_LIGHT_PARAMS_H
#define ENGINE_CC_LIGHT_PARAMS_H

enum
{
	CC_LIGHT_MAX_LAMPS = 24,
};

struct SCcLamp
{
	float m_X = 0.0f;
	float m_Y = 0.0f;
	float m_Range = 8.0f; // blocks, the light is gone at this distance
	float m_Size = 0.5f; // blocks, bigger = softer edges of the shadows
	float m_aColor[3] = {1.0f, 1.0f, 1.0f}; // with the brightness
};

struct SCcLightParams
{
	int m_MapTexture = -1; // texture of the map: one pixel per block, red above 0.5 = the block stops the light
	float m_OriginX = 0.0f; // top left corner of the screen, in blocks
	float m_OriginY = 0.0f;
	float m_SizeX = 1.0f; // size of the screen in blocks
	float m_SizeY = 1.0f;
	float m_Time = 0.0f;
	int m_Quality = 1; // 0 low .. 3 ultra: size of the light picture and rays per lamp
	float m_aAmbient[3] = {0.1f, 0.1f, 0.15f}; // what is left without any lamp
	float m_Haze = 0.5f; // glow of the air in the light
	float m_Dust = 0.5f; // specks of dust in the light
	float m_Vignette = 0.4f;

	bool m_Sun = false; // light from far away: lit where no block is on the way within m_SunReach blocks
	float m_aSunDir[2] = {-0.5f, -0.86f}; // to the sun (y goes down)
	float m_SunReach = 14.0f;
	float m_SunSoft = 1.0f;
	float m_aSunColor[3] = {1.0f, 0.8f, 0.6f};

	int m_NumLamps = 0;
	SCcLamp m_aLamps[CC_LIGHT_MAX_LAMPS];
};

#endif
