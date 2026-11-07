/* ChainsawClient: burning hook chain, burn marks on hit tees, fire flash on hammer strikes.
   Same soft yellow cel fire as the fire ring (fire_ice_shapes.h tongues 0..3).
   2026-11-05: same look, drawn with the shared CcFx atlas in fewer batches; the rope's waves are computed once per
   point instead of once per band, the palettes once per frame instead of once per spark, off screen bits skipped. */
#include "fire_chain.h"

#include "cc_fx_common.h"
#include "fire_ice_shapes.h"

#include <base/math.h>

#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>

#include "own_tee.h"

#include <algorithm>
#include <cmath>

static constexpr float TEE_HALF = 14.0f;
static constexpr float LINK = 24.0f; // distance between two chain links, like the normal chain
static constexpr int FLAME_TONGUES = 4; // the first four tongues of the atlas
static constexpr int FLAME_LAYERS = BcFireShapes::LAYERS;

static float Smooth(float a, float b, float x)
{
	const float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

static vec2 Turned(vec2 v, vec2 Turn)
{
	return vec2(v.x * Turn.x - v.y * Turn.y, v.x * Turn.y + v.y * Turn.x);
}

void CBcFireChainFx::OnInit()
{
	m_pAtlas = &CcFx::Atlas(Graphics());
	RefreshColors();
	OnReset();
}

void CBcFireChainFx::OnReset()
{
	for(CTeeState &Tee : m_aTees)
		Tee = CTeeState();
	m_NumFlames = 0;
	m_NumBurns = 0;
	m_NumFlashes = 0;
	m_Embers.Clear();
}

CBcFireChainFx::CPalette CBcFireChainFx::Palette()
{
	// chainsaw: the fire is exactly the picked color (hue, saturation and lightness as picked): the outer flames are
	// that color, the inner ones go lighter to a hot core, the glow and the cold iron darker
	const ColorHSLA Hsl(g_Config.m_BcFireChainColor);
	const auto Light = [&](float Lightness) {
		ColorHSLA Out = Hsl;
		Out.l = std::clamp(Lightness, 0.0f, 1.0f);
		Out.a = 1.0f;
		return color_cast<ColorRGBA>(Out);
	};
	const float l = Hsl.l;
	CPalette Palette;
	Palette.m_Edge = Light(l);
	Palette.m_Dark = Light(l * 0.82f);
	Palette.m_Body = Light(l + (1.0f - l) * 0.45f);
	Palette.m_Core = Light(l + (1.0f - l) * 0.88f);
	Palette.m_Glow = Light(l * 0.8f);
	Palette.m_HotIron = Light(l + (1.0f - l) * 0.3f);
	Palette.m_DarkIron = Light(l * 0.45f);
	return Palette;
}

CBcFireChainFx::CPalette CBcFireChainFx::MegaPalette()
{
	const ColorHSLA Hsl(g_Config.m_BcFireStrikeColor);
	const auto Shift = [&](float Hue, float Lightness) {
		ColorHSLA Out = Hsl;
		Out.h = std::fmod(Out.h + Hue + 1.0f, 1.0f);
		Out.s = std::max(Out.s, 0.9f);
		Out.l = Lightness;
		Out.a = 1.0f;
		return color_cast<ColorRGBA>(Out);
	};
	// deep orange outside, yellow inside, a hot white core: the strike hits harder than the soft chain fire
	CPalette Palette;
	Palette.m_Edge = Shift(0.0f, 0.55f);
	Palette.m_Dark = Shift(-0.025f, 0.47f);
	Palette.m_Body = Shift(0.045f, 0.66f);
	Palette.m_Core = ColorRGBA(1.0f, 0.96f, 0.82f, 1.0f);
	Palette.m_Glow = Shift(0.0f, 0.5f);
	Palette.m_HotIron = Shift(0.06f, 0.75f);
	Palette.m_DarkIron = Shift(-0.02f, 0.4f);
	return Palette;
}

void CBcFireChainFx::RefreshColors()
{
	m_Colors = Palette();
	m_MegaColors = MegaPalette();
}

bool CBcFireChainFx::ChainFor(int ClientId) const
{
	return g_Config.m_BcFireChain && ClientId >= 0 && ClientId < MAX_CLIENTS && (g_Config.m_BcFireChainOthers || BcIsOwnTee(GameClient(), ClientId));
}

bool CBcFireChainFx::StrikeFor(int ClientId) const
{
	return g_Config.m_BcFireStrike && (g_Config.m_BcFireStrikeOthers || BcIsOwnTee(GameClient(), ClientId));
}

float CBcFireChainFx::TeeAlpha(int ClientId) const
{
	if(GameClient()->IsOtherTeam(ClientId))
		return std::clamp(g_Config.m_ClShowOthersAlpha / 100.0f, 0.0f, 1.0f);
	return 1.0f;
}

vec2 CBcFireChainFx::AimDir(int ClientId) const
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

void CBcFireChainFx::EmitFlame(vec2 Pos, vec2 Vel, float Size, float LifeSpan, float Rise, float Drag, bool Mega)
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
	Flame.m_Tone = random_float(0.0f, 1.0f);
	Flame.m_Rise = Rise;
	Flame.m_Drag = Drag;
	Flame.m_Variant = (int)random_float(0.0f, FLAME_TONGUES - 0.01f);
	Flame.m_Flip = random_float(0.0f, 1.0f) < 0.5f;
	Flame.m_Mega = Mega;
}

void CBcFireChainFx::EmitEmber(vec2 Pos, vec2 Vel, float Size, float LifeSpan, float Gravity, bool Mega)
{
	CcFx::CSparks::CSpark *pSpark = m_Embers.NewSpark();
	if(!pSpark)
		return;
	const CPalette &Colors = Mega ? m_MegaColors : m_Colors;
	pSpark->m_Pos = Pos;
	pSpark->m_Vel = Vel;
	pSpark->SetDrag(0.3f);
	pSpark->m_Gravity = Gravity;
	pSpark->m_Stretch = 0.025f;
	pSpark->m_LifeSpan = LifeSpan;
	pSpark->m_StartSize = Size;
	pSpark->m_EndSize = Size * 0.3f;
	pSpark->m_StartAlpha = 1.0f;
	pSpark->m_EndAlpha = 0.0f;
	pSpark->m_Color = Colors.m_Core;
	pSpark->m_ShiftColor = true;
	pSpark->m_EndColor = Colors.m_Edge;
}

