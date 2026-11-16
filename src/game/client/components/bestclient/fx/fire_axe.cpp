/* ChainsawClient: golden axe instead of the hammer (see fire_axe.h, the picture is in fire_axe_art.h) */
#include "fire_axe.h"

#include "cc_fx_common.h"
#include "fire_axe_art.h"
#include "fire_axe_slash.h"
#include "fgf_glow.h"
#include "fire_ice_shapes.h"
#include "own_tee.h"

#include <base/math.h>

#include <engine/image.h>
#include <engine/sound.h>
#include <engine/storage.h>
#include <engine/shared/config.h>

#include <game/client/components/menus.h>
#include <game/client/components/sounds.h>
#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>

#include <algorithm>
#include <cmath>

namespace
{
constexpr float BASE_SIZE = 112.0f; // the axe cell at tee size 64, the hammer sprite is 96 x 72
constexpr float HAMMER_HEAD = 24.0f; // the hammer head from the sprite center, the axe head goes there
constexpr float TEE_HALF = 14.0f;

float Smooth(float a, float b, float x)
{
	const float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}
}

void CBcFireAxeFx::OnInit()
{
	m_pAtlas = &CcFx::Atlas(Graphics());
	LoadSounds();
	OnReset();
}

void CBcFireAxeFx::OnReset()
{
	for(CTee &Tee : m_aTees)
		Tee = CTee();
	m_NumFlames = 0;
	m_Sparks.Clear();
	m_NumSlashes = 0;
	m_NumCuts = 0;
	m_LastTime = -1.0f;
}

void CBcFireAxeFx::EnsureTexture()
{
	if(m_Texture.IsValid())
		return;
	// drawn once, the first time an axe shows (about 0.1 s of work)
	CImageInfo Image;
	Image.m_Width = BcAxe::TEX_W;
	Image.m_Height = BcAxe::TEX_H;
	Image.m_Format = CImageInfo::FORMAT_RGBA;
	Image.Allocate();
	BcAxe::BuildTexture(Image.m_pData);
	m_Texture = Graphics()->LoadTextureRawMove(Image, 0, "cc_fire_axe");
	m_BodyUv = {0.0f, 0.0f, 0.5f, 1.0f};
	m_GlowUv = {0.5f, 0.0f, 1.0f, 1.0f};
}

bool CBcFireAxeFx::On() const
{
	// no own switch: the axe is the model of the flaming hammer
	return g_Config.m_BcFireHammer != 0;
}

bool CBcFireAxeFx::AxeFor(int ClientId) const
{
	return On() && in_range(ClientId, MAX_CLIENTS - 1) && (g_Config.m_BcFireHammerOthers || BcIsOwnTee(GameClient(), ClientId));
}

float CBcFireAxeFx::SummonTime() const
{
	return std::max(0.3f, g_Config.m_BcFireAxeSummonTime / 10.0f);
}

ColorRGBA CBcFireAxeFx::FireColor(float Lightness) const
{
	// the color of the flaming hammer (bc_fire_aura_color), golden
	ColorHSLA Hsl(g_Config.m_BcFireAuraColor);
	Hsl.h = std::fmod(Hsl.h + 0.07f + 1.0f, 1.0f);
	Hsl.s = std::max(Hsl.s, 0.85f);
	Hsl.l = Lightness;
	Hsl.a = 1.0f;
	return color_cast<ColorRGBA>(Hsl);
}

vec2 CBcFireAxeFx::ToWorld(const CTee &Tee, float Px, float Py) const
{
	const float x = (Px / BcAxe::CELL - 0.5f) * Tee.m_Size;
	const float y = (Py / BcAxe::CELL - 0.5f) * Tee.m_Size;
	return Tee.m_Center + Tee.m_AxisX * x + Tee.m_AxisY * y;
}

vec2 CBcFireAxeFx::HeadPos(const CTee &Tee) const
{
	return ToWorld(Tee, BcAxe::HEAD_C.x, BcAxe::HEAD_C.y);
}

void CBcFireAxeFx::BeforeHammer(int ClientId, bool Ghost, bool Flip, float Alpha)
{
	if(Ghost || !in_range(ClientId, MAX_CLIENTS - 1))
		return;
	CTee &Tee = m_aTees[ClientId];
	Tee.m_Flip = Flip;
	Tee.m_Alpha = Alpha;
	Tee.m_Armed = true;
	const float Show = Tee.m_Init ? Tee.m_Show : (AxeFor(ClientId) ? 1.0f : 0.0f);
	const float Keep = 1.0f - Smooth(0.05f, 0.4f, Show);
	if(Keep < 0.999f)
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha * Keep);
}

