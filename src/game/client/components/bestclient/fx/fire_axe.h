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
   - Animations (bc_fire_axe_anim*, 08.11), all around the grip on top of the hammer's own motion: a wind-up and a hard
     swing on every hit with a golden trail behind the blade, a kick back when it hits a tee, a little sway and a glint
     over the blade, a spin in the hand when you take the hammer and now and then while you wait.
   - The elegant strike (bc_fire_axe_anim_slash): a thin golden crescent sweeps in front of the tee on every swing and
     a cross of light flashes where the blade hits a tee (the shapes are in fire_axe_slash.h). Three kinds of strike
     (bc_fire_axe_anim_attack): diagonal, a wide horizontal sweep and a thrust with the axe head, mixed in a combo.
   - Own sounds (bc_fire_axe_sound*): a soft swing and a hit, a heavier pair for the head bash; data/chainsaw/sounds/axe_*.
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
	void PreviewSwing(); // animations (bc_fire_axe_anim*): a swing with a hit
	void PreviewSpin(); // animations: a spin in the hand
	// CSounds::PlayAt for SOUND_HAMMER_FIRE / SOUND_HAMMER_HIT: true = a tee with the axe made it, the axe sound
	// plays instead (bc_fire_axe_sound_replace) and the hammer sound must not
	bool ReplaceHammerSound(int SoundId, vec2 Pos);

private:
	static constexpr int MAX_FLAMES = 384;
	static constexpr int MAX_TRAIL = 12; // samples of the golden trail behind the blade

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
		// animations (bc_fire_axe_anim*), seconds since they started (large = not playing)
		float m_SwingT = 100.0f;
		float m_ImpactT = 100.0f;
		float m_SpinT = 100.0f;
		float m_WaitT = 0.0f; // the hammer in hand without swings: an idle spin now and then
		float m_GlintT = 0.0f;
		vec2 m_aTrailHead[MAX_TRAIL];
		vec2 m_aTrailEdge[MAX_TRAIL];
		float m_aTrailAge[MAX_TRAIL];
		int m_TrailNum = 0;
		float m_SlashSide = 1.0f; // every strike sweeps the other way
		// the kind of the strike (BcAxeSlash::KIND_*, bc_fire_axe_anim_attack) and where it goes
		int m_AttackKind = 0;
		int m_Combo = 0;
		float m_LastAttack = -10.0f;
		float m_StrikeAim = 0.0f;
		// the axe sounds: not twice within a moment
		float m_LastSwingSound = -10.0f;
		float m_LastHitSound = -10.0f;
	};

	// the elegant strike (bc_fire_axe_anim_slash, fire_axe_slash.h)
	static constexpr int MAX_SLASHES = 32;
	static constexpr int MAX_CUTS = 32;
	struct CSlash
	{
		int m_ClientId;
		int m_Kind;
		vec2 m_Pos; // follows the tee
		float m_Aim;
		float m_Side;
		float m_Radius;
		float m_T;
		float m_GlitterAcc;
	};
	struct CCut
	{
		int m_Kind;
		vec2 m_Pos;
		float m_Angle;
		float m_Size;
		float m_T;
	};

	// the axe sounds (bc_fire_axe_sound*): a few takes of each, never the same one twice in a row
	enum
	{
		SOUND_SWING = 0,
		SOUND_HIT,
		SOUND_BASH_SWING,
		SOUND_BASH_HIT,
		NUM_SOUND_SETS
	};
	static constexpr int MAX_SOUND_TAKES = 4;
	int m_aaSounds[NUM_SOUND_SETS][MAX_SOUND_TAKES];
	int m_aNumSounds[NUM_SOUND_SETS] = {};
	int m_aLastTake[NUM_SOUND_SETS] = {};

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
	CSlash m_aSlashes[MAX_SLASHES];
	int m_NumSlashes = 0;
	CCut m_aCuts[MAX_CUTS];
	int m_NumCuts = 0;
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
	// animations: extra turn around the grip (radians, + = the way the hammer swings), size and glint of this frame
	bool AnimOn() const;
	float AnimSpeed() const;
	void AnimPose(int ClientId, float &Turn, float &Scale, float &Glint) const;
	void UpdateAnim(int ClientId, float Dt);
	void DrawTrail(const CTee &Tee, float Alpha);
	// the elegant strike
	bool SlashOn() const;
	vec2 StrikeDir(int ClientId) const;
	void AddSlash(int ClientId);
	void AddCut(int ClientId, vec2 Pos, float Angle);
	int PickAttack(CTee &Tee) const;
	void StartAttack(int ClientId);
	float SwingTime(int Kind) const;
	// the axe sounds
	void LoadSounds();
	bool SoundFor(int ClientId) const;
	void PlayAxeSound(int ClientId, int Set, vec2 Pos);
	void UpdateSlashes(float Dt);
	void DrawSlashes(const CcFx::CView &View);
};

#endif
