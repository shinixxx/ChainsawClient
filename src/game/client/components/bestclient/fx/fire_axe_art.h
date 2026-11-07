/* ChainsawClient: the golden axe that replaces the hammer with the flaming hammer on (bc_fire_hammer).
   Engine free, so the offline preview can include it: the picture is drawn by code into one RGBA texture,
   left cell = the axe (reference: Рабочий стол\axe.jpg — golden crescent blade, sun disk, two spear tips,
   blue grip, spiked sun ring at the end), right cell = its soft glow (white, the blurred shape in alpha).
   Texture frame of a cell: the shaft runs along v = 0.5 from the ring (small u) to the head (large u),
   the blade hangs to larger v, the spear tips point to smaller v. In game u goes along the hammer sprite
   axis (from the handle to the head) and larger v is the side the tee faces. */
#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_FIRE_AXE_ART_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_FIRE_AXE_ART_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace BcAxe
{
constexpr int CELL = 512;
constexpr int TEX_W = CELL * 2;
constexpr int TEX_H = CELL;

struct V2
{
	float x = 0.0f;
	float y = 0.0f;
	V2() = default;
	constexpr V2(float X, float Y) :
		x(X), y(Y) {}
	V2 operator+(V2 o) const { return V2(x + o.x, y + o.y); }
	V2 operator-(V2 o) const { return V2(x - o.x, y - o.y); }
	V2 operator*(float s) const { return V2(x * s, y * s); }
};
inline float Dot(V2 a, V2 b) { return a.x * b.x + a.y * b.y; }
inline float Len(V2 a) { return std::sqrt(a.x * a.x + a.y * a.y); }
inline V2 Norm(V2 a)
{
	const float l = Len(a);
	return l > 1e-6f ? a * (1.0f / l) : V2(0.0f, 0.0f);
}
inline float Clamp01(float x) { return std::clamp(x, 0.0f, 1.0f); }
inline float SmoothStep(float a, float b, float x)
{
	const float t = Clamp01((x - a) / (b - a));
	return t * t * (3.0f - 2.0f * t);
}

// ---- the shape, in cell pixels (y down) ----
constexpr float AXIS_Y = 256.0f;
constexpr V2 RING_C(66.0f, AXIS_Y);
constexpr float RING_R = 23.0f; // middle of the ring band
constexpr float RING_W = 6.5f; // half thickness
constexpr V2 GRIP_A(98.0f, AXIS_Y);
constexpr V2 GRIP_B(180.0f, AXIS_Y);
constexpr float GRIP_R = 12.0f;
constexpr V2 SHAFT_B(372.0f, AXIS_Y);
constexpr float SHAFT_R = 8.0f;
constexpr V2 HEAD_C(380.0f, AXIS_Y);
constexpr float HEAD_R = 40.0f;
constexpr V2 OUTER_C(376.0f, 300.0f); // blade: outer edge circle
constexpr float OUTER_R = 126.0f;
constexpr V2 INNER_C(380.0f, 214.0f); // blade: the inner cut
constexpr float INNER_R = 152.0f;
constexpr float EDGE_BAND = 15.0f; // pale cutting edge
constexpr float OUTLINE = 4.0f;

// in game: where the parts are in texture coordinates (0..1 of a cell)
constexpr float HEAD_U = HEAD_C.x / CELL;
constexpr float HEAD_V = HEAD_C.y / CELL;
constexpr float GRIP_U = (GRIP_A.x + GRIP_B.x) * 0.5f / CELL;

enum
{
	MAT_GOLD = 0,
	MAT_PALE,
	MAT_GRIP,
	MAT_SPEAR, // gold, the inner half pale
	MAT_BLADE, // gold, pale edge band, engraved line
	MAT_HEAD, // sun disk: rim, groove, rays
};

struct CPart
{
	int m_Mat;
	float m_Bevel; // how far in from the edge the surface rounds off
	float m_Outline;
	V2 m_A; // spear: base of the leaf, direction in m_B
	V2 m_B;
};

inline float SdCircle(V2 p, V2 c, float r) { return Len(p - c) - r; }
inline float SdSegment(V2 p, V2 a, V2 b, float r)
{
	const V2 pa = p - a, ba = b - a;
	const float h = Clamp01(Dot(pa, ba) / Dot(ba, ba));
	return Len(pa - ba * h) - r;
}
inline float SdRoundBox(V2 p, V2 c, V2 Half, float r)
{
	const V2 q(std::fabs(p.x - c.x) - Half.x + r, std::fabs(p.y - c.y) - Half.y + r);
	const V2 m(std::max(q.x, 0.0f), std::max(q.y, 0.0f));
	return Len(m) + std::min(std::max(q.x, q.y), 0.0f) - r;
}
inline float SdPolygon(const V2 *pV, int N, V2 p)
{
	float d = Dot(p - pV[0], p - pV[0]);
	float s = 1.0f;
	for(int i = 0, j = N - 1; i < N; j = i, i++)
	{
		const V2 e = pV[j] - pV[i];
		const V2 w = p - pV[i];
		const V2 b = w - e * Clamp01(Dot(w, e) / Dot(e, e));
		d = std::min(d, Dot(b, b));
		const bool c1 = p.y >= pV[i].y, c2 = p.y < pV[j].y, c3 = e.x * w.y > e.y * w.x;
		if((c1 && c2 && c3) || (!c1 && !c2 && !c3))
			s = -s;
	}
	return s * std::sqrt(d);
}

// spear tips: a long leaf on a short rod
struct CSpear
{
	V2 m_Base;
	V2 m_Dir;
	float m_Rod;
	float m_Leaf;
	float m_Width;
};
inline const CSpear &Spear(int i)
{
	static const CSpear s_aSpears[2] = {
		{V2(386.0f, 238.0f), Norm(V2(0.52f, -0.854f)), 64.0f, 118.0f, 21.0f},
		{V2(366.0f, 240.0f), Norm(V2(-0.58f, -0.815f)), 58.0f, 112.0f, 20.0f},
	};
	return s_aSpears[i];
}
inline float SdLeaf(V2 p, const CSpear &S)
{
	const V2 Side(-S.m_Dir.y, S.m_Dir.x);
	const V2 L0 = S.m_Base + S.m_Dir * S.m_Rod;
	const V2 aV[6] = {
		L0,
		L0 + S.m_Dir * (S.m_Leaf * 0.22f) + Side * S.m_Width,
		L0 + S.m_Dir * (S.m_Leaf * 0.62f) + Side * (S.m_Width * 0.55f),
		L0 + S.m_Dir * S.m_Leaf,
		L0 + S.m_Dir * (S.m_Leaf * 0.62f) - Side * (S.m_Width * 0.55f),
		L0 + S.m_Dir * (S.m_Leaf * 0.22f) - Side * S.m_Width,
	};
	return SdPolygon(aV, 6, p);
}

inline float SdBlade(V2 p)
{
	const float Outer = SdCircle(p, OUTER_C, OUTER_R);
	const float Inner = SdCircle(p, INNER_C, INNER_R);
	return std::max(Outer, -Inner);
}
inline float SdNeck(V2 p)
{
	const float x = HEAD_C.x;
	const V2 aV[8] = {
		V2(x - 27.0f, 284.0f), V2(x + 27.0f, 284.0f), V2(x + 15.0f, 318.0f), V2(x + 17.0f, 340.0f),
		V2(x + 36.0f, 376.0f), V2(x - 36.0f, 376.0f), V2(x - 17.0f, 340.0f), V2(x - 15.0f, 318.0f)};
	return SdPolygon(aV, 8, p);
}
inline float SdPommelSpikes(V2 p)
{
	float d = 1e9f;
	for(int i = 0; i < 9; i++)
	{
		const float a = (40.0f + i * 35.0f) * 3.14159265f / 180.0f; // the shaft side (0°) stays free
		const V2 Out(std::cos(a), std::sin(a));
		const V2 Tan(-Out.y, Out.x);
		const V2 aV[3] = {RING_C + Out * (RING_R + 2.0f) + Tan * 7.5f, RING_C + Out * (RING_R + RING_W + 13.0f), RING_C + Out * (RING_R + 2.0f) - Tan * 7.5f};
		d = std::min(d, SdPolygon(aV, 3, p));
	}
	return d;
}

enum
{
	PART_SPEAR_ROD0 = 0,
	PART_SPEAR_ROD1,
	PART_SPEAR0,
	PART_SPEAR1,
	PART_POMMEL_SPIKES,
	PART_RING,
	PART_SHAFT,
	PART_GRIP,
	PART_COLLAR_RING,
	PART_COLLAR_GRIP,
	PART_COLLAR_MID,
	PART_COLLAR_HEAD,
	PART_CAP,
	PART_BLADE,
	PART_NECK,
	PART_HEAD,
	PART_BOSS,
	NUM_PARTS
};

inline float PartSdf(int Part, V2 p)
{
	switch(Part)
	{
	case PART_SPEAR_ROD0:
	case PART_SPEAR_ROD1:
	{
		const CSpear &S = Spear(Part - PART_SPEAR_ROD0);
		return SdSegment(p, S.m_Base, S.m_Base + S.m_Dir * (S.m_Rod + 4.0f), 6.0f);
	}
	case PART_SPEAR0:
	case PART_SPEAR1: return SdLeaf(p, Spear(Part - PART_SPEAR0));
	case PART_POMMEL_SPIKES: return SdPommelSpikes(p);
	case PART_RING: return std::fabs(Len(p - RING_C) - RING_R) - RING_W;
	case PART_SHAFT: return SdSegment(p, GRIP_B, SHAFT_B, SHAFT_R);
	case PART_GRIP: return SdSegment(p, GRIP_A, GRIP_B, GRIP_R);
	case PART_COLLAR_RING: return SdRoundBox(p, V2(GRIP_A.x - 6.0f, AXIS_Y), V2(7.0f, 14.0f), 3.0f);
	case PART_COLLAR_GRIP: return SdRoundBox(p, V2(GRIP_B.x + 4.0f, AXIS_Y), V2(7.0f, 15.5f), 3.0f);
	case PART_COLLAR_MID: return SdRoundBox(p, V2(282.0f, AXIS_Y), V2(5.0f, 11.5f), 2.5f);
	case PART_COLLAR_HEAD: return SdRoundBox(p, V2(330.0f, AXIS_Y), V2(8.0f, 15.0f), 3.0f);
	case PART_CAP: return SdCircle(p, V2(HEAD_C.x + HEAD_R + 6.0f, AXIS_Y), 11.0f);
	case PART_BLADE: return SdBlade(p);
	case PART_NECK: return SdNeck(p);
	case PART_HEAD: return SdCircle(p, HEAD_C, HEAD_R);
	case PART_BOSS: return SdCircle(p, HEAD_C, 9.5f);
	}
	return 1e9f;
}

inline int PartMat(int Part)
{
	switch(Part)
	{
	case PART_SPEAR0:
	case PART_SPEAR1: return MAT_SPEAR;
	case PART_GRIP: return MAT_GRIP;
	case PART_BLADE: return MAT_BLADE;
	case PART_HEAD: return MAT_HEAD;
	default: return MAT_GOLD;
	}
}

inline float PartBevel(int Part)
{
	switch(Part)
	{
	case PART_BLADE: return 16.0f;
	case PART_HEAD: return 9.0f;
	case PART_SPEAR0:
	case PART_SPEAR1: return 9.0f;
	case PART_NECK: return 12.0f;
	default: return 7.0f;
	}
}

struct C3
{
	float r, g, b;
};
inline C3 Mix(C3 a, C3 b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
inline C3 Ramp(const C3 *pStops, const float *pAt, int N, float x)
{
	if(x <= pAt[0])
		return pStops[0];
	for(int i = 1; i < N; i++)
		if(x <= pAt[i])
			return Mix(pStops[i - 1], pStops[i], (x - pAt[i - 1]) / (pAt[i] - pAt[i - 1]));
	return pStops[N - 1];
}
inline C3 Gold(float l)
{
	static const C3 s_aC[] = {{0.50f, 0.25f, 0.03f}, {0.80f, 0.48f, 0.06f}, {0.97f, 0.72f, 0.13f}, {1.0f, 0.86f, 0.30f}, {1.0f, 0.96f, 0.66f}, {1.0f, 1.0f, 0.93f}};
	static const float s_aAt[] = {0.0f, 0.3f, 0.52f, 0.7f, 0.86f, 1.0f};
	return Ramp(s_aC, s_aAt, 6, l);
}
inline C3 Pale(float l)
{
	static const C3 s_aC[] = {{0.72f, 0.56f, 0.28f}, {0.93f, 0.84f, 0.60f}, {1.0f, 0.97f, 0.84f}, {1.0f, 1.0f, 0.98f}};
	static const float s_aAt[] = {0.0f, 0.4f, 0.72f, 1.0f};
	return Ramp(s_aC, s_aAt, 4, l);
}
inline C3 Blue(float l)
{
	static const C3 s_aC[] = {{0.04f, 0.06f, 0.17f}, {0.10f, 0.17f, 0.40f}, {0.24f, 0.42f, 0.74f}, {0.62f, 0.78f, 0.98f}};
	static const float s_aAt[] = {0.0f, 0.4f, 0.75f, 1.0f};
	return Ramp(s_aC, s_aAt, 4, l);
}

// brightness of a rounded surface: flat in the middle, it bends down to the outline, light from the top left
inline float Lit(V2 Grad, float Depth, float Bevel, float Bias)
{
	const float Slope = 1.0f - SmoothStep(0.0f, Bevel, Depth);
	const float k = 1.35f * Slope;
	float nx = Grad.x * k, ny = Grad.y * k, nz = 1.0f;
	const float nl = std::sqrt(nx * nx + ny * ny + nz * nz);
	nx /= nl;
	ny /= nl;
	nz /= nl;
	const float Lx = -0.45f, Ly = -0.72f, Lz = 0.53f; // normalized
	const float Diff = nx * Lx + ny * Ly + nz * Lz;
	return Clamp01(0.08f + 0.86f * Diff + Bias);
}

// a thin dark engraved stroke where Sd is 0
inline float Engrave(float Sd, float Width)
{
	return Clamp01(Width - std::fabs(Sd) + 0.5f);
}

inline C3 Shade(int Part, V2 p, float d, V2 Grad)
{
	const float Depth = -d;
	const float Bevel = PartBevel(Part);
	const C3 Groove = {0.58f, 0.31f, 0.04f};
	switch(PartMat(Part))
	{
	case MAT_GRIP:
	{
		// a round grip wrapped in a blue band: diagonal turns, dark seams between them
		const float Across = (p.y - AXIS_Y) / GRIP_R; // -1..1
		const float Round = std::sqrt(std::max(0.0f, 1.0f - Across * Across));
		float l = 0.18f + 0.55f * Round - 0.25f * Across;
		const float Turn = (p.x + (p.y - AXIS_Y) * 0.9f) / 11.0f;
		const float f = Turn - std::floor(Turn);
		l += 0.22f * std::sin(f * 3.14159265f) - 0.1f;
		C3 c = Blue(Clamp01(l));
		const float Seam = Clamp01(1.4f - std::fabs(f - 0.0f) * 11.0f) + Clamp01(1.4f - std::fabs(f - 1.0f) * 11.0f);
		return Mix(c, {0.02f, 0.03f, 0.09f}, Clamp01(Seam) * 0.85f);
	}
	case MAT_SPEAR:
	{
		const CSpear &S = Spear(Part - PART_SPEAR0);
		const V2 Side(-S.m_Dir.y, S.m_Dir.x);
		const float Cross = Dot(p - S.m_Base, Side);
		const float Along = Dot(p - S.m_Base, S.m_Dir) - S.m_Rod;
		const float l = Lit(Grad, Depth, Bevel, 0.0f);
		// the inner face is pale like polished steel, the outer is gold, a groove down the middle
		const bool Inner = Cross < 0.0f;
		C3 c = Inner ? Pale(Clamp01(l * 0.85f + 0.25f)) : Gold(l);
		const float Mid = Engrave(Cross, 1.1f) * SmoothStep(6.0f, 16.0f, Along) * SmoothStep(4.0f, 14.0f, Depth);
		return Mix(c, Groove, Mid * 0.8f);
	}
	case MAT_BLADE:
	{
		const float OuterDepth = -SdCircle(p, OUTER_C, OUTER_R);
		float l = Lit(Grad, Depth, Bevel, 0.0f);
		// a broad shine along the blade
		const V2 FromOuter = p - OUTER_C;
		const float Ang = std::atan2(FromOuter.y, FromOuter.x);
		l = Clamp01(l + 0.10f * std::cos(Ang * 2.0f - 1.2f));
		C3 c;
		if(OuterDepth < EDGE_BAND)
		{
			// pale cutting edge, a bright bevel line along it
			const float t = OuterDepth / EDGE_BAND;
			c = Pale(Clamp01(0.45f + 0.55f * l + 0.25f * (1.0f - t)));
			const float Shine = Clamp01(1.0f - std::fabs(OuterDepth - EDGE_BAND * 0.38f) / 2.2f);
			c = Mix(c, {1.0f, 1.0f, 1.0f}, Shine * 0.55f);
		}
		else
			c = Gold(l);
		// soft step where the edge meets the gold
		const float Step = Engrave(OuterDepth - EDGE_BAND, 1.0f);
		c = Mix(c, Groove, Step * 0.7f);
		// engraved line inside the gold, parallel to the edge
		const float Line = Engrave(OuterDepth - EDGE_BAND - 11.0f, 1.0f) * SmoothStep(4.0f, 12.0f, Depth);
		return Mix(c, Groove, Line * 0.75f);
	}
	case MAT_HEAD:
	{
		const float r = Len(p - HEAD_C);
		float l = Lit(Grad, Depth, Bevel, 0.0f);
		C3 c;
		if(r > 31.0f)
			c = Gold(Clamp01(l + 0.06f)); // raised rim
		else
		{
			// the sunk face is a little deeper, lit from the other side
			const V2 Dir = Norm(p - HEAD_C);
			const float Inside = Clamp01(0.62f + 0.25f * (Dir.x * 0.45f + Dir.y * 0.72f) * SmoothStep(25.0f, 31.0f, r));
			c = Gold(Inside * 0.92f);
			// sun rays
			const float a = std::atan2(p.y - HEAD_C.y, p.x - HEAD_C.x);
			const float Sector = 3.14159265f * 2.0f / 12.0f;
			const float k = a / Sector - std::floor(a / Sector) - 0.5f; // -0.5..0.5 inside a sector
			const float Arc = std::fabs(k) * Sector * r; // distance to the ray line
			const float Ray = Clamp01(2.4f * (1.0f - r / 27.0f) + 0.5f - Arc) * SmoothStep(11.0f, 13.0f, r) * (1.0f - SmoothStep(25.0f, 27.0f, r));
			c = Mix(c, Groove, Clamp01(Ray) * 0.85f);
		}
		const float Rim = Engrave(r - 31.0f, 1.2f);
		return Mix(c, Groove, Rim * 0.85f);
	}
	case MAT_PALE: return Pale(Lit(Grad, Depth, Bevel, 0.0f));
	default:
	{
		float l = Lit(Grad, Depth, Bevel, 0.0f);
		if(Part == PART_NECK)
		{
			// two engraved curls on the neck
			const float Curl0 = SdCircle(p, V2(HEAD_C.x - 9.0f, 336.0f), 9.0f);
			const float Curl1 = SdCircle(p, V2(HEAD_C.x + 9.0f, 336.0f), 9.0f);
			C3 c = Gold(l);
			const float Line = std::max(Engrave(Curl0, 1.0f) * (p.x < HEAD_C.x - 4.0f ? 1.0f : 0.0f), Engrave(Curl1, 1.0f) * (p.x > HEAD_C.x + 4.0f ? 1.0f : 0.0f));
			return Mix(c, Groove, Line * 0.75f);
		}
		if(Part == PART_BOSS)
			l = Clamp01(l + 0.12f);
		return Gold(l);
	}
	}
}

// the whole texture: TEX_W x TEX_H RGBA, straight alpha
inline void BuildTexture(uint8_t *pRgba)
{
	const C3 OutlineColor = {0.36f, 0.16f, 0.02f};
	std::vector<float> vAlpha(CELL * CELL, 0.0f);
	for(int y = 0; y < CELL; y++)
	{
		for(int x = 0; x < CELL; x++)
		{
			const V2 p(x + 0.5f, y + 0.5f);
			float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f; // premultiplied while compositing
			const auto Over = [&](C3 c, float Cov) {
				r = c.r * Cov + r * (1.0f - Cov);
				g = c.g * Cov + g * (1.0f - Cov);
				b = c.b * Cov + b * (1.0f - Cov);
				a = Cov + a * (1.0f - Cov);
			};
			for(int Part = 0; Part < NUM_PARTS; Part++)
			{
				const float d = PartSdf(Part, p);
				if(d > OUTLINE + 1.0f)
					continue;
				const float CovOutline = Clamp01(0.5f - (d - OUTLINE));
				Over(OutlineColor, CovOutline);
				const float CovFill = Clamp01(0.5f - d);
				if(CovFill <= 0.0f)
					continue;
				const float e = 0.75f;
				const V2 Grad = Norm(V2(PartSdf(Part, V2(p.x + e, p.y)) - PartSdf(Part, V2(p.x - e, p.y)), PartSdf(Part, V2(p.x, p.y + e)) - PartSdf(Part, V2(p.x, p.y - e))));
				Over(Shade(Part, p, d, Grad), CovFill);
			}
			uint8_t *pPx = &pRgba[(y * TEX_W + x) * 4];
			if(a > 0.0f)
			{
				pPx[0] = (uint8_t)std::lround(Clamp01(r / a) * 255.0f);
				pPx[1] = (uint8_t)std::lround(Clamp01(g / a) * 255.0f);
				pPx[2] = (uint8_t)std::lround(Clamp01(b / a) * 255.0f);
			}
			else
			{
				// transparent pixels keep the outline color, so mip maps do not darken into black
				pPx[0] = (uint8_t)(OutlineColor.r * 255.0f);
				pPx[1] = (uint8_t)(OutlineColor.g * 255.0f);
				pPx[2] = (uint8_t)(OutlineColor.b * 255.0f);
			}
			pPx[3] = (uint8_t)std::lround(Clamp01(a) * 255.0f);
			vAlpha[y * CELL + x] = a;
		}
	}

	// glow cell: the shape blurred three times (close to a gaussian), a bit fuller than the shape
	std::vector<float> vTmp(CELL * CELL);
	const int Radius = 9;
	for(int Pass = 0; Pass < 3; Pass++)
	{
		for(int y = 0; y < CELL; y++)
		{
			float Sum = 0.0f;
			for(int x = -Radius; x <= Radius; x++)
				Sum += x >= 0 && x < CELL ? vAlpha[y * CELL + x] : 0.0f;
			for(int x = 0; x < CELL; x++)
			{
				vTmp[y * CELL + x] = Sum / (2 * Radius + 1);
				const int Out = x - Radius, In = x + Radius + 1;
				if(Out >= 0)
					Sum -= vAlpha[y * CELL + Out];
				if(In < CELL)
					Sum += vAlpha[y * CELL + In];
			}
		}
		for(int x = 0; x < CELL; x++)
		{
			float Sum = 0.0f;
			for(int y = -Radius; y <= Radius; y++)
				Sum += y >= 0 && y < CELL ? vTmp[y * CELL + x] : 0.0f;
			for(int y = 0; y < CELL; y++)
			{
				vAlpha[y * CELL + x] = Sum / (2 * Radius + 1);
				const int Out = y - Radius, In = y + Radius + 1;
				if(Out >= 0)
					Sum -= vTmp[Out * CELL + x];
				if(In < CELL)
					Sum += vTmp[In * CELL + x];
			}
		}
	}
	for(int y = 0; y < CELL; y++)
	{
		for(int x = 0; x < CELL; x++)
		{
			const float v = Clamp01(vAlpha[y * CELL + x] * 1.7f);
			uint8_t *pPx = &pRgba[(y * TEX_W + CELL + x) * 4];
			pPx[0] = pPx[1] = pPx[2] = 255;
			pPx[3] = (uint8_t)std::lround(std::pow(v, 0.8f) * 255.0f);
		}
	}
}

// a point on the cutting edge, t = 0..1 from the back tip to the front tip, in cell pixels
inline V2 EdgePoint(float t)
{
	// the tips are where the two circles cross; walk the outer circle between them
	const V2 dc = INNER_C - OUTER_C;
	const float dl = Len(dc);
	const float a = (OUTER_R * OUTER_R - INNER_R * INNER_R + dl * dl) / (2.0f * dl);
	const float h = std::sqrt(std::max(0.0f, OUTER_R * OUTER_R - a * a));
	const V2 Mid = OUTER_C + dc * (a / dl);
	const V2 Perp(-dc.y / dl, dc.x / dl);
	const V2 T0 = Mid + Perp * h, T1 = Mid - Perp * h;
	float a0 = std::atan2(T0.y - OUTER_C.y, T0.x - OUTER_C.x);
	float a1 = std::atan2(T1.y - OUTER_C.y, T1.x - OUTER_C.x);
	// go round the bottom (away from the head)
	const float Pi = 3.14159265f;
	float Bottom = Pi * 0.5f;
	auto Wrap = [&](float x) { while(x < Bottom - Pi) x += 2.0f * Pi; while(x > Bottom + Pi) x -= 2.0f * Pi; return x; };
	a0 = Wrap(a0);
	a1 = Wrap(a1);
	if(a0 < a1)
		std::swap(a0, a1);
	const float Ang = a0 + (a1 - a0) * Clamp01(t);
	return OUTER_C + V2(std::cos(Ang), std::sin(Ang)) * (OUTER_R - 3.0f);
}
}

#endif
