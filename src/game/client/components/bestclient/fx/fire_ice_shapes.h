/* ChainsawClient: procedural shapes for the Fire Force style fire (no engine dependencies) */
#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_FIRE_ICE_SHAPES_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_FIRE_ICE_SHAPES_H

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace BcFireShapes
{
constexpr int CELL_W = 64;
constexpr int CELL_H = 128;
constexpr int VARIANTS = 8; // 0..3 hammer tongues, 4..7 ring flames
constexpr int RING_FIRST = 4;
constexpr int LAYERS = 3;
constexpr int ATLAS_W = CELL_W * VARIANTS;
constexpr int ATLAS_H = CELL_H * LAYERS;
// the round base of every tongue sits this far above the bottom of its cell (fraction of the height)
constexpr float BASE_FROM_BOTTOM = 0.15f;

struct CStroke
{
	float m_Bend; // radians of gentle bend over the whole length
	float m_Curl; // radians of the hooked curl at the tip
	float m_Radius; // px at the start
	float m_Length; // px along the spine
	float m_Wave; // edge waviness
};

struct CTongue
{
	float m_StartX; // px inside the cell
	CStroke m_Main;
	float m_BranchAt; // 0..1 along the main stroke, 0 = no branch
	float m_BranchTurn; // radians relative to the main heading
	CStroke m_Branch;
	float m_Branch2At = 0.0f; // optional second lick
	float m_Branch2Turn = 0.0f;
	CStroke m_Branch2 = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
};

// four hand-tuned tongues: a wide body with a hooked tip and a small curled side lick
inline const CTongue &Tongue(int Variant)
{
	static const CTongue s_aTongues[VARIANTS] = {
		{26.0f, {0.25f, 3.1f, 17.0f, 96.0f, 0.10f}, 0.34f, -0.95f, {-0.2f, -3.4f, 6.5f, 30.0f, 0.05f}},
		{38.0f, {-0.3f, -3.3f, 16.0f, 90.0f, 0.12f}, 0.40f, 0.9f, {0.2f, 3.6f, 6.0f, 28.0f, 0.05f}},
		{28.0f, {0.4f, 2.6f, 18.0f, 102.0f, 0.08f}, 0.30f, -1.05f, {0.0f, -2.8f, 7.5f, 36.0f, 0.06f}},
		{36.0f, {-0.15f, -3.9f, 15.0f, 84.0f, 0.12f}, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f, 0.0f, 0.0f}},
		// ring flames: S-curves with two or three curled tips
		{30.0f, {0.7f, -2.9f, 17.0f, 98.0f, 0.1f}, 0.3f, -1.1f, {-0.3f, -3.2f, 7.0f, 34.0f, 0.05f}, 0.5f, 0.9f, {0.2f, 3.0f, 5.5f, 26.0f, 0.05f}},
		{32.0f, {-0.35f, -2.4f, 18.0f, 96.0f, 0.08f}, 0.38f, 0.75f, {0.25f, 3.3f, 9.5f, 46.0f, 0.06f}},
		{24.0f, {0.9f, 2.2f, 19.0f, 88.0f, 0.1f}, 0.28f, -1.2f, {0.0f, -3.4f, 7.0f, 30.0f, 0.05f}, 0.55f, -0.7f, {0.0f, -2.6f, 5.0f, 22.0f, 0.05f}},
		{36.0f, {-0.6f, 2.8f, 15.0f, 104.0f, 0.12f}, 0.32f, 1.0f, {0.2f, 3.4f, 6.5f, 32.0f, 0.05f}},
	};
	return s_aTongues[Variant];
}

constexpr int SPINE = 56;
constexpr int BRANCH = 24;

struct CSpine
{
	int m_Count = 0;
	float m_aX[SPINE + 2 * BRANCH];
	float m_aY[SPINE + 2 * BRANCH];
	float m_aR[SPINE + 2 * BRANCH];
	float m_aT[SPINE + 2 * BRANCH]; // 0..1 along the whole flame, used to shorten inner layers
};