void CBcFireAxeFx::OnHammerDrawn(int ClientId, vec2 WeaponPos, float Rotation, float Scale)
{
	if(!in_range(ClientId, MAX_CLIENTS - 1))
		return;
	CTee &Tee = m_aTees[ClientId];
	if(!Tee.m_Armed)
		return;
	Tee.m_Armed = false;
	const float Show = Tee.m_Init ? Tee.m_Show : (AxeFor(ClientId) ? 1.0f : 0.0f);
	if(Show <= 0.001f)
		return;
	EnsureTexture();

	const float Alpha = Tee.m_Alpha;
	float Size = BASE_SIZE * Scale * g_Config.m_BcFireAxeSize / 100.0f;
	vec2 AxisX(std::cos(Rotation), std::sin(Rotation));
	vec2 AxisY = vec2(-AxisX.y, AxisX.x) * (Tee.m_Flip ? -1.0f : 1.0f);
	// the axe head sits where the hammer head was
	vec2 Center = WeaponPos + AxisX * (HAMMER_HEAD * Scale - (BcAxe::HEAD_U - 0.5f) * Size);
	// animations (bc_fire_axe_anim*): turn around the grip, a little bigger on a hit, a glint over the blade
	float GlintPos = -1.0f;
	const bool Anim = AnimOn() && Show >= 0.999f;
	if(Anim)
	{
		float Turn, AnimScale;
		AnimPose(ClientId, Turn, AnimScale, GlintPos);
		const float Angle = Turn * (Tee.m_Flip ? -1.0f : 1.0f); // the hammer swings the other way when looking left
		const float c = std::cos(Angle), s = std::sin(Angle);
		const auto Rot = [&](vec2 v) { return vec2(v.x * c - v.y * s, v.x * s + v.y * c); };
		const vec2 Grip = Center + AxisX * ((BcAxe::GRIP_U - 0.5f) * Size);
		Center = Grip + Rot(Center - Grip) * AnimScale;
		AxisX = Rot(AxisX);
		AxisY = Rot(AxisY);
		Size *= AnimScale;
		// the head bash: the head turns to where the tee aims, draws back a little and thrusts forward
		if(g_Config.m_BcFireAxeAnimSwing && Tee.m_AttackKind == BcAxeSlash::KIND_BASH && Tee.m_SwingT < SwingTime(BcAxeSlash::KIND_BASH))
		{
			const float t = Tee.m_SwingT / SwingTime(BcAxeSlash::KIND_BASH);
			const float w = Smooth(0.0f, 0.3f, t) * (1.0f - Smooth(0.62f, 1.0f, t));
			float Delta = Tee.m_StrikeAim - angle(AxisX);
			while(Delta > pi)
				Delta -= 2.0f * pi;
			while(Delta < -pi)
				Delta += 2.0f * pi;
			const float bc = std::cos(Delta * w), bs = std::sin(Delta * w);
			const auto BashRot = [&](vec2 v) { return vec2(v.x * bc - v.y * bs, v.x * bs + v.y * bc); };
			const vec2 BashGrip = Center + AxisX * ((BcAxe::GRIP_U - 0.5f) * Size);
			Center = BashGrip + BashRot(Center - BashGrip);
			AxisX = BashRot(AxisX);
			AxisY = BashRot(AxisY);
			float Push;
			if(t < 0.3f)
				Push = -7.0f * Smooth(0.0f, 0.3f, t);
			else if(t < 0.48f)
			{
				const float u = (t - 0.3f) / 0.18f;
				Push = mix(-7.0f, 16.0f, 1.0f - (1.0f - u) * (1.0f - u) * (1.0f - u));
			}
			else
				Push = 16.0f * (1.0f - Smooth(0.48f, 1.0f, t));
			Center += direction(Tee.m_StrikeAim) * (Push * Size / BASE_SIZE);
		}
	}
	Tee.m_Center = Center;
	Tee.m_AxisX = AxisX;
	Tee.m_AxisY = AxisY;
	Tee.m_Size = Size;
	Tee.m_DrawTime = Client()->LocalTime();

	// the golden trail: one sample about every 11 ms while the axe swings or spins (the same length at any FPS)
	const bool Moving = Anim && g_Config.m_BcFireAxeAnimTrail &&
			    ((g_Config.m_BcFireAxeAnimSwing && Tee.m_SwingT < SwingTime(Tee.m_AttackKind)) || (g_Config.m_BcFireAxeAnimSpin && Tee.m_SpinT < 0.55f));
	if(Moving)
	{
		const BcAxe::V2 e = BcAxe::EdgePoint(0.5f);
		const vec2 TrailHead = HeadPos(Tee), TrailEdge = ToWorld(Tee, e.x, e.y);
		if(Tee.m_TrailNum > 0 && Tee.m_aTrailAge[Tee.m_TrailNum - 1] < 0.011f)
		{
			Tee.m_aTrailHead[Tee.m_TrailNum - 1] = TrailHead;
			Tee.m_aTrailEdge[Tee.m_TrailNum - 1] = TrailEdge;
		}
		else
		{
			if(Tee.m_TrailNum == MAX_TRAIL)
			{
				for(int i = 1; i < MAX_TRAIL; i++)
				{
					Tee.m_aTrailHead[i - 1] = Tee.m_aTrailHead[i];
					Tee.m_aTrailEdge[i - 1] = Tee.m_aTrailEdge[i];
					Tee.m_aTrailAge[i - 1] = Tee.m_aTrailAge[i];
				}
				Tee.m_TrailNum--;
			}
			Tee.m_aTrailHead[Tee.m_TrailNum] = TrailHead;
			Tee.m_aTrailEdge[Tee.m_TrailNum] = TrailEdge;
			Tee.m_aTrailAge[Tee.m_TrailNum] = 0.0f;
			Tee.m_TrailNum++;
		}
	}
	if(Tee.m_TrailNum >= 2 && g_Config.m_BcFireAxeAnimTrail && AnimOn() && !BcFxSuppressed(GameClient()))
		DrawTrail(Tee, Tee.m_Alpha);

	const bool Done = Show >= 0.999f;
	// the axe burns in from the head outwards
	const float Reach = Smooth(0.2f, 0.86f, Show) * 0.85f + 0.08f;
	const auto Reveal = [&](float u) { return Done ? 1.0f : std::clamp((Reach - std::fabs(u - BcAxe::HEAD_U)) / 0.1f, 0.0f, 1.0f); };
	const auto Front = [&](float u) { return Done ? 0.0f : std::clamp(1.0f - std::fabs(Reach - std::fabs(u - BcAxe::HEAD_U)) / 0.12f, 0.0f, 1.0f); };
	const float WhiteHot = Done ? 0.0f : 1.0f - Smooth(0.62f, 1.0f, Show);
	const float Heat = g_Config.m_BcFireAxeHeat ? Tee.m_Heat : 0.0f;
	const float Time = Client()->LocalTime();
	const float Flicker = 0.92f + 0.08f * std::sin(Time * 11.0f + ClientId) + 0.05f * std::sin(Time * 23.0f);

	const auto Slices = [&](const CcFx::CUv &Uv, int Num, float ExtraScale, const auto &VertexColor) {
		const float S = Size * ExtraScale;
		for(int i = 0; i < Num; i++)
		{
			const float u0 = (float)i / Num, u1 = (float)(i + 1) / Num;
			const vec2 Top = Tee.m_Center - AxisY * (S * 0.5f), Bottom = Tee.m_Center + AxisY * (S * 0.5f);
			const vec2 X0 = AxisX * ((u0 - 0.5f) * S), X1 = AxisX * ((u1 - 0.5f) * S);
			const float U0 = Uv.m_U0 + u0 * (Uv.m_U1 - Uv.m_U0), U1 = Uv.m_U0 + u1 * (Uv.m_U1 - Uv.m_U0);
			const ColorRGBA C0 = VertexColor(u0), C1 = VertexColor(u1);
			if(C0.a <= 0.002f && C1.a <= 0.002f)
				continue;
			Graphics()->QuadsSetSubsetFree(U0, Uv.m_V0, U1, Uv.m_V0, U0, Uv.m_V1, U1, Uv.m_V1);
			Graphics()->SetColor4(C0, C1, C0, C1);
			const IGraphics::CFreeformItem Item(Top + X0, Top + X1, Bottom + X0, Bottom + X1);
			Graphics()->QuadsDrawFreeform(&Item, 1);
			CcFx::CountQuads(1);
		}
	};
	const int NumSlices = Done ? 1 : 16;

	// 1. glow behind the axe
	{
		const float Glow = g_Config.m_BcFireAxeGlow / 100.0f;
		float GlowAlpha = 0.22f * Glow + 0.85f * Heat * Flicker + 0.55f * Tee.m_Flash + 0.75f * WhiteHot * Show;
		GlowAlpha = std::min(GlowAlpha, 1.0f) * Smooth(0.12f, 0.55f, Show) * Alpha;
		if(GlowAlpha > 0.01f)
		{
			const ColorRGBA GlowColor = FgfGlow::Whiten(FireColor(0.56f), 0.25f * Tee.m_Flash + 0.35f * WhiteHot);
			const float GlowScale = 1.0f + 0.2f * Heat + 0.12f * Tee.m_Flash + 0.1f * WhiteHot;
			CcFx::Begin(Graphics(), m_Texture, true);
			Slices(m_GlowUv, 1, GlowScale, [&](float) { return GlowColor.WithAlpha(GlowAlpha); });
			CcFx::End(Graphics());
		}
	}
	// 2. the axe
	CcFx::Begin(Graphics(), m_Texture, false);
	Slices(m_BodyUv, NumSlices, 1.0f, [&](float u) { return ColorRGBA(1.0f, 1.0f, 1.0f, Reveal(u) * Alpha); });
	CcFx::End(Graphics());
	// 3. white heat while it forms, red-gold heat from hits
	const float HeatOver = 0.5f * Heat * Flicker + 0.35f * Tee.m_Flash;
	// the glint (animations): a light band runs from the grip over the head and the blade
	const bool Glint = GlintPos >= 0.0f;
	const float GlintU = mix(0.12f, 1.02f, GlintPos);
	const auto GlintAt = [&](float u) {
		if(!Glint)
			return 0.0f;
		const float d = (u - GlintU) / 0.07f;
		return 0.8f * std::exp(-d * d) * std::sin(pi * GlintPos);
	};
	if(WhiteHot > 0.01f || HeatOver > 0.01f || Glint)
	{
		const ColorRGBA Hot = FireColor(0.6f);
		CcFx::Begin(Graphics(), m_Texture, true);
		Slices(m_BodyUv, Glint ? 24 : NumSlices, 1.0f, [&](float u) {
			const float Shine = GlintAt(u);
			const float White = std::min(1.0f, WhiteHot + Front(u) * 0.8f + Shine);
			const ColorRGBA c = FgfGlow::Whiten(Hot, White);
			return c.WithAlpha(std::min(1.0f, Reveal(u) * (0.95f * White + HeatOver)) * Alpha);
		});
		CcFx::End(Graphics());
	}
	// as the hammer code had it
	Graphics()->BlendNormal();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, Alpha);
}

void CBcFireAxeFx::EmitFlame(vec2 Pos, vec2 Vel, float Size, float LifeSpan, float Hot)
{
	if(m_NumFlames >= MAX_FLAMES)
		return;
	CFlame &Flame = m_aFlames[m_NumFlames++];
	Flame.m_Pos = Pos;
	Flame.m_Vel = Vel;
	Flame.m_Life = 0.0f;
	Flame.m_LifeSpan = LifeSpan;
	Flame.m_Size = Size;
	Flame.m_Hot = Hot;
	Flame.m_Variant = (int)random_float(0.0f, 3.99f); // the hammer tongues of the atlas
	Flame.m_Flip = random_float(0.0f, 1.0f) < 0.5f;
}

