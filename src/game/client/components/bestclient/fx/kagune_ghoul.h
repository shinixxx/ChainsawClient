/* ChainsawClient: the ghoul eye and the kagune coming out (part of bc_kagune, no own settings), see kagune_ghoul_core.h.
   - CBcKaguneFx::RenderBack calls Emerge() right after it posed a kagune: the red gathers behind the tee, the
     tentacles grow out of it, the eye opens (all into the kagune's own particles and batches);
   - CPlayers calls OnTeeDrawn() right after a tee is drawn: the kakugan is drawn over one of its eyes
     (veins, eye, iris: 1 draw call; the glow: 1 more), only for tees whose kagune is out. */
#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_KAGUNE_GHOUL_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_KAGUNE_GHOUL_H

#include <base/vmath.h>

#include <engine/graphics.h>
#include <engine/shared/protocol.h>

#include <game/client/component.h>

#include "kagune_ghoul_core.h"

class CAnimState;
class CTeeRenderInfo;

class CBcKaguneGhoulFx : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;

	// CBcKaguneFx, right after BcKagune::Update for this tee
	void Emerge(int ClientId, BcKagune::CTee &Kagune, const BcKagune::CInput &In, BcKagune::CParticles &Particles);
	// CPlayers, right after the tee is drawn (the same values RenderTee got)
	void OnTeeDrawn(int ClientId, bool Ghost, bool Local, const CAnimState *pAnim, const CTeeRenderInfo *pInfo, int Emote, vec2 Dir, vec2 Pos, float Alpha, vec2 BodyScale, float BodyAngle);

private:
	BcGhoul::CEmerge m_aEmerge[MAX_CLIENTS];
	float m_aSeen[MAX_CLIENTS] = {}; // LocalTime of the last Emerge
	IGraphics::CTextureHandle m_Texture;

	void EnsureTexture();
};

#endif