inline void AddStroke(CSpine &Spine, const CStroke &S, float x, float y, float Heading0, int Count, float TStart, float TEnd, float Phase)
{
	const float Step = S.m_Length / (Count - 1);
	for(int i = 0; i < Count; i++)
	{
		const float t = i / (float)(Count - 1);
		const int k = Spine.m_Count++;
		Spine.m_aX[k] = x;
		Spine.m_aY[k] = y;
		Spine.m_aT[k] = TStart + (TEnd - TStart) * t;
		// round belly, fast taper, thin curled tip, wavy edge
		const float Belly = t < 0.1f ? std::sqrt(std::max(0.0f, 1.0f - (0.1f - t) * (0.1f - t) / 0.01f)) : 1.0f;
		const float Wave = 1.0f + S.m_Wave * std::sin(t * 17.0f + Phase);
		Spine.m_aR[k] = std::max(0.8f, S.m_Radius * std::pow(1.0f - t, 1.15f) * (0.72f + 0.28f * Belly) * Wave);
		const float CurlT = std::max(0.0f, (t - 0.4f) / 0.6f);
		const float Heading = Heading0 + S.m_Bend * t + S.m_Curl * CurlT * CurlT;
		x += std::cos(Heading) * Step;
		y += std::sin(Heading) * Step;
	}
}

inline void BuildSpine(int Variant, CSpine &Spine)
{
	const CTongue &T = Tongue(Variant);
	Spine.m_Count = 0;
	const float Up = -1.5707963f;
	AddStroke(Spine, T.m_Main, T.m_StartX, CELL_H * (1.0f - BASE_FROM_BOTTOM), Up, SPINE, 0.0f, 1.0f, Variant * 1.7f);
	if(T.m_BranchAt > 0.0f)
	{
		const int At = (int)(T.m_BranchAt * (SPINE - 1));
		const float Heading = Up + T.m_Main.m_Bend * T.m_BranchAt + T.m_BranchTurn;
		AddStroke(Spine, T.m_Branch, Spine.m_aX[At], Spine.m_aY[At], Heading, BRANCH, T.m_BranchAt + 0.15f, 1.0f, Variant * 2.3f);
	}
	if(T.m_Branch2At > 0.0f)
	{
		const int At = (int)(T.m_Branch2At * (SPINE - 1));
		const float Heading = Up + T.m_Main.m_Bend * T.m_Branch2At + T.m_Branch2Turn;
		AddStroke(Spine, T.m_Branch2, Spine.m_aX[At], Spine.m_aY[At], Heading, BRANCH, T.m_Branch2At + 0.15f, 1.0f, Variant * 3.1f);
	}
}

// signed distance (px) from a point to a nested layer of the tongue
// Layer 0 = whole tongue, 1 = body (thinner, shorter), 2 = hot core near the base
inline float LayerDistance(const CSpine &Spine, int Layer, float px, float py)
{
	static const float s_aScale[LAYERS] = {1.0f, 0.7f, 0.4f};
	static const float s_aInset[LAYERS] = {0.0f, 1.6f, 1.6f};
	static const float s_aReach[LAYERS] = {1.0f, 0.78f, 0.45f};
	static const float s_aDrop[LAYERS] = {0.0f, 2.5f, 5.0f};
	float Best = 1e9f;
	for(int i = 0; i < Spine.m_Count; i++)
	{
		if(Spine.m_aT[i] > s_aReach[Layer])
			continue;
		const float R = Spine.m_aR[i] * s_aScale[Layer] - s_aInset[Layer];
		if(R <= 0.2f)
			continue;
		const float dx = px - Spine.m_aX[i];
		const float dy = py - (Spine.m_aY[i] + s_aDrop[Layer]);
		Best = std::min(Best, std::sqrt(dx * dx + dy * dy) - R);
	}
	return Best;
}

// shark-fin flames for the ring (variants RING_FIRST..): wide base on the ring, convex front edge,
// concave back edge, tip swept back against the spin. Front = +x of the cell.
struct CFin
{
	float m_TipU; // -1..1, where the tip leans (negative = back)
	float m_Height; // px from the base to the tip
	float m_Hollow; // how deep the back edge is carved in
	float m_Bulge; // how round the front edge is
};

inline const CFin &Fin(int Variant)
{
	static const CFin s_aFins[VARIANTS - RING_FIRST] = {
		{-0.55f, 58.0f, 0.35f, 1.7f},
		{-0.75f, 50.0f, 0.42f, 1.9f},
		{-0.35f, 64.0f, 0.3f, 1.5f},
		{-0.65f, 54.0f, 0.46f, 1.8f},
	};
	return s_aFins[Variant - RING_FIRST];
}

