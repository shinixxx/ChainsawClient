/* ChainsawClient: burning hook chain, burn marks on hit tees, fire flash on hammer strikes */
#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_FIRE_CHAIN_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_FIRE_CHAIN_H

#include <base/color.h>
#include <base/vmath.h>

#include <engine/graphics.h>
#include <engine/shared/protocol.h>

#include <game/client/component.h>

#include "cc_fx_common.h"
#include "fgf_glow.h"

// The chain itself is drawn by CPlayers::RenderHook through RenderChain (under the tees, like the normal chain);
// flames, burn marks and strike flashes are drawn in OnRender, after the players. Fixed pools, no allocations.
// Everything uses the shared CcFx atlas: the rope with its head is 5 batches (was 7), the front part 4 (was 6).
class CBcFireChainFx : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnReset() override;
	void OnRender() override;

	// true when the burning chain was drawn instead of the normal one
	bool RenderChain(int ClientId, vec2 TeePos, vec2 HookPos, float Alpha);

	// menu preview
	void PreviewStrike(vec2 Pos);
	void StrikeShake(int ClientId, bool Hit); // chainsaw: bc_fire_strike_shake

private:
	static constexpr int MAX_FLAMES = 640;
	static constexpr int MAX_BURNS = 48;
	static constexpr int MAX_FLASHES = 16;
	static constexpr int BURNS_PER_TEE = 3;
	static constexpr int MAX_ROPE_SEGMENTS = 160;

	struct CPalette
	{
		ColorRGBA m_Edge;
		ColorRGBA m_Dark;
		ColorRGBA m_Body;
		ColorRGBA m_Core;
		ColorRGBA m_Glow;
		ColorRGBA m_HotIron; // chain links
		ColorRGBA m_DarkIron;
	};

	struct CTeeState
	{
		bool m_Seen = false;
		int m_AttackTick = -1;
		int m_LastHooked = -1;
		float m_ChainLastTime = -1.0f;
		float m_ChainFlameAcc = 0.0f;
		float m_HeadFlameAcc = 0.0f;
		float m_EmberAcc = 0.0f;
	};

	struct CFlame
	{
		vec2 m_Pos;
		vec2 m_Vel;
		float m_Life;
		float m_LifeSpan;
		float m_Size;
		float m_Phase;
		float m_Tone;
		float m_Rise; // 1 = points up like a candle, 0 = points where it flies
		float m_Drag;
		int m_Variant;
		bool m_Flip;
		bool m_Mega = false; // colored like the mega strike
	};

	struct CBurn
	{
		int m_Victim;
		vec2 m_Offset; // from the victim's center, the mark sits on its body
		float m_Life;
		float m_LifeSpan;
		float m_Size;
		float m_Phase;
		float m_EmberAcc;
		float m_Rot;
	};

	struct CFlash
	{
		vec2 m_Pos;
		float m_Life;
		float m_LifeSpan;
		float m_Size;
		bool m_Hit;
		bool m_Mega = false;
	};

	// one tongue as drawn this frame: the three layers and the glow share it
	struct CTongueQuad
	{
		vec2 m_Center;
		vec2 m_Right;
		vec2 m_Up;
		vec2 m_GlowAt;
		float m_GlowSize;
		ColorRGBA m_aColors[3];
		ColorRGBA m_Glow;
		int m_Variant;
		bool m_Flip;
		bool m_HasGlow;
	};

	const CcFx::CAtlas *m_pAtlas = nullptr;
	CTeeState m_aTees[MAX_CLIENTS];
	CFlame m_aFlames[MAX_FLAMES];
	int m_NumFlames = 0;
	CBurn m_aBurns[MAX_BURNS];
	int m_NumBurns = 0;
	CFlash m_aFlashes[MAX_FLASHES];
	int m_NumFlashes = 0;
	CTongueQuad m_aTongueQuads[MAX_FLAMES + MAX_BURNS];
	CcFx::CSparks m_Embers;
	CPalette m_Colors; // refreshed every frame, so the many sparks of a strike don't convert colors each
	CPalette m_MegaColors;
	float m_ColorsTime = -1.0f; // LocalTime of the last refresh
	float m_LastTime = 0.0f;
	float m_Time = 0.0f;
	float m_Quality = 1.0f;

	static CPalette Palette();
	static CPalette MegaPalette(); // bc_fire_strike_color: deep orange, a hot white core
	void RefreshColors();
	bool ChainFor(int ClientId) const;
	bool StrikeFor(int ClientId) const;
	float TeeAlpha(int ClientId) const;
	vec2 AimDir(int ClientId) const;

	void EmitFlame(vec2 Pos, vec2 Vel, float Size, float LifeSpan, float Rise, float Drag, bool Mega = false);
	void EmitEmber(vec2 Pos, vec2 Vel, float Size, float LifeSpan, float Gravity, bool Mega = false);
	// bc_fire_strike_mega: a huge orange explosion, fire flies all around
	void MegaBurst(vec2 Pos, vec2 Dir, bool Hit, float Scale);
	void AddBurn(int Victim, vec2 From);
	void Strike(int ClientId);
	void FireBurst(vec2 Pos, vec2 Dir, bool Hit, float Scale);

	void UpdateTee(int ClientId);
	void UpdateParticles(float Passed);
	// bc_fire_chain_style 1: two twisted burning strands with a white hot core from the head to the tee, then the
	// hook head as a harpoon of fire; 4 batches in the same order as before
	void DrawRopeAndHead(int ClientId, vec2 HookPos, vec2 Dir, float Distance, float Alpha, float Now, const CPalette &Colors);
	void Draw();
};

#endif