void CBcFireChainFx::AddBurn(int Victim, vec2 From)
{
	if(!g_Config.m_BcFireBurn || Victim < 0 || Victim >= MAX_CLIENTS)
		return;
	const vec2 VictimPos = GameClient()->m_aClients[Victim].m_RenderPos;
	vec2 Side = From - VictimPos;
	Side = length(Side) > 0.001f ? normalize(Side) : vec2(0.0f, -1.0f);
	const vec2 Offset = rotate(Side, random_float(-25.0f, 25.0f)) * random_float(6.0f, 9.5f);

	// a few marks per tee: the oldest one makes room
	int Count = 0;
	int Oldest = -1;
	for(int i = 0; i < m_NumBurns; i++)
	{
		if(m_aBurns[i].m_Victim != Victim)
			continue;
		Count++;
		if(Oldest < 0 || m_aBurns[i].m_Life > m_aBurns[Oldest].m_Life)
			Oldest = i;
		// right next to a fresh one: that one flares up again instead
		if(distance(m_aBurns[i].m_Offset, Offset) < 4.0f && m_aBurns[i].m_Life < 1.0f)
		{
			m_aBurns[i].m_Life = 0.0f;
			return;
		}
	}
	CBurn *pBurn = nullptr;
	if(Count >= BURNS_PER_TEE && Oldest >= 0)
		pBurn = &m_aBurns[Oldest];
	else if(m_NumBurns < MAX_BURNS)
		pBurn = &m_aBurns[m_NumBurns++];
	if(!pBurn)
		return;
	pBurn->m_Victim = Victim;
	pBurn->m_Offset = Offset;
	pBurn->m_Life = 0.0f;
	pBurn->m_LifeSpan = g_Config.m_BcFireBurnTime / 10.0f;
	pBurn->m_Size = random_float(4.5f, 6.0f);
	pBurn->m_Phase = random_float(0.0f, 2.0f * pi);
	pBurn->m_EmberAcc = 0.0f;
	pBurn->m_Rot = random_float(0.0f, 2.0f * pi);
	// the moment it catches: a few sparks
	for(int i = 0; i < 5; i++)
		EmitEmber(VictimPos + Offset, direction(random_float(0.0f, 2.0f * pi)) * random_float(40.0f, 110.0f) + vec2(0.0f, -40.0f), random_float(2.5f, 3.5f), random_float(0.2f, 0.4f), 60.0f);
}

void CBcFireChainFx::MegaBurst(vec2 Pos, vec2 Dir, bool Hit, float Scale)
{
	const float Quality = std::max(0.5f, m_Quality);
	const float Power = Hit ? 1.0f : 0.55f; // a swing into the air is a smaller blast
	const float Base = angle(Dir);
	// the light: a huge orange glow, the blast wave rings
	if(m_NumFlashes < MAX_FLASHES)
		m_aFlashes[m_NumFlashes++] = {Pos, 0.0f, Hit ? 0.55f : 0.32f, (Hit ? 150.0f : 85.0f) * Scale, Hit, true};
	m_Embers.AddRing(Pos, 0.38f * (0.6f + 0.4f * Power), 16.0f * Scale, 230.0f * Scale * Power, 1.0f, m_MegaColors.m_Glow);
	m_Embers.AddRing(Pos, 0.24f, 10.0f * Scale, 140.0f * Scale * Power, 0.9f, m_MegaColors.m_Body);

	// fire bursts out to every side, more of it where the hammer pushes
	const int NumTongues = (int)(34.0f * Power * Quality);
	for(int i = 0; i < NumTongues; i++)
	{
		const bool Forward = i % 3 == 0;
		const float Angle = Forward ? Base + random_float(-0.6f, 0.6f) : Base + (i + random_float(-0.4f, 0.4f)) * 2.0f * pi / NumTongues;
		const float Speed = random_float(Forward ? 420.0f : 260.0f, Forward ? 820.0f : 560.0f) * Scale * (0.7f + 0.3f * Power);
		EmitFlame(Pos, direction(Angle) * Speed, random_float(16.0f, 32.0f) * Scale * (0.7f + 0.3f * Power), random_float(0.35f, 0.6f), 0.2f, 4.5f, true);
	}
	// a fireball rolls up from the hit
	const int NumBall = (int)(14.0f * Power * Quality);
	for(int i = 0; i < NumBall; i++)
	{
		const vec2 At = Pos + direction(random_float(0.0f, 2.0f * pi)) * random_float(0.0f, 18.0f) * Scale;
		EmitFlame(At, vec2(random_float(-60.0f, 60.0f), -random_float(40.0f, 140.0f)) * Scale, random_float(22.0f, 38.0f) * Scale * (0.6f + 0.4f * Power),
			random_float(0.45f, 0.75f), 1.0f, 2.0f, true);
	}
	// sparks and burning bits fly far and fall
	const int NumSparks = (int)(60.0f * Power * m_Quality);
	for(int i = 0; i < NumSparks; i++)
	{
		const float Angle = i % 2 ? Base + random_float(-1.0f, 1.0f) : random_float(0.0f, 2.0f * pi);
		EmitEmber(Pos, direction(Angle) * random_float(250.0f, 900.0f) * Scale + vec2(0.0f, -80.0f), random_float(3.0f, 5.5f), random_float(0.4f, 0.9f), 260.0f, true);
	}
	const int NumBits = (int)(12.0f * Power * m_Quality);
	for(int i = 0; i < NumBits; i++)
		EmitEmber(Pos, direction(random_float(0.0f, 2.0f * pi)) * random_float(150.0f, 450.0f) * Scale + vec2(0.0f, -180.0f), random_float(6.0f, 9.0f), random_float(0.7f, 1.2f), 420.0f, true);
}

