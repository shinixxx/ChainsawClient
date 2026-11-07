/* ChainsawClient: golden axe instead of the hammer (see fire_axe.h, the picture is in fire_axe_art.h) */
#include "fire_axe.h"

#include "cc_fx_common.h"
#include "fire_axe_art.h"
#include "fgf_glow.h"
#include "fire_ice_shapes.h"
#include "own_tee.h"

#include <base/math.h>

#include <engine/image.h>
#include <engine/shared/config.h>

#include <game/client/components/menus.h>
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
	OnReset();
}

void CBcFireAxeFx::OnReset()
{
	for(CTee &Tee : m_aTees)
		Tee = CTee();
	m_NumFlames = 0;
	m_Sparks.Clear();
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
	const float Size = BASE_SIZE * Scale * g_Config.m_BcFireAxeSize / 100.0f;
	const vec2 AxisX(std::cos(Rotation), std::sin(Rotation));
	const vec2 AxisY = vec2(-AxisX.y, AxisX.x) * (Tee.m_Flip ? -1.0f : 1.0f);
	// the axe head sits where the hammer head was
	Tee.m_Center = WeaponPos + AxisX * (HAMMER_HEAD * Scale - (BcAxe::HEAD_U - 0.5f) * Size);
	Tee.m_AxisX = AxisX;
	Tee.m_AxisY = AxisY;
	Tee.m_Size = Size;
	Tee.m_DrawTime = Client()->LocalTime();

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
	if(WhiteHot > 0.01f || HeatOver > 0.01f)
	{
		const ColorRGBA Hot = FireColor(0.6f);
		CcFx::Begin(Graphics(), m_Texture, true);
		Slices(m_BodyUv, NumSlices, 1.0f, [&](float u) {
			const float White = std::min(1.0f, WhiteHot + Front(u) * 0.8f);
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
	vec2 Dir = direction(GameClient()->m_aClients[ClientId].m_RenderCur.m_Angle / 256.0f);
	for(int Dummy = 0; Dummy < NUM_DUMMIES; Dummy++)
	{
		if(GameClient()->m_aLocalIds[Dummy] != ClientId)
			continue;
		const vec2 Mouse = GameClient()->m_Controls.m_aMousePos[Dummy];
		if(length(Mouse) > 0.001f)
			Dir = normalize(Mouse);
	}
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
			Tee.m_Show = Target;
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

	if(Tee.m_AttackTick >= 0 && AttackTick != Tee.m_AttackTick && Weapon == WEAPON_HAMMER && Tee.m_Show > 0.5f && Drawn && g_Config.m_BcFireAxeHeat)
		CheckHit(ClientId);
	Tee.m_AttackTick = AttackTick;

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
	m_Sparks.Update(Dt);

	// light, tongues and sparks of all axes: one additive batch with the shared atlas
	bool AnyLight = false;
	for(const CTee &Tee : m_aTees)
		AnyLight |= Tee.m_Init && Now - Tee.m_DrawTime < 0.15f && ((Tee.m_Show > 0.0f && Tee.m_Show < 1.0f) || Tee.m_Flash > 0.0f);
	if(!AnyLight && m_NumFlames == 0 && !m_Sparks.HasAny())
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
	m_Sparks.Draw(Graphics(), Atlas, View);
	CcFx::End(Graphics());
}
