/* ChainsawClient: kagune (Tokyo Ghoul, Kaneki's rinkaku) instead of the hammer (bc_kagune).
   CPlayers calls OnHammer() where it would draw the hammer: it skips the hammer sprite and remembers the tee's
   aim, attack and motion. CBcKaguneFxBack (rendered right before CPlayers) then moves and draws the kagune of
   every tee behind all tees in 3 draw calls, at the tee's current position; the sparks of the stabs are drawn in
   front of the tees by OnRender (1 draw call). The simulation and the mesh live in kagune_core.h.
   bc_kagune_gun keeps the kagune out with the pistol (read from the snapshot here, the pistol is drawn as usual),
   bc_kagune_blood: a strike that touches another tee makes it bleed (drops fly, stick to the map, the wound drips). */
#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_KAGUNE_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_KAGUNE_H

#include <base/color.h>
#include <base/vmath.h>

#include <engine/graphics.h>
#include <engine/shared/protocol.h>

#include <game/client/component.h>

#include "kagune_core.h"

class CBcKaguneFx : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnReset() override;
	void OnRender() override;

	// CPlayers, instead of drawing the hammer: true = the kagune replaces it (the hammer is not drawn).
	// Vel in units per tick, AttackTime in seconds since the attack.
	bool OnHammer(int ClientId, bool Ghost, vec2 Pos, vec2 Direction, vec2 Vel, bool InAir, bool Inactive, int AttackTick, float AttackTime, float Alpha, float Size);
	// CBcKaguneFxBack, before the players
	void RenderBack();

private:
	struct CRecord
	{
		float m_Time = -1.0f; // LocalTime of the last OnHammer
		vec2 m_Aim = vec2(1.0f, 0.0f);
		vec2 m_Vel = vec2(0.0f, 0.0f); // units per second
		bool m_InAir = false;
		bool m_Resting = false;
		int m_AttackTick = -1;
		float m_AttackTime = 100.0f;
		float m_Alpha = 1.0f;
		float m_Size = 64.0f;
	};

	BcKagune::CTee m_aTees[MAX_CLIENTS];
	CRecord m_aRecords[MAX_CLIENTS];
	bool m_aDrawn[MAX_CLIENTS] = {};
	BcKagune::CParticles m_Particles;
	BcKagune::CPalette m_Palette;
	IGraphics::CTextureHandle m_Texture;
	int m_TextureColor = -1; // bc_kagune_color the texture was baked with
	int m_TextureStyle = -1; // bc_kagune_style the texture was baked with
	float m_aBleed[MAX_CLIENTS] = {}; // seconds the wound of a tee still drips
	vec2 m_aBleedDir[MAX_CLIENTS];
	float m_aBleedAcc[MAX_CLIENTS] = {};
	float m_LastTime = -1.0f;
	bool m_Any = false;

	bool ShowFor(int ClientId) const;
	bool GunRecord(int ClientId, CRecord &Rec);
	bool HookRecord(int ClientId, CRecord &Rec); // chainsaw: the kagune is the hook too
	void CheckWounds(int Owner, float Scale);
	void UpdateBleeding(float Dt);
	void UpdateTexture();
	BcKagune::CParams Params(int ClientId) const;
};

class CBcKaguneFxBack : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
};

#endif
