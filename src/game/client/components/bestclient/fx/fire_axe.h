/* ChainsawClient: golden axe instead of the hammer whenever the flaming hammer is on (bc_fire_hammer), no own switch.
   The picture is drawn by code (fire_axe_art.h, reference Рабочий стол\axe.jpg).
   - CPlayers calls BeforeHammer() right before the hammer sprite: the hammer fades out while the axe takes its place.
   - CBcFireIceFx::OnHammerDrawn() calls OnHammerDrawn() right after it: the axe is drawn exactly there (still before
     the tee body, like the hammer): glow, body, white heat — 2 or 3 draw calls per axe.
   - Turning the flaming hammer on with the hammer in hand calls the axe out of fire and light: sparks and tongues spiral
     into the hand, a light grows, the axe burns in from the head outwards, white hot, then cools to gold with a flash.
     Turning it off burns it away again. bc_fire_axe_summon_switch: the same when you take the hammer.
   - Every hit on a tee heats the axe up (bc_fire_axe_heat): it glows brighter and brighter and burns with fire, the
     heat stays bc_fire_axe_heat_time and then cools down.
   OnRender (after the players) draws the sparks, the tongues and the light of all axes in one batch. */
#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_FIRE_AXE_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_FIRE_AXE_H

#include <base/color.h>
#include <base/vmath.h>

#include <engine/graphics.h>
#include <engine/shared/protocol.h>

#include <game/client/component.h>

#include "cc_fx_common.h"

class CBcFireAxeFx : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnReset() override;
	void OnRender() override;

	// CPlayers, right before the hammer sprite (the rotation is already set): fades the hammer out
	void BeforeHammer(int ClientId, bool Ghost, bool Flip, float Alpha);
	// CBcFireIceFx::OnHammerDrawn, right after the hammer sprite: draws the axe there
	void OnHammerDrawn(int ClientId, vec2 WeaponPos, float Rotation, float Scale);

	// menu buttons, on our own tee
	void PreviewSummon();
	void PreviewHit();

private:
	static constexpr int MAX_FLAMES = 384;

	struct CTee
	{
		bool m_Init = false;
		float m_Show = 0.0f; // 0 = hammer, 1 = axe
		bool m_Burst = true; // the flash at the end of the summon is done
		int m_Weapon = -1;
		int m_AttackTick = -1;
		// the last BeforeHammer
		bool m_Flip = false;
		float m_Alpha = 1.0f;
		bool m_Armed = false; // BeforeHammer came for this sprite
		// the last drawn axe
		float m_DrawTime = -10.0f;
		vec2 m_Center = vec2(0.0f, 0.0f);
		vec2 m_AxisX = vec2(1.0f, 0.0f);
		vec2 m_AxisY = vec2(0.0f, 1.0f);
		float m_Size = 112.0f;
		// heat from hits
		float m_Heat = 0.0f;
		float m_HeatHold = 0.0f;
		float m_Flash = 0.0f;
		float m_FlameAcc = 0.0f;
		float m_SparkAcc = 0.0f;
		bool m_NoWait = false; // the menu test button: play the summon with the menu open
	};

	struct CFlame
	{
		vec2 m_Pos;
		vec2 m_Vel;
		float m_Life;
		float m_LifeSpan;
		float m_Size;
		float m_Hot; // 0 = golden, 1 = white
		int m_Variant;
		bool m_Flip;
	};

	IGraphics::CTextureHandle m_Texture;
	CcFx::CUv m_BodyUv;
	CcFx::CUv m_GlowUv;
	const CcFx::CAtlas *m_pAtlas = nullptr;
	CTee m_aTees[MAX_CLIENTS];
	CFlame m_aFlames[MAX_FLAMES];
	int m_NumFlames = 0;
	CcFx::CSparks m_Sparks;
	float m_LastTime = -1.0f;
	bool m_WasOn = false;

	bool On() const;
	bool AxeFor(int ClientId) const;
	float SummonTime() const;
	void EnsureTexture();
	vec2 ToWorld(const CTee &Tee, float Px, float Py) const; // cell pixels to the world
	vec2 HeadPos(const CTee &Tee) const;
	ColorRGBA FireColor(float Lightness) const;

	void EmitFlame(vec2 Pos, vec2 Vel, float Size, float LifeSpan, float Hot);
	void EmitSpark(vec2 Pos, vec2 Vel, float Size, float LifeSpan, ColorRGBA From, ColorRGBA To, float Drag);
	void Burst(int ClientId, float Power);
	void CheckHit(int ClientId);
	void UpdateTee(int ClientId, float Dt, bool MenuOpen);
	void UpdateFlames(float Dt);
};

#endif