void CBcFireAxeFx::EmitSpark(vec2 Pos, vec2 Vel, float Size, float LifeSpan, ColorRGBA From, ColorRGBA To, float Drag)
{
	CcFx::CSparks::CSpark *pSpark = m_Sparks.NewSpark();
	if(!pSpark)
		return;
	pSpark->m_Pos = Pos;
	pSpark->m_Vel = Vel;
	pSpark->m_Gravity = -40.0f;
	pSpark->SetDrag(Drag);
	pSpark->m_Stretch = 0.012f;
	pSpark->m_Life = 0.0f;
	pSpark->m_LifeSpan = LifeSpan;
	pSpark->m_StartSize = Size;
	pSpark->m_EndSize = Size * 0.3f;
	pSpark->m_StartAlpha = 1.0f;
	pSpark->m_EndAlpha = 0.0f;
	pSpark->m_Color = From;
	pSpark->m_EndColor = To;
	pSpark->m_ShiftColor = true;
}

void CBcFireAxeFx::Burst(int ClientId, float Power)
{
	const CTee &Tee = m_aTees[ClientId];
	if(Client()->LocalTime() - Tee.m_DrawTime > 0.2f || BcFxSuppressed(GameClient()))
		return;
	const vec2 Head = HeadPos(Tee);
	const float k = Tee.m_Size / BASE_SIZE;
	const ColorRGBA Gold = FireColor(0.62f);
	const ColorRGBA White(1.0f, 0.98f, 0.9f, 1.0f);
	m_Sparks.AddRing(Head, 0.32f, 12.0f * k, 78.0f * k * (0.7f + 0.3f * Power), 0.9f * Tee.m_Alpha, FgfGlow::Whiten(Gold, 0.5f));
	const int Num = (int)(30.0f * Power);
	for(int i = 0; i < Num; i++)
	{
		const vec2 Dir = direction(random_float(0.0f, 2.0f * pi));
		EmitSpark(Head + Dir * random_float(2.0f, 14.0f) * k, Dir * random_float(140.0f, 430.0f) * k + vec2(0.0f, -50.0f), random_float(3.0f, 5.5f) * k, random_float(0.25f, 0.55f), White, Gold, 0.08f);
	}
	for(int i = 0; i < (int)(10.0f * Power); i++)
	{
		const vec2 Dir = direction(random_float(0.0f, 2.0f * pi));
		EmitFlame(Head + Dir * random_float(4.0f, 20.0f) * k, Dir * random_float(50.0f, 140.0f) * k + vec2(0.0f, -70.0f), random_float(12.0f, 20.0f) * k, random_float(0.25f, 0.4f), 0.7f);
	}
}

void CBcFireAxeFx::CheckHit(int ClientId)
{
	// like the server: every tee whose center is within 1.5 tee sizes of the swing start is hit
	const vec2 Pos = GameClient()->m_aClients[ClientId].m_RenderPos;
	const vec2 Dir = StrikeDir(ClientId);
	const vec2 Start = Pos + Dir * (TEE_HALF * 2.0f * 0.75f);
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
	if(!Hit)
		return;
	CTee &Tee = m_aTees[ClientId];
	// animations: the axe shakes and kicks back a little after the blade went in
	Tee.m_ImpactT = Tee.m_AttackKind == BcAxeSlash::KIND_BASH ? -0.14f : -0.12f; // when the swing strikes
	// the elegant strike: a cross of light (a burst for the head bash) where the blade went in
	if(SlashOn())
		AddCut(ClientId, mix(Start, Target, 0.6f), angle(Dir));
	PlayAxeSound(ClientId, Tee.m_AttackKind == BcAxeSlash::KIND_BASH ? SOUND_BASH_HIT : SOUND_HIT, mix(Start, Target, 0.6f));
	if(!g_Config.m_BcFireAxeHeat)
		return;
	// every hit adds heat: the axe burns brighter and brighter
	Tee.m_Heat = std::min(1.0f, Tee.m_Heat + 0.34f);
	Tee.m_HeatHold = g_Config.m_BcFireAxeHeatTime / 10.0f;
	Tee.m_Flash = 1.0f;
	if(BcFxSuppressed(GameClient()))
		return;
	const vec2 Contact = mix(Start, Target, 0.5f);
	const float k = Tee.m_Size / BASE_SIZE;
	const ColorRGBA Gold = FireColor(0.6f);
	const int Num = (int)(10.0f + 16.0f * Tee.m_Heat);
	for(int i = 0; i < Num; i++)
	{
		const vec2 Out = direction(angle(Dir) + random_float(-1.2f, 1.2f));
		EmitSpark(Contact, Out * random_float(160.0f, 480.0f) * k + vec2(0.0f, -60.0f), random_float(3.0f, 5.0f) * k, random_float(0.25f, 0.5f), ColorRGBA(1.0f, 1.0f, 0.92f, 1.0f), Gold, 0.1f);
	}
	m_Sparks.AddRing(Contact, 0.25f, 8.0f * k, (40.0f + 30.0f * Tee.m_Heat) * k, 0.85f * Tee.m_Alpha, FgfGlow::Whiten(Gold, 0.6f));
}

