/* ChainsawClient: fire ring aura (Fire Force style), flaming hammer, landing shockwave, freeze ice.
   2026-11-05: same look, drawn with the shared CcFx atlas: the ring in 3 batches (was 8), the front part in 3 (was
   up to 10), every tongue's geometry computed once per frame instead of once per layer, off screen particles skipped. */
#include "fire_ice.h"

#include "cc_fx_common.h"
#include "fire_ice_shapes.h"

#include <base/math.h>

#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>

#include "own_tee.h"

#include <algorithm>
#include <cmath>

static constexpr float TEE_HALF = 14.0f; // half of the physical tee size (28)
static constexpr float HAMMER_HEAD = 24.0f; // hammer head offset from the sprite center
static constexpr float AURA_RADIUS = 54.0f;
static constexpr int NUM_BEADS = 28; // default of bc_fire_aura_beads (chainsaw)

static float Smooth(float a, float b, float x)
{
	const float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

void CBcFireIceFx::OnInit()
{
	m_pAtlas = &CcFx::Atlas(Graphics());
	OnReset();
}

void CBcFireIceFx::OnReset()
{
	for(CTeeState &Tee : m_aTees)
		Tee = CTeeState();
	m_NumAuraFlames = 0;
	m_NumFlames = 0;
	m_NumPetals = 0;
	m_NumShards = 0;
	m_NumWaves = 0;
	m_Embers.Clear();
	m_Dust.Clear();
}

CBcFireIceFx::CPalette CBcFireIceFx::FirePalette()
{
	const ColorHSLA Hsl(g_Config.m_BcFireAuraColor);
	const auto Shift = [&](float Hue, float Lightness) {
		ColorHSLA Out = Hsl;
		Out.h = std::fmod(Out.h + Hue + 1.0f, 1.0f);
		Out.s = std::max(Out.s, 0.85f);
		Out.l = Lightness;
		Out.a = 1.0f;
		return color_cast<ColorRGBA>(Out);
	};
	// soft yellow fire without a dark rim, white in the middle, some tongues a bit deeper
	CPalette Palette;
	Palette.m_Edge = Shift(0.08f, 0.7f);
	Palette.m_Dark = Shift(0.06f, 0.6f);
	Palette.m_Gold = Shift(0.07f, 0.6f);
	Palette.m_Body = Shift(0.09f, 0.83f);
	Palette.m_Core = ColorRGBA(1.0f, 0.99f, 0.94f, 1.0f);
	Palette.m_Glow = Shift(0.06f, 0.55f);
	Palette.m_PetalBody = Shift(0.065f, 0.55f);
	Palette.m_PetalEdge = Shift(-0.01f, 0.48f);
	return Palette;
}

ColorRGBA CBcFireIceFx::IceColor()
{
	return color_cast<ColorRGBA>(ColorHSLA(g_Config.m_BcFreezeIceColor));
}

bool CBcFireIceFx::AuraFor(int ClientId) const
{
	return g_Config.m_BcFireAura && (g_Config.m_BcFireAuraOthers || BcIsOwnTee(GameClient(), ClientId));
}

bool CBcFireIceFx::HammerFor(int ClientId) const
{
	return g_Config.m_BcFireHammer && (g_Config.m_BcFireHammerOthers || BcIsOwnTee(GameClient(), ClientId));
}

bool CBcFireIceFx::ShockFor(int ClientId) const
{
	return g_Config.m_BcLandShockwave && (g_Config.m_BcLandShockwaveOthers || BcIsOwnTee(GameClient(), ClientId));
}

bool CBcFireIceFx::IceFor(int ClientId) const
{
	return g_Config.m_BcFreezeIce && (g_Config.m_BcFreezeIceOthers || BcIsOwnTee(GameClient(), ClientId));
}

float CBcFireIceFx::TeeAlpha(int ClientId) const
{
	if(GameClient()->IsOtherTeam(ClientId))
		return std::clamp(g_Config.m_ClShowOthersAlpha / 100.0f, 0.0f, 1.0f);
	return 1.0f;
}

bool CBcFireIceFx::IsGrounded(vec2 Pos) const
{
	return Collision()->CheckPoint(vec2(Pos.x + TEE_HALF, Pos.y + TEE_HALF + 5.0f)) ||
	       Collision()->CheckPoint(vec2(Pos.x - TEE_HALF, Pos.y + TEE_HALF + 5.0f));
}

bool CBcFireIceFx::IsFrozen(int ClientId) const
{
	const CGameClient::CClientData &Client = GameClient()->m_aClients[ClientId];
	if(BcIsOwnTee(GameClient(), ClientId) && GameClient()->Predict())
		return Client.m_Predicted.m_FreezeEnd != 0 || Client.m_Predicted.m_LiveFrozen;
	return Client.m_FreezeEnd != 0 || Client.m_DeepFrozen || Client.m_LiveFrozen;
}

vec2 CBcFireIceFx::TeeVel(int ClientId) const
{
	const CGameClient::CClientData &Client = GameClient()->m_aClients[ClientId];
	if(BcIsOwnTee(GameClient(), ClientId) && GameClient()->Predict())
		return Client.m_Predicted.m_Vel;
	return vec2(Client.m_RenderCur.m_VelX, Client.m_RenderCur.m_VelY) / 256.0f;
}

vec2 CBcFireIceFx::AimDir(int ClientId) const
{
	for(int Dummy = 0; Dummy < NUM_DUMMIES; Dummy++)
	{
		if(GameClient()->m_aLocalIds[Dummy] != ClientId)
			continue;
		const vec2 Mouse = GameClient()->m_Controls.m_aMousePos[Dummy];
		if(length(Mouse) > 0.001f)
			return normalize(Mouse);
	}
	return direction(GameClient()->m_aClients[ClientId].m_RenderCur.m_Angle / 256.0f);
}

void CBcFireIceFx::EmitFlame(vec2 Pos, vec2 Vel, float Size, float LifeSpan)
{
	if(m_NumFlames >= MAX_FLAMES)
		return;
	CFlame &Flame = m_aFlames[m_NumFlames++];
	Flame.m_Pos = Pos;
	Flame.m_Vel = Vel;
	Flame.m_Life = 0.0f;
	Flame.m_LifeSpan = LifeSpan;
	Flame.m_Size = Size;
	Flame.m_Phase = random_float(0.0f, 2.0f * pi);
	Flame.m_Variant = (int)random_float(0.0f, NUM_HAMMER_VARIANTS - 0.01f);
	Flame.m_Flip = random_float(0.0f, 1.0f) < 0.5f;
	Flame.m_Tone = random_float(0.0f, 1.0f);
}

void CBcFireIceFx::EmitPetal(vec2 Pos, vec2 Vel, float Size, float LifeSpan)
{
	if(m_NumPetals >= MAX_PETALS)
		return;
	CPetal &Petal = m_aPetals[m_NumPetals++];
	Petal.m_Pos = Pos;
	Petal.m_Vel = Vel;
	Petal.m_Rot = random_float(0.0f, 2.0f * pi);
	Petal.m_RotSpeed = random_float(-5.0f, 5.0f);
	Petal.m_Life = 0.0f;
	Petal.m_LifeSpan = LifeSpan;
	Petal.m_Size = Size;
	Petal.m_Heat = random_float(0.0f, 1.0f);
}

void CBcFireIceFx::EmitEmber(vec2 Pos, vec2 Vel, float Size, float LifeSpan, ColorRGBA From, ColorRGBA To)
{
	CcFx::CSparks::CSpark *pSpark = m_Embers.NewSpark();
	if(!pSpark)
		return;
	pSpark->m_Pos = Pos;
	pSpark->m_Vel = Vel;
	pSpark->SetDrag(0.25f);
	pSpark->m_Gravity = 60.0f;
	pSpark->m_Stretch = 0.025f;
	pSpark->m_LifeSpan = LifeSpan;
	pSpark->m_StartSize = Size;
	pSpark->m_EndSize = Size * 0.3f;
	pSpark->m_StartAlpha = 1.0f;
	pSpark->m_EndAlpha = 0.0f;
	pSpark->m_Color = From;
	pSpark->m_ShiftColor = true;
	pSpark->m_EndColor = To;
}

void CBcFireIceFx::AddWave(vec2 Pos, float Radius, float LifeSpan, float Flat, float Alpha, ColorRGBA Color)
{
	if(m_NumWaves >= MAX_WAVES)
		return;
	m_aWaves[m_NumWaves++] = {Pos, 0.0f, LifeSpan, Radius, Flat, Alpha, Color};
}

void CBcFireIceFx::Shockwave(int ClientId, vec2 Feet, float Power)
{
	const float Strength = Power * g_Config.m_BcLandShockwavePower / 100.0f;
	const bool Fire = AuraFor(ClientId);
	const CPalette Palette = FirePalette();
	const float Alpha = TeeAlpha(ClientId);
	// chainsaw: bc_land_shockwave_fire / bc_land_shockwave_color
	const ColorRGBA WaveColor = Fire && g_Config.m_BcLandShockwaveFire ? Palette.m_Body : color_cast<ColorRGBA>(ColorHSLA(g_Config.m_BcLandShockwaveColor)).WithAlpha(1.0f);

	AddWave(Feet, (46.0f + 64.0f * Power) * std::sqrt(Strength), 0.42f, 0.3f, 0.9f * Alpha, WaveColor);
	AddWave(Feet - vec2(0.0f, 6.0f), (26.0f + 34.0f * Power) * std::sqrt(Strength), 0.28f, 1.0f, 0.35f * Alpha, WaveColor);

	const int NumDust = (int)((6.0f + 14.0f * Power) * std::min(Strength, 2.0f) * m_Quality * g_Config.m_BcLandShockwaveDust / 100.0f); // chainsaw: bc_land_shockwave_dust
	for(int i = 0; i < NumDust; i++)
	{
		CcFx::CSparks::CSpark *pSpark = m_Dust.NewSpark();
		if(!pSpark)
			break;
		const float Side = (i % 2 == 0) ? 1.0f : -1.0f;
		pSpark->m_Pos = Feet + vec2(Side * random_float(0.0f, 10.0f), random_float(-4.0f, 0.0f));
		pSpark->m_Vel = vec2(Side * random_float(110.0f, 330.0f) * std::sqrt(Strength), -random_float(15.0f, 95.0f));
		pSpark->SetDrag(0.04f);
		pSpark->m_Gravity = 90.0f;
		pSpark->m_LifeSpan = random_float(0.45f, 0.8f);
		pSpark->m_StartSize = random_float(7.0f, 11.0f);
		pSpark->m_EndSize = random_float(20.0f, 32.0f);
		pSpark->m_StartAlpha = 0.38f * Alpha;
		pSpark->m_EndAlpha = 0.0f;
		const float Grey = random_float(0.55f, 0.7f);
		pSpark->m_Color = ColorRGBA(Grey, Grey * 0.93f, Grey * 0.82f, 1.0f);
	}
	if(Fire)
	{
		const int NumPetals = (int)(10.0f * Power * m_Quality) + 3;
		for(int i = 0; i < NumPetals; i++)
			EmitPetal(Feet, vec2(random_float(-1.0f, 1.0f) * 260.0f, -random_float(80.0f, 240.0f)), random_float(7.0f, 11.0f), random_float(0.5f, 0.9f));
	}
}

void CBcFireIceFx::HammerSwing(int ClientId)
{
	const vec2 Pos = GameClient()->m_aClients[ClientId].m_RenderPos;
	const vec2 Dir = AimDir(ClientId);
	const vec2 Start = Pos + Dir * (TEE_HALF * 2.0f * 0.75f);

	// the server hits every tee whose center is within ProximityRadius * 0.5 + ProximityRadius of the start
	bool Hit = false;
	vec2 Target = Start;
	float Best = TEE_HALF * 3.0f;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i == ClientId || !GameClient()->m_Snap.m_aCharacters[i].m_Active)
			continue;
		const vec2 Other = GameClient()->m_aClients[i].m_RenderPos;
		const float Dist = distance(Other, Start);
		if(Dist < Best)
		{
			Best = Dist;
			Hit = true;
			Target = Other;
		}
	}
	const vec2 Contact = Hit ? mix(Start, Target, 0.5f) : Start + Dir * 8.0f;
	const float Alpha = TeeAlpha(ClientId);

	if(HammerFor(ClientId))
	{
		const CPalette Palette = FirePalette();
		const float Sparks = g_Config.m_BcFireHammerSparks / 100.0f * m_Quality;
		const float Base = angle(Dir);
		const int NumSparks = (int)((Hit ? 22.0f : 8.0f) * Sparks);
		for(int i = 0; i < NumSparks; i++)
		{
			const float Spread = Hit ? 1.1f : 0.6f;
			const vec2 Vel = direction(Base + random_float(-Spread, Spread)) * random_float(180.0f, Hit ? 560.0f : 320.0f) + vec2(0.0f, -60.0f);
			EmitEmber(Contact, Vel, random_float(3.0f, 5.0f), random_float(0.25f, 0.55f), Palette.m_Core, Palette.m_Edge);
		}
		const int NumPetals = (int)((Hit ? 12.0f : 4.0f) * Sparks);
		for(int i = 0; i < NumPetals; i++)
			EmitPetal(Contact, direction(Base + random_float(-1.0f, 1.0f)) * random_float(120.0f, 320.0f) + vec2(0.0f, -80.0f), random_float(7.0f, 11.0f), random_float(0.5f, 0.9f));
		const int NumFlames = Hit ? 9 : 4;
		for(int i = 0; i < NumFlames; i++)
			EmitFlame(Contact + vec2(random_float(-8.0f, 8.0f), random_float(-8.0f, 8.0f)),
				Dir * random_float(60.0f, 160.0f) + vec2(random_float(-40.0f, 40.0f), -random_float(60.0f, 120.0f)),
				random_float(12.0f, Hit ? 22.0f : 16.0f), random_float(0.3f, 0.45f));
		if(Hit)
		{
			m_aTees[ClientId].m_HitFlare = 1.0f;
			AddWave(Contact, 42.0f, 0.28f, 1.0f, 0.95f * Alpha, Palette.m_Core);
		}
	}
	if(Hit && ShockFor(ClientId))
	{
		const float Strength = std::sqrt(g_Config.m_BcLandShockwavePower / 100.0f);
		const ColorRGBA HitWave = FgfGlow::Whiten(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_BcLandShockwaveColor)).WithAlpha(1.0f), 0.5f); // chainsaw: bc_land_shockwave_color
		AddWave(Contact, 72.0f * Strength, 0.34f, 1.0f, 0.55f * Alpha, HitWave);
		AddWave(Contact, 40.0f * Strength, 0.22f, 1.0f, 0.4f * Alpha, HitWave);
	}
}