void CBcFireChainFx::FireBurst(vec2 Pos, vec2 Dir, bool Hit, float Scale)
{
	if(g_Config.m_BcFireStrikeMega && (Hit || !g_Config.m_BcFireStrikeMegaHitsOnly)) // chainsaw: on every swing, or only when the hammer hits a tee
	{
		MegaBurst(Pos, Dir, Hit, Scale);
		return;
	}
	if(m_NumFlashes < MAX_FLASHES)
		m_aFlashes[m_NumFlashes++] = {Pos, 0.0f, Hit ? 0.24f : 0.16f, (Hit ? 64.0f : 38.0f) * Scale, Hit};
	// tongues burst out of the hit like petals of a flower, bending up as they burn out
	const int NumTongues = (int)((Hit ? 11.0f : 5.0f) * std::max(0.5f, m_Quality));
	const float Base = angle(Dir);
	for(int i = 0; i < NumTongues; i++)
	{
		const float Angle = Hit ? Base + (i + random_float(-0.3f, 0.3f)) * 2.0f * pi / NumTongues : Base + random_float(-0.7f, 0.7f);
		EmitFlame(Pos, direction(Angle) * random_float(Hit ? 150.0f : 90.0f, Hit ? 280.0f : 170.0f) * Scale, random_float(11.0f, Hit ? 19.0f : 14.0f) * Scale,
			random_float(0.22f, 0.34f), 0.25f, 6.0f);
	}
	const int NumSparks = (int)((Hit ? 16.0f : 6.0f) * m_Quality);
	for(int i = 0; i < NumSparks; i++)
		EmitEmber(Pos, direction(Base + random_float(-1.3f, 1.3f)) * random_float(160.0f, Hit ? 480.0f : 280.0f) * Scale + vec2(0.0f, -50.0f),
			random_float(3.0f, 4.5f), random_float(0.25f, 0.5f), 70.0f);
	if(Hit)
		m_Embers.AddRing(Pos, 0.22f, 10.0f * Scale, 70.0f * Scale, 0.8f, m_Colors.m_Body);
}

void CBcFireChainFx::Strike(int ClientId)
{
	const vec2 Pos = GameClient()->m_aClients[ClientId].m_RenderPos;
	const vec2 Dir = AimDir(ClientId);
	const vec2 Start = Pos + Dir * (TEE_HALF * 2.0f * 0.75f);

	// the server hits every tee whose center is within ProximityRadius * 0.5 + ProximityRadius of the start
	int Victim = -1;
	float Best = TEE_HALF * 3.0f;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(i == ClientId || !GameClient()->m_Snap.m_aCharacters[i].m_Active)
			continue;
		const float Dist = distance(GameClient()->m_aClients[i].m_RenderPos, Start);
		if(Dist < Best)
		{
			Best = Dist;
			Victim = i;
		}
	}
	StrikeShake(ClientId, Victim >= 0); // chainsaw: bc_fire_strike_shake
	const float Scale = g_Config.m_BcFireStrikeSize / 100.0f;
	if(Victim >= 0)
	{
		const vec2 Target = GameClient()->m_aClients[Victim].m_RenderPos;
		const vec2 Contact = mix(Start, Target, 0.5f);
		FireBurst(Contact, normalize(Target - Pos), true, Scale);
		AddBurn(Victim, Pos);
	}
	else
		FireBurst(Start + Dir * 8.0f, Dir, false, Scale);
}

void CBcFireChainFx::StrikeShake(int ClientId, bool Hit)
{
	// chainsaw: bc_fire_strike_shake, only your own strikes shake your screen; the mega strike shakes harder
	if(g_Config.m_BcFireStrikeShake <= 0 || ClientId != GameClient()->m_Snap.m_LocalClientId)
		return;
	const bool Mega = g_Config.m_BcFireStrikeMega && (Hit || !g_Config.m_BcFireStrikeMegaHitsOnly);
	GameClient()->m_Lightning.AddShake(g_Config.m_BcFireStrikeShake / 100.0f * (Mega ? (Hit ? 12.0f : 7.0f) : (Hit ? 5.0f : 2.5f)));
}

void CBcFireChainFx::PreviewStrike(vec2 Pos)
{
	RefreshColors();
	StrikeShake(GameClient()->m_Snap.m_LocalClientId, true);
	FireBurst(Pos + vec2(40.0f, 0.0f), vec2(1.0f, 0.0f), true, g_Config.m_BcFireStrikeSize / 100.0f);
}

