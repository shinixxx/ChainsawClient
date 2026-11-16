/* ChainsawClient: settings of the post-processing pass ("shaders") sent to the renderer once per frame.
   The pass runs after the world and before the HUD, so the interface keeps its colors.
   08.11: after the one-shader pass (color, bloom, vignette...) a chain of filters (BC_FILTER_*): every enabled filter
   is a full screen pass of its own, in the order of the list; each gets 4 numbers and a color. */
#ifndef ENGINE_BC_POSTFX_PARAMS_H
#define ENGINE_BC_POSTFX_PARAMS_H

// the filters, in the order they are applied (the shader sources are in bc_postfx_opengl3.inl, same order)
enum
{
	BC_FILTER_GAMMA = 0,
	BC_FILTER_HUE,
	BC_FILTER_BW,
	BC_FILTER_SEPIA,
	BC_FILTER_INVERT,
	BC_FILTER_THERMAL,
	BC_FILTER_NIGHTVISION,
	BC_FILTER_SPLIT,
	BC_FILTER_HORROR,
	BC_FILTER_WAVES,
	BC_FILTER_FISHEYE,
	BC_FILTER_DRUNK,
	BC_FILTER_SWIRL,
	BC_FILTER_BLUR,
	BC_FILTER_TILTSHIFT,
	BC_FILTER_DREAM,
	BC_FILTER_POSTERIZE,
	BC_FILTER_OIL,
	BC_FILTER_SKETCH,
	BC_FILTER_WATERCOLOR,
	BC_FILTER_CARTOON,
	BC_FILTER_EDGES,
	BC_FILTER_PIXEL,
	BC_FILTER_HEX,
	BC_FILTER_STAINED,
	BC_FILTER_HALFTONE,
	BC_FILTER_ASCII,
	BC_FILTER_GAMEBOY,
	BC_FILTER_DITHER,
	BC_FILTER_GLITCH,
	BC_FILTER_VHS,
	BC_FILTER_CRT,
	NUM_BC_FILTERS
};

struct SBcFilter
{
	int m_Id = 0;
	float m_aP[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	float m_aColor[3] = {0.0f, 0.0f, 0.0f};
};

struct SBcPostFxParams
{
	bool m_Base = true; // the one-shader pass below (off when only filters are on)
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

	int m_NumFilters = 0;
	SBcFilter m_aFilters[NUM_BC_FILTERS];
};

#endif