void CBcFireIceFx::IceBurst(vec2 Pos, float Amount, bool Shatter)
{
	const ColorRGBA Ice = IceColor();
	const int NumShards = (int)((Shatter ? 9.0f : 22.0f) * Amount * g_Config.m_BcFreezeIceAmount / 100.0f * m_Quality);
	for(int i = 0; i < NumShards && m_NumShards < MAX_SHARDS; i++)
	{
		CShard &Shard = m_aShards[m_NumShards++];
		const vec2 Out = direction(random_float(0.0f, 2.0f * pi));
		Shard.m_Pos = Pos + Out * random_float(4.0f, 14.0f);
		Shard.m_Vel = Out * (Shatter ? random_float(80.0f, 230.0f) : random_float(170.0f, 430.0f)) + vec2(0.0f, -random_float(60.0f, 160.0f));
		Shard.m_Rot = angle(Shard.m_Vel);
		Shard.m_RotSpeed = random_float(-14.0f, 14.0f);
		Shard.m_Life = 0.0f;
		Shard.m_LifeSpan = random_float(0.6f, 1.15f);
		Shard.m_Length = random_float(7.0f, 14.0f) * (Shatter ? 0.75f : 1.0f);
		Shard.m_Width = random_float(3.0f, 5.5f);
		Shard.m_Whiten = random_float(0.0f, 0.65f);
	}
	if(!Shatter)
	{
		AddWave(Pos, 66.0f, 0.38f, 1.0f, 0.85f, Ice);
		AddWave(Pos, 38.0f, 0.25f, 1.0f, 0.6f, FgfGlow::Whiten(Ice, 0.6f));
	}
	const int NumGlints = (int)((Shatter ? 5.0f : 12.0f) * m_Quality * g_Config.m_BcFreezeIceGlints / 100.0f); // chainsaw: bc_freeze_ice_glints
	for(int i = 0; i < NumGlints; i++)
		EmitEmber(Pos + direction(random_float(0.0f, 2.0f * pi)) * random_float(6.0f, 22.0f),
			direction(random_float(0.0f, 2.0f * pi)) * random_float(30.0f, 120.0f),
			random_float(3.0f, 6.0f), random_float(0.35f, 0.7f), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), Ice);
}

