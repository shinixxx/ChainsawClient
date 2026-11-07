/* ChainsawClient: settings of the post-processing pass ("shaders") sent to the renderer once per frame.
   The pass runs after the world and before the HUD, so the interface keeps its colors. */
#ifndef ENGINE_BC_POSTFX_PARAMS_H
#define ENGINE_BC_POSTFX_PARAMS_H

struct SBcPostFxParams
{
	float m_Saturation = 1.0f; // 1 = as it is
	float m_Vibrance = 0.0f; // extra saturation for dull colors
	float m_Contrast = 1.0f;
	float m_Brightness = 1.0f;
	float m_Warmth = 0.0f; // -1 cold .. 1 warm
	float m_Vignette = 0.0f;
	float m_Bloom = 0.0f;
	float m_BloomThreshold = 0.6f;
	float m_BloomRadius = 1.0f;
	float m_Sharpen = 0.0f;
	float m_Grain = 0.0f;
	float m_Chroma = 0.0f; // chromatic aberration at the edges
	float m_Time = 0.0f;
	float m_aTint[3] = {1.0f, 1.0f, 1.0f}; // multiplies the picture (color of the light)
};

#endif
