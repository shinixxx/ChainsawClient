/* ChainsawClient: the ghoul eye and the kagune coming out (see kagune_ghoul.h, the picture is in kagune_ghoul_core.h) */
#include "kagune_ghoul.h"

#include "cc_fx_common.h"
#include "own_tee.h"

#include <base/math.h>
#include <base/str.h>

#include <engine/image.h>
#include <engine/shared/config.h>

#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>

#include <algorithm>
#include <cmath>

void CBcKaguneGhoulFx::OnReset()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aEmerge[i] = BcGhoul::CEmerge();
		m_aSeen[i] = -10.0f;
	}
}

void CBcKaguneGhoulFx::EnsureTexture()
{
	if(m_Texture.IsValid())
		return;
	CImageInfo Image;
	Image.m_Width = BcGhoul::TEX_W;
	Image.m_Height = BcGhoul::TEX_H;
	Image.m_Format = CImageInfo::FORMAT_RGBA;
	Image.Allocate();
	BcGhoul::BuildTexture(Image.m_pData);
	m_Texture = Graphics()->LoadTextureRawMove(Image, 0, "cc_ghoul_eye");
}

void CBcKaguneGhoulFx::Emerge(int ClientId, BcKagune::CTee &Kagune, const BcKagune::CInput &In, BcKagune::CParticles &Particles)
{
	if(!in_range(ClientId, MAX_CLIENTS - 1))
		return;
	const float Amount = BcFxSuppressed(GameClient()) ? 0.0f : g_Config.m_BcKaguneParticles / 100.0f;
	BcGhoul::Emerge(m_aEmerge[ClientId], Kagune, In, Particles, Amount);
	m_aSeen[ClientId] = LocalTime();
}

void CBcKaguneGhoulFx::OnTeeDrawn(int ClientId, bool Ghost, bool Local, const CAnimState *pAnim, const CTeeRenderInfo *pInfo, int Emote, vec2 Dir, vec2 Pos, float Alpha, vec2 BodyScale, float BodyAngle)
{
	if(Ghost || !in_range(ClientId, MAX_CLIENTS - 1) || !g_Config.m_BcKagune)
		return;
	const BcGhoul::CEmerge &E = m_aEmerge[ClientId];
	// the kagune of this tee is out (it was posed this frame or the one before)
	if(E.m_Eye <= 0.0f || LocalTime() - m_aSeen[ClientId] > 0.2f)
		return;
	// which eye for this skin and emote (closed eyes are left as they are)
	static_assert(EMOTE_NORMAL == BcGhoul::EYE_NORMAL && EMOTE_PAIN == BcGhoul::EYE_PAIN && EMOTE_HAPPY == BcGhoul::EYE_HAPPY && EMOTE_SURPRISE == BcGhoul::EYE_SURPRISE && EMOTE_ANGRY == BcGhoul::EYE_ANGRY && EMOTE_BLINK == BcGhoul::EYE_BLINK);
	const bool Derp = str_comp(GameClient()->m_aClients[ClientId].m_aSkinName, "derp") == 0;
	int Sclera;
	BcGhoul::CIris Iris;
	if(!BcGhoul::EyeVariant(Derp, Emote, Sclera, Iris))
		return;
	// 0.7 tees have their eyes in one picture
	if(pInfo->m_aSixup[g_Config.m_ClDummy].PartTexture(protocol7::SKINPART_BODY).IsValid())
		return;
	EnsureTexture();

	// the eye of the tee, the same numbers RenderTee6 uses
	float AnimScale, BaseSize;
	CRenderTools::GetRenderTeeAnimScaleAndBaseSize(pInfo, AnimScale, BaseSize);
	if(g_Config.m_TcTinyTees && (Local || g_Config.m_TcTinyTeesOthers))
	{
		const float Tiny = 0.7f * g_Config.m_TcTinyTeeSize / 100.0f;
		BaseSize *= Tiny;
		AnimScale *= Tiny;
	}
	const vec2 BodyPos = Pos + vec2(pAnim->GetBody()->m_X, pAnim->GetBody()->m_Y) * AnimScale;
	const float EyeW = BaseSize * 0.40f * BodyScale.x;
	const float EyeH = BaseSize * 0.40f * BodyScale.y;
	const float Separation = (0.075f - 0.010f * absolute(Dir.x)) * BaseSize * BodyScale.x;
	const vec2 Offset = vec2(Dir.x * 0.125f * BodyScale.x, (-0.05f + Dir.y * 0.10f) * BodyScale.y) * BaseSize;
	// the second eye (drawn mirrored), like Kaneki's left one
	const vec2 Center = BodyPos + vec2(Separation + Offset.x, Offset.y);
	const float Mirror = -1.0f;
	const float Rotation = pAnim->GetBody()->m_Angle * pi * 2.0f + BodyAngle;
	const vec2 AxisX = direction(Rotation), AxisY = vec2(-AxisX.y, AxisX.x);
	// a point of the skin's 32 unit eye cell in the world
	const auto CellPoint = [&](float u, float v) { return Center + AxisX * ((u / 32.0f - 0.5f) * EyeW * Mirror) + AxisY * ((v / 32.0f - 0.5f) * EyeH); };

	const BcGhoul::CEyeLook Look = BcGhoul::EyeLook(E.m_Eye, E.m_Time);
	const vec2 IrisAt = CellPoint(Iris.m_X, Iris.m_Y);
	const auto Cell = [](int c) { return CcFx::CUv{BcGhoul::CellU0(c), 0.0f, BcGhoul::CellU1(c), 1.0f}; };
	// Right points to u1: mirrored like the skin's second eye
	const vec2 Right = AxisX * Mirror;

	CcFx::Begin(Graphics(), m_Texture, false);
	if(Look.m_VeinAlpha > 0.01f)
	{
		const float r = EyeW * 0.8f * Look.m_VeinScale;
		CcFx::Quad(Graphics(), IrisAt, Right * r, -AxisY * r, Cell(BcGhoul::CELL_VEINS), ColorRGBA(1.0f, 1.0f, 1.0f, Look.m_VeinAlpha * Alpha));
	}
	{
		// the black spreads out of the iris point
		const float s = Look.m_ScleraScale;
		const vec2 At = IrisAt + (Center - IrisAt) * s;
		CcFx::Quad(Graphics(), At, Right * (EyeW * 0.5f * s), -AxisY * (EyeH * 0.5f * s), Cell(Sclera), ColorRGBA(1.0f, 1.0f, 1.0f, Look.m_ScleraAlpha * Alpha));
	}
	if(Look.m_IrisScale > 0.01f)
	{
		const float Rx = Iris.m_Rx / 32.0f * EyeW / BcGhoul::IRIS_R * Look.m_IrisScale;
		const float Ry = Iris.m_Ry / 32.0f * EyeH / BcGhoul::IRIS_R * Look.m_IrisScale;
		CcFx::Quad(Graphics(), IrisAt, Right * Rx, -AxisY * Ry, Cell(BcGhoul::CELL_IRIS), ColorRGBA(1.0f, 1.0f, 1.0f, Look.m_IrisAlpha * Alpha));
	}
	CcFx::End(Graphics());
	if(Look.m_GlowAlpha > 0.01f)
	{
		CcFx::Begin(Graphics(), m_Texture, true);
		const float g = EyeW * 0.75f;
		CcFx::Quad(Graphics(), IrisAt, AxisX * g, -AxisY * g, Cell(BcGhoul::CELL_GLOW), ColorRGBA(1.0f, 0.08f, 0.06f, std::min(1.0f, Look.m_GlowAlpha) * Alpha));
		CcFx::End(Graphics());
	}
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}