void CBcFireIceFx::OnHammerDrawn(int ClientId, vec2 WeaponPos, float Rotation, float Scale)
{
	GameClient()->m_BcFireAxeFx.OnHammerDrawn(ClientId, WeaponPos, Rotation, Scale); // chainsaw: the golden axe
	if(!in_range(ClientId, MAX_CLIENTS - 1) || !HammerFor(ClientId))
		return;
	CTeeState &Tee = m_aTees[ClientId];
	Tee.m_HammerHead = WeaponPos + vec2(std::cos(Rotation), std::sin(Rotation)) * HAMMER_HEAD * Scale;
	Tee.m_HammerSeen = 0.0f;
}

void CBcFireIceFx::PreviewIce(vec2 Pos)
{
	IceBurst(Pos, 1.0f, false);
	const int Local = GameClient()->m_Snap.m_LocalClientId;
	if(in_range(Local, MAX_CLIENTS - 1))
		m_aTees[Local].m_Frost = 1.0f;
}

void CBcFireIceFx::PreviewShockwave(vec2 Pos)
{
	const int Local = GameClient()->m_Snap.m_LocalClientId;
	Shockwave(in_range(Local, MAX_CLIENTS - 1) ? Local : 0, Pos + vec2(0.0f, TEE_HALF), 1.0f);
}

