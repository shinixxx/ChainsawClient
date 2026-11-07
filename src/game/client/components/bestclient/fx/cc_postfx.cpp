/* ChainsawClient: "shaders", see cc_postfx.h. */
#include "cc_postfx.h"

#include <base/str.h>

#include <engine/bc_postfx_params.h>
#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>

#include <cmath>

bool CCcPostFx::AnyEnabled()
{
	return g_Config.m_BcPostFxColor || g_Config.m_BcPostFxBloom || g_Config.m_BcPostFxVignette || g_Config.m_BcPostFxSharpen || g_Config.m_BcPostFxGrain || g_Config.m_BcPostFxChroma;
}

bool CCcPostFx::RendererSupported()
{
	return str_comp_nocase(g_Config.m_GfxBackend, "OpenGL") == 0;
}

void CCcPostFx::OnRender()
{
	if(!AnyEnabled() || g_Config.m_BcHeadless)
		return;
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	SBcPostFxParams Params;
	if(g_Config.m_BcPostFxColor)
	{
		Params.m_Saturation = g_Config.m_BcPostFxSaturation / 100.0f;
		Params.m_Vibrance = g_Config.m_BcPostFxVibrance / 100.0f;
		Params.m_Contrast = g_Config.m_BcPostFxContrast / 100.0f;
		Params.m_Brightness = g_Config.m_BcPostFxBrightness / 100.0f;
		Params.m_Warmth = g_Config.m_BcPostFxWarmth / 100.0f;
		const ColorRGBA Tint = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_BcPostFxTint));
		Params.m_aTint[0] = Tint.r;
		Params.m_aTint[1] = Tint.g;
		Params.m_aTint[2] = Tint.b;
	}
	if(g_Config.m_BcPostFxBloom)
	{
		Params.m_Bloom = g_Config.m_BcPostFxBloomAmount / 100.0f;
		Params.m_BloomThreshold = 0.3f + 0.65f * g_Config.m_BcPostFxBloomThreshold / 100.0f;
		Params.m_BloomRadius = g_Config.m_BcPostFxBloomRadius / 100.0f * 2.0f;
	}
	if(g_Config.m_BcPostFxVignette)
		Params.m_Vignette = g_Config.m_BcPostFxVignetteAmount / 100.0f * 0.8f;
	if(g_Config.m_BcPostFxSharpen)
		Params.m_Sharpen = g_Config.m_BcPostFxSharpenAmount / 100.0f;
	if(g_Config.m_BcPostFxGrain)
		Params.m_Grain = g_Config.m_BcPostFxGrainAmount / 100.0f;
	if(g_Config.m_BcPostFxChroma)
		Params.m_Chroma = g_Config.m_BcPostFxChromaAmount / 100.0f;
	Params.m_Time = (float)std::fmod(Client()->LocalTime(), 1000.0);
	Graphics()->BcPostFx(Params);
}