void CBcFireAxeFx::UpdateTee(int ClientId, float Dt, bool MenuOpen)
{
	CTee &Tee = m_aTees[ClientId];
	if(!GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
	{
		if(Tee.m_Init)
			Tee = CTee();
		return;
	}
	const bool Own = BcIsOwnTee(GameClient(), ClientId);
	CCharacter *pPredicted = Own && GameClient()->Predict() ? GameClient()->m_PredictedWorld.GetCharacterById(ClientId) : nullptr;
	const CGameClient::CSnapState::CCharacterInfo &Snap = GameClient()->m_Snap.m_aCharacters[ClientId];
	const int AttackTick = pPredicted ? pPredicted->GetAttackTick() : Snap.m_Cur.m_AttackTick;
	const int Weapon = pPredicted ? pPredicted->GetActiveWeapon() : Snap.m_Cur.m_Weapon;
	const float Target = AxeFor(ClientId) ? 1.0f : 0.0f;

	if(!Tee.m_Init)
	{
		Tee.m_Init = true;
		Tee.m_Show = Target;
		Tee.m_Burst = true;
		Tee.m_Weapon = Weapon;
		Tee.m_AttackTick = AttackTick;
		return;
	}

	// taking the hammer
	if(Weapon == WEAPON_HAMMER && Tee.m_Weapon != WEAPON_HAMMER)
	{
		if(Target > 0.5f && g_Config.m_BcFireAxeSummon && g_Config.m_BcFireAxeSummonSwitch)
		{
			Tee.m_Show = 0.0f;
			Tee.m_Burst = false;
		}
		else
		{
			Tee.m_Show = Target;
			// animations: the axe spins in the hand when you take it
			Tee.m_SpinT = 0.0f;
			Tee.m_WaitT = 0.0f;
		}
	}
	else if(Weapon != WEAPON_HAMMER)
	{
		Tee.m_Show = Target;
		Tee.m_Burst = true;
	}
	Tee.m_Weapon = Weapon;

	const bool Drawn = Client()->LocalTime() - Tee.m_DrawTime < 0.15f;
	const float Before = Tee.m_Show;
	if(Tee.m_Show < Target)
	{
		// with the menu open the summon waits, so that it is seen when the menu closes
		if(!(MenuOpen && Own && !Tee.m_NoWait))
			Tee.m_Show = std::min(Target, Tee.m_Show + Dt / SummonTime());
	}
	else if(Tee.m_Show > Target)
		Tee.m_Show = std::max(Target, Tee.m_Show - Dt / (SummonTime() * 0.6f));
	if(Tee.m_Show >= 1.0f)
		Tee.m_NoWait = false;
	if(!Tee.m_Burst && Before < 0.86f && Tee.m_Show >= 0.86f && Target > 0.5f)
	{
		Tee.m_Burst = true;
		Tee.m_Flash = 1.0f;
		Burst(ClientId, 1.0f);
	}

	Tee.m_Flash = std::max(0.0f, Tee.m_Flash - Dt / 0.4f);
	if(Tee.m_HeatHold > 0.0f)
		Tee.m_HeatHold -= Dt;
	else
		Tee.m_Heat = std::max(0.0f, Tee.m_Heat - Dt * 0.3f);

	if(Tee.m_AttackTick >= 0 && AttackTick != Tee.m_AttackTick && Weapon == WEAPON_HAMMER && Tee.m_Show > 0.5f && Drawn)
	{
		// animations: every swing winds up and strikes (a spin stops for it), with its crescent and its sound
		StartAttack(ClientId);
		if(g_Config.m_BcFireAxeHeat || (AnimOn() && g_Config.m_BcFireAxeAnimImpact) || SlashOn() || SoundFor(ClientId))
			CheckHit(ClientId);
	}
	Tee.m_AttackTick = AttackTick;
	UpdateAnim(ClientId, Dt);

	if(!Drawn || Tee.m_Show <= 0.0f || BcFxSuppressed(GameClient()))
		return;
	const float k = Tee.m_Size / BASE_SIZE;
	const vec2 Head = HeadPos(Tee);

	// fire and light gather in the hand while the axe forms
	const bool Summoning = Target > 0.5f && Tee.m_Show < 0.72f && !(MenuOpen && Own && !Tee.m_NoWait);
	if(Summoning)
	{
		Tee.m_SparkAcc += Dt * 110.0f;
		while(Tee.m_SparkAcc >= 1.0f)
		{
			Tee.m_SparkAcc -= 1.0f;
			const vec2 Out = direction(random_float(0.0f, 2.0f * pi));
			const vec2 From = Head + Out * random_float(40.0f, 80.0f) * k;
			const vec2 Tangent(-Out.y, Out.x);
			EmitSpark(From, (Head - From) * random_float(2.4f, 3.4f) + Tangent * random_float(70.0f, 130.0f) * k, random_float(2.5f, 4.5f) * k, random_float(0.25f, 0.35f),
				FireColor(0.62f), ColorRGBA(1.0f, 1.0f, 0.95f, 1.0f), 0.3f);
		}
		Tee.m_FlameAcc += Dt * 34.0f;
		while(Tee.m_FlameAcc >= 1.0f)
		{
			Tee.m_FlameAcc -= 1.0f;
			const vec2 Out = direction(random_float(0.0f, 2.0f * pi));
			const vec2 From = Head + Out * random_float(30.0f, 55.0f) * k;
			EmitFlame(From, (Head - From) * random_float(2.0f, 2.8f) + vec2(0.0f, -30.0f), random_float(10.0f, 16.0f) * k, random_float(0.3f, 0.42f), 0.5f);
		}
		return;
	}

	// heat from hits: tongues along the edge, the spear tips and the head, embers
	const float Heat = g_Config.m_BcFireAxeHeat ? Tee.m_Heat : 0.0f;
	if(Heat <= 0.02f)
		return;
	Tee.m_FlameAcc += Dt * 52.0f * Heat;
	while(Tee.m_FlameAcc >= 1.0f)
	{
		Tee.m_FlameAcc -= 1.0f;
		vec2 From;
		const float Pick = random_float(0.0f, 1.0f);
		if(Pick < 0.68f)
		{
			const BcAxe::V2 e = BcAxe::EdgePoint(random_float(0.0f, 1.0f));
			From = ToWorld(Tee, e.x, e.y);
		}
		else if(Pick < 0.88f)
		{
			const BcAxe::CSpear &S = BcAxe::Spear(random_float(0.0f, 1.0f) < 0.5f ? 0 : 1);
			const BcAxe::V2 p = S.m_Base + S.m_Dir * (S.m_Rod + S.m_Leaf * random_float(0.2f, 0.9f));
			From = ToWorld(Tee, p.x, p.y);
		}
		else
			From = Head + direction(random_float(0.0f, 2.0f * pi)) * random_float(0.0f, 14.0f) * k;
		EmitFlame(From, vec2(random_float(-30.0f, 30.0f), -random_float(70.0f, 150.0f)) * k, (8.0f + 9.0f * Heat) * random_float(0.8f, 1.2f) * k, random_float(0.28f, 0.48f), 0.15f + 0.45f * Heat * random_float(0.0f, 1.0f));
	}
	Tee.m_SparkAcc += Dt * 16.0f * Heat;
	while(Tee.m_SparkAcc >= 1.0f)
	{
		Tee.m_SparkAcc -= 1.0f;
		const BcAxe::V2 e = BcAxe::EdgePoint(random_float(0.0f, 1.0f));
		EmitSpark(ToWorld(Tee, e.x, e.y), vec2(random_float(-40.0f, 40.0f), -random_float(60.0f, 160.0f)) * k, random_float(2.0f, 3.5f) * k, random_float(0.4f, 0.8f),
			ColorRGBA(1.0f, 0.95f, 0.8f, 1.0f), FireColor(0.5f), 0.4f);
	}
}

// ---- animations (bc_fire_axe_anim*), 08.11 ----
namespace
{
constexpr float SWING_TIME = 0.3f; // wind up, strike, settle
constexpr float SPIN_TIME = 0.55f;
constexpr float IMPACT_TIME = 0.32f;
constexpr float GLINT_TIME = 0.5f;
constexpr float GLINT_EVERY = 3.5f;
constexpr float IDLE_SPIN_AFTER = 8.0f; // seconds with the hammer in hand and no swing
constexpr float TRAIL_LIFE = 0.13f;

// + = the way the hammer swings, 1 = 43 degrees past the hammer
float SwingCurve(float t)
{
	if(t < 0.18f)
	{
		const float u = t / 0.18f;
		return -0.35f * (1.0f - (1.0f - u) * (1.0f - u)); // wind up, slowing down
	}
	if(t < 0.42f)
	{
		const float u = (t - 0.18f) / 0.24f;
		return -0.35f + 1.35f * u * u; // the strike, faster and faster
	}
	const float u = (t - 0.42f) / 0.58f;
	return std::exp(-3.5f * u) * std::cos(9.0f * u) * (1.0f - u); // overshoot and settle
}
}

bool CBcFireAxeFx::AnimOn() const
{
	return g_Config.m_BcFireAxeAnim != 0;
}

float CBcFireAxeFx::AnimSpeed() const
{
	return std::clamp(g_Config.m_BcFireAxeAnimSpeed, 50, 200) / 100.0f;
}

void CBcFireAxeFx::AnimPose(int ClientId, float &Turn, float &Scale, float &Glint) const
{
	const CTee &Tee = m_aTees[ClientId];
	Turn = 0.0f;
	Scale = 1.0f;
	Glint = -1.0f;
	float Busy = 0.0f; // the sway calms down while something else plays
	if(g_Config.m_BcFireAxeAnimSwing && Tee.m_SwingT < SwingTime(Tee.m_AttackKind))
	{
		const float t = Tee.m_SwingT / SwingTime(Tee.m_AttackKind);
		if(Tee.m_AttackKind == BcAxeSlash::KIND_HORIZONTAL)
		{
			// wider, and the axe comes a little towards you while it sweeps round
			Turn += 1.05f * SwingCurve(t);
			Scale += 0.07f * std::sin(pi * std::clamp(t / 0.7f, 0.0f, 1.0f));
		}
		else if(Tee.m_AttackKind == BcAxeSlash::KIND_BASH)
		{
			// the turn and the thrust of the head bash are in OnHammerDrawn (they need the aim), here only a swell at the blow
			Scale += 0.08f * std::exp(-std::pow((t - 0.5f) / 0.12f, 2.0f));
		}
		else
			Turn += 0.75f * SwingCurve(t);
		Busy = 1.0f;
	}
	if(g_Config.m_BcFireAxeAnimSpin && Tee.m_SpinT < SPIN_TIME)
	{
		const float t = Tee.m_SpinT / SPIN_TIME;
		Turn += 2.0f * pi * t * t * (3.0f - 2.0f * t);
		Busy = 1.0f;
	}
	if(g_Config.m_BcFireAxeAnimImpact && Tee.m_ImpactT >= 0.0f && Tee.m_ImpactT < IMPACT_TIME)
	{
		const float t = Tee.m_ImpactT;
		const float Fade = 1.0f - t / IMPACT_TIME;
		Turn += 0.13f * std::sin(t * 75.0f) * std::exp(-t * 13.0f) * Fade;
		Scale += 0.14f * std::exp(-t * 15.0f) * Fade;
	}
	if(g_Config.m_BcFireAxeAnimIdle)
	{
		const float Now = Client()->LocalTime() * AnimSpeed();
		Turn += (1.0f - Busy) * (0.05f * std::sin(Now * 1.9f + ClientId * 1.7f) + 0.015f * std::sin(Now * 4.3f + ClientId));
		if(Tee.m_GlintT >= 0.0f && Tee.m_GlintT < GLINT_TIME)
			Glint = Tee.m_GlintT / GLINT_TIME;
	}
}

void CBcFireAxeFx::UpdateAnim(int ClientId, float Dt)
{
	CTee &Tee = m_aTees[ClientId];
	const float d = Dt * AnimSpeed();
	Tee.m_SwingT = std::min(Tee.m_SwingT + d, 100.0f);
	Tee.m_ImpactT = std::min(Tee.m_ImpactT + d, 100.0f);
	Tee.m_SpinT = std::min(Tee.m_SpinT + d, 100.0f);
	// the trail fades in real time: the same length at any animation speed
	for(int i = 0; i < Tee.m_TrailNum; i++)
		Tee.m_aTrailAge[i] += Dt;
	int Old = 0;
	while(Old < Tee.m_TrailNum && Tee.m_aTrailAge[Old] > TRAIL_LIFE)
		Old++;
	if(Old > 0)
	{
		for(int i = Old; i < Tee.m_TrailNum; i++)
		{
			Tee.m_aTrailHead[i - Old] = Tee.m_aTrailHead[i];
			Tee.m_aTrailEdge[i - Old] = Tee.m_aTrailEdge[i];
			Tee.m_aTrailAge[i - Old] = Tee.m_aTrailAge[i];
		}
		Tee.m_TrailNum -= Old;
	}
	// a glint now and then
	Tee.m_GlintT += d;
	if(Tee.m_GlintT > GLINT_EVERY)
		Tee.m_GlintT = -random_float(0.0f, 1.5f);
	// the hammer in hand and nothing happens: spin the axe now and then
	const bool Waiting = Tee.m_Weapon == WEAPON_HAMMER && Tee.m_Show >= 1.0f && Tee.m_SwingT > 1.0f && Tee.m_SpinT > SPIN_TIME;
	if(Waiting && AnimOn() && g_Config.m_BcFireAxeAnimSpin)
	{
		Tee.m_WaitT += d;
		if(Tee.m_WaitT > IDLE_SPIN_AFTER)
		{
			Tee.m_SpinT = 0.0f;
			Tee.m_WaitT = -random_float(0.0f, 4.0f);
		}
	}
	else if(Tee.m_Weapon != WEAPON_HAMMER)
		Tee.m_WaitT = 0.0f;
}

void CBcFireAxeFx::DrawTrail(const CTee &Tee, float Alpha)
{
	// a golden crescent behind the blade: from the head (clear) to the edge (bright), then a soft fade past the edge
	const CcFx::CUv &W = m_pAtlas->m_White;
	const ColorRGBA Gold = FgfGlow::Whiten(FireColor(0.6f), 0.35f);
	const float Glow = std::clamp(g_Config.m_BcFireAxeGlow / 100.0f, 0.3f, 1.5f);
	CcFx::Begin(Graphics(), m_pAtlas->m_Texture, true);
	Graphics()->QuadsSetSubsetFree(W.m_U0, W.m_V0, W.m_U0, W.m_V0, W.m_U0, W.m_V0, W.m_U0, W.m_V0);
	for(int i = 0; i + 1 < Tee.m_TrailNum; i++)
	{
		const auto Strength = [&](int j) {
			const float Life = 1.0f - Tee.m_aTrailAge[j] / TRAIL_LIFE;
			return std::clamp(Life, 0.0f, 1.0f) * std::clamp(Life, 0.0f, 1.0f) * 0.75f * Glow * Alpha;
		};
		const float a0 = Strength(i), a1 = Strength(i + 1);
		if(a0 <= 0.005f && a1 <= 0.005f)
			continue;
		const vec2 H0 = Tee.m_aTrailHead[i], H1 = Tee.m_aTrailHead[i + 1];
		const vec2 E0 = Tee.m_aTrailEdge[i], E1 = Tee.m_aTrailEdge[i + 1];
		const vec2 O0 = E0 + (E0 - H0) * 0.2f, O1 = E1 + (E1 - H1) * 0.2f;
		const ColorRGBA Clear = Gold.WithAlpha(0.0f);
		Graphics()->SetColor4(Gold.WithAlpha(a0), Gold.WithAlpha(a1), Clear, Clear);
		const IGraphics::CFreeformItem Inner(E0, E1, H0, H1);
		Graphics()->QuadsDrawFreeform(&Inner, 1);
		Graphics()->SetColor4(Clear, Clear, Gold.WithAlpha(a0), Gold.WithAlpha(a1));
		const IGraphics::CFreeformItem Outer(O0, O1, E0, E1);
		Graphics()->QuadsDrawFreeform(&Outer, 1);
		CcFx::CountQuads(2);
	}
	CcFx::End(Graphics());
}

void CBcFireAxeFx::PreviewSwing()
{
	const int Local = GameClient()->m_Snap.m_LocalClientId;
	if(!in_range(Local, MAX_CLIENTS - 1))
		return;
	CTee &Tee = m_aTees[Local];
	// the next strike of the combo (or the chosen kind), as if it hit a tee right in front
	StartAttack(Local);
	Tee.m_ImpactT = Tee.m_AttackKind == BcAxeSlash::KIND_BASH ? -0.14f : -0.12f;
	const vec2 Dir = StrikeDir(Local);
	const vec2 Contact = GameClient()->m_aClients[Local].m_RenderPos + Dir * 38.0f * (Tee.m_Size / BASE_SIZE);
	if(SlashOn())
		AddCut(Local, Contact, angle(Dir));
	PlayAxeSound(Local, Tee.m_AttackKind == BcAxeSlash::KIND_BASH ? SOUND_BASH_HIT : SOUND_HIT, Contact);
}

bool CBcFireAxeFx::ReplaceHammerSound(int SoundId, vec2 Pos)
{
	if(!g_Config.m_BcFireAxeSoundReplace || !g_Config.m_SndGame)
		return false;
	const bool Fire = SoundId == SOUND_HAMMER_FIRE;
	const int Set = Fire ? SOUND_SWING : SOUND_HIT;
	if(m_aNumSounds[Set] <= 0)
		return false; // no axe sound to play instead: keep the hammer
	const float Now = Client()->LocalTime();

	// 08.11 (the user: "just mute them for the time"): while our own tee holds the axe, every hammer swing / hit
	// sound is muted, whoever made it; ours become axe sounds right here if they did not play already
	const int OwnId = GameClient()->m_aLocalIds[g_Config.m_ClDummy];
	if(in_range(OwnId, MAX_CLIENTS - 1) && m_aTees[OwnId].m_Init && m_aTees[OwnId].m_Weapon == WEAPON_HAMMER && SoundFor(OwnId))
	{
		const CTee &Own = m_aTees[OwnId];
		const float Dist = distance(GameClient()->m_aClients[OwnId].m_RenderPos, Pos);
		const float Last = Fire ? Own.m_LastSwingSound : Own.m_LastHitSound;
		// ours: the swing where we are, the hit a little in front; not if the axe sound has just played
		if(Dist < (Fire ? 120.0f : 200.0f) && Now - Last > 0.3f && (Fire || Now - Own.m_LastSwingSound < 0.6f))
		{
			const bool Bash = Own.m_AttackKind == BcAxeSlash::KIND_BASH && Now - Own.m_LastAttack < 0.5f;
			PlayAxeSound(OwnId, Fire ? (Bash ? SOUND_BASH_SWING : SOUND_SWING) : (Bash ? SOUND_BASH_HIT : SOUND_HIT), Pos);
		}
		return true;
	}

	// the swing sound comes from where the tee is; the hit from the hit tee, at most about 1.5 tee sizes in front of it
	// of all tees with a hammer the nearest one made it; only if that one has the axe (and its sounds) it is replaced,
	// so a plain hammer right next to us keeps its sound
	// a tee that has just swung (by any attack tick we know: the sound can come before we saw the swing) is the one
	// that made it, even far from where it is drawn (fast hammer flies, no prediction); without one, the nearest
	// tee with a hammer close by
	int Swung = -1, Near = -1;
	float SwungDist = 1e9f, NearDist = 1e9f;
	const int Window = Client()->GameTickSpeed() * 2 / 5;
	const auto Recent = [&](int AttackTick, int Tick) { return AttackTick > 0 && Tick - AttackTick <= Window && Tick >= AttackTick - 2; };
	const int GameTick = Client()->GameTick(g_Config.m_ClDummy);
	const int PredTick = Client()->PredGameTick(g_Config.m_ClDummy);
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		const CTee &Tee = m_aTees[i];
		// the sounds come while the snapshot is read: m_Snap is not filled yet (m_Active is 0 there), so our own record
		// of the tee counts (m_Init goes away when the tee does; off screen tees are not drawn, but they count too)
		if(!Tee.m_Init || Tee.m_Weapon != WEAPON_HAMMER)
			continue;
		bool Just = Now - Tee.m_LastAttack < 0.4f || Recent(Tee.m_AttackTick, GameTick) || Recent(Tee.m_AttackTick, PredTick);
		Just |= Recent(GameClient()->m_Snap.m_aCharacters[i].m_Cur.m_AttackTick, GameTick);
		if(BcIsOwnTee(GameClient(), i) && GameClient()->Predict())
		{
			CCharacter *pPredicted = GameClient()->m_PredictedWorld.GetCharacterById(i);
			if(pPredicted)
				Just |= Recent(pPredicted->GetAttackTick(), PredTick);
		}
		const float Dist = distance(GameClient()->m_aClients[i].m_RenderPos, Pos);
		if(Just && Dist < SwungDist)
		{
			SwungDist = Dist;
			Swung = i;
		}
		if(Dist < NearDist)
		{
			NearDist = Dist;
			Near = i;
		}
	}
	int Best = -1;
	if(Swung >= 0 && SwungDist < 160.0f)
		Best = Swung;
	else if(Fire && Near >= 0 && NearDist < (BcIsOwnTee(GameClient(), Near) ? 48.0f : 80.0f))
		Best = Near; // a hit always belongs to a tee that swung
	if(Best < 0 || !SoundFor(Best))
		return false;
	// the axe sound was not played for this one yet (the swing was not seen, the hit was not found): play it here
	const CTee &Tee = m_aTees[Best];
	const float Last = Fire ? Tee.m_LastSwingSound : Tee.m_LastHitSound;
	if(Now - Last > 0.3f)
	{
		const bool Bash = Tee.m_AttackKind == BcAxeSlash::KIND_BASH && Now - Tee.m_LastAttack < 0.5f;
		PlayAxeSound(Best, Fire ? (Bash ? SOUND_BASH_SWING : SOUND_SWING) : (Bash ? SOUND_BASH_HIT : SOUND_HIT), Pos);
	}
	return true;
}