void CBcFireIceFx::UpdateTee(int ClientId, float Passed)
{
	CTeeState &Tee = m_aTees[ClientId];
	if(!GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
	{
		if(Tee.m_Seen)
			Tee = CTeeState();
		return;
	}

	const CGameClient::CClientData &Client = GameClient()->m_aClients[ClientId];
	const vec2 Pos = Client.m_RenderPos;
	const bool Own = BcIsOwnTee(GameClient(), ClientId);
	CCharacter *pPredicted = Own && GameClient()->Predict() ? GameClient()->m_PredictedWorld.GetCharacterById(ClientId) : nullptr;
	const CGameClient::CSnapState::CCharacterInfo &Snap = GameClient()->m_Snap.m_aCharacters[ClientId];
	const int AttackTick = pPredicted ? pPredicted->GetAttackTick() : Snap.m_Cur.m_AttackTick;
	const int Weapon = pPredicted ? pPredicted->GetActiveWeapon() : Snap.m_Cur.m_Weapon;

	if(!Tee.m_Seen || distance(Pos, Tee.m_LastPos) > 320.0f)
	{
		// spawn or teleport: start clean, no fake landing / freeze bursts
		Tee = CTeeState();
		Tee.m_Seen = true;
		Tee.m_LastPos = Pos;
		Tee.m_WasGrounded = IsGrounded(Pos);
		Tee.m_WasFrozen = IsFrozen(ClientId);
		Tee.m_AttackTick = AttackTick;
		Tee.m_Alpha = TeeAlpha(ClientId);
		return;
	}

	Tee.m_Visible = m_View.Sees(Pos, 0.0f) && GameClient()->OptimizerAllowRenderPos(Pos);
	Tee.m_Alpha = TeeAlpha(ClientId);

	Tee.m_LandCooldown = std::max(0.0f, Tee.m_LandCooldown - Passed);
	Tee.m_IceCooldown = std::max(0.0f, Tee.m_IceCooldown - Passed);
	Tee.m_HitFlare = std::max(0.0f, Tee.m_HitFlare - Passed / 0.35f);
	Tee.m_HammerSeen += Passed;

	// landing
	const bool Grounded = IsGrounded(Pos);
	const vec2 Vel = TeeVel(ClientId);
	if(ShockFor(ClientId) && Tee.m_Visible && !Tee.m_WasGrounded && Grounded && Tee.m_LandCooldown <= 0.0f)
	{
		// chainsaw: the fastest fall of the flight (the predicted speed is already 0 when the drawn tee touches the ground);
		// bc_land_shockwave_sens 70 = a normal jump gives a small puff, falls from higher up a full wave
		const float Impact = std::max(Tee.m_LastVelY, Vel.y);
		const float Threshold = 18.0f - 0.08f * g_Config.m_BcLandShockwaveSens;
		if(Impact > Threshold)
		{
			Shockwave(ClientId, Pos + vec2(0.0f, TEE_HALF), std::clamp(0.15f + (Impact - Threshold) / 16.0f, 0.0f, 1.0f));
			Tee.m_LandCooldown = 0.15f;
		}
	}
	Tee.m_LastVelY = Grounded ? 0.0f : std::max(Tee.m_LastVelY, Vel.y); // chainsaw: keep the fastest fall
	Tee.m_WasGrounded = Grounded;

	// freeze
	const bool Frozen = IsFrozen(ClientId);
	Tee.m_Frozen = Frozen;
	if(IceFor(ClientId))
	{
		if(Frozen && !Tee.m_WasFrozen && Tee.m_IceCooldown <= 0.0f && Tee.m_Visible)
		{
			IceBurst(Pos, 1.0f, false);
			Tee.m_Frost = 1.0f;
			Tee.m_IceCooldown = 0.5f;
		}
		else if(!Frozen && Tee.m_WasFrozen && Tee.m_Visible)
			IceBurst(Pos, 1.0f, true);

		if(Frozen)
		{
			Tee.m_Frost = std::max(0.45f, Tee.m_Frost - Passed / 1.0f);
			Tee.m_GlintAcc += Passed * 5.0f * m_Quality * g_Config.m_BcFreezeIceGlints / 100.0f; // chainsaw: bc_freeze_ice_glints
			const ColorRGBA Ice = IceColor();
			while(Tee.m_GlintAcc >= 1.0f && Tee.m_Visible)
			{
				Tee.m_GlintAcc -= 1.0f;
				EmitEmber(Pos + direction(random_float(0.0f, 2.0f * pi)) * random_float(8.0f, 24.0f), vec2(0.0f, random_float(-25.0f, -5.0f)),
					random_float(2.5f, 4.5f), random_float(0.4f, 0.7f), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), Ice);
			}
		}
		else
			Tee.m_Frost = std::max(0.0f, Tee.m_Frost - Passed / 0.6f);
	}
	else
		Tee.m_Frost = 0.0f;
	Tee.m_WasFrozen = Frozen;

	// hammer swing / hit
	if(Tee.m_AttackTick >= 0 && AttackTick != Tee.m_AttackTick && Weapon == WEAPON_HAMMER && Tee.m_Visible && (HammerFor(ClientId) || ShockFor(ClientId)))
		HammerSwing(ClientId);
	Tee.m_AttackTick = AttackTick;

	// fire ring: flames grow mostly from the top and the sides, like the reference art
	const float AuraTarget = AuraFor(ClientId) && Tee.m_Visible ? (Frozen ? 0.3f : 1.0f) : 0.0f;
	if(Tee.m_Aura < AuraTarget)
		Tee.m_Aura = std::min(AuraTarget, Tee.m_Aura + Passed / 0.35f);
	else
		Tee.m_Aura = std::max(AuraTarget, Tee.m_Aura - Passed / 0.35f);
	if(Tee.m_Aura > 0.01f && Tee.m_Visible)
	{
		const float Size = g_Config.m_BcFireAuraSize / 100.0f;
		const float Flames = g_Config.m_BcFireAuraFlames / 100.0f;
		Tee.m_AuraFlameAcc += Passed * 30.0f * Flames * m_Quality * Tee.m_Aura;
		while(Tee.m_AuraFlameAcc >= 1.0f && m_NumAuraFlames < MAX_AURA_FLAMES)
		{
			Tee.m_AuraFlameAcc -= 1.0f;
			// at 100% the fire grows mostly from the top; the higher the percent, the more evenly it covers the whole ring
			const float Even = std::clamp((Flames - 1.0f) / 1.5f, 0.0f, 1.0f);
			for(int Try = 0; Try < 8; Try++)
			{
				const float Angle = random_float(0.0f, 2.0f * pi);
				const float TopRaw = std::clamp((-std::sin(Angle) + 0.35f) / 1.35f, 0.0f, 1.0f);
				const float Chance = mix(0.65f + 0.35f * std::pow(TopRaw, 1.3f), 1.0f, Even);
				if(random_float(0.0f, 1.0f) >= Chance)
					continue;
				const float Top = mix(TopRaw, 0.6f, Even);
				CAuraFlame &Flame = m_aAuraFlames[m_NumAuraFlames++];
				Flame.m_Owner = ClientId;
				Flame.m_Angle = Angle;
				Flame.m_Life = 0.0f;
				Flame.m_LifeSpan = random_float(0.9f, 1.8f);
				Flame.m_Size = (14.0f + 5.0f * Top) * random_float(0.85f, 1.15f) * Size;
				Flame.m_Phase = random_float(0.0f, 2.0f * pi);
				Flame.m_Lean = random_float(-0.25f, 0.25f);
				Flame.m_Tone = random_float(0.0f, 1.0f);
				Flame.m_Variant = RING_FIRST_VARIANT + (int)random_float(0.0f, 3.99f);
				Flame.m_Flip = g_Config.m_BcFireAuraSpin == 0 ? random_float(0.0f, 1.0f) < 0.5f : false; // fin tips sweep back against the spin
				break;
			}
		}
		Tee.m_AuraFlameAcc = std::min(Tee.m_AuraFlameAcc, 1.0f);
		Tee.m_AuraPetalAcc += Passed * 7.0f * Flames * m_Quality * Tee.m_Aura;
		while(Tee.m_AuraPetalAcc >= 1.0f)
		{
			Tee.m_AuraPetalAcc -= 1.0f;
			const vec2 Out = direction(random_float(pi * 1.15f, pi * 1.85f));
			EmitPetal(Pos + Out * AURA_RADIUS * Size, Out * 30.0f + vec2(random_float(-20.0f, 20.0f), -random_float(40.0f, 110.0f)),
				random_float(8.0f, 12.0f) * std::sqrt(Size), random_float(0.8f, 1.4f));
		}
	}

	// flaming hammer: tongues stream off the head and trail behind fast swings
	if(HammerFor(ClientId) && Tee.m_HammerSeen < 0.1f && Tee.m_Visible)
	{
		const vec2 Swing = (Tee.m_HammerHead - Tee.m_LastHammerHead) / std::max(Passed, 0.001f);
		Tee.m_HammerFlameAcc += Passed * 38.0f * m_Quality * g_Config.m_BcFireHammerFlames / 100.0f; // chainsaw: bc_fire_hammer_flames
		while(Tee.m_HammerFlameAcc >= 1.0f)
		{
			Tee.m_HammerFlameAcc -= 1.0f;
			EmitFlame(Tee.m_HammerHead + vec2(random_float(-6.0f, 6.0f), random_float(-6.0f, 6.0f)),
				vec2(random_float(-25.0f, 25.0f), -random_float(60.0f, 120.0f)) - Swing * 0.15f, random_float(9.0f, 13.0f), random_float(0.28f, 0.45f));
		}
		if(random_float(0.0f, 1.0f) < Passed * 5.0f * m_Quality)
			EmitPetal(Tee.m_HammerHead, vec2(random_float(-40.0f, 40.0f), -random_float(70.0f, 140.0f)), random_float(6.0f, 9.0f), random_float(0.6f, 1.0f));
	}
	Tee.m_LastHammerHead = Tee.m_HammerHead;
	Tee.m_LastPos = Pos;
}

