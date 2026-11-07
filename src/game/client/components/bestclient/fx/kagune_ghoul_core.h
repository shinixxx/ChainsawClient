/* ChainsawClient: the ghoul eye (kakugan) and the way the kagune comes out - part of bc_kagune, no own settings.
   Engine free like kagune_core.h, so the offline preview runs exactly this code.

   The kagune comes out: first something red gathers behind the tee (red cells pulled in from around, a swelling
   light where the tentacles grow), the tentacles are buds there, then they grow out one after another.
   Emerge() does it right after BcKagune::Update by shortening the tentacles towards their roots; the cells go into
   the kagune's own particles (no draw calls of its own).

   The eye: one eye of the tee (like Kaneki's left one) turns into a kakugan while the kagune is out: the white goes
   black, a red iris opens from the pupil (black ring around it, a black dot in the middle), thin veins crawl out
   around the eye, a soft red glow beats with the kagune. The derp skin gets its own eye: its big round eyeball,
   black instead of white, the red iris where its pupil was. The picture is drawn by code (BuildTexture). */
#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_KAGUNE_GHOUL_CORE_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_KAGUNE_GHOUL_CORE_H

#include "kagune_core.h"

#include <vector>

namespace BcGhoul
{
using BcKagune::Clamp01;
using BcKagune::Smooth;

// ---- the texture: 8 cells of 64x64 in a row; a cell is the 32x32 eye cell of a skin at twice the size ----
constexpr int CELL = 64;
constexpr int TEX_W = CELL * 8;
constexpr int TEX_H = CELL;
enum
{
	CELL_SCLERA = 0, // the black eye, default skin
	CELL_SCLERA_ANGRY, // the same with the angry lid
	CELL_SCLERA_DERP, // the derp eyeball
	CELL_IRIS, // red iris, black ring, black dot (centered, radius IRIS_R of the half cell)
	CELL_VEINS, // thin dark red veins around a center
	CELL_GLOW, // soft round light
	CELL_SCLERA_DERP_HAPPY, // the derp eyeball squinting under its lid (happy)
	NUM_CELLS
};
constexpr float IRIS_R = 0.86f; // the iris disk in the iris cell, of the half cell

inline float CellU0(int Cell) { return (float)(Cell * CELL) / TEX_W; }
inline float CellU1(int Cell) { return (float)((Cell + 1) * CELL) / TEX_W; }

// where the iris sits in a skin's 32 unit eye cell, and its radii
struct CIris
{
	float m_X, m_Y, m_Rx, m_Ry;
};
inline CIris IrisOf(int Sclera)
{
	if(Sclera == CELL_SCLERA_DERP)
		return {7.8f, 12.4f, 3.7f, 3.7f};
	if(Sclera == CELL_SCLERA_ANGRY)
		return {16.3f, 17.6f, 3.3f, 3.8f};
	return {16.5f, 16.3f, 3.5f, 4.4f};
}

// the emotes (the same numbers as the game: EMOTE_NORMAL, PAIN, HAPPY, SURPRISE, ANGRY, BLINK)
enum
{
	EYE_NORMAL = 0,
	EYE_PAIN,
	EYE_HAPPY,
	EYE_SURPRISE,
	EYE_ANGRY,
	EYE_BLINK,
};
// which kakugan for a skin and an emote; false = the eye is closed there (it is left as it is).
// The default tee has its eyes shut when in pain (>) and happy (^) and while blinking; derp keeps them open
// on every emote: pain and angry look like normal, happy squints under a lid, surprised looks to the other side.
inline bool EyeVariant(bool Derp, int Emote, int &Sclera, CIris &Iris)
{
	if(Emote == EYE_BLINK)
		return false;
	if(Derp)
	{
		Sclera = Emote == EYE_HAPPY ? CELL_SCLERA_DERP_HAPPY : CELL_SCLERA_DERP;
		if(Emote == EYE_HAPPY)
			Iris = {8.2f, 6.4f, 3.3f, 3.3f}; // it looks up
		else if(Emote == EYE_SURPRISE)
			Iris = {13.6f, 12.2f, 3.5f, 3.5f};
		else
			Iris = IrisOf(CELL_SCLERA_DERP);
		return true;
	}
	if(Emote == EYE_PAIN || Emote == EYE_HAPPY)
		return false;
	Sclera = Emote == EYE_ANGRY ? CELL_SCLERA_ANGRY : CELL_SCLERA;
	Iris = IrisOf(Sclera);
	return true;
}

struct V2
{
	float x, y;
};
inline float Len(float x, float y) { return std::sqrt(x * x + y * y); }
inline float SdEllipse(float x, float y, float Cx, float Cy, float Rx, float Ry)
{
	// a good enough distance for antialiasing
	const float dx = (x - Cx) / Rx, dy = (y - Cy) / Ry;
	const float k = Len(dx, dy);
	return (k - 1.0f) * std::min(Rx, Ry);
}
inline float SdPolygon(const V2 *pV, int N, float x, float y)
{
	float d = (x - pV[0].x) * (x - pV[0].x) + (y - pV[0].y) * (y - pV[0].y);
	float s = 1.0f;
	for(int i = 0, j = N - 1; i < N; j = i, i++)
	{
		const float ex = pV[j].x - pV[i].x, ey = pV[j].y - pV[i].y;
		const float wx = x - pV[i].x, wy = y - pV[i].y;
		const float t = Clamp01((wx * ex + wy * ey) / (ex * ex + ey * ey));
		const float bx = wx - ex * t, by = wy - ey * t;
		d = std::min(d, bx * bx + by * by);
		const bool c1 = y >= pV[i].y, c2 = y < pV[j].y, c3 = ex * wy > ey * wx;
		if((c1 && c2 && c3) || (!c1 && !c2 && !c3))
			s = -s;
	}
	return s * std::sqrt(d);
}
// the derp eyeball in its 32 unit cell (traced from the skin)
inline float SdDerp(float x, float y)
{
	static const V2 s_aV[] = {{4.6f, 6.5f}, {13.6f, 6.3f}, {16.4f, 7.7f}, {18.7f, 9.7f}, {18.7f, 13.4f}, {16.6f, 15.5f}, {15.0f, 17.3f},
		{13.0f, 19.3f}, {12.0f, 20.5f}, {5.0f, 20.7f}, {3.3f, 19.1f}, {3.3f, 7.6f}};
	return SdPolygon(s_aV, (int)(sizeof(s_aV) / sizeof(s_aV[0])), x, y);
}
// the happy derp eye: the same eyeball, higher, squinting under a lid (traced from the skin)
inline float SdDerpHappy(float x, float y)
{
	static const V2 s_aV[] = {{6.0f, 0.2f}, {14.0f, 0.2f}, {16.2f, 1.2f}, {18.2f, 2.6f}, {19.2f, 3.6f}, {19.2f, 8.0f}, {17.8f, 9.4f}, {16.2f, 11.2f},
		{15.0f, 12.4f}, {13.6f, 14.0f}, {12.4f, 15.4f}, {11.6f, 16.0f}, {7.0f, 16.0f}, {4.0f, 14.6f}, {3.0f, 13.4f}, {3.0f, 3.0f}, {4.5f, 1.6f}};
	return SdPolygon(s_aV, (int)(sizeof(s_aV) / sizeof(s_aV[0])), x, y);
}
inline float SdSclera(int Cell, float x, float y)
{
	if(Cell == CELL_SCLERA_DERP_HAPPY)
		return SdDerpHappy(x, y);
	if(Cell == CELL_SCLERA_DERP)
		return SdDerp(x, y);
	// the default eye is a black upright oval; a little bigger, so it covers the skin's eye
	const float Oval = SdEllipse(x, y, 16.5f, 16.6f, 5.2f, 8.5f);
	if(Cell == CELL_SCLERA_ANGRY)
		return std::max(Oval, ((x - y) - 5.6f) * 0.7071f); // the lid of the angry eye
	return Oval;
}

inline void Put(uint8_t *pRgba, int X, int Y, float r, float g, float b, float a)
{
	uint8_t *p = &pRgba[(Y * TEX_W + X) * 4];
	p[0] = (uint8_t)std::lround(Clamp01(r) * 255.0f);
	p[1] = (uint8_t)std::lround(Clamp01(g) * 255.0f);
	p[2] = (uint8_t)std::lround(Clamp01(b) * 255.0f);
	p[3] = (uint8_t)std::lround(Clamp01(a) * 255.0f);
}

// a vein: a wavy branch from the inner radius out, with a fork; distance in cell units (half cell = 1)
inline float SdVeins(float x, float y)
{
	// smooth thin veins that start at the edge of the eye and run outwards, a short twig on some
	float d = 1e9f;
	constexpr int BRANCHES = 7;
	for(int b = 0; b < BRANCHES; b++)
	{
		const float Base = b * 2.0f * 3.14159265f / BRANCHES + 0.45f * std::sin(b * 2.3f + 0.5f);
		const float Reach = 0.78f + 0.16f * std::sin(b * 1.7f + 1.0f);
		const float Bend = 0.28f * std::sin(b * 3.1f);
		constexpr int Steps = 8;
		float px = std::cos(Base) * 0.4f, py = std::sin(Base) * 0.4f;
		for(int k = 1; k <= Steps; k++)
		{
			const float s = (float)k / Steps;
			const float r = 0.4f + (Reach - 0.4f) * s;
			const float a = Base + Bend * s * s;
			const float qx = std::cos(a) * r, qy = std::sin(a) * r;
			const float ex = qx - px, ey = qy - py;
			const float t = Clamp01(((x - px) * ex + (y - py) * ey) / (ex * ex + ey * ey));
			const float w = 0.03f * (1.0f - 0.75f * (k - 1.0f + t) / Steps);
			d = std::min(d, Len(x - px - ex * t, y - py - ey * t) - w);
			if(k == 4 && (b % 2) == 0)
			{
				const float fa = a + 0.55f;
				const float fx = qx + std::cos(fa) * 0.16f, fy = qy + std::sin(fa) * 0.16f;
				const float gx = fx - qx, gy = fy - qy;
				const float u = Clamp01(((x - qx) * gx + (y - qy) * gy) / (gx * gx + gy * gy));
				d = std::min(d, Len(x - qx - gx * u, y - qy - gy * u) - 0.014f * (1.0f - u));
			}
			px = qx;
			py = qy;
		}
	}
	return d;
}

inline void BuildTexture(uint8_t *pRgba)
{
	const float Px = 32.0f / CELL; // one texel in skin eye cell units
	for(int Cell = 0; Cell < NUM_CELLS; Cell++)
	{
		for(int Y = 0; Y < CELL; Y++)
		{
			for(int X = 0; X < CELL; X++)
			{
				const int TX = Cell * CELL + X;
				// skin eye cell units (0..32) and the centered half-cell units (-1..1)
				const float ex = (X + 0.5f) * Px, ey = (Y + 0.5f) * Px;
				const float cx = (X + 0.5f) / (CELL * 0.5f) - 1.0f, cy = (Y + 0.5f) / (CELL * 0.5f) - 1.0f;
				switch(Cell)
				{
				case CELL_SCLERA:
				case CELL_SCLERA_ANGRY:
				case CELL_SCLERA_DERP:
				case CELL_SCLERA_DERP_HAPPY:
				{
					const float d = SdSclera(Cell, ex, ey);
					const float Outline = Cell == CELL_SCLERA_DERP || Cell == CELL_SCLERA_DERP_HAPPY ? 1.05f : 0.0f; // derp has its black outline
					const float Cov = Clamp01((0.5f * Px - d) / Px);
					// almost black, a dark red sheen towards the bottom of the eye, the outline pure black
					const float Inside = Clamp01((-d - Outline) / Px + 0.5f);
					const float Sheen = 0.06f * Smooth(-0.2f, 1.0f, (ey - 10.0f) / 12.0f);
					Put(pRgba, TX, Y, mix(0.0f, 0.05f + Sheen, Inside), mix(0.0f, 0.005f, Inside), mix(0.0f, 0.012f, Inside), Cov);
					break;
				}
				case CELL_IRIS:
				{
					const float r = Len(cx, cy) / IRIS_R;
					const float a = std::atan2(cy, cx);
					const float Edge = Clamp01((1.0f - r) * CELL * 0.5f * IRIS_R + 0.5f);
					// red: bright in the middle, deep at the edge, fine streaks around
					const float Streak = 0.86f + 0.14f * std::sin(a * 15.0f + 2.0f * std::sin(a * 4.0f));
					float R = (1.0f - 0.42f * Smooth(0.2f, 0.8f, r)) * Streak;
					float G = 0.07f * (1.0f - Smooth(0.0f, 0.5f, r));
					float B = 0.06f * (1.0f - Smooth(0.0f, 0.6f, r));
					// the black ring around and the black dot in the middle
					const float Ring = Smooth(0.74f, 0.82f, r);
					const float Dot = 1.0f - Smooth(0.22f, 0.3f, r);
					const float Dark = std::max(Ring, Dot);
					R *= 1.0f - Dark;
					G *= 1.0f - Dark;
					B *= 1.0f - Dark;
					// a wet glint
					const float Glint = 1.0f - Smooth(0.08f, 0.14f, Len(cx + 0.3f * IRIS_R, cy + 0.34f * IRIS_R));
					R = mix(R, 1.0f, Glint * 0.85f);
					G = mix(G, 0.9f, Glint * 0.85f);
					B = mix(B, 0.9f, Glint * 0.85f);
					Put(pRgba, TX, Y, R, G, B, Edge);
					break;
				}
				case CELL_VEINS:
				{
					const float d = SdVeins(cx, cy);
					const float Cov = Clamp01((0.5f / (CELL * 0.5f) - d) * CELL * 0.5f);
					const float Fade = 1.0f - Smooth(0.6f, 0.98f, Len(cx, cy));
					Put(pRgba, TX, Y, 0.42f, 0.01f, 0.04f, Cov * Fade * 0.85f);
					break;
				}
				case CELL_GLOW:
				{
					const float r = Len(cx, cy);
					const float Inv = Clamp01(1.0f - r);
					Put(pRgba, TX, Y, 1.0f, 1.0f, 1.0f, Inv * Inv * (0.35f + 0.65f * Inv));
					break;
				}
				}
			}
		}
	}
	// the cells not used stay empty
	for(int Y = 0; Y < CELL; Y++)
		for(int X = NUM_CELLS * CELL; X < TEX_W; X++)
			Put(pRgba, X, Y, 0.0f, 0.0f, 0.0f, 0.0f);
}

// ---- the kagune coming out ----
constexpr float GATHER_TIME = 0.32f; // red gathers behind the tee
constexpr float GROW_TIME = 0.42f; // the tentacles grow out
constexpr float GROW_STAGGER = 0.045f; // one after another

struct CEmerge
{
	float m_T = -1.0f; // seconds since it started to come out, < 0 = not out
	float m_LastPresence = 0.0f;
	float m_Eye = 0.0f; // 0 .. 1 the kakugan
	float m_MoteAcc = 0.0f;
	float m_Time = 0.0f;
};

// right after BcKagune::Update for this tee
inline void Emerge(CEmerge &E, BcKagune::CTee &K, const BcKagune::CInput &In, BcKagune::CParticles &Particles, float Amount)
{
	using namespace BcKagune;
	const float Dt = In.m_Dt;
	E.m_Time += Dt;
	if(K.m_Presence > 0.0f && E.m_LastPresence <= 0.0f)
	{
		E.m_T = 0.0f;
		E.m_MoteAcc = 0.0f;
		// the light that swells where the tentacles will break out
		if(Amount > 0.0f)
		{
			BcKagune::CMote *pFlash = Particles.New();
			if(pFlash)
			{
				pFlash->m_Pos = K.m_Root;
				pFlash->m_Vel = vec2(0.0f, 0.0f);
				pFlash->m_LifeSpan = GATHER_TIME + 0.14f;
				pFlash->m_Size = 34.0f * In.m_Scale;
				pFlash->m_Tone = 0.6f;
				pFlash->m_Rise = 0.0f;
				pFlash->m_Flash = true;
			}
		}
	}
	else if(K.m_Presence <= 0.0f)
		E.m_T = -1.0f;
	E.m_LastPresence = K.m_Presence;
	if(E.m_T >= 0.0f)
		E.m_T += Dt;

	const float t = E.m_T;
	if(t >= 0.0f && t < GATHER_TIME + GROW_TIME + GROW_STAGGER * K.m_Count)
	{
		// buds that beat while the red gathers, then they grow out one after another
		for(int i = 0; i < K.m_Count; i++)
		{
			CTentacle &T = K.m_aTentacles[i];
			const float Ti = t - GATHER_TIME - GROW_STAGGER * i;
			float g;
			if(Ti < 0.0f)
				g = 0.06f + 0.18f * Smooth(0.0f, GATHER_TIME, t) + 0.035f * std::sin(t * 38.0f + i * 1.3f); // a beating red mass over the back
			else
				g = 0.24f + 0.76f * EaseOutBack(Clamp01(Ti / GROW_TIME));
			const vec2 Root = T.m_aPos[0];
			for(int j = 1; j < SIM_POINTS; j++)
				T.m_aPos[j] = Root + (T.m_aPos[j] - Root) * g;
			T.m_RestLength *= std::max(g, 0.05f);
			T.m_TrailNum = 0;
		}
		// red cells pulled in from around
		if(t < GATHER_TIME + 0.06f && Amount > 0.0f && Dt > 0.0f)
		{
			E.m_MoteAcc += Dt * 190.0f * Amount;
			while(E.m_MoteAcc >= 1.0f)
			{
				E.m_MoteAcc -= 1.0f;
				BcKagune::CMote *pMote = Particles.New();
				if(!pMote)
					break;
				const float a = RandRange(Particles.m_Seed, 0.0f, 2.0f * PI);
				const vec2 Out(std::cos(a), std::sin(a));
				const float r = RandRange(Particles.m_Seed, 26.0f, 50.0f) * In.m_Scale;
				pMote->m_Pos = K.m_Root + Out * r;
				// in to the root with a little swirl, they arrive as the tentacles break out
				pMote->m_Vel = -Out * (r / 0.2f) + vec2(-Out.y, Out.x) * RandRange(Particles.m_Seed, 30.0f, 80.0f) * In.m_Scale;
				pMote->m_LifeSpan = RandRange(Particles.m_Seed, 0.17f, 0.24f);
				pMote->m_Size = RandRange(Particles.m_Seed, 2.0f, 3.8f) * In.m_Scale;
				pMote->m_Tone = 0.25f * Rand01(Particles.m_Seed); // deep red, few light ones
				pMote->m_Drag = 0.4f;
				pMote->m_Rise = 0.0f;
			}
		}
	}

	// the eye opens a moment after the red starts to gather, closes quickly when the kagune goes in
	if(K.m_Presence > 0.5f && t >= 0.12f)
		E.m_Eye = std::min(1.0f, E.m_Eye + Dt / 0.38f);
	else if(K.m_Presence < 0.5f)
		E.m_Eye = std::max(0.0f, E.m_Eye - Dt / 0.15f);
}

// how the eye looks at Eye (0 .. 1): its parts scale and alpha
struct CEyeLook
{
	float m_ScleraScale, m_ScleraAlpha;
	float m_IrisScale, m_IrisAlpha;
	float m_VeinScale, m_VeinAlpha;
	float m_GlowAlpha;
};
inline CEyeLook EyeLook(float Eye, float Time)
{
	CEyeLook L;
	L.m_ScleraScale = 0.35f + 0.65f * Smooth(0.0f, 0.45f, Eye);
	L.m_ScleraAlpha = Smooth(0.0f, 0.25f, Eye);
	L.m_IrisScale = BcKagune::EaseOutBack(Clamp01((Eye - 0.25f) / 0.55f));
	L.m_IrisAlpha = Smooth(0.25f, 0.4f, Eye);
	L.m_VeinScale = 0.45f + 0.55f * Smooth(0.35f, 1.0f, Eye);
	L.m_VeinAlpha = 0.6f * Smooth(0.35f, 1.0f, Eye);
	// a flash as it opens, then it beats with the kagune
	const float Open = std::max(0.0f, 1.0f - std::fabs(Eye - 0.62f) / 0.3f);
	const float x = BcKagune::Fract(Time / 1.15f);
	const float Beat = std::exp(-std::pow(x / 0.05f, 2.0f)) + 0.7f * std::exp(-std::pow((x - 0.17f) / 0.06f, 2.0f));
	L.m_GlowAlpha = Smooth(0.3f, 0.7f, Eye) * (0.32f + 0.2f * Beat) + 0.6f * Open;
	return L;
}
}

#endif