float CBcFireAxeFx::SwingTime(int Kind) const
{
	return Kind == BcAxeSlash::KIND_HORIZONTAL ? 0.32f : SWING_TIME;
}

int CBcFireAxeFx::PickAttack(CTee &Tee) const
{
	switch(g_Config.m_BcFireAxeAnimAttack)
	{
	case 1: return BcAxeSlash::KIND_DIAGONAL;
	case 2: return BcAxeSlash::KIND_HORIZONTAL;
	case 3: return BcAxeSlash::KIND_BASH;
	case 4: return (int)random_float(0.0f, BcAxeSlash::NUM_KINDS - 0.001f);
	default: break;
	}
	// the combo: diagonal, diagonal (the other way), horizontal, diagonal, head bash; a pause starts it over
	static const int s_aCombo[] = {BcAxeSlash::KIND_DIAGONAL, BcAxeSlash::KIND_DIAGONAL, BcAxeSlash::KIND_HORIZONTAL, BcAxeSlash::KIND_DIAGONAL, BcAxeSlash::KIND_BASH};
	const int Num = (int)(sizeof(s_aCombo) / sizeof(s_aCombo[0]));
	if(Client()->LocalTime() - Tee.m_LastAttack > 1.2f)
		Tee.m_Combo = 0;
	const int Kind = s_aCombo[Tee.m_Combo % Num];
	Tee.m_Combo = (Tee.m_Combo + 1) % Num;
	return Kind;
}