void CBcFireIceFx::UpdateParticles(float Passed)
{
	for(int i = 0; i < m_NumAuraFlames;)
	{
		CAuraFlame &Flame = m_aAuraFlames[i];
		Flame.m_Life += Passed;
		if(Flame.m_Life >= Flame.m_LifeSpan || !m_aTees[Flame.m_Owner].m_Seen)
		{
			Flame = m_aAuraFlames[--m_NumAuraFlames];
			continue;
		}
		Flame.m_Angle += 0.6f * g_Config.m_BcFireAuraSpin / 100.0f * Passed; // the fins turn together with the ring beads
		i++;
	}

	const float FlameDrag = std::pow(0.3f, Passed);
	for(int i = 0; i < m_NumFlames;)
	{
		CFlame &Flame = m_aFlames[i];
		Flame.m_Life += Passed;
		if(Flame.m_Life >= Flame.m_LifeSpan)
		{
			Flame = m_aFlames[--m_NumFlames];
			continue;
		}
		Flame.m_Vel.y -= 150.0f * Passed;
		Flame.m_Vel *= FlameDrag;
		Flame.m_Pos += Flame.m_Vel * Passed;
		i++;
	}

	const float PetalDrag = std::pow(0.5f, Passed);
	for(int i = 0; i < m_NumPetals;)
	{
		CPetal &Petal = m_aPetals[i];
		Petal.m_Life += Passed;
		if(Petal.m_Life >= Petal.m_LifeSpan)
		{
			Petal = m_aPetals[--m_NumPetals];
			continue;
		}
		Petal.m_Vel.y -= 20.0f * Passed;
		Petal.m_Vel.x += std::sin(m_Time * 3.0f + Petal.m_Rot) * 40.0f * Passed;
		Petal.m_Vel *= PetalDrag;
		Petal.m_Pos += Petal.m_Vel * Passed;
		Petal.m_Rot += Petal.m_RotSpeed * Passed;
		i++;
	}

	const float ShardDrag = std::pow(0.6f, Passed);
	for(int i = 0; i < m_NumShards;)
	{
		CShard &Shard = m_aShards[i];
		Shard.m_Life += Passed;
		if(Shard.m_Life >= Shard.m_LifeSpan)
		{
			Shard = m_aShards[--m_NumShards];
			continue;
		}
		Shard.m_Vel.y += 900.0f * Passed;
		Shard.m_Vel *= ShardDrag;
		Shard.m_Rot += Shard.m_RotSpeed * Passed;
		const vec2 NewPos = Shard.m_Pos + Shard.m_Vel * Passed;
		if(Collision()->CheckPoint(NewPos))
		{
			if(Collision()->CheckPoint(vec2(NewPos.x, Shard.m_Pos.y)))
				Shard.m_Vel.x *= -0.45f;
			if(Collision()->CheckPoint(vec2(Shard.m_Pos.x, NewPos.y)))
				Shard.m_Vel.y *= -0.45f;
			Shard.m_RotSpeed *= 0.5f;
		}
		else
			Shard.m_Pos = NewPos;
		i++;
	}

	for(int i = 0; i < m_NumWaves;)
	{
		m_aWaves[i].m_Life += Passed;
		if(m_aWaves[i].m_Life >= m_aWaves[i].m_LifeSpan)
		{
			m_aWaves[i] = m_aWaves[--m_NumWaves];
			continue;
		}
		i++;
	}

	m_Embers.Update(Passed);
	m_Dust.Update(Passed);
}

