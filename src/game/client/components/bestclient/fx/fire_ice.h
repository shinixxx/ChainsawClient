/* ChainsawClient: fire ring aura (Fire Force style), flaming hammer, landing shockwave, freeze ice */
#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_FIRE_ICE_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_FIRE_ICE_H

#include <base/color.h>
#include <base/vmath.h>

#include <engine/graphics.h>
#include <engine/shared/protocol.h>

#include <game/client/component.h>

#include "cc_fx_common.h"
#include "fgf_glow.h"

// Front part: updates everything and draws hammer fire, sparks, petals, shockwaves and ice.
// Fixed-size pools only, nothing is allocated while playing. All masks live in the shared CcFx atlas, so the ring
// draws in 3 batches and the front part in 3, and every tongue's corners are computed once per frame.
class CBcFireIceFx : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnReset() override;
	void OnRender() override;

	// called by CPlayers right after the hammer sprite is drawn
	void OnHammerDrawn(int ClientId, vec2 WeaponPos, float Rotation, float Scale);
	// called by CBcFireIceFxBack (rendered before the players)
	void RenderBack();

	// menu preview buttons
	void PreviewIce(vec2 Pos);
	void PreviewShockwave(vec2 Pos);

	static constexpr int NUM_FLAME_VARIANTS = 8; // atlas columns: 0..3 hammer, 4..7 ring
	static constexpr int NUM_HAMMER_VARIANTS = 4;
	static constexpr int RING_FIRST_VARIANT = 4;
	static constexpr int NUM_FLAME_LAYERS = 3;

private:
	static constexpr int MAX_AURA_FLAMES = 768;
	static constexpr int MAX_FLAMES = 512;
	static constexpr int MAX_PETALS = 256;
	static constexpr int MAX_SHARDS = 256;
	static constexpr int MAX_WAVES = 24;

	struct CPalette
	{
		ColorRGBA m_Edge; // outer flame layer (no dark rim: same soft yellow family)
		ColorRGBA m_Dark; // some tongues are a little deeper yellow
		ColorRGBA m_Gold; // ring flames: the same golden yellow as the glowing ring
		ColorRGBA m_PetalBody; // petals keep the warm golden colors
		ColorRGBA m_PetalEdge;
		ColorRGBA m_Body; // middle flame layer
		ColorRGBA m_Core; // hot core, almost white
		ColorRGBA m_Glow; // additive light
	};

	struct CTeeState
	{
		bool m_Seen = false;
		vec2 m_LastPos = vec2(0.0f, 0.0f);
		bool m_WasGrounded = true;
		float m_LastVelY = 0.0f;
		float m_LandCooldown = 0.0f;
		bool m_WasFrozen = false;
		float m_IceCooldown = 0.0f;
		float m_Frost = 0.0f;
		float m_GlintAcc = 0.0f;
		int m_AttackTick = -1;
		float m_Aura = 0.0f;
		float m_AuraFlameAcc = 0.0f;
		float m_AuraPetalAcc = 0.0f;
		float m_HammerFlameAcc = 0.0f;
		float m_HammerSeen = 1.0f;
		vec2 m_HammerHead = vec2(0.0f, 0.0f);
		vec2 m_LastHammerHead = vec2(0.0f, 0.0f);
		float m_HitFlare = 0.0f;
		bool m_Visible = false;
		bool m_Frozen = false;
		float m_Alpha = 1.0f; // TeeAlpha of the last update
	};

	// ring flames live in tee-local coordinates so the ring never smears
	struct CAuraFlame
	{
		int m_Owner;
		float m_Angle;
		float m_Life;
		float m_LifeSpan;
		float m_Size;
		float m_Phase;
		float m_Lean;
		float m_Tone;
		int m_Variant;
		bool m_Flip;
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
		int m_Variant;
		bool m_Flip;
	};

	struct CPetal
	{
		vec2 m_Pos;
		vec2 m_Vel;
		float m_Rot;
		float m_RotSpeed;
		float m_Life;
		float m_LifeSpan;
		float m_Size;
		float m_Heat;
	};

	struct CShard
	{
		vec2 m_Pos;
		vec2 m_Vel;
		float m_Rot;
		float m_RotSpeed;
		float m_Life;
		float m_LifeSpan;
		float m_Length;
		float m_Width;
		float m_Whiten;
	};

	struct CWave
	{
		vec2 m_Pos;
		float m_Life;
		float m_LifeSpan;
		float m_Radius;
		float m_Flat; // 1 = full circle, < 1 = squashed ground ring
		float m_Alpha;
		ColorRGBA m_Color;
	};

	// one tongue as drawn this frame: the three layers and the glow share it
	struct CTongueQuad
	{
		vec2 m_Center;
		vec2 m_Right;
		vec2 m_Up;
		vec2 m_GlowAt;
		float m_GlowSize;
		float m_Alpha;
		float m_GlowAlpha;
		ColorRGBA m_Layer0;
		int m_Variant;
		bool m_Flip;
	};

	const CcFx::CAtlas *m_pAtlas = nullptr;
	CTongueQuad m_aTongueQuads[MAX_AURA_FLAMES > MAX_FLAMES ? MAX_AURA_FLAMES : MAX_FLAMES];
	CcFx::CView m_View; // this frame, 200 units of margin
	bool m_TeesCleared = false;

	CTeeState m_aTees[MAX_CLIENTS];
	CAuraFlame m_aAuraFlames[MAX_AURA_FLAMES];
	int m_NumAuraFlames = 0;
	CFlame m_aFlames[MAX_FLAMES];
	int m_NumFlames = 0;
	CPetal m_aPetals[MAX_PETALS];
	int m_NumPetals = 0;
	CShard m_aShards[MAX_SHARDS];
	int m_NumShards = 0;
	CWave m_aWaves[MAX_WAVES];
	int m_NumWaves = 0;
	CcFx::CSparks m_Embers; // additive: sparks, ice glints
	CcFx::CSparks m_Dust; // normal blend: landing dust
	float m_LastTime = 0.0f;
	float m_Time = 0.0f;
	float m_Quality = 1.0f;

	static CPalette FirePalette();
	static ColorRGBA IceColor();

	bool AuraFor(int ClientId) const;
	bool HammerFor(int ClientId) const;
	bool ShockFor(int ClientId) const;
	bool IceFor(int ClientId) const;
	float TeeAlpha(int ClientId) const;
	bool IsGrounded(vec2 Pos) const;
	bool IsFrozen(int ClientId) const;
	vec2 TeeVel(int ClientId) const;
	vec2 AimDir(int ClientId) const;

	void EmitFlame(vec2 Pos, vec2 Vel, float Size, float LifeSpan);
	void EmitPetal(vec2 Pos, vec2 Vel, float Size, float LifeSpan);
	void EmitEmber(vec2 Pos, vec2 Vel, float Size, float LifeSpan, ColorRGBA From, ColorRGBA To);
	void AddWave(vec2 Pos, float Radius, float LifeSpan, float Flat, float Alpha, ColorRGBA Color);

	void Shockwave(int ClientId, vec2 Feet, float Power);
	void HammerSwing(int ClientId);
	void IceBurst(vec2 Pos, float Amount, bool Shatter);

	void UpdateTee(int ClientId, float Passed);
	void UpdateParticles(float Passed);

	void DrawFront();
};

class CBcFireIceFxBack : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
};

#endif