void CBcFireAxeFx::StartAttack(int ClientId)
{
	CTee &Tee = m_aTees[ClientId];
	Tee.m_StrikeAim = angle(StrikeDir(ClientId));
	Tee.m_AttackKind = PickAttack(Tee);
	Tee.m_LastAttack = Client()->LocalTime();
	Tee.m_SwingT = 0.0f;
	Tee.m_SpinT = 100.0f;
	Tee.m_WaitT = 0.0f;
	if(SlashOn())
		AddSlash(ClientId);
	PlayAxeSound(ClientId, Tee.m_AttackKind == BcAxeSlash::KIND_BASH ? SOUND_BASH_SWING : SOUND_SWING, GameClient()->m_aClients[ClientId].m_RenderPos);
}

// ---- the axe sounds (bc_fire_axe_sound*), 08.11 ----
namespace
{
// read directly: the sound pack logic of LoadOpus skips files that are not in the pack (like CChainsawGui::LoadOwnOpus)
int LoadAxeOpus(IStorage *pStorage, ISound *pSound, const char *pPath)
{
	void *pData = nullptr;
	unsigned DataSize = 0;
	if(!pSound->IsSoundEnabled() || !pStorage->ReadFile(pPath, IStorage::TYPE_ALL, &pData, &DataSize))
		return -1;
	const int Id = pSound->LoadOpusFromMem(pData, DataSize, false, pPath);
	free(pData);
	return Id;
}
}

void CBcFireAxeFx::LoadSounds()
{
	static const char *const s_apSetName[NUM_SOUND_SETS] = {"axe_swing", "axe_hit", "axe_bash_swing", "axe_bash_hit"};
	for(int Set = 0; Set < NUM_SOUND_SETS; Set++)
	{
		m_aNumSounds[Set] = 0;
		m_aLastTake[Set] = -1;
		for(int Take = 1; Take <= MAX_SOUND_TAKES; Take++)
		{
			char aPath[128];
			str_format(aPath, sizeof(aPath), "chainsaw/sounds/%s%d.opus", s_apSetName[Set], Take);
			if(!Storage()->FileExists(aPath, IStorage::TYPE_ALL))
				break;
			const int Id = LoadAxeOpus(Storage(), Sound(), aPath);
			if(Id >= 0)
				m_aaSounds[Set][m_aNumSounds[Set]++] = Id;
		}
	}
}

bool CBcFireAxeFx::SoundFor(int ClientId) const
{
	return g_Config.m_BcFireAxeSound && g_Config.m_BcFireAxeSoundVolume > 0 && AxeFor(ClientId) &&
	       (g_Config.m_BcFireAxeSoundOthers || BcIsOwnTee(GameClient(), ClientId));
}