bool CBcFireChainFx::RenderChain(int ClientId, vec2 TeePos, vec2 HookPos, float Alpha)
{
	if(!ChainFor(ClientId) || BcFxSuppressed(GameClient()))
		return false;
	CcFx::CTimer Timer(CcFx::STAT_FIRE_CHAIN);

	CTeeState &Tee = m_aTees[ClientId];
	const float Now = LocalTime();
	const float Passed = Tee.m_ChainLastTime < 0.0f ? 0.0f : std::clamp(Now - Tee.m_ChainLastTime, 0.0f, 0.1f) * GameClient()->GetAnimationPlaybackSpeed();
	Tee.m_ChainLastTime = Now;
	if(m_ColorsTime != Now)
	{
		RefreshColors();
		m_ColorsTime = Now;
	}
	const CPalette &Colors = m_Colors;
	const float Distance = distance(TeePos, HookPos);
	const float Glow = g_Config.m_BcFireChainGlow / 100.0f; // chainsaw: bc_fire_chain_glow
	const vec2 Dir = Distance > 0.001f ? (TeePos - HookPos) / Distance : vec2(1.0f, 0.0f);
	const float Rotation = angle(Dir) + pi;
	const int NumLinks = std::min(256, (int)(Distance / LINK));

	// hooked a tee: it catches fire where the chain bit
	const int Hooked = GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur.m_HookedPlayer;
	if(Hooked >= 0 && Hooked < MAX_CLIENTS && Hooked != Tee.m_LastHooked && Hooked != ClientId)
	{
		AddBurn(Hooked, TeePos);
		if(m_NumFlashes < MAX_FLASHES)
			m_aFlashes[m_NumFlashes++] = {HookPos, 0.0f, 0.16f, 34.0f, false};
	}
	Tee.m_LastHooked = Hooked;

	const CcFx::CAtlas &Atlas = *m_pAtlas;
	const CcFx::CView View = CcFx::View(Graphics(), 20.0f);

	// soft heat under the links; the flicker phase moves 1.3 radians per link, turned instead of a sin per link
	CcFx::Begin(Graphics(), Atlas.m_Texture, true);
	{
		const vec2 Turn = direction(1.3f);
		vec2 Wave = direction(Now * 9.0f + 1.3f); // .y = sin(Now * 9 + i * 1.3) for i = 1
		for(int i = 1; i <= NumLinks; i++)
		{
			const vec2 At = HookPos + Dir * (i * LINK);
			if(View.Sees(At, 15.0f))
			{
				const float Flicker = 0.8f + 0.2f * Wave.y;
				CcFx::QuadAxis(Graphics(), At, 30.0f, 30.0f, Atlas.m_Glow, Colors.m_Glow.WithAlpha(0.3f * Glow * Flicker * Alpha));
			}
			Wave = Turned(Wave, Turn);
		}
		CcFx::QuadAxis(Graphics(), HookPos, 46.0f, 46.0f, Atlas.m_Glow, Colors.m_Glow.WithAlpha(0.55f * Glow * Alpha));
	}
	CcFx::End(Graphics());

	// bc_fire_chain_style 1: a burning rope instead of the links, the head is a harpoon of fire
	if(g_Config.m_BcFireChainStyle == 1)
		DrawRopeAndHead(ClientId, HookPos, Dir, Distance, Alpha, Now, Colors);
	else
	{
		// the links: iron glowing from red hot to yellow, a wave of heat runs along the chain
		const auto HeatAt = [&](int i) {
			return 0.55f + 0.45f * std::sin(i * 0.9f - Now * 7.0f);
		};
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteHookChain);
		Graphics()->QuadsBegin();
		Graphics()->QuadsSetRotation(Rotation);
		for(int i = 1; i <= NumLinks; i++)
		{
			const vec2 At = HookPos + Dir * (i * LINK);
			Graphics()->SetColor(FgfGlow::Blend(Colors.m_DarkIron, Colors.m_HotIron, HeatAt(i)).WithAlpha(Alpha));
			IGraphics::CQuadItem Quad(At.x, At.y, 24.0f, 16.0f);
			Graphics()->QuadsDraw(&Quad, 1);
		}
		Graphics()->QuadsEnd();
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteHookHead);
		Graphics()->QuadsBegin();
		Graphics()->QuadsSetRotation(Rotation);
		Graphics()->SetColor(Colors.m_HotIron.WithAlpha(Alpha));
		IGraphics::CQuadItem Head(HookPos.x, HookPos.y, 24.0f, 16.0f);
		Graphics()->QuadsDraw(&Head, 1);
		Graphics()->QuadsEnd();

		// the same sprites once more, added on top: the links shine
		Graphics()->BlendAdditive();
		Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteHookChain);
		Graphics()->QuadsBegin();
		Graphics()->QuadsSetRotation(Rotation);
		for(int i = 1; i <= NumLinks; i++)
		{
			const vec2 At = HookPos + Dir * (i * LINK);
			Graphics()->SetColor(Colors.m_Body.WithAlpha(0.5f * HeatAt(i) * Alpha));
			IGraphics::CQuadItem Quad(At.x, At.y, 24.0f, 16.0f);
			Graphics()->QuadsDraw(&Quad, 1);
		}
		Graphics()->QuadsEnd();
		Graphics()->BlendNormal();
		Graphics()->QuadsSetRotation(0.0f);
	}
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);

	// flames licking up from the links, more at the head; sparks drip off
	if(Passed > 0.0f)
	{
		const float Flames = g_Config.m_BcFireChainFlames / 100.0f * m_Quality * Alpha;
		Tee.m_ChainFlameAcc += Passed * 6.0f * std::max(1, NumLinks) * Flames;
		while(Tee.m_ChainFlameAcc >= 1.0f)
		{
			Tee.m_ChainFlameAcc -= 1.0f;
			const vec2 At = HookPos + Dir * random_float(LINK * 0.5f, std::max(LINK, Distance - 12.0f)) + vec2(random_float(-3.0f, 3.0f), random_float(-3.0f, 3.0f));
			EmitFlame(At, vec2(random_float(-15.0f, 15.0f), -random_float(40.0f, 85.0f)), random_float(7.0f, 11.0f), random_float(0.28f, 0.48f), 1.0f, 1.2f);
		}
		Tee.m_HeadFlameAcc += Passed * 16.0f * Flames;
		while(Tee.m_HeadFlameAcc >= 1.0f)
		{
			Tee.m_HeadFlameAcc -= 1.0f;
			EmitFlame(HookPos + vec2(random_float(-5.0f, 5.0f), random_float(-5.0f, 5.0f)), vec2(random_float(-20.0f, 20.0f), -random_float(60.0f, 110.0f)),
				random_float(10.0f, 14.0f), random_float(0.3f, 0.45f), 1.0f, 1.2f);
		}
		Tee.m_EmberAcc += Passed * (1.5f * std::max(1, NumLinks) + 4.0f) * Flames;
		while(Tee.m_EmberAcc >= 1.0f)
		{
			Tee.m_EmberAcc -= 1.0f;
			const vec2 At = HookPos + Dir * random_float(0.0f, Distance);
			EmitEmber(At, vec2(random_float(-30.0f, 30.0f), random_float(-60.0f, 10.0f)), random_float(2.0f, 3.2f), random_float(0.3f, 0.6f), 120.0f);
		}
	}
	return true;
}