// signed distance (px, approximate) to a nested layer of a fin
inline float FinDistance(int Variant, int Layer, float px, float py)
{
	static const float s_aScale[LAYERS] = {1.0f, 0.66f, 0.38f};
	const CFin &F = Fin(Variant);
	const float HalfW = CELL_W * 0.47f;
	const float BaseY = CELL_H * (1.0f - BASE_FROM_BOTTOM);
	const float Pivot = 0.1f; // nested layers shrink towards the middle of the base
	const float s = s_aScale[Layer];
	const float u = Pivot + ((px - CELL_W * 0.5f) / HalfW - Pivot) / s;
	const float h = (BaseY - py) / (F.m_Height * s);
	const float FrontBase = 0.85f;
	const float BackBase = -0.65f;
	const float hc = std::clamp(h, 0.0f, 1.0f);
	const float Front = FrontBase + (F.m_TipU - FrontBase) * std::pow(hc, F.m_Bulge);
	const float Back = BackBase + (F.m_TipU - BackBase) * hc + F.m_Hollow * std::sin(3.14159265f * hc) * std::pow(1.0f - hc, 0.3f);
	const float dFront = (u - Front) * HalfW * s;
	const float dBack = (Back - u) * HalfW * s;
	const float dTop = (h - 1.0f) * F.m_Height * s;
	const float dBottom = -h * F.m_Height * s - 3.0f; // the root reaches a little under the ring line
	return std::max(std::max(dFront, dBack), std::max(dTop, dBottom));
}

// fills an ATLAS_W x ATLAS_H alpha mask: columns = variants, rows = layers
inline void BuildFlameAtlas(uint8_t *pAlpha)
{
	for(int Variant = 0; Variant < VARIANTS; Variant++)
	{
		CSpine Spine;
		BuildSpine(Variant, Spine);
		for(int Layer = 0; Layer < LAYERS; Layer++)
		{
			for(int py = 0; py < CELL_H; py++)
			{
				for(int px = 0; px < CELL_W; px++)
				{
					const float d = Variant >= RING_FIRST ? FinDistance(Variant, Layer, px + 0.5f, py + 0.5f) : LayerDistance(Spine, Layer, px + 0.5f, py + 0.5f);
					// crisp cel-shaded edge with one pixel of anti-aliasing, faded at the cell border
					float a = std::clamp(0.5f - d, 0.0f, 1.0f);
					const float Border = std::min(std::min((float)px, (float)(CELL_W - 1 - px)), std::min((float)py, (float)(CELL_H - 1 - py)));
					a *= std::clamp(Border / 2.0f, 0.0f, 1.0f);
					pAlpha[(Layer * CELL_H + py) * ATLAS_W + Variant * CELL_W + px] = (uint8_t)std::lround(a * 255.0f);
				}
			}
		}
	}
}

// circle line: Radius and Sigma are fractions of the half size; Pixel > 0 gives a hard line
inline void BuildRing(uint8_t *pAlpha, int Size, float Radius, float Sigma, float Pixel)
{
	for(int y = 0; y < Size; y++)
	{
		for(int x = 0; x < Size; x++)
		{
			const float dx = (x + 0.5f) / Size * 2.0f - 1.0f;
			const float dy = (y + 0.5f) / Size * 2.0f - 1.0f;
			const float Dist = std::sqrt(dx * dx + dy * dy);
			const float d = Dist - Radius;
			float a;
			if(Pixel > 0.0f)
				a = std::clamp((Sigma - std::fabs(d)) / Pixel + 0.5f, 0.0f, 1.0f);
			else
				a = std::exp(-(d * d) / (Sigma * Sigma));
			a *= std::clamp((1.0f - Dist) / 0.02f, 0.0f, 1.0f);
			pAlpha[y * Size + x] = (uint8_t)std::lround(a * 255.0f);
		}
	}
}

// petal-shaped ember, pointed at both ends
inline void BuildPetal(uint8_t *pAlpha, int Size)
{
	for(int y = 0; y < Size; y++)
	{
		for(int x = 0; x < Size; x++)
		{
			const float u = (x + 0.5f) / Size * 2.0f - 1.0f;
			const float v = (y + 0.5f) / Size * 2.0f - 1.0f;
			const float Half = 0.42f * std::pow(std::max(0.0f, 1.0f - u * u), 0.9f) * (1.0f + 0.25f * u);
			const float Inside = (Half - std::fabs(v + 0.12f * u * u)) * Size * 0.5f;
			pAlpha[y * Size + x] = (uint8_t)std::lround(std::clamp(Inside + 0.5f, 0.0f, 1.0f) * 255.0f);
		}
	}
}
}

#endif