void CBcFireAxeFx::PlayAxeSound(int ClientId, int Set, vec2 Pos)
{
	if(!SoundFor(ClientId) || !g_Config.m_SndGame || Set < 0 || Set >= NUM_SOUND_SETS || m_aNumSounds[Set] <= 0)
		return;
	CTee &Tee = m_aTees[ClientId];
	// not twice within a moment (a fast clicker, a hit right after a hit)
	const float Now = Client()->LocalTime();
	const bool Swing = Set == SOUND_SWING || Set == SOUND_BASH_SWING;
	float &Last = Swing ? Tee.m_LastSwingSound : Tee.m_LastHitSound;
	if(Now - Last < 0.07f)
		return;
	Last = Now;
	// another take than the last one, a slightly different loudness: it does not sound like a loop
	int Take = (int)random_float(0.0f, m_aNumSounds[Set] - 0.001f);
	if(m_aNumSounds[Set] > 1 && Take == m_aLastTake[Set])
		Take = (Take + 1) % m_aNumSounds[Set];
	m_aLastTake[Set] = Take;
	// as loud as the hammer sounds they replace (the hammer plays at 1.0); our own tee right in the ears, not by position
	const float Base = Swing ? 0.8f : 1.0f;
	const float Volume = std::clamp(g_Config.m_BcFireAxeSoundVolume / 100.0f * Base * random_float(0.88f, 1.0f), 0.0f, 1.0f);
	if(BcIsOwnTee(GameClient(), ClientId))
		GameClient()->m_Sounds.PlaySample(CSounds::CHN_WORLD, m_aaSounds[Set][Take], 0, Volume);
	else
		GameClient()->m_Sounds.PlaySampleAt(CSounds::CHN_WORLD, m_aaSounds[Set][Take], 0, Volume, Pos);
}

// ---- the elegant strike (bc_fire_axe_anim_slash), 08.11 ----
bool CBcFireAxeFx::SlashOn() const
{
	return AnimOn() && g_Config.m_BcFireAxeAnimSlash && !BcFxSuppressed(GameClient());
}

vec2 CBcFireAxeFx::StrikeDir(int ClientId) const
{
	// where the tee aims: the mouse for our own tees, the snapshot angle for the others
	vec2 Dir = direction(GameClient()->m_aClients[ClientId].m_RenderCur.m_Angle / 256.0f);
	for(int Dummy = 0; Dummy < NUM_DUMMIES; Dummy++)
	{
		if(GameClient()->m_aLocalIds[Dummy] != ClientId)
			continue;
		const vec2 Mouse = GameClient()->m_Controls.m_aMousePos[Dummy];
		if(length(Mouse) > 0.001f)
			Dir = normalize(Mouse);
	}
	return Dir;
}

void CBcFireAxeFx::AddSlash(int ClientId)
{
	if(m_NumSlashes >= MAX_SLASHES)
		return;
	CTee &Tee = m_aTees[ClientId];
	Tee.m_SlashSide = -Tee.m_SlashSide;
	CSlash &Slash = m_aSlashes[m_NumSlashes++];
	Slash.m_ClientId = ClientId;
	Slash.m_Kind = Tee.m_AttackKind;
	Slash.m_Pos = GameClient()->m_aClients[ClientId].m_RenderPos;
	Slash.m_Aim = Tee.m_StrikeAim;
	Slash.m_Side = Tee.m_SlashSide;
	Slash.m_Radius = 46.0f * (Tee.m_Size / BASE_SIZE) * std::clamp(g_Config.m_BcFireAxeAnimSlashSize, 50, 200) / 100.0f;
	Slash.m_T = 0.0f;
	Slash.m_GlitterAcc = 0.0f;
}

void CBcFireAxeFx::AddCut(int ClientId, vec2 Pos, float Angle)
{
	if(m_NumCuts >= MAX_CUTS)
		return;
	CCut &Cut = m_aCuts[m_NumCuts++];
	Cut.m_Kind = m_aTees[ClientId].m_AttackKind;
	Cut.m_Pos = Pos;
	Cut.m_Angle = Angle;
	Cut.m_Size = 26.0f * (m_aTees[ClientId].m_Size / BASE_SIZE) * std::clamp(g_Config.m_BcFireAxeAnimSlashSize, 50, 200) / 100.0f;
	// when the crescent crosses the tee / the head arrives
	Cut.m_T = Cut.m_Kind == BcAxeSlash::KIND_BASH ? -0.12f : (Cut.m_Kind == BcAxeSlash::KIND_HORIZONTAL ? -0.08f : -0.06f);
}

void CBcFireAxeFx::UpdateSlashes(float Dt)
{
	const float d = Dt * AnimSpeed();
	const ColorRGBA Gold = FireColor(0.62f);
	for(int i = 0; i < m_NumSlashes;)
	{
		CSlash &Slash = m_aSlashes[i];
		Slash.m_T += d;
		if(Slash.m_T >= BcAxeSlash::SLASH_LIFE)
		{
			Slash = m_aSlashes[--m_NumSlashes];
			continue;
		}
		// the crescent stays with the tee
		if(GameClient()->m_Snap.m_aCharacters[Slash.m_ClientId].m_Active)
			Slash.m_Pos = GameClient()->m_aClients[Slash.m_ClientId].m_RenderPos;
		// a few tiny glitters fall off the leading tip while it sweeps
		if(Slash.m_T < 0.1f && Slash.m_Kind != BcAxeSlash::KIND_BASH)
		{
			const BcAxeSlash::CArc Arc = Slash.m_Kind == BcAxeSlash::KIND_HORIZONTAL ? BcAxeSlash::HorizontalArc(Slash.m_Pos, Slash.m_Aim, Slash.m_Radius) :
												  BcAxeSlash::DiagonalArc(Slash.m_Pos, Slash.m_Aim, Slash.m_Side, Slash.m_Radius);
			Slash.m_GlitterAcc += Dt * 70.0f;
			while(Slash.m_GlitterAcc >= 1.0f)
			{
				Slash.m_GlitterAcc -= 1.0f;
				float Tail, Head;
				BcAxeSlash::SlashRange(Slash.m_T, Tail, Head);
				const vec2 At = BcAxeSlash::ArcPoint(Arc, Head);
				const vec2 Along = BcAxeSlash::ArcPoint(Arc, std::min(1.0f, Head + 0.02f)) - BcAxeSlash::ArcPoint(Arc, std::max(0.0f, Head - 0.02f));
				const vec2 Fwd = length(Along) > 0.001f ? normalize(Along) : vec2(1.0f, 0.0f);
				const vec2 Out = normalize(At - Slash.m_Pos + vec2(0.0f, 0.001f));
				const float k = Slash.m_Radius / 46.0f;
				EmitSpark(At, (Fwd * random_float(30.0f, 90.0f) + Out * random_float(10.0f, 50.0f)) * k, random_float(1.6f, 2.8f) * k, random_float(0.25f, 0.45f),
					ColorRGBA(1.0f, 0.98f, 0.9f, 1.0f), Gold, 0.15f);
			}
		}
		i++;
	}
	for(int i = 0; i < m_NumCuts;)
	{
		CCut &Cut = m_aCuts[i];
		Cut.m_T += d;
		if(Cut.m_T >= BcAxeSlash::CUT_LIFE)
		{
			Cut = m_aCuts[--m_NumCuts];
			continue;
		}
		i++;
	}
}

void CBcFireAxeFx::DrawSlashes(const CcFx::CView &View)
{
	// inside the additive batch of OnRender (the atlas): flat colored quads from the white point
	if(m_NumSlashes == 0 && m_NumCuts == 0)
		return;
	const CcFx::CUv &W = m_pAtlas->m_White;
	const ColorRGBA Gold = FireColor(0.6f);
	Graphics()->QuadsSetSubsetFree(W.m_U0, W.m_V0, W.m_U0, W.m_V0, W.m_U0, W.m_V0, W.m_U0, W.m_V0);
	int Quads = 0;
	const auto Emit = [&](vec2 P0, vec2 P1, vec2 P2, vec2 P3, ColorRGBA C0, ColorRGBA C1, ColorRGBA C2, ColorRGBA C3) {
		Graphics()->SetColor4(C0, C1, C2, C3);
		const IGraphics::CFreeformItem Item(P0, P1, P2, P3);
		Graphics()->QuadsDrawFreeform(&Item, 1);
		Quads++;
	};
	for(int i = 0; i < m_NumSlashes; i++)
	{
		const CSlash &Slash = m_aSlashes[i];
		if(!View.Sees(Slash.m_Pos, Slash.m_Radius * 1.6f))
			continue;
		if(Slash.m_Kind == BcAxeSlash::KIND_BASH)
			BcAxeSlash::Thrust(Slash.m_Pos, Slash.m_Aim, Slash.m_Radius * 1.1f, Slash.m_T, Gold, Emit);
		else if(Slash.m_Kind == BcAxeSlash::KIND_HORIZONTAL)
			BcAxeSlash::Slash(BcAxeSlash::HorizontalArc(Slash.m_Pos, Slash.m_Aim, Slash.m_Radius), Slash.m_T, Gold, 1.0f, 0.17f, Emit);
		else
			BcAxeSlash::Slash(BcAxeSlash::DiagonalArc(Slash.m_Pos, Slash.m_Aim, Slash.m_Side, Slash.m_Radius), Slash.m_T, Gold, 1.0f, 0.2f, Emit);
	}
	for(int i = 0; i < m_NumCuts; i++)
	{
		const CCut &Cut = m_aCuts[i];
		if(!View.Sees(Cut.m_Pos, Cut.m_Size * 1.6f))
			continue;
		if(Cut.m_Kind == BcAxeSlash::KIND_BASH)
			BcAxeSlash::Star(Cut.m_Pos, Cut.m_Angle, Cut.m_Size * 0.95f, Cut.m_T, Gold, Emit);
		else
			BcAxeSlash::Cut(Cut.m_Pos, Cut.m_Kind == BcAxeSlash::KIND_HORIZONTAL ? 0.0f : Cut.m_Angle, Cut.m_Size, Cut.m_T, Gold, Emit);
	}
	CcFx::CountQuads(Quads);
}