void CBcFireIceFx::RenderBack()
{
	if(!g_Config.m_BcFireAura && m_NumAuraFlames == 0)
		return;
	if(BcFxSuppressed(GameClient()))
		return;

	const CPalette Palette = FirePalette();
	const float Size = g_Config.m_BcFireAuraSize / 100.0f;
	const float Radius = AURA_RADIUS * Size;
	const float Spin = 0.6f * g_Config.m_BcFireAuraSpin / 100.0f;
	const CcFx::CView View = CcFx::View(Graphics(), 10.0f);
	const float Glow = g_Config.m_BcFireAuraGlow / 100.0f; // chainsaw: bc_fire_aura_glow
	const int NumBeads = std::clamp(g_Config.m_BcFireAuraBeads, 0, 2 * NUM_BEADS); // chainsaw: bc_fire_aura_beads

	// the rings drawn this frame: the halo reaches 2.1 radii from the center
	bool aRing[MAX_CLIENTS];
	bool Any = false;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		const CTeeState &Tee = m_aTees[i];
		aRing[i] = Tee.m_Aura > 0.01f && Tee.m_Visible && View.Sees(GameClient()->m_aClients[i].m_RenderPos, Radius * 2.1f);
		Any |= aRing[i];
	}
	if(!Any)
		return;
	CcFx::CTimer Timer(CcFx::STAT_FIRE_RING);
	const CcFx::CAtlas &Atlas = *m_pAtlas;

	// soft light behind everything + wide halo of the ring
	CcFx::Begin(Graphics(), Atlas.m_Texture, true);
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!aRing[i])
			continue;
		const CTeeState &Tee = m_aTees[i];
		const vec2 Pos = GameClient()->m_aClients[i].m_RenderPos;
		const float Flicker = 0.9f + 0.07f * std::sin(m_Time * 9.0f + i) + 0.03f * std::sin(m_Time * 21.0f + i * 3);
		CcFx::QuadAxis(Graphics(), Pos, Radius * 4.2f, Radius * 4.2f, Atlas.m_Glow, Palette.m_Glow.WithAlpha(0.22f * Glow * Tee.m_Aura * Flicker * Tee.m_Alpha));
		const float Halo = 2.0f * Radius / CcFx::HALO_RADIUS;
		CcFx::QuadAxis(Graphics(), Pos, Halo, Halo, Atlas.m_RingHalo, Palette.m_Glow.WithAlpha(0.6f * Glow * Tee.m_Aura * Tee.m_Alpha));
	}
	CcFx::End(Graphics());

	// shark-fin flames sitting on the ring: they stay on the line, turn with it and burn stronger / weaker
	int NumFins = 0;
	for(int i = 0; i < m_NumAuraFlames; i++)
	{
		const CAuraFlame &Flame = m_aAuraFlames[i];
		if(!aRing[Flame.m_Owner])
			continue;
		const CTeeState &Tee = m_aTees[Flame.m_Owner];
		const vec2 Out = direction(Flame.m_Angle);
		const float Progress = Flame.m_Life / Flame.m_LifeSpan;
		const vec2 Dir = rotate(Out, 0.06f * std::sin(m_Time * 5.0f + Flame.m_Phase) * 180.0f / pi);
		const float Flicker = 0.75f + 0.2f * std::sin(m_Time * 9.0f + Flame.m_Phase) + 0.12f * std::sin(m_Time * 23.0f + Flame.m_Phase * 2.0f);
		const float FlameSize = Flame.m_Size * Smooth(0.0f, 0.25f, Progress) * (1.0f - Smooth(0.7f, 1.0f, Progress)) * Flicker;
		if(FlameSize < 1.0f)
			continue;
		const vec2 Base = GameClient()->m_aClients[Flame.m_Owner].m_RenderPos + Out * (Radius - 1.0f);
		CTongueQuad &Quad = m_aTongueQuads[NumFins++];
		CcFx::TongueQuad(Base, Dir, FlameSize, Quad.m_Center, Quad.m_Right, Quad.m_Up);
		Quad.m_GlowAt = Base + Dir * (FlameSize * 0.7f);
		Quad.m_GlowSize = FlameSize * 2.6f;
		const float Fade = std::clamp((1.0f - Progress) / 0.12f, 0.0f, 1.0f);
		Quad.m_Alpha = Fade * Tee.m_Aura * Tee.m_Alpha;
		Quad.m_GlowAlpha = 0.2f * Glow * Tee.m_Aura * Tee.m_Alpha;
		Quad.m_Variant = Flame.m_Variant;
		Quad.m_Flip = Flame.m_Flip;
	}
	// ring flames use exactly the ring colors: golden glow, light line, white core
	const ColorRGBA aLayerColors[NUM_FLAME_LAYERS] = {Palette.m_Gold, Palette.m_Body, Palette.m_Core};
	CcFx::Begin(Graphics(), Atlas.m_Texture, false);
	for(int Layer = 0; Layer < NUM_FLAME_LAYERS; Layer++)
	{
		for(int i = 0; i < NumFins; i++)
		{
			const CTongueQuad &Quad = m_aTongueQuads[i];
			const CcFx::CUv &Uv = Atlas.m_aaFlame[Quad.m_Variant][Layer];
			CcFx::Quad(Graphics(), Quad.m_Center, Quad.m_Right, Quad.m_Up, Quad.m_Flip ? Uv.Flipped() : Uv, aLayerColors[Layer].WithAlpha(Quad.m_Alpha));
		}
	}
	// a slightly darker edge on the inner side of the ring (in the same batch as the fins: the fin glows that
	// used to be drawn before it add a little light on it now, too faint to see, and save two draw calls)
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!aRing[i])
			continue;
		const CTeeState &Tee = m_aTees[i];
		const vec2 Pos = GameClient()->m_aClients[i].m_RenderPos;
		const float Inner = 2.0f * (Radius - 3.0f) / CcFx::LINE_RADIUS;
		CcFx::QuadAxis(Graphics(), Pos, Inner, Inner, Atlas.m_RingLine, ColorRGBA(0.42f, 0.22f, 0.04f, 0.45f * Tee.m_Aura * Tee.m_Alpha));
	}
	CcFx::End(Graphics());

	// light: soft glow around every tongue, the burning line of the ring on top of the fin roots, running light beads
	const float BeadSize = 7.0f * std::sqrt(Size);
	const float BeadStep = 2.0f * pi / std::max(1, NumBeads);
	const vec2 BeadTurn = direction(BeadStep); // one bead further around the ring
	const vec2 TwinkleTurn = direction(1.3f); // the next bead twinkles 1.3 radians later
	CcFx::Begin(Graphics(), Atlas.m_Texture, true);
	for(int i = 0; i < NumFins; i++)
	{
		const CTongueQuad &Quad = m_aTongueQuads[i];
		CcFx::QuadAxis(Graphics(), Quad.m_GlowAt, Quad.m_GlowSize, Quad.m_GlowSize, Atlas.m_Glow, Palette.m_Glow.WithAlpha(Quad.m_GlowAlpha));
	}
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!aRing[i])
			continue;
		const CTeeState &Tee = m_aTees[i];
		const vec2 Pos = GameClient()->m_aClients[i].m_RenderPos;
		const float Alpha = Tee.m_Aura * Tee.m_Alpha;
		const float Line = 2.0f * Radius / CcFx::LINE_RADIUS;
		CcFx::QuadAxis(Graphics(), Pos, Line, Line, Atlas.m_RingLine, Palette.m_Body.WithAlpha(0.9f * Alpha));
		CcFx::QuadAxis(Graphics(), Pos, Line, Line, Atlas.m_RingCore, Palette.m_Core.WithAlpha(Alpha));
		// angle and twinkle phase advance by fixed steps from bead to bead: turn unit vectors instead of calling sin/cos 56 times
		vec2 Around = direction(m_Time * Spin);
		vec2 Twinkle = direction(m_Time * 7.0f + i); // .y is sin(m_Time * 7 + Bead * 1.3 + i)
		for(int Bead = 0; Bead < NumBeads; Bead++)
		{
			const float Light = 0.6f + 0.4f * Twinkle.y;
			CcFx::QuadAxis(Graphics(), Pos + Around * Radius, BeadSize, BeadSize, Atlas.m_Glow, ColorRGBA(1.0f, 1.0f, 1.0f, std::min(1.0f, 1.3f * Light) * Alpha));
			Around = vec2(Around.x * BeadTurn.x - Around.y * BeadTurn.y, Around.x * BeadTurn.y + Around.y * BeadTurn.x);
			Twinkle = vec2(Twinkle.x * TwinkleTurn.x - Twinkle.y * TwinkleTurn.y, Twinkle.x * TwinkleTurn.y + Twinkle.y * TwinkleTurn.x);
		}
	}
	CcFx::End(Graphics());

	Graphics()->QuadsSetRotation(0.0f);
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void CBcFireIceFx::DrawFront()
{
	bool AnyGlow = false;
	for(const CTeeState &Tee : m_aTees)
		AnyGlow |= Tee.m_Visible && (Tee.m_HammerSeen < 0.1f || Tee.m_HitFlare > 0.0f || Tee.m_Frost > 0.01f);
	const bool AnyBehind = m_NumWaves > 0 || AnyGlow;
	const bool AnyBody = m_Dust.HasAny() || m_NumFlames > 0 || m_NumPetals > 0 || m_NumShards > 0;
	const bool AnyLight = m_NumFlames > 0 || m_NumPetals > 0 || m_Embers.HasAny();
	if(!AnyBehind && !AnyBody && !AnyLight)
		return;

	const CPalette Palette = FirePalette();
	const CcFx::CAtlas &Atlas = *m_pAtlas;
	const CcFx::CView View = CcFx::View(Graphics(), 10.0f);

	// 1 (additive): shockwaves, light around burning hammer heads, frost on frozen tees
	if(AnyBehind)
	{
		CcFx::Begin(Graphics(), Atlas.m_Texture, true);
		for(int i = 0; i < m_NumWaves; i++)
		{
			const CWave &Wave = m_aWaves[i];
			const float Progress = Wave.m_Life / Wave.m_LifeSpan;
			const float Spread = 1.0f - (1.0f - Progress) * (1.0f - Progress);
			const float Width = 2.0f * Wave.m_Radius * mix(0.25f, 1.0f, Spread) / CcFx::RING_BAND;
			if(!View.Sees(Wave.m_Pos, Width * 0.5f))
				continue;
			CcFx::QuadAxis(Graphics(), Wave.m_Pos, Width, Width * Wave.m_Flat, Atlas.m_Ring, Wave.m_Color.WithAlpha(Wave.m_Alpha * (1.0f - Progress)));
		}
		if(AnyGlow)
		{
			const ColorRGBA Ice = IceColor();
			for(int i = 0; i < MAX_CLIENTS; i++)
			{
				const CTeeState &Tee = m_aTees[i];
				if(!Tee.m_Visible)
					continue;
				const float Alpha = Tee.m_Alpha;
				if(Tee.m_HammerSeen < 0.1f && HammerFor(i))
				{
					const float HammerGlow = g_Config.m_BcFireHammerGlow / 100.0f; // chainsaw: bc_fire_hammer_glow
					const float Flicker = 0.85f + 0.1f * std::sin(m_Time * 12.0f + i) + 0.05f * std::sin(m_Time * 27.0f + i);
					const float GlowSize = 60.0f * (1.0f + 0.6f * Tee.m_HitFlare);
					CcFx::QuadAxis(Graphics(), Tee.m_HammerHead, GlowSize, GlowSize, Atlas.m_Glow, Palette.m_Glow.WithAlpha((0.45f + 0.4f * Tee.m_HitFlare) * HammerGlow * Flicker * Alpha));
					CcFx::QuadAxis(Graphics(), Tee.m_HammerHead, GlowSize * 0.35f, GlowSize * 0.35f, Atlas.m_Glow, Palette.m_Core.WithAlpha(0.35f * HammerGlow * Flicker * Alpha));
				}
				if(Tee.m_Frost > 0.01f)
				{
					const float Frost = g_Config.m_BcFreezeIceFrost / 100.0f; // chainsaw: bc_freeze_ice_frost
					const vec2 Pos = GameClient()->m_aClients[i].m_RenderPos;
					CcFx::QuadAxis(Graphics(), Pos, 92.0f, 92.0f, Atlas.m_Glow, Ice.WithAlpha(0.5f * Frost * Tee.m_Frost * Alpha));
					CcFx::QuadAxis(Graphics(), Pos, 58.0f, 58.0f, Atlas.m_Glow, ColorRGBA(1.0f, 1.0f, 1.0f, 0.28f * Frost * Tee.m_Frost * Alpha));
					// four twinkling frost stars on the body
					for(int Star = 0; Star < 4; Star++)
					{
						const float A = Star * pi * 0.5f + 0.6f + i;
						const vec2 At = Pos + direction(A) * 17.0f;
						const float Twinkle = 0.5f + 0.5f * std::sin(m_Time * 5.0f + Star * 1.7f + i);
						const float Len = 14.0f * (0.6f + 0.4f * Twinkle);
						const ColorRGBA Color(1.0f, 1.0f, 1.0f, std::min(1.0f, 0.7f * Frost) * Tee.m_Frost * Twinkle * Alpha);
						CcFx::QuadRotated(Graphics(), At, Len, 2.2f, m_Time * 0.8f + Star, Atlas.m_Glow, Color);
						CcFx::QuadRotated(Graphics(), At, Len, 2.2f, m_Time * 0.8f + Star + pi * 0.5f, Atlas.m_Glow, Color);
					}
				}
			}
		}
		CcFx::End(Graphics());
	}

	// hammer tongues: the geometry once, used by the three layers and the glow
	int NumTongues = 0;
	for(int i = 0; i < m_NumFlames; i++)
	{
		const CFlame &Flame = m_aFlames[i];
		const float Progress = Flame.m_Life / Flame.m_LifeSpan;
		const float FlameSize = Flame.m_Size * Smooth(0.0f, 0.2f, Progress) * (1.0f - 0.85f * Smooth(0.4f, 1.0f, Progress));
		if(FlameSize < 1.0f || !View.Sees(Flame.m_Pos, FlameSize * 2.0f))
			continue;
		vec2 Dir = normalize(vec2(Flame.m_Vel.x * 0.006f, -1.0f));
		Dir = rotate(Dir, 0.12f * std::sin(m_Time * 6.0f + Flame.m_Phase) * 180.0f / pi);
		CTongueQuad &Quad = m_aTongueQuads[NumTongues++];
		CcFx::TongueQuad(Flame.m_Pos, Dir, FlameSize, Quad.m_Center, Quad.m_Right, Quad.m_Up);
		Quad.m_GlowAt = Flame.m_Pos + Dir * (FlameSize * 0.7f);
		Quad.m_GlowSize = FlameSize * 2.6f;
		Quad.m_Alpha = std::clamp((1.0f - Progress) / 0.12f, 0.0f, 1.0f);
		Quad.m_GlowAlpha = 0.22f;
		Quad.m_Layer0 = FgfGlow::Blend(Palette.m_Edge, Palette.m_Dark, std::max(0.0f, Flame.m_Tone - 0.55f) / 0.45f);
		Quad.m_Variant = Flame.m_Variant;
		Quad.m_Flip = Flame.m_Flip;
	}

	// 2 (normal): landing dust, hammer tongues (same cel-shaded fire as the ring), petals, ice shards
	if(AnyBody)
	{
		CcFx::Begin(Graphics(), Atlas.m_Texture, false);
		if(m_Dust.HasAny())
			m_Dust.Draw(Graphics(), Atlas, View);
		const ColorRGBA aLayerColors[NUM_FLAME_LAYERS] = {Palette.m_Edge, Palette.m_Body, Palette.m_Core};
		for(int Layer = 0; Layer < NUM_FLAME_LAYERS; Layer++)
		{
			for(int i = 0; i < NumTongues; i++)
			{
				const CTongueQuad &Quad = m_aTongueQuads[i];
				const CcFx::CUv &Uv = Atlas.m_aaFlame[Quad.m_Variant][Layer];
				const ColorRGBA Color = Layer == 0 ? Quad.m_Layer0 : aLayerColors[Layer];
				CcFx::Quad(Graphics(), Quad.m_Center, Quad.m_Right, Quad.m_Up, Quad.m_Flip ? Uv.Flipped() : Uv, Color.WithAlpha(Quad.m_Alpha));
			}
		}
		for(int i = 0; i < m_NumPetals; i++)
		{
			const CPetal &Petal = m_aPetals[i];
			if(!View.Sees(Petal.m_Pos, Petal.m_Size))
				continue;
			const float Progress = Petal.m_Life / Petal.m_LifeSpan;
			const ColorRGBA Color = FgfGlow::Blend(Palette.m_PetalBody, Palette.m_PetalEdge, std::clamp(Progress * 1.2f - 0.2f * Petal.m_Heat, 0.0f, 1.0f));
			CcFx::QuadRotated(Graphics(), Petal.m_Pos, Petal.m_Size, Petal.m_Size, Petal.m_Rot, Atlas.m_Petal, Color.WithAlpha(std::min(1.0f, (1.0f - Progress) * 2.5f)));
		}
		if(m_NumShards > 0)
		{
			const ColorRGBA Ice = IceColor();
			for(int i = 0; i < m_NumShards; i++)
			{
				const CShard &Shard = m_aShards[i];
				if(!View.Sees(Shard.m_Pos, Shard.m_Length))
					continue;
				const float Progress = Shard.m_Life / Shard.m_LifeSpan;
				const float Fade = std::min(1.0f, (1.0f - Progress) * 3.0f);
				const vec2 Dir = direction(Shard.m_Rot);
				const vec2 Along = Dir * Shard.m_Length * 0.5f;
				const vec2 Across = vec2(-Dir.y, Dir.x) * Shard.m_Width * 0.5f;
				CcFx::Shape(Graphics(), Shard.m_Pos + Along, Shard.m_Pos + Across, Shard.m_Pos - Across, Shard.m_Pos - Along * 0.6f, FgfGlow::Whiten(Ice, Shard.m_Whiten).WithAlpha(0.9f * Fade));
			}
		}
		CcFx::End(Graphics());
	}

	// 3 (additive): light around the tongues and petals, sparks
	if(AnyLight)
	{
		CcFx::Begin(Graphics(), Atlas.m_Texture, true);
		for(int i = 0; i < NumTongues; i++)
		{
			const CTongueQuad &Quad = m_aTongueQuads[i];
			CcFx::QuadAxis(Graphics(), Quad.m_GlowAt, Quad.m_GlowSize, Quad.m_GlowSize, Atlas.m_Glow, Palette.m_Glow.WithAlpha(Quad.m_GlowAlpha));
		}
		for(int i = 0; i < m_NumPetals; i++)
		{
			const CPetal &Petal = m_aPetals[i];
			if(!View.Sees(Petal.m_Pos, Petal.m_Size))
				continue;
			const float Progress = Petal.m_Life / Petal.m_LifeSpan;
			CcFx::QuadAxis(Graphics(), Petal.m_Pos, Petal.m_Size * 2.0f, Petal.m_Size * 2.0f, Atlas.m_Glow, Palette.m_Glow.WithAlpha(0.4f * (1.0f - Progress)));
		}
		if(m_Embers.HasAny())
			m_Embers.Draw(Graphics(), Atlas, View);
		CcFx::End(Graphics());
	}
	Graphics()->QuadsSetRotation(0.0f);
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void CBcFireIceFx::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	const float Time = LocalTime();
	const float Passed = std::clamp(Time - m_LastTime, 0.0f, 0.1f) * GameClient()->GetAnimationPlaybackSpeed();
	m_LastTime = Time;
	m_Time += Passed;

	const bool Enabled = g_Config.m_BcFireAura || g_Config.m_BcFireHammer || g_Config.m_BcLandShockwave || g_Config.m_BcFreezeIce;
	const bool Alive = m_NumAuraFlames > 0 || m_NumFlames > 0 || m_NumPetals > 0 || m_NumShards > 0 || m_NumWaves > 0 || m_Embers.HasAny() || m_Dust.HasAny();
	if(!Enabled && !Alive)
	{
		// everything off: forget the tees once, not every frame
		if(!m_TeesCleared)
		{
			for(CTeeState &Tee : m_aTees)
				Tee = CTeeState();
			m_TeesCleared = true;
		}
		return;
	}
	m_TeesCleared = false;
	if(BcFxSuppressed(GameClient()))
	{
		OnReset();
		return;
	}
	CcFx::CTimer Timer(CcFx::STAT_FIRE_FRONT);

	// automatic level of detail: fewer particles when the frame rate drops
	const float FrameTime = Client()->RenderFrameTime();
	const float TargetQuality = FrameTime > 1.0f / 40.0f ? 0.4f : (FrameTime > 1.0f / 90.0f ? 0.7f : 1.0f);
	m_Quality = mix(m_Quality, TargetQuality, std::min(1.0f, Passed * 2.0f));

	m_View = CcFx::View(Graphics(), 200.0f);
	if(Enabled)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
			UpdateTee(i, Passed);
	}
	UpdateParticles(Passed);
	DrawFront();
}

void CBcFireIceFxBack::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	CcFx::StatsFrame();
	GameClient()->m_BcFireIceFx.RenderBack();
}