void CBcFireChainFx::DrawRopeAndHead(int ClientId, vec2 HookPos, vec2 Dir, float Distance, float Alpha, float Now, const CPalette &Colors)
{
	const CcFx::CAtlas &Atlas = *m_pAtlas;
	const vec2 Normal(-Dir.y, Dir.x);
	const float RopeWidth = g_Config.m_BcFireChainWidth / 100.0f; // chainsaw: bc_fire_chain_width
	const float Glow = g_Config.m_BcFireChainGlow / 100.0f; // chainsaw: bc_fire_chain_glow

	// the rope: every wave the five bands need, once per point (thick at the head, thinner towards the hand, it
	// sways a little in its own heat, two strands wind around the middle)
	const bool HasRope = Distance >= 1.0f;
	const int Segments = std::clamp((int)(Distance / 6.0f), 1, (int)MAX_ROPE_SEGMENTS);
	const float Step = Distance / Segments;
	vec2 aBase[MAX_ROPE_SEGMENTS + 1];
	float aThick[MAX_ROPE_SEGMENTS + 1];
	float aTwistSin[MAX_ROPE_SEGMENTS + 1];
	float aTwistCos[MAX_ROPE_SEGMENTS + 1];
	float aHeat[MAX_ROPE_SEGMENTS]; // in the middle of each segment
	if(HasRope)
	{
		for(int k = 0; k <= Segments; k++)
		{
			const float At = k * Step;
			const float Sway = 1.2f * std::sin(At * 0.05f - Now * 5.0f) * std::min(1.0f, At / 30.0f);
			aBase[k] = HookPos + Dir * At + Normal * Sway;
			aThick[k] = mix(1.15f, 0.8f, At / Distance) * RopeWidth;
			const float Twist = At * 0.32f - Now * 13.0f;
			aTwistSin[k] = std::sin(Twist);
			aTwistCos[k] = std::cos(Twist);
			if(k < Segments)
			{
				const float Mid = At + Step * 0.5f;
				aHeat[k] = 0.6f + 0.4f * std::sin(Mid * 0.09f - Now * 9.0f) * std::sin(Mid * 0.023f + Now * 3.0f);
			}
		}
	}
	// one band along the rope; Twist 0 = the middle, +1 / -1 = the two strands (phase 0 / pi: sin and cos change sign)
	const auto Band = [&](float HalfWidth, float Twist, auto ColorAt) {
		const auto Point = [&](int j) {
			return Twist != 0.0f ? aBase[j] + Normal * (3.0f * aThick[j] * aTwistSin[j] * Twist) : aBase[j];
		};
		const auto Width = [&](int j) {
			// a twisted strand looks thinner where it turns away
			return HalfWidth * aThick[j] * (Twist != 0.0f ? 0.75f + 0.25f * aTwistCos[j] * Twist : 1.0f);
		};
		vec2 P0 = Point(0);
		float W0 = Width(0);
		for(int k = 0; k < Segments; k++)
		{
			const vec2 P1 = Point(k + 1);
			const float W1 = Width(k + 1);
			CcFx::Shape(Graphics(), P1 - Normal * W1, P1 + Normal * W1, P0 - Normal * W0, P0 + Normal * W0, ColorAt(k));
			P0 = P1;
			W0 = W1;
		}
	};

	// the head
	const vec2 Fwd = -Dir;
	const vec2 HeadNormal(-Fwd.y, Fwd.x);
	const float Phase = ClientId * 1.7f;
	const float Pulse = 0.88f + 0.12f * std::sin(Now * 17.0f + Phase) * std::sin(Now * 6.0f + Phase);
	const int Variant = ((int)(Now * 6.0f) + ClientId) % FLAME_TONGUES;

	// 1 (normal): the burning body, then the two strands twisted around each other
	if(HasRope)
	{
		CcFx::Begin(Graphics(), Atlas.m_Texture, false);
		Band(4.6f, 0.0f, [&](int k) { return FgfGlow::Blend(Colors.m_DarkIron, Colors.m_Dark, aHeat[k]).WithAlpha(0.9f * Alpha); });
		Band(2.3f, 1.0f, [&](int k) { return FgfGlow::Blend(Colors.m_Edge, Colors.m_Body, aHeat[k]).WithAlpha(Alpha); });
		Band(2.3f, -1.0f, [&](int k) { return FgfGlow::Blend(Colors.m_Dark, Colors.m_Edge, 1.0f - aHeat[k] * 0.5f).WithAlpha(Alpha); });
		CcFx::End(Graphics());
	}

	// 2 (additive): a white hot core shines through the rope; heat around the tip
	CcFx::Begin(Graphics(), Atlas.m_Texture, true);
	if(HasRope)
	{
		Band(0.8f, 0.0f, [&](int k) { return Colors.m_Core.WithAlpha(0.55f * aHeat[k] * Alpha); });
		Band(3.4f, 0.0f, [&](int k) { return Colors.m_Body.WithAlpha(0.15f * Glow * aHeat[k] * Alpha); });
	}
	const vec2 GlowAt = HookPos + Fwd * 4.0f;
	CcFx::QuadAxis(Graphics(), GlowAt, 36.0f * Pulse, 36.0f * Pulse, Atlas.m_Glow, Colors.m_Glow.WithAlpha(0.35f * Glow * Alpha));
	CcFx::End(Graphics());

	// 3 (normal): the hot point of the harpoon under the fire: a dark orange blade, a yellow one inside, a white edge;
	// then a harpoon of fire: a spear tongue where the hook flies, two barbs bent back
	CcFx::Begin(Graphics(), Atlas.m_Texture, false);
	const auto Blade = [&](float Length, float Back, float Half, ColorRGBA Color) {
		const vec2 Tip = HookPos + Fwd * Length;
		const vec2 Base = HookPos - Fwd * Back;
		CcFx::Shape(Graphics(), Tip, Tip, Base - HeadNormal * Half, Base + HeadNormal * Half, Color);
	};
	Blade(16.0f * Pulse, 4.0f, 8.0f, FgfGlow::Blend(Colors.m_DarkIron, Colors.m_Dark, 0.6f).WithAlpha(Alpha));
	Blade(12.5f * Pulse, 1.5f, 5.0f, FgfGlow::Blend(Colors.m_Edge, Colors.m_Body, 0.6f).WithAlpha(Alpha));
	Blade(8.0f * Pulse, -1.0f, 2.2f, Colors.m_Core.WithAlpha(Alpha));

	struct CHeadTongue
	{
		vec2 m_Center;
		vec2 m_Right;
		vec2 m_Up;
		int m_Variant;
		bool m_Flip;
	};
	CHeadTongue aTongues[3];
	for(int Side = -1, n = 0; Side <= 1; Side += 2, n++)
	{
		const float Bend = 140.0f + 8.0f * std::sin(Now * 9.0f + Phase + Side);
		CcFx::TongueQuad(HookPos - Fwd * 2.0f + HeadNormal * (Side * 3.5f), rotate(Fwd, Side * Bend), 11.0f * Pulse, aTongues[n].m_Center, aTongues[n].m_Right, aTongues[n].m_Up);
		aTongues[n].m_Variant = (Variant + (Side > 0 ? 1 : 2)) % FLAME_TONGUES;
		aTongues[n].m_Flip = Side < 0;
	}
	CcFx::TongueQuad(HookPos - Fwd * 10.0f, Fwd, 17.0f * Pulse, aTongues[2].m_Center, aTongues[2].m_Right, aTongues[2].m_Up);
	aTongues[2].m_Variant = Variant;
	aTongues[2].m_Flip = false;
	const ColorRGBA aLayerColors[FLAME_LAYERS] = {FgfGlow::Blend(Colors.m_DarkIron, Colors.m_Dark, 0.5f), Colors.m_Body, Colors.m_Core};
	for(int Layer = 0; Layer < FLAME_LAYERS; Layer++)
	{
		for(const CHeadTongue &Tongue : aTongues)
		{
			const CcFx::CUv &Uv = Atlas.m_aaFlame[Tongue.m_Variant][Layer];
			CcFx::Quad(Graphics(), Tongue.m_Center, Tongue.m_Right, Tongue.m_Up, Tongue.m_Flip ? Uv.Flipped() : Uv, aLayerColors[Layer].WithAlpha(Alpha));
		}
	}
	CcFx::End(Graphics());

	// 4 (additive): the white hot point where the rope ties into the head
	CcFx::Begin(Graphics(), Atlas.m_Texture, true);
	CcFx::QuadAxis(Graphics(), HookPos, 20.0f, 20.0f, Atlas.m_Glow, Colors.m_Body.WithAlpha(0.35f * Glow * Alpha));
	CcFx::QuadAxis(Graphics(), HookPos, 9.0f * Pulse, 9.0f * Pulse, Atlas.m_Glow, Colors.m_Core.WithAlpha(0.85f * Alpha));
	CcFx::End(Graphics());
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void CBcFireChainFx::UpdateTee(int ClientId)
{
	CTeeState &Tee = m_aTees[ClientId];
	const CGameClient::CSnapState::CCharacterInfo &Snap = GameClient()->m_Snap.m_aCharacters[ClientId];
	if(!Snap.m_Active)
	{
		if(Tee.m_Seen)
			Tee = CTeeState();
		return;
	}
	if(Snap.m_Cur.m_HookedPlayer < 0)
		Tee.m_LastHooked = -1;

	const bool Own = BcIsOwnTee(GameClient(), ClientId);
	CCharacter *pPredicted = Own && GameClient()->Predict() ? GameClient()->m_PredictedWorld.GetCharacterById(ClientId) : nullptr;
	const int AttackTick = pPredicted ? pPredicted->GetAttackTick() : Snap.m_Cur.m_AttackTick;
	const int Weapon = pPredicted ? pPredicted->GetActiveWeapon() : Snap.m_Cur.m_Weapon;
	if(!Tee.m_Seen)
	{
		Tee.m_Seen = true;
		Tee.m_AttackTick = AttackTick;
		return;
	}
	if(AttackTick != Tee.m_AttackTick && Weapon == WEAPON_HAMMER && StrikeFor(ClientId))
	{
		const vec2 Pos = GameClient()->m_aClients[ClientId].m_RenderPos;
		const CScreenRect Screen = Graphics()->GetScreen();
		if(Pos.x > Screen.m_TopLeft.x - 150.0f && Pos.x < Screen.m_BottomRight.x + 150.0f && Pos.y > Screen.m_TopLeft.y - 150.0f &&
			Pos.y < Screen.m_BottomRight.y + 150.0f && GameClient()->OptimizerAllowRenderPos(Pos))
			Strike(ClientId);
	}
	Tee.m_AttackTick = AttackTick;
}

void CBcFireChainFx::UpdateParticles(float Passed)
{
	for(int i = 0; i < m_NumFlames;)
	{
		CFlame &Flame = m_aFlames[i];
		Flame.m_Life += Passed;
		if(Flame.m_Life >= Flame.m_LifeSpan)
		{
			Flame = m_aFlames[--m_NumFlames];
			continue;
		}
		Flame.m_Vel.y -= 120.0f * Flame.m_Rise * Passed;
		Flame.m_Vel *= std::exp(-Flame.m_Drag * Passed);
		Flame.m_Pos += Flame.m_Vel * Passed;
		i++;
	}

	for(int i = 0; i < m_NumBurns;)
	{
		CBurn &Burn = m_aBurns[i];
		Burn.m_Life += Passed;
		if(Burn.m_Life >= Burn.m_LifeSpan || !GameClient()->m_Snap.m_aCharacters[Burn.m_Victim].m_Active)
		{
			Burn = m_aBurns[--m_NumBurns];
			continue;
		}
		// the little fire on the mark: one tongue kept alive, sparks now and then
		const float Fire = 1.0f - Smooth(0.55f, 0.85f, Burn.m_Life / Burn.m_LifeSpan);
		if(Fire > 0.05f)
		{
			Burn.m_EmberAcc += Passed * 2.5f * m_Quality;
			while(Burn.m_EmberAcc >= 1.0f)
			{
				Burn.m_EmberAcc -= 1.0f;
				const vec2 At = GameClient()->m_aClients[Burn.m_Victim].m_RenderPos + Burn.m_Offset;
				EmitEmber(At + vec2(0.0f, -4.0f), vec2(random_float(-15.0f, 15.0f), -random_float(40.0f, 80.0f)), random_float(1.8f, 2.6f), random_float(0.3f, 0.6f), -20.0f);
			}
		}
		i++;
	}

	for(int i = 0; i < m_NumFlashes;)
	{
		m_aFlashes[i].m_Life += Passed;
		if(m_aFlashes[i].m_Life >= m_aFlashes[i].m_LifeSpan)
		{
			m_aFlashes[i] = m_aFlashes[--m_NumFlashes];
			continue;
		}
		i++;
	}
	m_Embers.Update(Passed);
}

void CBcFireChainFx::Draw()
{
	if(m_NumFlames == 0 && m_NumBurns == 0 && m_NumFlashes == 0 && !m_Embers.HasAny())
		return;
	const CPalette &Colors = m_Colors;
	const CPalette &Mega = m_MegaColors;
	const CcFx::CAtlas &Atlas = *m_pAtlas;
	const CcFx::CView View = CcFx::View(Graphics(), 10.0f);

	// 1 (normal): burn marks, a black soot spot on the body
	if(m_NumBurns > 0)
	{
		CcFx::Begin(Graphics(), Atlas.m_Texture, false);
		for(int i = 0; i < m_NumBurns; i++)
		{
			const CBurn &Burn = m_aBurns[i];
			const vec2 At = GameClient()->m_aClients[Burn.m_Victim].m_RenderPos + Burn.m_Offset;
			if(!View.Sees(At, Burn.m_Size * 2.0f))
				continue;
			const float Fade = (1.0f - Smooth(0.7f, 1.0f, Burn.m_Life / Burn.m_LifeSpan)) * Smooth(0.0f, 0.08f, Burn.m_Life) * TeeAlpha(Burn.m_Victim);
			const vec2 Skew = direction(Burn.m_Rot) * Burn.m_Size * 0.35f;
			const ColorRGBA Soot(0.06f, 0.035f, 0.02f, 0.75f * Fade);
			CcFx::QuadAxis(Graphics(), At, Burn.m_Size * 2.6f, Burn.m_Size * 2.6f, Atlas.m_Glow, Soot);
			CcFx::QuadAxis(Graphics(), At + Skew, Burn.m_Size * 1.8f, Burn.m_Size * 1.8f, Atlas.m_Glow, Soot);
			CcFx::QuadAxis(Graphics(), At - Skew * 0.4f, Burn.m_Size * 1.3f, Burn.m_Size * 1.3f, Atlas.m_Glow, ColorRGBA(0.0f, 0.0f, 0.0f, 0.9f * Fade));
		}
		CcFx::End(Graphics());
	}

	// 2 (additive): strike flashes, the glowing rim of the marks
	if(m_NumFlashes > 0 || m_NumBurns > 0)
	{
		CcFx::Begin(Graphics(), Atlas.m_Texture, true);
		for(int i = 0; i < m_NumFlashes; i++)
		{
			const CFlash &Flash = m_aFlashes[i];
			const float Progress = Flash.m_Life / Flash.m_LifeSpan;
			const vec2 At = Flash.m_Pos;
			if(Flash.m_Mega)
			{
				// a wide orange blaze that lingers, a bright yellow body, a white hot core that is gone in a blink
				const float Fade = (1.0f - Progress) * (1.0f - Progress);
				const float Blink = std::max(0.0f, 1.0f - Progress * 3.5f);
				const float Size = Flash.m_Size * (0.55f + 0.75f * std::sqrt(Progress));
				if(!View.Sees(At, Size * 1.6f))
					continue;
				CcFx::QuadAxis(Graphics(), At, Size * 3.2f, Size * 3.2f, Atlas.m_Glow, Mega.m_Glow.WithAlpha(0.75f * Fade));
				CcFx::QuadAxis(Graphics(), At, Size * 1.8f, Size * 1.8f, Atlas.m_Glow, Mega.m_Edge.WithAlpha(0.9f * Fade));
				CcFx::QuadAxis(Graphics(), At, Size, Size, Atlas.m_Glow, Mega.m_Body.WithAlpha(0.9f * Fade));
				CcFx::QuadAxis(Graphics(), At, Flash.m_Size * 0.9f, Flash.m_Size * 0.9f, Atlas.m_Glow, Mega.m_Core.WithAlpha((Flash.m_Hit ? 1.0f : 0.7f) * Blink));
				continue;
			}
			const float Fade = (1.0f - Progress) * (1.0f - Progress);
			const float Size = Flash.m_Size * (0.6f + 0.9f * std::sqrt(Progress));
			if(!View.Sees(At, Size))
				continue;
			CcFx::QuadAxis(Graphics(), At, Size * 2.0f, Size * 2.0f, Atlas.m_Glow, Colors.m_Glow.WithAlpha(0.9f * Fade));
			CcFx::QuadAxis(Graphics(), At, Size * 0.7f, Size * 0.7f, Atlas.m_Glow, Colors.m_Core.WithAlpha((Flash.m_Hit ? 0.95f : 0.6f) * Fade));
		}
		for(int i = 0; i < m_NumBurns; i++)
		{
			const CBurn &Burn = m_aBurns[i];
			const vec2 At = GameClient()->m_aClients[Burn.m_Victim].m_RenderPos + Burn.m_Offset;
			if(!View.Sees(At, Burn.m_Size * 1.6f))
				continue;
			const float Heat = (1.0f - Smooth(0.3f, 0.8f, Burn.m_Life / Burn.m_LifeSpan)) * TeeAlpha(Burn.m_Victim);
			const float Flicker = 0.75f + 0.25f * std::sin(m_Time * 11.0f + Burn.m_Phase) * std::sin(m_Time * 4.3f + Burn.m_Phase * 2.0f);
			CcFx::QuadAxis(Graphics(), At, Burn.m_Size * 3.2f, Burn.m_Size * 3.2f, Atlas.m_Glow, Colors.m_Glow.WithAlpha(0.55f * Heat * Flicker));
		}
		CcFx::End(Graphics());
	}

	// tongues: the chain's, the strikes' and one small fire on each burn mark; the geometry once for 3 layers + glow
	const ColorRGBA aLayerColors[FLAME_LAYERS] = {Colors.m_Edge, Colors.m_Body, Colors.m_Core};
	const ColorRGBA aMegaColors[FLAME_LAYERS] = {Mega.m_Edge, Mega.m_Body, Mega.m_Core};
	int NumQuads = 0;
	for(int i = 0; i < m_NumFlames; i++)
	{
		const CFlame &Flame = m_aFlames[i];
		const float Progress = Flame.m_Life / Flame.m_LifeSpan;
		const float FlameSize = Flame.m_Size * Smooth(0.0f, 0.18f, Progress) * (1.0f - 0.85f * Smooth(0.4f, 1.0f, Progress));
		if(FlameSize < 1.0f || !View.Sees(Flame.m_Pos, FlameSize * 2.0f))
			continue;
		const vec2 Fly = length(Flame.m_Vel) > 1.0f ? normalize(Flame.m_Vel) : vec2(0.0f, -1.0f);
		const vec2 Up = normalize(vec2(Flame.m_Vel.x * 0.006f, -1.0f));
		const float Rise = mix(Flame.m_Rise, 1.0f, Smooth(0.3f, 1.0f, Progress));
		vec2 Dir = normalize(mix(Fly, Up, Rise));
		Dir = rotate(Dir, 0.12f * std::sin(m_Time * 6.0f + Flame.m_Phase) * 180.0f / pi);
		CTongueQuad &Quad = m_aTongueQuads[NumQuads++];
		CcFx::TongueQuad(Flame.m_Pos, Dir, FlameSize, Quad.m_Center, Quad.m_Right, Quad.m_Up);
		const float Fade = std::clamp((1.0f - Progress) / 0.12f, 0.0f, 1.0f);
		const CPalette &FlameColors = Flame.m_Mega ? Mega : Colors;
		Quad.m_aColors[0] = FgfGlow::Blend(FlameColors.m_Edge, FlameColors.m_Dark, std::max(0.0f, Flame.m_Tone - 0.55f) / 0.45f).WithAlpha(Fade);
		Quad.m_aColors[1] = (Flame.m_Mega ? aMegaColors[1] : aLayerColors[1]).WithAlpha(Fade);
		Quad.m_aColors[2] = (Flame.m_Mega ? aMegaColors[2] : aLayerColors[2]).WithAlpha(Fade);
		Quad.m_GlowAt = Flame.m_Pos + Dir * (FlameSize * 0.7f);
		Quad.m_GlowSize = FlameSize * 2.6f;
		Quad.m_Glow = Flame.m_Mega ? Mega.m_Glow.WithAlpha(0.32f) : Colors.m_Glow.WithAlpha(0.2f);
		Quad.m_HasGlow = true;
		Quad.m_Variant = Flame.m_Variant;
		Quad.m_Flip = Flame.m_Flip;
	}
	for(int i = 0; i < m_NumBurns; i++)
	{
		const CBurn &Burn = m_aBurns[i];
		const float Fire = 1.0f - Smooth(0.55f, 0.85f, Burn.m_Life / Burn.m_LifeSpan);
		const float Flicker = 0.85f + 0.15f * std::sin(m_Time * 9.0f + Burn.m_Phase * 3.0f);
		const float FlameSize = 8.0f * Fire * Flicker * Smooth(0.0f, 0.12f, Burn.m_Life);
		const vec2 Base = GameClient()->m_aClients[Burn.m_Victim].m_RenderPos + Burn.m_Offset;
		if(FlameSize < 1.0f || !View.Sees(Base, FlameSize * 2.0f))
			continue;
		const vec2 Dir = rotate(vec2(0.0f, -1.0f), (0.15f * std::sin(m_Time * 5.0f + Burn.m_Phase) + 0.07f * std::sin(m_Time * 13.0f + Burn.m_Phase)) * 180.0f / pi);
		CTongueQuad &Quad = m_aTongueQuads[NumQuads++];
		CcFx::TongueQuad(Base, Dir, FlameSize, Quad.m_Center, Quad.m_Right, Quad.m_Up);
		const float Alpha = TeeAlpha(Burn.m_Victim);
		for(int Layer = 0; Layer < FLAME_LAYERS; Layer++)
			Quad.m_aColors[Layer] = aLayerColors[Layer].WithAlpha(Alpha);
		Quad.m_HasGlow = false;
		Quad.m_Variant = i % FLAME_TONGUES;
		Quad.m_Flip = (i & 1) != 0;
	}

	// 3 (normal): the tongues, layer by layer so overlapping ones melt into one fire
	if(NumQuads > 0)
	{
		CcFx::Begin(Graphics(), Atlas.m_Texture, false);
		for(int Layer = 0; Layer < FLAME_LAYERS; Layer++)
		{
			for(int i = 0; i < NumQuads; i++)
			{
				const CTongueQuad &Quad = m_aTongueQuads[i];
				const CcFx::CUv &Uv = Atlas.m_aaFlame[Quad.m_Variant][Layer];
				CcFx::Quad(Graphics(), Quad.m_Center, Quad.m_Right, Quad.m_Up, Quad.m_Flip ? Uv.Flipped() : Uv, Quad.m_aColors[Layer]);
			}
		}
		CcFx::End(Graphics());
	}

	// 4 (additive): light around the tongues, sparks
	if(NumQuads > 0 || m_Embers.HasAny())
	{
		CcFx::Begin(Graphics(), Atlas.m_Texture, true);
		for(int i = 0; i < NumQuads; i++)
		{
			const CTongueQuad &Quad = m_aTongueQuads[i];
			if(Quad.m_HasGlow)
				CcFx::QuadAxis(Graphics(), Quad.m_GlowAt, Quad.m_GlowSize, Quad.m_GlowSize, Atlas.m_Glow, Quad.m_Glow);
		}
		if(m_Embers.HasAny())
			m_Embers.Draw(Graphics(), Atlas, View);
		CcFx::End(Graphics());
	}
	Graphics()->QuadsSetRotation(0.0f);
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}

void CBcFireChainFx::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	const float Time = LocalTime();
	const float Passed = std::clamp(Time - m_LastTime, 0.0f, 0.1f) * GameClient()->GetAnimationPlaybackSpeed();
	m_LastTime = Time;
	m_Time += Passed;

	const bool Enabled = g_Config.m_BcFireChain || g_Config.m_BcFireStrike || g_Config.m_BcFireBurn;
	const bool Alive = m_NumFlames > 0 || m_NumBurns > 0 || m_NumFlashes > 0 || m_Embers.HasAny();
	if(!Enabled && !Alive)
		return;
	if(BcFxSuppressed(GameClient()))
	{
		OnReset();
		return;
	}
	CcFx::CTimer Timer(CcFx::STAT_FIRE_CHAIN);
	if(m_ColorsTime != Time)
	{
		RefreshColors();
		m_ColorsTime = Time;
	}

	const float FrameTime = Client()->RenderFrameTime();
	const float TargetQuality = FrameTime > 1.0f / 40.0f ? 0.4f : (FrameTime > 1.0f / 90.0f ? 0.7f : 1.0f);
	m_Quality = mix(m_Quality, TargetQuality, std::min(1.0f, Passed * 2.0f));

	for(int i = 0; i < MAX_CLIENTS; i++)
		UpdateTee(i);
	UpdateParticles(Passed);
	Draw();
}