void CBcFireAxeFx::PreviewSpin()
{
	const int Local = GameClient()->m_Snap.m_LocalClientId;
	if(!in_range(Local, MAX_CLIENTS - 1))
		return;
	CTee &Tee = m_aTees[Local];
	Tee.m_SpinT = 0.0f;
	Tee.m_SwingT = 100.0f;
	Tee.m_WaitT = 0.0f;
}

void CBcFireAxeFx::UpdateFlames(float Dt)
{
	for(int i = 0; i < m_NumFlames;)
	{
		CFlame &Flame = m_aFlames[i];
		Flame.m_Life += Dt;
		if(Flame.m_Life >= Flame.m_LifeSpan)
		{
			Flame = m_aFlames[--m_NumFlames];
			continue;
		}
		Flame.m_Pos += Flame.m_Vel * Dt;
		Flame.m_Vel *= std::pow(0.25f, Dt);
		Flame.m_Vel.y -= 90.0f * Dt;
		i++;
	}
}

void CBcFireAxeFx::PreviewSummon()
{
	const int Local = GameClient()->m_Snap.m_LocalClientId;
	if(!in_range(Local, MAX_CLIENTS - 1) || !AxeFor(Local))
		return;
	m_aTees[Local].m_Show = 0.0f;
	m_aTees[Local].m_Burst = false;
	m_aTees[Local].m_NoWait = true;
}

void CBcFireAxeFx::PreviewHit()
{
	const int Local = GameClient()->m_Snap.m_LocalClientId;
	if(!in_range(Local, MAX_CLIENTS - 1))
		return;
	CTee &Tee = m_aTees[Local];
	Tee.m_Heat = std::min(1.0f, Tee.m_Heat + 0.34f);
	Tee.m_HeatHold = g_Config.m_BcFireAxeHeatTime / 10.0f;
	Tee.m_Flash = 1.0f;
	Burst(Local, 0.5f);
}

void CBcFireAxeFx::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	const float Now = Client()->LocalTime();
	float Dt = m_LastTime < 0.0f ? 0.0f : Now - m_LastTime;
	m_LastTime = Now;
	Dt = std::clamp(Dt, 0.0f, 0.1f);
	if(GameClient()->IsWorldPaused() || GameClient()->IsDemoPlaybackPaused())
		Dt = 0.0f;

	// the flaming hammer was turned on (or off) right now: the axes come out of the fire (or burn away)
	const bool OnNow = On();
	if(OnNow != m_WasOn)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			CTee &Tee = m_aTees[i];
			if(!Tee.m_Init)
				continue;
			if(OnNow && g_Config.m_BcFireAxeSummon && Tee.m_Weapon == WEAPON_HAMMER)
			{
				Tee.m_Show = 0.0f;
				Tee.m_Burst = false;
			}
			else if(OnNow)
				Tee.m_Show = 1.0f;
			else if(Tee.m_Show > 0.5f && Tee.m_Weapon == WEAPON_HAMMER)
				Burst(i, 0.8f);
		}
		m_WasOn = OnNow;
	}

	const bool MenuOpen = GameClient()->m_Menus.IsActive();
	for(int i = 0; i < MAX_CLIENTS; i++)
		UpdateTee(i, Dt, MenuOpen);
	UpdateFlames(Dt);
	UpdateSlashes(Dt);
	m_Sparks.Update(Dt);

	// light, tongues and sparks of all axes: one additive batch with the shared atlas
	bool AnyLight = false;
	for(const CTee &Tee : m_aTees)
		AnyLight |= Tee.m_Init && Now - Tee.m_DrawTime < 0.15f && ((Tee.m_Show > 0.0f && Tee.m_Show < 1.0f) || Tee.m_Flash > 0.0f);
	if(!AnyLight && m_NumFlames == 0 && !m_Sparks.HasAny() && m_NumSlashes == 0 && m_NumCuts == 0)
		return;
	const CcFx::CAtlas &Atlas = *m_pAtlas;
	const CcFx::CView View = CcFx::View(Graphics(), 100.0f);
	CcFx::Begin(Graphics(), Atlas.m_Texture, true);
	for(int i = 0; i < MAX_CLIENTS && AnyLight; i++)
	{
		const CTee &Tee = m_aTees[i];
		if(!Tee.m_Init || Now - Tee.m_DrawTime >= 0.15f)
			continue;
		const vec2 Head = HeadPos(Tee);
		const float k = Tee.m_Size / BASE_SIZE;
		// a light grows in the hand and gives way to the axe
		float Light = 0.0f;
		if(Tee.m_Show > 0.0f && Tee.m_Show < 1.0f)
			Light = Smooth(0.0f, 0.3f, Tee.m_Show) * (1.0f - Smooth(0.6f, 0.95f, Tee.m_Show));
		Light = std::max(Light, Tee.m_Flash * 0.8f);
		if(Light <= 0.01f || !View.Sees(Head, 80.0f * k))
			continue;
		const float Pulse = 0.9f + 0.1f * std::sin(Now * 30.0f + i);
		CcFx::QuadAxis(Graphics(), Head, 150.0f * k * Light, 150.0f * k * Light, Atlas.m_Glow, FireColor(0.55f).WithAlpha(0.7f * Light * Tee.m_Alpha));
		CcFx::QuadAxis(Graphics(), Head, 62.0f * k * Light * Pulse, 62.0f * k * Light * Pulse, Atlas.m_Glow, ColorRGBA(1.0f, 0.98f, 0.9f, 0.95f * Light * Tee.m_Alpha));
	}
	const ColorRGBA Gold = FireColor(0.6f);
	for(int i = 0; i < m_NumFlames; i++)
	{
		const CFlame &Flame = m_aFlames[i];
		const float t = Flame.m_Life / Flame.m_LifeSpan;
		const float Grow = std::sin(t * pi);
		const float Size = Flame.m_Size * (0.55f + 0.6f * Grow);
		if(!View.Sees(Flame.m_Pos, Size * 2.0f))
			continue;
		const vec2 Dir = normalize(vec2(Flame.m_Vel.x * 0.004f, -1.0f));
		vec2 Center, Right, Up;
		CcFx::TongueQuad(Flame.m_Pos, Dir, Size, Center, Right, Up);
		if(Flame.m_Flip)
			Right = -Right;
		const float Fade = std::pow(Grow, 0.7f);
		CcFx::Quad(Graphics(), Center, Right, Up, Atlas.m_aaFlame[Flame.m_Variant][0], Gold.WithAlpha(0.55f * Fade));
		CcFx::Quad(Graphics(), Center, Right, Up, Atlas.m_aaFlame[Flame.m_Variant][BcFireShapes::LAYERS - 1], FgfGlow::Whiten(Gold, 0.55f + 0.45f * Flame.m_Hot).WithAlpha(0.8f * Fade));
	}
	DrawSlashes(View);
	m_Sparks.Draw(Graphics(), Atlas, View);
	CcFx::End(Graphics());
}
