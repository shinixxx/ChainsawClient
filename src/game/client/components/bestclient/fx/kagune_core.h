/* ChainsawClient: kagune (Tokyo Ghoul, Kaneki's rinkaku) instead of the hammer - simulation, mesh and texture.
   Only base/vmath.h and base/color.h are used, so the offline preview runs exactly this code.

   Every tentacle is a chain of SIM_POINTS points hanging from the tee's lower back. The motion is done in angles:
   every link has a target angle (a living pose: a fan behind the tee with S bends and hooked tips, slow waves,
   breathing, soft muscle twitches) and follows it with a critically damped smoother - it never overshoots and
   behaves the same at any frame rate, so nothing shakes. The tips follow slower than the roots (the motion runs
   down the tentacle), and a lagging copy of the tee position lets the tentacles trail behind when the tee moves.
   All inputs (facing, run, fall, air, rest) are smoothed too, a turn swings the fan over the head.

   Attacks on hammer hits: one tentacle stabs, two stab together (crossing like scissors), or all of them press
   together into one twisted spear and strike as one; bc_kagune_attack picks the kind or runs a combo.

   The mesh is a smooth spline strip: the body texture (crimson, dark outline, glowing veins, speckles), an
   overlay of diamond scales towards the tip, the veins glowing with light flowing to the tip, a soft glow behind. */
#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_KAGUNE_CORE_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_KAGUNE_CORE_H

#include <base/color.h>
#include <base/vmath.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace BcKagune
{
constexpr int MAX_TENTACLES = 8;
constexpr int SIM_POINTS = 12;
constexpr int LINKS = SIM_POINTS - 1;
constexpr int MAX_SAMPLES = 64;
constexpr int MAX_MOTES = 1024;
constexpr float PI = 3.14159265358979f;

// texture 256 x 512, everything in rows 0..383 repeats along u:
// rows 0..127 the body, 128..255 the scales overlay, 256..383 the glowing veins (white mask),
// 384..447 a soft band (glow), 448..511 sprites (round glow, star)
constexpr int TEX_W = 256;
constexpr int TEX_H = 512;
constexpr float BODY_V0 = 2.0f / TEX_H;
constexpr float BODY_V1 = 126.0f / TEX_H;
constexpr float SCALE_V0 = 130.0f / TEX_H;
constexpr float SCALE_V1 = 254.0f / TEX_H;
constexpr float VEIN_V0 = 258.0f / TEX_H;
constexpr float VEIN_V1 = 382.0f / TEX_H;
constexpr float SOFT_V0 = 386.0f / TEX_H;
constexpr float SOFT_V1 = 446.0f / TEX_H;
constexpr float SOFT_U = 32.0f / TEX_W; // the soft band is only 64 texels wide (it is the same along u)
constexpr float TEX_UNITS = 64.0f; // world units of tentacle (at scale 1) per texture repeat
constexpr float BODY_EDGE = 0.92f; // the body ends here across the band (1 = the edge of the geometry)
struct CSpriteUv
{
	float m_U0, m_V0, m_U1, m_V1;
};
constexpr CSpriteUv GLOW_UV = {0.0f / TEX_W, 448.0f / TEX_H, 64.0f / TEX_W, 512.0f / TEX_H};
constexpr CSpriteUv STAR_UV = {64.0f / TEX_W, 448.0f / TEX_H, 128.0f / TEX_W, 512.0f / TEX_H};
constexpr CSpriteUv SPIKE_UV = {129.0f / TEX_W, 449.0f / TEX_H, 191.0f / TEX_W, 511.0f / TEX_H}; // base at u0, tip at u1, soft at v0/v1
constexpr CSpriteUv DROP_UV = {192.0f / TEX_W, 448.0f / TEX_H, 256.0f / TEX_W, 512.0f / TEX_H}; // a solid round drop
constexpr CSpriteUv SPLAT_UV = {64.0f / TEX_W, 384.0f / TEX_H, 128.0f / TEX_W, 448.0f / TEX_H}; // blood on a floor: an uneven pool with droplets around
constexpr CSpriteUv SPLAT2_UV = {128.0f / TEX_W, 384.0f / TEX_H, 192.0f / TEX_W, 448.0f / TEX_H}; // another one
constexpr CSpriteUv DRIP_UV = {192.0f / TEX_W, 384.0f / TEX_H, 256.0f / TEX_W, 448.0f / TEX_H}; // blood on a wall: a blot running down

// the kinds of kagune (bc_kagune_style), after the examples in examples-of-kagune
enum
{
	STYLE_RINKAKU = 0, // crimson ribbons with glowing veins and scales (kagune.png)
	STYLE_KAKUJA, // black violet armour plates, red glowing seams, claws, red tendrils (1.jpg)
	STYLE_UKAKU, // a wing of magenta crystal shards with teeth and flowing streamers, shoots shards (2.jpg)
	STYLE_BONE, // white bone segments with red eyes, red flesh joints, spikes (3.jpg)
	NUM_STYLES,
};

struct CStyle
{
	float m_Length; // times the length
	float m_Width; // times the width
	float m_Curl, m_Bend, m_Hook, m_Wave; // times the rest pose parts
	float m_ElevTop, m_ElevBottom; // the fan (degrees above the line behind the tee), first and last tentacle
	float m_RootUp; // the roots move up (units at scale 1)
	int m_Plates; // segments per texture repeat (0 = smooth)
	float m_Joint; // width of the joints between plates (1 = no joints)
	int m_Spikes; // spikes per texture repeat
	int m_SpikeSides; // 1 = alternate sides, 2 = both sides, 3 = only the lower side (teeth)
	float m_SpikeLength; // times the width
	float m_SpikeLean; // towards the tip
	int m_Tendrils; // thin strands winding along every tentacle
	float m_TendrilWidth; // units at scale 1
	float m_Glow; // the glow behind
	bool m_Scales; // the scales overlay
};

inline const CStyle &StyleOf(int Style)
{
	static const CStyle s_aStyles[NUM_STYLES] = {
		// length width curl bend hook wave elevtop elevbottom rootup plates joint spikes sides spikelen lean tendrils twidth glow scales
		{1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 82.0f, -4.0f, 0.0f, 0, 1.0f, 0, 0, 0.0f, 0.0f, 0, 0.0f, 0.22f, true},
		{1.12f, 1.3f, 1.1f, 1.2f, 0.75f, 0.9f, 80.0f, -10.0f, 0.0f, 4, 0.86f, 4, 1, 1.5f, 0.75f, 1, 1.5f, 0.3f, false},
		{0.9f, 1.55f, 0.3f, 0.0f, 0.0f, 0.5f, 74.0f, 12.0f, 8.0f, 0, 1.0f, 6, 3, 0.75f, 0.45f, 2, 2.2f, 0.3f, false},
		{1.05f, 1.2f, 0.9f, 0.8f, 0.35f, 0.8f, 82.0f, -6.0f, 0.0f, 3, 0.55f, 3, 2, 1.0f, 0.8f, 0, 0.0f, 0.12f, false},
	};
	return s_aStyles[std::clamp(Style, 0, NUM_STYLES - 1)];
}

enum
{
	ATTACK_COMBO = 0,
	ATTACK_SINGLE,
	ATTACK_DOUBLE,
	ATTACK_UNITED,
	ATTACK_RANDOM,
	NUM_ATTACK_MODES,
};
enum
{
	KIND_SINGLE = 0,
	KIND_DOUBLE,
	KIND_UNITED,
};

inline float Clamp01(float x) { return std::clamp(x, 0.0f, 1.0f); }
inline float Smooth(float a, float b, float x)
{
	const float t = Clamp01((x - a) / (b - a));
	return t * t * (3.0f - 2.0f * t);
}
inline float Fract(float x) { return x - std::floor(x); }
inline float Rad(float Degrees) { return Degrees * PI / 180.0f; }
inline float WrapPi(float a)
{
	a = std::fmod(a + PI, 2.0f * PI);
	if(a < 0.0f)
		a += 2.0f * PI;
	return a - PI;
}

inline float Rand01(uint32_t &State)
{
	State ^= State << 13;
	State ^= State >> 17;
	State ^= State << 5;
	return (State & 0xffffff) / (float)0x1000000;
}
inline float RandRange(uint32_t &State, float Min, float Max) { return Min + (Max - Min) * Rand01(State); }

// critically damped follower (Game Programming Gems 4, 1.10): smooth, never overshoots, stable and the same at
// any frame rate; Time is roughly the time to reach the target
inline float SmoothDamp(float Cur, float Target, float &Vel, float Time, float Dt)
{
	Time = std::max(0.0001f, Time);
	const float Omega = 2.0f / Time;
	const float x = Omega * Dt;
	const float Exp = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
	const float Change = Cur - Target;
	const float Temp = (Vel + Omega * Change) * Dt;
	Vel = (Vel - Omega * Temp) * Exp;
	return Target + (Change + Temp) * Exp;
}
inline vec2 SmoothDamp(vec2 Cur, vec2 Target, vec2 &Vel, float Time, float Dt)
{
	return vec2(SmoothDamp(Cur.x, Target.x, Vel.x, Time, Dt), SmoothDamp(Cur.y, Target.y, Vel.y, Time, Dt));
}

// sin(Start + m * Step) for m = 0, 1, 2, ...: a unit vector turned by a fixed step instead of a sin per sample
struct CWave
{
	vec2 m_At;
	vec2 m_Turn;
	CWave(float Start, float Step) :
		m_At(std::cos(Start), std::sin(Start)), m_Turn(std::cos(Step), std::sin(Step)) {}
	float Sin() const { return m_At.y; }
	float Cos() const { return m_At.x; }
	void Next() { m_At = vec2(m_At.x * m_Turn.x - m_At.y * m_Turn.y, m_At.x * m_Turn.y + m_At.y * m_Turn.x); }
};

inline float Pow10(float x)
{
	const float x2 = x * x, x4 = x2 * x2;
	return x4 * x4 * x2;
}
inline float Pow14(float x)
{
	const float x2 = x * x, x4 = x2 * x2;
	return x4 * x4 * x4 * x2;
}

struct CPalette
{
	ColorRGBA m_Deep; // almost black red: the outline, seams
	ColorRGBA m_Dark;
	ColorRGBA m_Mid; // the body crimson
	ColorRGBA m_Bright; // lit crimson
	ColorRGBA m_Hot; // pinkish red light: the veins, scale rims
	ColorRGBA m_Glow; // additive light
	ColorRGBA m_Spark;
};

inline ColorRGBA Hsl(float h, float s, float l)
{
	return color_cast<ColorRGBA>(ColorHSLA(Fract(h), Clamp01(s), Clamp01(l), 1.0f));
}

inline CPalette MakePalette(const ColorHSLA &Base, int Style = STYLE_RINKAKU)
{
	// the ukaku crystal is the kagune color turned towards magenta (red -> purple pink)
	const float h = Base.h - (Style == STYLE_UKAKU ? 0.115f : 0.012f);
	const float s = std::max(Base.s, 0.55f) * (Style == STYLE_UKAKU ? 0.8f : 0.7f);
	CPalette P;
	if(Style == STYLE_KAKUJA)
	{
		// the ramp is the black violet armour, the light (seams, glow, sparks) is the kagune color
		const float hs = std::min(1.0f, s * 1.3f);
		P.m_Deep = Hsl(0.76f, 0.2f, 0.025f);
		P.m_Dark = Hsl(0.76f, 0.22f, 0.07f);
		P.m_Mid = Hsl(0.77f, 0.2f, 0.14f);
		P.m_Bright = Hsl(0.78f, 0.22f, 0.27f);
		P.m_Hot = Hsl(h + 0.006f, hs, 0.55f);
		P.m_Glow = Hsl(h + 0.012f, hs, 0.42f);
		P.m_Spark = Hsl(h + 0.012f, hs, 0.55f);
		return P;
	}
	if(Style == STYLE_BONE)
	{
		// white bone, the light is the kagune color
		const float hs = std::min(1.0f, s * 1.3f);
		P.m_Deep = Hsl(0.6f, 0.08f, 0.25f);
		P.m_Dark = Hsl(0.6f, 0.06f, 0.5f);
		P.m_Mid = Hsl(0.1f, 0.05f, 0.78f);
		P.m_Bright = Hsl(0.1f, 0.05f, 0.92f);
		P.m_Hot = Hsl(h + 0.006f, hs, 0.5f);
		P.m_Glow = Hsl(h + 0.012f, hs, 0.42f);
		P.m_Spark = Hsl(h + 0.012f, hs, 0.55f);
		return P;
	}
	// the rinkaku: a clean, vivid red (like the reference video), the kagune color
	const float rh = Base.h;
	const float rs = std::min(0.95f, std::max(Base.s, 0.55f) * 0.95f);
	P.m_Deep = Hsl(rh - 0.008f, rs, 0.12f);
	P.m_Dark = Hsl(rh - 0.006f, rs, 0.27f);
	P.m_Mid = Hsl(rh - 0.004f, rs, 0.4f);
	P.m_Bright = Hsl(rh, rs, 0.48f);
	P.m_Hot = Hsl(rh + 0.006f, rs, 0.6f);
	P.m_Glow = Hsl(rh + 0.012f, rs, 0.45f);
	P.m_Spark = Hsl(rh + 0.012f, rs, 0.58f);
	return P;
}

inline ColorRGBA Ramp(const CPalette &P, float b)
{
	// value 0 .. 1.25 -> deep, dark, mid, bright, hot
	const ColorRGBA *apStops[] = {&P.m_Deep, &P.m_Dark, &P.m_Mid, &P.m_Bright, &P.m_Hot};
	static const float s_aAt[] = {0.0f, 0.32f, 0.68f, 1.0f, 1.25f};
	b = std::clamp(b, 0.0f, 1.25f);
	int i = 0;
	while(i < 3 && b > s_aAt[i + 1])
		i++;
	const float t = (b - s_aAt[i]) / (s_aAt[i + 1] - s_aAt[i]);
	const ColorRGBA &A = *apStops[i];
	const ColorRGBA &B = *apStops[i + 1];
	return ColorRGBA(A.r + (B.r - A.r) * t, A.g + (B.g - A.g) * t, A.b + (B.b - A.b) * t, 1.0f);
}

// the same palette turned a bit in hue and lightness (other shades of the same red: wine, scarlet)
inline CPalette ShiftPalette(const CPalette &P, float Dh, float Dl)
{
	const auto Shift = [&](const ColorRGBA &C) {
		const ColorHSLA H = color_cast<ColorHSLA>(C);
		return Hsl(H.h + Dh, H.s, H.l + Dl);
	};
	CPalette R = P;
	R.m_Deep = Shift(P.m_Deep);
	R.m_Dark = Shift(P.m_Dark);
	R.m_Mid = Shift(P.m_Mid);
	R.m_Bright = Shift(P.m_Bright);
	R.m_Hot = Shift(P.m_Hot);
	return R;
}

// the glowing veins: thin wandering lines along the tentacle, fading in and out; u 0..1 (periodic), c -1..1
inline float VeinMask(float u, float c, float &Halo)
{
	struct CVein
	{
		float m_Off, m_A1, m_F1, m_P1, m_A2, m_F2, m_P2, m_Fade, m_FadeP, m_W;
	};
	static const CVein s_aVeins[] = {
		{-0.18f, 0.22f, 1.0f, 0.3f, 0.07f, 5.0f, 1.1f, 1.0f, 0.2f, 0.030f},
		{0.22f, 0.18f, 2.0f, 2.1f, 0.06f, 7.0f, 0.4f, 2.0f, 1.9f, 0.026f},
		{0.02f, 0.30f, 1.0f, 4.0f, 0.05f, 9.0f, 2.7f, 3.0f, 3.3f, 0.022f},
		{0.45f, 0.10f, 3.0f, 5.2f, 0.04f, 11.0f, 0.9f, 2.0f, 4.6f, 0.020f},
		{-0.48f, 0.10f, 2.0f, 1.5f, 0.05f, 8.0f, 5.5f, 3.0f, 0.8f, 0.020f},
	};
	float Core = 0.0f;
	Halo = 0.0f;
	for(const CVein &V : s_aVeins)
	{
		const float At = V.m_Off + V.m_A1 * std::sin(2.0f * PI * V.m_F1 * u + V.m_P1) + V.m_A2 * std::sin(2.0f * PI * V.m_F2 * u + V.m_P2);
		const float Fade = Smooth(-0.2f, 0.5f, std::sin(2.0f * PI * V.m_Fade * u + V.m_FadeP));
		const float d = c - At;
		Core = std::max(Core, Fade * std::exp(-(d * d) / (V.m_W * V.m_W)));
		Halo = std::max(Halo, Fade * std::exp(-(d * d) / (0.11f * 0.11f)));
	}
	const float Inside = 1.0f - Smooth(0.7f, 0.86f, std::fabs(c));
	Halo *= Inside;
	return Core * Inside;
}

inline ColorRGBA MixColor(const ColorRGBA &A, const ColorRGBA &B, float t)
{
	return ColorRGBA(A.r + (B.r - A.r) * t, A.g + (B.g - A.g) * t, A.b + (B.b - A.b) * t, A.a + (B.a - A.a) * t);
}

// a fixed pseudo random value for a whole number
inline float Hash01(int n)
{
	uint32_t x = (uint32_t)n * 2654435761u + 0x9e3779b9u;
	x ^= x >> 15;
	x *= 0x2c1b3c6du;
	x ^= x >> 12;
	return (x & 0xffff) / 65535.0f;
}

// a fixed pseudo random value for a grid cell
inline float HashCell(int x, int y, int Seed)
{
	return Hash01((int)((uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^ (uint32_t)Seed * 83492791u));
}

// smooth value noise 0..1, repeating every Period along X (X = u * Period for a texture repeating along u)
inline float PeriodicNoise(float X, float Y, int Period, int Seed)
{
	const float Fx = std::floor(X), Fy = std::floor(Y);
	const int ix = (int)Fx, iy = (int)Fy;
	float tx = X - Fx, ty = Y - Fy;
	tx = tx * tx * (3.0f - 2.0f * tx);
	ty = ty * ty * (3.0f - 2.0f * ty);
	const auto H = [&](int i, int j) { return HashCell(((i % Period) + Period) % Period, j, Seed); };
	const float a = H(ix, iy) + (H(ix + 1, iy) - H(ix, iy)) * tx;
	const float b = H(ix, iy + 1) + (H(ix + 1, iy + 1) - H(ix, iy + 1)) * tx;
	return a + (b - a) * ty;
}

// cells (Worley): the distances to the nearest and the second nearest cell point, repeating every Period along X;
// F2 - F1 is 0 on the borders between the cells
inline void Cells(float X, float Y, int Period, int Seed, float &F1, float &F2)
{
	const int ix = (int)std::floor(X), iy = (int)std::floor(Y);
	F1 = F2 = 1e9f;
	for(int j = iy - 1; j <= iy + 1; j++)
	{
		for(int i = ix - 1; i <= ix + 1; i++)
		{
			const int w = ((i % Period) + Period) % Period;
			const float Px = i + 0.12f + 0.76f * HashCell(w, j, Seed);
			const float Py = j + 0.12f + 0.76f * HashCell(w, j, Seed + 101);
			const float d = std::sqrt((X - Px) * (X - Px) + (Y - Py) * (Y - Py));
			if(d < F1)
			{
				F2 = F1;
				F1 = d;
			}
			else if(d < F2)
				F2 = d;
		}
	}
}

// the body (rows 0..127) and the glowing light (rows 256..383) of the other kinds; Put(x, y, r, g, b, a)
template<class FPut, class FSpeckle>
void BuildStyleBody(FPut &&Put, const CPalette &P, int Style, FSpeckle &&Speckle)
{
	for(int y = 0; y < 128; y++)
	{
		const float c = (y + 0.5f) / 128.0f * 2.0f - 1.0f;
		const float ac = std::fabs(c) / BODY_EDGE;
		const float Alpha = 1.0f - Smooth(0.9f, 1.0f, ac);
		const float Inside = 1.0f - Smooth(0.78f, 0.9f, ac);
		const float Nz = std::sqrt(std::max(0.0f, 1.0f - std::min(1.0f, ac) * std::min(1.0f, ac)));
		for(int x = 0; x < TEX_W; x++)
		{
			const float u = (x + 0.5f) / TEX_W;
			const float Dots = Speckle(x + 0.5f, y + 0.5f);
			ColorRGBA Col;
			float Light = 0.0f; // the glow layer
			float a = Alpha;
			if(Style == STYLE_KAKUJA)
			{
				// armour plates overlapping towards the tip, a hard gloss line, red seams and red cracks
				const float f = Fract(u * 4.0f);
				float v = (0.3f + 0.62f * std::pow(Nz, 1.2f)) * (0.72f + 0.38f * Smooth(0.0f, 0.9f, f)) + 0.6f * Dots;
				v += 0.18f * std::exp(-std::pow((f - 0.94f) / 0.03f, 2.0f)) * Nz; // the raised plate end
				v *= 1.0f - 0.8f * Smooth(0.82f, 0.95f, ac); // outline
				Col = Ramp(P, v);
				const float Spec = (0.35f * std::exp(-std::pow((c + 0.42f) / 0.07f, 2.0f)) + 0.35f * std::exp(-std::pow((c - 0.42f) / 0.07f, 2.0f))) * (0.35f + 0.65f * Smooth(0.1f, 0.9f, f));
				Col = MixColor(Col, ColorRGBA(0.62f, 0.56f, 0.72f, 1.0f), Spec * 0.7f);
				const float Seam = (std::exp(-std::pow(f / 0.035f, 2.0f)) + std::exp(-std::pow((f - 1.0f) / 0.035f, 2.0f))) * Inside;
				float Halo;
				const float Crack = VeinMask(u, c * 1.15f, Halo) * (0.5f + 0.5f * Smooth(0.2f, 0.6f, f));
				const float Red = std::max(Seam, Crack * 0.85f);
				Col = MixColor(Col, P.m_Hot, Red * 0.85f);
				Light = std::min(1.0f, std::max(Seam, Crack) + 0.35f * Halo * Inside);
			}
			else if(Style == STYLE_UKAKU)
			{
				// a crystal: two sets of facets, every facet its own brightness, light edges between them
				const float g = u * 5.0f + c * 0.9f;
				const float h = u * 3.0f - c * 1.2f;
				const int Gi = (((int)std::floor(g)) % 5 + 5) % 5;
				const int Hi = (((int)std::floor(h)) % 3 + 3) % 3;
				const float Fg = Fract(g), Fh = Fract(h);
				float v = 0.3f + 0.32f * Hash01(Gi * 7 + 1) + 0.2f * Hash01(Hi * 13 + 5) + 0.3f * Nz + 0.3f * Dots;
				const float Edge = std::max(std::exp(-std::pow(std::min(Fg, 1.0f - Fg) / 0.03f, 2.0f)), 0.7f * std::exp(-std::pow(std::min(Fh, 1.0f - Fh) / 0.025f, 2.0f))) * Inside;
				v += 0.35f * Edge;
				v *= 1.0f - 0.6f * Smooth(0.84f, 0.96f, ac);
				Col = Ramp(P, v);
				Light = Edge * 0.8f;
				a = Alpha * (0.86f + 0.14f * Nz);
			}
			else
			{
				// bone plates with a red eye in the middle, red flesh joints between them
				const float f = Fract(u * 3.0f);
				const float Plate = Smooth(0.08f, 0.14f, f) * (1.0f - Smooth(0.86f, 0.92f, f));
				// white: lit from the middle, a dark lip at both ends of a plate
				float w = 0.45f + 0.55f * std::pow(Nz, 0.8f) + 0.4f * Dots;
				w *= 1.0f - 0.35f * (std::exp(-std::pow((f - 0.16f) / 0.025f, 2.0f)) + std::exp(-std::pow((f - 0.84f) / 0.025f, 2.0f)));
				w += 0.12f * std::exp(-std::pow((c + 0.4f) / 0.1f, 2.0f));
				ColorRGBA White = MixColor(P.m_Dark, P.m_Bright, Clamp01(w));
				White = MixColor(White, P.m_Deep, Smooth(0.84f, 0.97f, ac));
				// the flesh: ribbed red
				const float Rib = 0.5f + 0.5f * std::sin(f * 2.0f * PI * 18.0f);
				const ColorRGBA Flesh = MixColor(MixColor(P.m_Hot, ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f), 0.55f), P.m_Hot, Clamp01(0.35f + 0.5f * Nz + 0.15f * Rib));
				Col = MixColor(Flesh, White, Plate);
				// the eye: 21 x 21 units of plate, a ring, a red disc with a light spot
				const float du = (f - 0.5f) * 21.3f, dc = c * 10.8f;
				const float d = std::sqrt(du * du + dc * dc);
				const float Ring = Smooth(3.6f, 3.9f, d) * (1.0f - Smooth(4.5f, 4.8f, d));
				const float Disc = 1.0f - Smooth(3.6f, 3.9f, d);
				ColorRGBA Eye = MixColor(MixColor(P.m_Hot, ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f), 0.4f), P.m_Hot, Clamp01(1.0f - d / 3.6f));
				const float Spot = std::exp(-((du + 1.2f) * (du + 1.2f) + (dc + 1.2f) * (dc + 1.2f)) / 0.8f);
				Eye = MixColor(Eye, ColorRGBA(1.0f, 0.85f, 0.85f, 1.0f), Spot * 0.6f);
				Col = MixColor(Col, P.m_Deep, Ring * Plate * 0.8f);
				Col = MixColor(Col, Eye, Disc * Plate);
				Light = Disc * Plate * 0.9f + (1.0f - Plate) * 0.25f * Inside;
			}
			Put(x, y, Col.r, Col.g, Col.b, a);
			if(Light > 0.002f)
				Put(x, 256 + y, 1.0f, 1.0f, 1.0f, Light);
		}
	}
}

// blood on the map (rows 384..447, x 64..255): two uneven pools with droplets around, a blot running down a wall;
// gray shading (the color is multiplied): dried darker rim, wet lighter middle, a light spot
template<class FPut>
void BuildBloodSprites(FPut &&Put)
{
	for(int Kind = 0; Kind < 3; Kind++)
	{
		uint32_t Seed = 0x51ed2701u + Kind * 977u;
		const float P1 = RandRange(Seed, 0.0f, 6.28f), P2 = RandRange(Seed, 0.0f, 6.28f), P3 = RandRange(Seed, 0.0f, 6.28f);
		float aDx[9], aDy[9], aDr[9];
		for(int n = 0; n < 9; n++)
		{
			const float a = RandRange(Seed, 0.0f, 6.28f);
			const float r = RandRange(Seed, 0.62f, 0.88f);
			aDx[n] = std::cos(a) * r;
			aDy[n] = std::sin(a) * r * (Kind == 2 ? 0.5f : 1.0f) + (Kind == 2 ? -0.45f : 0.0f);
			aDr[n] = RandRange(Seed, 0.03f, 0.08f);
		}
		for(int y = 0; y < 64; y++)
		{
			for(int x = 0; x < 64; x++)
			{
				const float px = (x + 0.5f) / 32.0f - 1.0f, py = (y + 0.5f) / 32.0f - 1.0f;
				float Inside = 0.0f; // 0 outside .. 1 inside
				float Depth = 0.0f; // 0 at the rim .. 1 in the middle (for the shading)
				const auto Add = [&](float d, float r, float Soft) {
					const float In = 1.0f - Smooth(r - Soft, r, d);
					Inside = std::max(Inside, In);
					Depth = std::max(Depth, Clamp01(1.0f - d / std::max(0.001f, r)));
				};
				if(Kind < 2)
				{
					// the pool: a wobbly circle
					const float a = std::atan2(py, px);
					const float r = 0.48f + 0.08f * std::sin(5.0f * a + P1) + 0.05f * std::sin(9.0f * a + P2) + 0.03f * std::sin(14.0f * a + P3);
					Add(std::sqrt(px * px + py * py), r, 0.05f);
				}
				else
				{
					// the blot and its run down
					const float bx = px, by = py + 0.45f;
					const float a = std::atan2(by, bx);
					const float r = 0.32f + 0.05f * std::sin(5.0f * a + P1) + 0.03f * std::sin(9.0f * a + P2);
					Add(std::sqrt(bx * bx + by * by), r, 0.05f);
					if(py > -0.45f && py < 0.82f)
					{
						const float t = (py + 0.45f) / 1.27f;
						const float w = 0.13f - 0.08f * t + 0.015f * std::sin(t * 17.0f + P3);
						Add(std::fabs(px - 0.03f * std::sin(t * 5.0f + P1)), w, 0.035f);
					}
					Add(std::sqrt(px * px + (py - 0.82f) * (py - 0.82f)), 0.08f, 0.03f);
				}
				for(int n = 0; n < 9; n++)
				{
					const float dx = px - aDx[n], dy = py - aDy[n];
					Add(std::sqrt(dx * dx + dy * dy), aDr[n], 0.025f);
				}
				if(Inside <= 0.0f)
					continue;
				const float SpotY = py + 0.2f + (Kind == 2 ? 0.45f : 0.0f);
				const float Spot = std::exp(-((px + 0.16f) * (px + 0.16f) + SpotY * SpotY) / 0.012f);
				const float Shade = 0.42f + 0.22f * Smooth(0.0f, 0.5f, Depth) + 0.4f * Spot;
				Put(64 + Kind * 64 + x, 384 + y, Shade, Shade, Shade, Inside);
			}
		}
	}
}

// fills TEX_W x TEX_H RGBA
inline void BuildTexture(uint8_t *pRgba, const CPalette &P, int Style = STYLE_RINKAKU)
{
	std::fill(pRgba, pRgba + TEX_W * TEX_H * 4, 0);
	const auto Put = [&](int x, int y, float r, float g, float b, float a) {
		uint8_t *p = &pRgba[(y * TEX_W + x) * 4];
		p[0] = (uint8_t)std::lround(Clamp01(r) * 255.0f);
		p[1] = (uint8_t)std::lround(Clamp01(g) * 255.0f);
		p[2] = (uint8_t)std::lround(Clamp01(b) * 255.0f);
		p[3] = (uint8_t)std::lround(Clamp01(a) * 255.0f);
	};

	// speckles: small light and dark dots, wrapped along u
	constexpr int NUM_DOTS = 260;
	float aDotX[NUM_DOTS], aDotC[NUM_DOTS], aDotR[NUM_DOTS], aDotT[NUM_DOTS];
	uint32_t Seed = 0x6b43a9b5u;
	for(int i = 0; i < NUM_DOTS; i++)
	{
		aDotX[i] = RandRange(Seed, 0.0f, (float)TEX_W);
		aDotC[i] = RandRange(Seed, -0.85f, 0.85f);
		aDotR[i] = RandRange(Seed, 0.6f, 1.5f);
		aDotT[i] = Rand01(Seed) < 0.6f ? RandRange(Seed, 0.08f, 0.2f) : -RandRange(Seed, 0.06f, 0.14f);
	}
	const auto Speckle = [&](float x, float y) {
		float Sum = 0.0f;
		for(int i = 0; i < NUM_DOTS; i++)
		{
			float dx = std::fabs(x - aDotX[i]);
			dx = std::min(dx, TEX_W - dx);
			if(dx > 3.0f)
				continue;
			const float dy = y - (aDotC[i] * 0.5f + 0.5f) * 128.0f;
			if(std::fabs(dy) > 3.0f)
				continue;
			const float d2 = dx * dx + dy * dy;
			Sum += aDotT[i] * std::exp(-d2 / (aDotR[i] * aDotR[i]));
		}
		return Sum;
	};

	if(Style != STYLE_RINKAKU)
		BuildStyleBody(Put, P, Style, Speckle);
	else
	{
		// the rinkaku, after the art: a deep crimson body, round (darker to its edges), a cracked network of glowing
		// red lines under the skin (big cells and fine ones, brighter in glowing patches), the long veins, white
		// wet flecks; the pattern is laid on a cylinder (pressed together at the edges), in world units
		constexpr float RADIUS = 9.0f; // world units of the round body behind c = BODY_EDGE
		constexpr int NUM_FLECKS = 60;
		const CPalette Wine = ShiftPalette(P, -0.02f, -0.06f); // deeper crimson, towards rose
		const CPalette Scarlet = ShiftPalette(P, 0.02f, 0.03f); // light scarlet, a touch warmer
		float aFx[NUM_FLECKS], aFy[NUM_FLECKS], aFl[NUM_FLECKS], aFw[NUM_FLECKS], aFb[NUM_FLECKS];
		uint32_t FleckSeed = 0x2f6e2b1du;
		for(int i = 0; i < NUM_FLECKS; i++)
		{
			aFx[i] = RandRange(FleckSeed, 0.0f, TEX_UNITS);
			aFy[i] = RandRange(FleckSeed, -12.5f, 12.5f);
			aFl[i] = RandRange(FleckSeed, 1.4f, 3.0f);
			aFw[i] = RandRange(FleckSeed, 0.22f, 0.38f);
			aFb[i] = Rand01(FleckSeed) < 0.3f ? RandRange(FleckSeed, 0.85f, 1.0f) : RandRange(FleckSeed, 0.45f, 0.8f);
		}
		for(int y = 0; y < 128; y++)
		{
			const float c = (y + 0.5f) / 128.0f * 2.0f - 1.0f; // -1 .. 1 across
			const float ac = std::fabs(c) / BODY_EDGE; // 1 = the body's edge
			const float Alpha = 1.0f - Smooth(0.9f, 1.0f, ac);
			const float Nz = std::sqrt(std::max(0.0f, 1.0f - std::min(1.0f, ac) * std::min(1.0f, ac)));
			const float Inside = 1.0f - Smooth(0.78f, 0.9f, ac);
			// around the cylinder: world units from the middle line
			const float Yw = RADIUS * std::asin(std::clamp(c / BODY_EDGE, -1.0f, 1.0f));
			for(int x = 0; x < TEX_W; x++)
			{
				const float u = (x + 0.5f) / TEX_W;
				const float Xw = u * TEX_UNITS;
				float Halo;
				const float Vein = VeinMask(u, c, Halo);
				// glowing patches
				const float Cloud = 0.6f * PeriodicNoise(u * 4.0f, Yw / 16.0f, 4, 21) + 0.4f * PeriodicNoise(u * 9.0f, Yw / 7.1f, 9, 22);
				// the cracks: borders of cells, a bit wavy
				const float Wx = 0.45f * (PeriodicNoise(u * 6.0f, Yw / 10.0f, 6, 31) - 0.5f);
				const float Wy = 0.45f * (PeriodicNoise(u * 6.0f, Yw / 10.0f + 7.3f, 6, 32) - 0.5f);
				float F1, F2;
				Cells(u * 6.0f + Wx, Yw / 10.7f + Wy, 6, 41, F1, F2);
				const float Var = 0.15f + 0.85f * Smooth(0.25f, 0.75f, PeriodicNoise(u * 12.0f, Yw / 5.0f, 12, 43)); // brighter and fainter along a crack, broken off
				const float Big = std::exp(-std::pow((F2 - F1) / 0.12f, 2.0f)) * Var;
				const float BigHalo = std::exp(-std::pow((F2 - F1) / 0.4f, 2.0f)) * Var;
				// white wet flecks, a bit longer along the tentacle
				float Fleck = 0.0f;
				for(int i = 0; i < NUM_FLECKS; i++)
				{
					float dx = std::fabs(Xw - aFx[i]);
					dx = std::min(dx, TEX_UNITS - dx);
					const float dy = Yw - aFy[i];
					if(dx > 2.0f * aFl[i] || std::fabs(dy) > 2.0f * aFw[i])
						continue;
					Fleck = std::max(Fleck, aFb[i] * std::exp(-(dx * dx) / (aFl[i] * aFl[i]) - (dy * dy) / (aFw[i] * aFw[i])));
				}
				Fleck *= Inside;
				const float Fibre = 0.035f * std::sin(c * 46.0f + 2.5f * std::sin(u * 2.0f * PI * 3.0f)) + 0.03f * std::sin(u * 2.0f * PI * 23.0f + c * 9.0f);
				// a clean, even red body; the cracks and the long veins are thin dark red lines on it (like leaf veins)
				float Value = 0.6f + 0.32f * std::pow(Nz, 0.75f) + 0.08f * (Cloud - 0.5f) + 0.5f * Fibre + Speckle(x + 0.5f, y + 0.5f);
				Value -= 0.34f * Big * Smooth(0.35f, 0.7f, Var) * Inside; // broken, not a full net
				Value -= 0.36f * Vein - 0.05f * Halo;
				Value += 0.2f * std::exp(-std::pow((ac - 0.78f) / 0.035f, 2.0f)); // the thin light line inside the outline
				Value *= 1.0f - 0.62f * Smooth(0.8f, 0.94f, ac); // the dark outline
				ColorRGBA Col = Ramp(P, Value);
				// many shades of red: wine patches and scarlet patches over the body, softly blending
				{
					const float Shade = 0.65f * PeriodicNoise(u * 5.0f, Yw / 9.0f, 5, 61) + 0.35f * PeriodicNoise(u * 11.0f, Yw / 4.5f, 11, 62);
					const float ToWine = 1.0f - Smooth(0.28f, 0.48f, Shade);
					const float ToScarlet = Smooth(0.55f, 0.75f, Shade);
					Col = MixColor(Col, Ramp(Wine, Value), 0.12f * ToWine);
					Col = MixColor(Col, Ramp(Scarlet, Value), 0.0f * ToScarlet);
				}
				// white: parts of the cracks burn white, and the wet flecks
				// (only a few faint wet flecks are left of the white)
				const float White = std::min(1.0f, Fleck) * Inside;
				Col = MixColor(Col, ColorRGBA(1.0f, 0.9f, 0.88f, 1.0f), White * 0.3f);
				if(Alpha > 0.0f)
					Put(x, y, Col.r, Col.g, Col.b, Alpha * (0.94f + 0.06f * Nz));
				// the glowing layer: the cracks, the veins, the flecks
				// the flowing light: soft red glow between the dark lines, no bright lines
				const float Light = std::min(1.0f, 0.35f * BigHalo * (1.0f - Big) * (0.4f + 0.8f * Cloud) + 0.15f * Halo) * Inside;
				if(Light > 0.002f)
					Put(x, 256 + y, 1.0f, 1.0f, 1.0f, Light);
			}
		}

		// scales: faint chevrons ">" pointing to the tip, in staggered rows; a thin dark groove with a light raised edge
		// behind it, the rest transparent (the body shows through)
		{
			constexpr int PER_REPEAT = 7;
			constexpr float A = TEX_UNITS / PER_REPEAT; // along, between chevrons
			constexpr float B = 6.3f; // across, between rows
			constexpr float K = 0.7f; // how sharp the ">" is
			for(int y = 0; y < 128; y++)
			{
				const float c = (y + 0.5f) / 128.0f * 2.0f - 1.0f;
				const float Mask = 1.0f - Smooth(0.62f, 0.8f, std::fabs(c));
				if(Mask <= 0.0f)
					continue;
				const float Yw = RADIUS * std::asin(std::clamp(c / BODY_EDGE, -1.0f, 1.0f));
				const int Row = (int)std::floor(Yw / B);
				const float Ly = Yw - (Row + 0.5f) * B; // -B/2 .. B/2 in the row
				const float Arm = 1.0f - Smooth(0.3f * B, 0.48f * B, std::fabs(Ly)); // separate marks, not a zigzag
				const float Shift = (Row & 1) ? 0.5f * A : 0.0f;
				for(int x = 0; x < TEX_W; x++)
				{
					const float Xw = (x + 0.5f) / TEX_W * TEX_UNITS;
					// lines of X + K |y| = const are ">" with the point towards +X (the tip)
					const float f = Fract((Xw - Shift + K * std::fabs(Ly)) / A);
					const float Groove = std::exp(-std::pow(std::min(f, 1.0f - f) / 0.024f, 2.0f));
					const float Edge = std::exp(-std::pow((f - 0.95f) / 0.026f, 2.0f)); // the raised edge just before the groove
					// the groove is at f = 0, the edge just before it: the chevron is the one whose groove is nearest
					const int Index = (int)std::floor((Xw - Shift + K * std::fabs(Ly)) / A + 0.5f);
					const float Pick = Hash01((Index % PER_REPEAT + PER_REPEAT) % PER_REPEAT * 17 + Row * 5);
					if(Pick < 0.4f) // a part of them is left out: fewer, scattered scales
						continue;
					const float Own = 0.75f + 0.25f * Pick;
					const float Dark = Groove * Arm * Own, Light = Edge * Arm * Own;
					if(Dark + Light < 0.004f)
						continue;
					const ColorRGBA Col = MixColor(Ramp(P, 0.3f), Ramp(P, 1.2f), Light / (Dark + Light));
					Put(x, 128 + y, Col.r, Col.g, Col.b, Mask * std::min(1.0f, 0.75f * Dark + 0.6f * Light));
				}
			}
		}
	}

	// soft band: white, alpha falls off to both edges
	for(int y = 384; y < 448; y++)
	{
		const float c = std::fabs((y - 384 + 0.5f) / 64.0f * 2.0f - 1.0f);
		const float Inv = 1.0f - c;
		const float a = Inv <= 0.0f ? 0.0f : Inv * Inv * (0.35f + 0.65f * Inv);
		for(int x = 0; x < 64; x++)
			Put(x, y, 1.0f, 1.0f, 1.0f, a);
	}
	BuildBloodSprites(Put);

	// sprites: a round glow, a four pointed star
	for(int y = 0; y < 64; y++)
	{
		for(int x = 0; x < 64; x++)
		{
			const float dx = (x + 0.5f) / 32.0f - 1.0f;
			const float dy = (y + 0.5f) / 32.0f - 1.0f;
			const float d = std::sqrt(dx * dx + dy * dy);
			const float Inv = std::max(0.0f, 1.0f - d);
			Put(x, 448 + y, 1.0f, 1.0f, 1.0f, Inv * Inv * (0.35f + 0.65f * Inv));
			const float Arms = std::max(std::exp(-std::fabs(dx) * 16.0f) * std::exp(-std::fabs(dy) * 3.2f), std::exp(-std::fabs(dy) * 16.0f) * std::exp(-std::fabs(dx) * 3.2f));
			const float Star = std::min(1.0f, Arms + 0.8f * std::exp(-d * d * 30.0f)) * Clamp01((1.0f - d) * 4.0f);
			Put(64 + x, 448 + y, 1.0f, 1.0f, 1.0f, Star);
			// a spike: base at the left, tip at the right, soft sides (top and bottom), lighter towards the tip
			// and on its upper half
			const float su = (x + 0.5f) / 64.0f, sv = (y + 0.5f) / 64.0f;
			const float Side = Smooth(0.0f, 0.16f, sv) * Smooth(0.0f, 0.16f, 1.0f - sv);
			const float Shade = 0.62f + 0.28f * su + 0.18f * std::exp(-std::pow((sv - 0.32f) / 0.12f, 2.0f));
			Put(128 + x, 448 + y, Shade, Shade, Shade, Side);
			// a drop: round with a soft rim, darker to the edge, a wet light spot up left (the color is multiplied, so
			// the drop is drawn with a brighter color: 1 here is the light spot)
			const float Spot = std::exp(-((dx + 0.3f) * (dx + 0.3f) + (dy + 0.35f) * (dy + 0.35f)) / 0.035f);
			const float DropShade = 0.55f - 0.15f * Smooth(0.4f, 0.95f, d) + 0.45f * Spot;
			Put(192 + x, 448 + y, DropShade, DropShade, DropShade, 1.0f - Smooth(0.78f, 0.98f, d));
		}
	}
}

struct CParams
{
	int m_Count = 4;
	float m_Length = 1.0f;
	float m_Width = 1.0f;
	float m_Shimmer = 1.0f;
	float m_Particles = 1.0f;
	int m_Samples = MAX_SAMPLES; // mesh detail per tentacle
	int m_AttackMode = ATTACK_COMBO;
	int m_Style = STYLE_RINKAKU;
};

struct CInput
{
	vec2 m_Pos = vec2(0.0f, 0.0f); // tee center
	vec2 m_Aim = vec2(1.0f, 0.0f); // unit aim direction
	vec2 m_Vel = vec2(0.0f, 0.0f); // units per second
	float m_Scale = 1.0f; // tee size / 64
	float m_Dt = 0.0f; // seconds since the last update, 0 = paused
	float m_AttackTime = 100.0f; // seconds since the last attack
	int m_AttackId = -1; // changes with every attack
	bool m_Active = true; // the kagune is out
	bool m_InAir = false;
	bool m_Resting = false; // afk, paused, sitting
};

struct CSamples
{
	vec2 m_aPos[MAX_SAMPLES];
	vec2 m_aNormal[MAX_SAMPLES];
	float m_aWidth[MAX_SAMPLES]; // half width of the visible body
	float m_aS[MAX_SAMPLES]; // 0 at the root, 1 at the tip
	float m_aBand[MAX_SAMPLES]; // the light flowing along the veins
	float m_aHue[MAX_SAMPLES]; // -1 .. 1: the light shifts a little
	float m_aMix[MAX_SAMPLES]; // 0 .. 1: color of the vein light
	float m_aBend[MAX_SAMPLES]; // -1 .. 1: how much it bends towards the + normal side (that side is squeezed, darker)
	int m_Num = 0;
	float m_URepeat = 2.0f; // texture repeats over the length
	float m_UOff = 0.0f; // every tentacle shows another part of the pattern
	float m_Down = 1.0f; // the side (+1 / -1 along the normals) that faces down (the teeth of a wing)
};

struct CTentacle
{
	vec2 m_aPos[SIM_POINTS];
	vec2 m_aPrev[SIM_POINTS]; // last frame (for the particles)
	float m_aAngle[LINKS]; // the smoothed link angles (not wrapped, continuous in time)
	float m_aAngleVel[LINKS];
	float m_aGoal[LINKS]; // last frame's target angles, unwrapped the same way
	float m_Phase = 0.0f;
	float m_TwitchIn = 2.0f; // seconds to the next twitch
	float m_TwitchT = -1.0f; // seconds into the current twitch, < 0 = none
	float m_TwitchAmp = 0.0f;
	// the strike
	int m_StrikeKind = KIND_SINGLE;
	float m_StrikeT = -1.0f; // seconds since the strike was ordered, < 0 = none
	float m_StrikeDelay = 0.0f;
	float m_StrikeReach = 1.0f;
	vec2 m_StrikeAim = vec2(1.0f, 0.0f);
	bool m_StrikeHit = false;
	int m_StrikeSlot = 0; // place in a double or united strike
	int m_StrikeSlots = 1;
	float m_StrikeTurn = 0.0f; // last frame's turn of the first link (keeps its way around)
	bool m_StrikeTurnSet = false;
	float m_StrikeWeight = 0.0f; // 0 = rest pose, 1 = fully in the strike
	bool m_BloodDone = false; // this strike has drawn blood already
	vec2 m_aTrail[8]; // where the tip was during a strike (oldest first), for the motion trail
	int m_TrailNum = 0;
	float m_TrailAcc = 0.0f;
	float m_RestLength = 1.0f;
};

struct CTee
{
	bool m_Init = false;
	int m_Count = 0;
	float m_Presence = 0.0f;
	float m_Facing = 1.0f; // smoothed -1 .. 1 (left .. right)
	float m_Turn = 0.0f; // 0 = facing right, 1 = facing left, smoothed
	float m_TurnVel = 0.0f;
	float m_Run = 0.0f, m_RunVel = 0.0f; // -1 .. 1 forward speed, smoothed
	float m_Fall = 0.0f, m_FallVel = 0.0f; // -1 .. 1 vertical speed, smoothed
	float m_Air = 0.0f, m_AirVel = 0.0f; // 0 .. 1
	float m_Rest = 0.0f, m_RestVel = 0.0f; // 0 .. 1
	vec2 m_Lag = vec2(0.0f, 0.0f); // a lagging copy of the tee position
	vec2 m_LagVel = vec2(0.0f, 0.0f);
	float m_Time = 0.0f;
	float m_Flare = 0.0f; // the others rear up after a strike
	int m_LastAttackId = -1;
	float m_LastAttackAt = -100.0f;
	int m_Combo = 0;
	int m_NextStriker = 0;
	float m_MoteAcc = 0.0f;
	vec2 m_LastRoot = vec2(0.0f, 0.0f);
	vec2 m_Root = vec2(0.0f, 0.0f); // where the roots are this frame (for the root glow)
	uint32_t m_Seed = 1;
	int m_Style = STYLE_RINKAKU;
	CTentacle m_aTentacles[MAX_TENTACLES];
	CSamples m_aSamples[MAX_TENTACLES];
};

struct CMote
{
	vec2 m_Pos;
	vec2 m_Vel;
	float m_Life;
	float m_LifeSpan;
	float m_Size;
	float m_Tone;
	float m_Drag;
	float m_Rise;
	float m_Stretch; // > 0: drawn as a streak along the velocity
	bool m_Front; // in front of the tees (strike sparks)
	bool m_Flash;
	bool m_Star;
	bool m_Shard; // a crystal shard of the ukaku (drawn as a spike along the velocity)
	bool m_Blood; // a drop of blood (normal blend, falls, sticks to walls and the ground)
	bool m_Splat; // a drop that hit a wall or the ground
	bool m_SplatFlat; // lies on a floor or a ceiling (else on a wall)
	bool m_Mist; // a fine spray of blood (soft, grows and fades, does not stick)
	int m_Variant; // which splat sprite, bit 2 = mirrored
};

// the particle layers
enum
{
	LAYER_BACK = 0, // additive, behind the tees: red cells
	LAYER_FRONT, // additive, in front of the tees: sparks, flashes, shards
	LAYER_SPLATS, // normal, behind the tees: blood on walls and the ground
	LAYER_DROPS, // normal, in front of the tees: flying blood
};

inline int LayerOf(const CMote &M)
{
	if(M.m_Blood)
		return M.m_Splat ? LAYER_SPLATS : LAYER_DROPS;
	return M.m_Front ? LAYER_FRONT : LAYER_BACK;
}

struct CParticles
{
	CMote m_aMotes[MAX_MOTES];
	int m_Num = 0;
	uint32_t m_Seed = 12345;

	CMote *New()
	{
		if(m_Num >= MAX_MOTES)
			return nullptr;
		CMote *p = &m_aMotes[m_Num++];
		*p = CMote();
		p->m_Life = 0.0f;
		p->m_Drag = 1.5f;
		p->m_Rise = 25.0f;
		p->m_Stretch = 0.0f;
		p->m_Front = false;
		p->m_Flash = false;
		p->m_Star = false;
		p->m_Shard = false;
		p->m_Blood = false;
		p->m_Splat = false;
		p->m_SplatFlat = true;
		p->m_Mist = false;
		p->m_Variant = 0;
		return p;
	}
	// Solid(vec2) -> bool: the map; blood drops stick where they hit it
	template<class FSolid>
	void Update(float Dt, FSolid &&Solid)
	{
		for(int i = 0; i < m_Num;)
		{
			CMote &M = m_aMotes[i];
			M.m_Life += Dt;
			if(M.m_Life >= M.m_LifeSpan)
			{
				M = m_aMotes[--m_Num];
				continue;
			}
			if(M.m_Splat)
			{
				i++;
				continue;
			}
			M.m_Vel *= std::exp(-M.m_Drag * Dt);
			M.m_Vel.y -= M.m_Rise * Dt;
			const vec2 From = M.m_Pos;
			M.m_Pos += M.m_Vel * Dt;
			if(M.m_Blood && !M.m_Mist && Solid(M.m_Pos))
			{
				// find the surface between the last two positions, stay just outside of it
				vec2 Out = From, In = M.m_Pos;
				for(int n = 0; n < 4; n++)
				{
					const vec2 Mid = (Out + In) * 0.5f;
					if(Solid(Mid))
						In = Mid;
					else
						Out = Mid;
				}
				M.m_Pos = Out;
				// which surface: the map right under it (floor), right over it (ceiling) or else a wall
				const bool Floor = Solid(Out + vec2(0.0f, 3.0f));
				const bool Ceiling = !Floor && Solid(Out - vec2(0.0f, 3.0f));
				M.m_SplatFlat = Floor || Ceiling;
				M.m_Vel = vec2(0.0f, 0.0f);
				M.m_Splat = true;
				M.m_Life = 0.0f;
				M.m_LifeSpan = 2.2f + 1.2f * Rand01(m_Seed);
				M.m_Size *= 2.0f + 1.0f * Rand01(m_Seed);
				M.m_Variant = (Rand01(m_Seed) < 0.5f ? 0 : 1) | (Rand01(m_Seed) < 0.5f ? 4 : 0);
				if(Ceiling)
					M.m_Variant |= 8;
			}
			i++;
		}
	}
	void Update(float Dt)
	{
		Update(Dt, [](vec2) { return false; });
	}
};

// ---- pose ----

inline vec2 Dir(float Facing, float Elevation)
{
	// Elevation above the horizontal line behind the tee
	return vec2(-Facing * std::cos(Elevation), -std::sin(Elevation));
}

// the world angle of a direction Elevation above the line behind the tee; Turn 0 = facing right, 1 = left,
// in between the direction swings over the head
inline float Mirror(float Elevation, float Turn)
{
	return (Elevation - PI) + Turn * (PI - 2.0f * Elevation);
}

inline float EaseOutBack(float t)
{
	const float c1 = 1.4f;
	const float c3 = c1 + 1.0f;
	const float x = t - 1.0f;
	return 1.0f + c3 * x * x * x + c1 * x * x;
}

inline vec2 RootOf(const CTee &Tee, int i, int Count, const CInput &In)
{
	const float k = Count > 1 ? i / (Count - 1.0f) : 0.5f;
	return In.m_Pos + vec2(-Tee.m_Facing * (9.0f + 2.0f * k), 3.0f + 5.0f * k - StyleOf(Tee.m_Style).m_RootUp) * In.m_Scale;
}

inline float LengthOf(int i, int Count, const CParams &Params, const CInput &In)
{
	const float k = Count > 1 ? i / (Count - 1.0f) : 0.5f;
	return 112.0f * StyleOf(Params.m_Style).m_Length * Params.m_Length * In.m_Scale * (1.0f + 0.1f * std::sin(PI * k) - 0.14f * k);
}

// the rest pose of tentacle i: LINKS target angles
inline void RestPose(const CTee &Tee, int i, const CParams &Params, const CInput &In, float Grow, float *pAngle, float &SegLength)
{
	const int Count = Tee.m_Count;
	const CTentacle &T = Tee.m_aTentacles[i];
	const float k = Count > 1 ? i / (Count - 1.0f) : 0.5f;
	const float t = Tee.m_Time;
	const float Rest = Tee.m_Rest;
	const CStyle &Style = StyleOf(Tee.m_Style);
	float Elev = mix(Rad(Style.m_ElevTop + (Style.m_ElevBottom - Style.m_ElevTop) * k), Rad(26.0f + (-36.0f - 26.0f) * k), Rest);
	float Curl = mix(Rad(48.0f - 22.0f * k), Rad(22.0f - 12.0f * k), Rest) * Style.m_Curl;
	if(Count == 1)
	{
		Elev = Rad(40.0f);
		Curl = Rad(45.0f);
	}
	// the fan breathes
	Elev += Rad(4.0f) * std::sin(0.9f * t + i * 1.7f);
	// running forward streams it back, moving backwards pushes it up; falling lifts it, jumping lets it trail down
	const float Run = Tee.m_Run;
	if(Run > 0.0f)
	{
		Elev *= 1.0f - 0.4f * Run;
		Curl *= 1.0f - 0.5f * Run;
	}
	else
		Elev += Rad(18.0f) * -Run;
	Elev += Rad(22.0f) * Tee.m_Fall;
	const float Middle = Rad(32.0f);
	const float Spread = 1.0f + 0.18f * Tee.m_Air + 0.25f * Tee.m_Flare;
	Elev = Middle + (Elev - Middle) * Spread;

	const float Amp = Style.m_Wave * Rad(mix(9.0f, 5.0f, Rest)) * (1.0f + 0.6f * Tee.m_Air) * (1.0f + 0.5f * std::fabs(Run));
	const float Length = LengthOf(i, Count, Params, In) * (1.0f + 0.03f * std::sin(1.2f * t + T.m_Phase)) * Grow;
	SegLength = Length / LINKS;
	// the S bend of the reference (a counter bend in the middle), the hooked tip; less of both while running
	const float Bend = Style.m_Bend * Rad(26.0f) * (i % 2 == 0 ? 1.0f : 0.7f) * (1.0f - 0.5f * std::max(0.0f, Run));
	const float Hook = Style.m_Hook * Rad(62.0f) * (1.0f - 0.4f * std::max(0.0f, Run));
	// a soft twitch running down the outer part
	const float Twitch = T.m_TwitchT >= 0.0f ? T.m_TwitchAmp * std::pow(std::sin(PI * Clamp01(T.m_TwitchT / 0.55f)), 2.0f) : 0.0f;
	constexpr float Ds = 1.0f / LINKS;
	CWave Slow(1.7f * t - 2.6f * 0.5f * Ds + T.m_Phase, -2.6f * Ds);
	CWave Fast(3.1f * t - 5.0f * 0.5f * Ds + 1.7f * T.m_Phase, -5.0f * Ds);
	CWave S(2.0f * PI * 0.9f * 0.5f * Ds + 0.3f, 2.0f * PI * 0.9f * Ds);
	for(int j = 0; j < LINKS; j++)
	{
		const float s = (j + 0.5f) * Ds;
		const float Wave = Amp * Slow.Sin() * (0.25f + 0.75f * s) + Rad(3.0f) * Fast.Sin() * s;
		const float Elevation = Elev + Curl * s - Bend * S.Sin() * s + Hook * Smooth(0.7f, 1.0f, s) + Wave + Twitch * Smooth(0.25f, 1.0f, s);
		pAngle[j] = Mirror(Elevation, Tee.m_Turn);
		Slow.Next();
		Fast.Next();
		S.Next();
	}
}

// LINKS angles and the segment length of a curve given by At(u), u = 0 .. 1, split in equal lengths
template<class FAt>
float CurveAngles(FAt &&At, float *pAngle)
{
	constexpr int FINE = 48;
	vec2 aFine[FINE + 1];
	float aDist[FINE + 1];
	aDist[0] = 0.0f;
	for(int n = 0; n <= FINE; n++)
	{
		aFine[n] = At(n / (float)FINE);
		if(n > 0)
			aDist[n] = aDist[n - 1] + distance(aFine[n], aFine[n - 1]);
	}
	const float Length = aDist[FINE];
	vec2 Last = aFine[0];
	int n = 0;
	for(int j = 1; j < SIM_POINTS; j++)
	{
		const float Want = Length * j / (float)LINKS;
		while(n < FINE - 1 && aDist[n + 1] < Want)
			n++;
		const float Span = aDist[n + 1] - aDist[n];
		const vec2 p = mix(aFine[n], aFine[n + 1], Span > 0.0001f ? Clamp01((Want - aDist[n]) / Span) : 0.0f);
		pAngle[j - 1] = angle(p - Last);
		Last = p;
	}
	return Length / LINKS;
}

// the time course of the strikes: Weight 0 = rest pose .. 1 = in the strike; Reach how far the tip goes
struct CStrikeCurve
{
	float m_Weight;
	float m_Reach; // times m_StrikeReach
	float m_HitAt;
	float m_End;
};

inline CStrikeCurve StrikeCurve(int Kind, float t)
{
	CStrikeCurve C;
	if(Kind == KIND_UNITED)
	{
		// press together and draw back (gather), thrust, hold, pull back
		C.m_HitAt = 0.13f;
		C.m_End = 0.5f;
		C.m_Weight = t < 0.08f ? Smooth(0.0f, 0.08f, t) : 1.0f - Smooth(0.26f, 0.5f, t);
		const float Thrust = Smooth(0.08f, 0.13f, t);
		C.m_Reach = 0.42f + (1.0f - 0.42f) * Thrust + 0.1f * std::exp(-std::pow((t - 0.15f) / 0.035f, 2.0f));
		return C;
	}
	const float Rise = Kind == KIND_DOUBLE ? 0.055f : 0.05f;
	const float Hold = Kind == KIND_DOUBLE ? 0.13f : 0.11f;
	C.m_End = Kind == KIND_DOUBLE ? 0.36f : 0.32f;
	C.m_HitAt = Rise;
	if(t < Rise)
	{
		const float x = t / Rise;
		C.m_Weight = 1.0f - (1.0f - x) * (1.0f - x);
	}
	else
		C.m_Weight = 1.0f - Smooth(Hold, C.m_End, t);
	C.m_Reach = 1.0f + 0.1f * std::exp(-std::pow((t - Rise - 0.01f) / 0.03f, 2.0f));
	return C;
}

// the stab: a curve from the root over the shoulder to the aimed point; Side bends it sideways (the two of a
// double strike cross like scissors), for the united strike every tentacle winds around the common spear
inline float StrikePose(const CTee &Tee, int i, const CInput &In, const CTentacle &T, float Reach, float *pAngle)
{
	const vec2 Root = RootOf(Tee, i, Tee.m_Count, In);
	const vec2 Aim = T.m_StrikeAim;
	const float Forward = Clamp01((dot(Aim, vec2(Tee.m_Facing, 0.0f)) + 0.35f) / 0.9f);
	const vec2 Over = In.m_Pos + vec2(-Tee.m_Facing * 14.0f, -60.0f) * In.m_Scale;
	if(T.m_StrikeKind == KIND_UNITED)
	{
		// the common spear starts between the roots, every tentacle comes from its own root into it and winds
		// around its centre line, the tips meet in one point
		const vec2 Base = RootOf(Tee, 0, Tee.m_Count, In) * 0.5f + RootOf(Tee, Tee.m_Count - 1, Tee.m_Count, In) * 0.5f;
		const vec2 Target = In.m_Pos + Aim * Reach;
		const vec2 OverHigh = In.m_Pos + vec2(-Tee.m_Facing * 22.0f, -72.0f) * In.m_Scale;
		const vec2 Control = mix((Base + Target) * 0.5f, OverHigh, Forward);
		const float Slots = (float)std::max(1, T.m_StrikeSlots);
		const float Phase = 2.0f * PI * T.m_StrikeSlot / Slots;
		const float Radius = 5.5f * In.m_Scale * std::min(1.0f, Slots / 3.0f);
		return CurveAngles([&](float u) {
			const vec2 Centre = Base * ((1.0f - u) * (1.0f - u)) + Control * (2.0f * u * (1.0f - u)) + Target * (u * u);
			const vec2 Along = (Control - Base) * (2.0f * (1.0f - u)) + (Target - Control) * (2.0f * u);
			const float Len = length(Along);
			const vec2 Side = Len > 0.001f ? vec2(-Along.y, Along.x) / Len : vec2(0.0f, 1.0f);
			const float Wind = std::sin(Phase + 2.0f * PI * 1.6f * u) * Radius * std::pow(1.0f - u, 0.7f);
			return Centre + (Root - Base) * ((1.0f - u) * (1.0f - u) * (1.0f - u)) + Side * Wind;
		},
			pAngle);
	}
	const vec2 Target = In.m_Pos + Aim * Reach;
	vec2 Control = mix((Root + Target) * 0.5f, Over, Forward);
	if(T.m_StrikeKind == KIND_DOUBLE)
	{
		const vec2 Line = Target - Root;
		const float Len = length(Line);
		if(Len > 0.001f)
			Control += vec2(-Line.y, Line.x) / Len * ((T.m_StrikeSlot == 0 ? 1.0f : -1.0f) * 16.0f * In.m_Scale);
	}
	return CurveAngles([&](float u) { return Root * ((1.0f - u) * (1.0f - u)) + Control * (2.0f * u * (1.0f - u)) + Target * (u * u); }, pAngle);
}

// ---- simulation ----

inline int PickAttackKind(CTee &Tee, const CParams &Params, int Count)
{
	int Kind = KIND_SINGLE;
	switch(Params.m_AttackMode)
	{
	case ATTACK_SINGLE: Kind = KIND_SINGLE; break;
	case ATTACK_DOUBLE: Kind = KIND_DOUBLE; break;
	case ATTACK_UNITED: Kind = KIND_UNITED; break;
	case ATTACK_RANDOM:
	{
		const float r = Rand01(Tee.m_Seed);
		Kind = r < 0.5f ? KIND_SINGLE : (r < 0.8f ? KIND_DOUBLE : KIND_UNITED);
		break;
	}
	default:
	{
		// combo: single, single, double, single, double, united; a pause starts it over
		static const int s_aCombo[] = {KIND_SINGLE, KIND_SINGLE, KIND_DOUBLE, KIND_SINGLE, KIND_DOUBLE, KIND_UNITED};
		Kind = s_aCombo[Tee.m_Combo % 6];
		break;
	}
	}
	if(Count < 2)
		Kind = KIND_SINGLE;
	return Kind;
}

inline void StrikeSparks(CParticles &Particles, const CParams &Params, const CInput &In, vec2 Hit, vec2 Aim, bool Big)
{
	if(Params.m_Particles <= 0.0f)
		return;
	// the ukaku fires crystal shards on along the strike
	if(Params.m_Style == STYLE_UKAKU)
	{
		const int Shards = Big ? 7 : 3;
		for(int n = 0; n < Shards; n++)
		{
			CMote *pShard = Particles.New();
			if(!pShard)
				break;
			const vec2 Out = rotate(Aim, RandRange(Particles.m_Seed, Big ? -16.0f : -9.0f, Big ? 16.0f : 9.0f));
			pShard->m_Pos = Hit - Aim * 10.0f * In.m_Scale;
			pShard->m_Vel = Out * RandRange(Particles.m_Seed, 900.0f, 1300.0f) * In.m_Scale;
			pShard->m_LifeSpan = RandRange(Particles.m_Seed, 0.22f, 0.34f);
			pShard->m_Size = RandRange(Particles.m_Seed, 3.0f, 4.5f) * In.m_Scale;
			pShard->m_Tone = RandRange(Particles.m_Seed, 0.5f, 1.0f);
			pShard->m_Drag = 1.5f;
			pShard->m_Rise = 0.0f;
			pShard->m_Front = true;
			pShard->m_Shard = true;
		}
	}
	const int Num = (int)((Big ? 34.0f : 16.0f) * Params.m_Particles);
	for(int n = 0; n < Num; n++)
	{
		CMote *pMote = Particles.New();
		if(!pMote)
			break;
		const vec2 Out = rotate(Aim, RandRange(Particles.m_Seed, Big ? -75.0f : -55.0f, Big ? 75.0f : 55.0f));
		pMote->m_Pos = Hit;
		pMote->m_Vel = Out * RandRange(Particles.m_Seed, 220.0f, Big ? 820.0f : 620.0f) * In.m_Scale;
		pMote->m_LifeSpan = RandRange(Particles.m_Seed, 0.16f, Big ? 0.45f : 0.36f);
		pMote->m_Size = RandRange(Particles.m_Seed, 2.2f, Big ? 4.2f : 3.6f) * In.m_Scale;
		pMote->m_Tone = RandRange(Particles.m_Seed, 0.4f, 1.0f);
		pMote->m_Drag = 4.0f;
		pMote->m_Rise = -300.0f; // they fall
		pMote->m_Stretch = 0.035f;
		pMote->m_Front = true;
	}
	CMote *pFlash = Particles.New();
	if(pFlash)
	{
		pFlash->m_Pos = Hit;
		pFlash->m_Vel = vec2(0.0f, 0.0f);
		pFlash->m_LifeSpan = Big ? 0.22f : 0.14f;
		pFlash->m_Size = (Big ? 54.0f : 30.0f) * In.m_Scale;
		pFlash->m_Tone = 1.0f;
		pFlash->m_Rise = 0.0f;
		pFlash->m_Front = true;
		pFlash->m_Flash = true;
	}
	if(Big)
	{
		CMote *pStar = Particles.New();
		if(pStar)
		{
			pStar->m_Pos = Hit;
			pStar->m_Vel = vec2(0.0f, 0.0f);
			pStar->m_LifeSpan = 0.2f;
			pStar->m_Size = 34.0f * In.m_Scale;
			pStar->m_Tone = 1.0f;
			pStar->m_Rise = 0.0f;
			pStar->m_Front = true;
			pStar->m_Flash = true;
			pStar->m_Star = true;
		}
	}
}

inline void Update(CTee &Tee, const CInput &In, const CParams &Params, CParticles &Particles)
{
	const float Dt = std::clamp(In.m_Dt, 0.0f, 0.05f);
	const int Count = std::clamp(Params.m_Count, 1, MAX_TENTACLES);
	Tee.m_Style = std::clamp(Params.m_Style, 0, NUM_STYLES - 1);
	const float WantTurn = std::fabs(In.m_Aim.x) > 0.05f ? (In.m_Aim.x < 0.0f ? 1.0f : 0.0f) : Tee.m_Turn > 0.5f ? 1.0f : 0.0f;
	const float WantRun = std::clamp(In.m_Vel.x * Tee.m_Facing / 650.0f, -1.0f, 1.0f);
	const float WantFall = std::clamp(In.m_Vel.y / 900.0f, -1.0f, 1.0f);

	// out or away: bursts out with a little overshoot, pulls back in quickly
	const float Before = Tee.m_Presence;
	if(In.m_Active)
		Tee.m_Presence = std::min(1.0f, Tee.m_Presence + Dt / 0.22f);
	else
		Tee.m_Presence = std::max(0.0f, Tee.m_Presence - Dt / 0.14f);
	const float Grow = std::max(0.05f, EaseOutBack(Tee.m_Presence));

	// teleport or respawn: no rubber band across the map
	const bool Jump = Tee.m_Init && distance(In.m_Pos, Tee.m_LastRoot) > 160.0f * In.m_Scale;
	bool Snap = Jump;
	if(!Tee.m_Init || Tee.m_Count != Count || (Before <= 0.0f && Tee.m_Presence > 0.0f))
	{
		const bool Fresh = !Tee.m_Init || Tee.m_Count != Count;
		Tee.m_Init = true;
		Tee.m_Count = Count;
		if(Fresh)
		{
			Tee.m_Seed = Tee.m_Seed * 747796405u + 2891336453u;
			for(int i = 0; i < MAX_TENTACLES; i++)
			{
				CTentacle &T = Tee.m_aTentacles[i];
				T = CTentacle();
				T.m_Phase = RandRange(Tee.m_Seed, 0.0f, 2.0f * PI);
				T.m_TwitchIn = RandRange(Tee.m_Seed, 0.8f, 3.5f);
			}
		}
		Snap = true;
		// a burst of red cells where it breaks out
		if(Tee.m_Presence > 0.0f && Params.m_Particles > 0.0f)
		{
			const int Num = (int)(14.0f * Params.m_Particles);
			for(int n = 0; n < Num; n++)
			{
				CMote *pMote = Particles.New();
				if(!pMote)
					break;
				const vec2 Out = Dir(WantTurn > 0.5f ? -1.0f : 1.0f, RandRange(Particles.m_Seed, Rad(-30.0f), Rad(100.0f)));
				pMote->m_Pos = In.m_Pos + vec2(-(WantTurn > 0.5f ? -1.0f : 1.0f) * 10.0f, 5.0f) * In.m_Scale + Out * 6.0f * In.m_Scale;
				pMote->m_Vel = Out * RandRange(Particles.m_Seed, 60.0f, 200.0f) * In.m_Scale;
				pMote->m_LifeSpan = RandRange(Particles.m_Seed, 0.35f, 0.8f);
				pMote->m_Size = RandRange(Particles.m_Seed, 2.0f, 4.0f) * In.m_Scale;
				pMote->m_Tone = Rand01(Particles.m_Seed);
				pMote->m_Drag = 3.0f;
			}
		}
	}
	if(Snap)
	{
		// start from the pose itself, no swinging in
		Tee.m_Turn = WantTurn;
		Tee.m_TurnVel = 0.0f;
		Tee.m_Facing = 1.0f - 2.0f * Tee.m_Turn;
		Tee.m_Run = std::clamp(In.m_Vel.x * Tee.m_Facing / 650.0f, -1.0f, 1.0f);
		Tee.m_Fall = WantFall;
		Tee.m_Air = In.m_InAir ? 1.0f : 0.0f;
		Tee.m_Rest = In.m_Resting ? 1.0f : 0.0f;
		Tee.m_RunVel = Tee.m_FallVel = Tee.m_AirVel = Tee.m_RestVel = 0.0f;
		Tee.m_Lag = In.m_Pos;
		Tee.m_LagVel = vec2(0.0f, 0.0f);
	}
	Tee.m_LastRoot = In.m_Pos;
	if(Dt <= 0.0f && !Snap)
		return;
	Tee.m_Time += Dt;
	Tee.m_Flare = std::max(0.0f, Tee.m_Flare - Dt / 0.45f);

	// every input glides: a turn swings the fan over the head, landing or starting to run never snaps it
	Tee.m_Turn = Clamp01(SmoothDamp(Tee.m_Turn, WantTurn, Tee.m_TurnVel, 0.11f, Dt));
	Tee.m_Facing = 1.0f - 2.0f * Tee.m_Turn;
	Tee.m_Run = SmoothDamp(Tee.m_Run, WantRun, Tee.m_RunVel, 0.16f, Dt);
	Tee.m_Fall = SmoothDamp(Tee.m_Fall, WantFall, Tee.m_FallVel, 0.14f, Dt);
	Tee.m_Air = SmoothDamp(Tee.m_Air, In.m_InAir ? 1.0f : 0.0f, Tee.m_AirVel, 0.16f, Dt);
	Tee.m_Rest = SmoothDamp(Tee.m_Rest, In.m_Resting ? 1.0f : 0.0f, Tee.m_RestVel, 0.5f, Dt);
	Tee.m_Lag = SmoothDamp(Tee.m_Lag, In.m_Pos, Tee.m_LagVel, 0.13f, Dt);
	vec2 Lag = (Tee.m_Lag - In.m_Pos) * 0.75f;
	const float LagMax = 34.0f * In.m_Scale;
	if(length(Lag) > LagMax)
		Lag = normalize(Lag) * LagMax;

	// a new hammer attack
	if(In.m_AttackId != Tee.m_LastAttackId)
	{
		const bool Fresh = Tee.m_LastAttackId != -1 && In.m_AttackTime < 0.3f && In.m_Active;
		Tee.m_LastAttackId = In.m_AttackId;
		if(Fresh)
		{
			Tee.m_Combo = Tee.m_Time - Tee.m_LastAttackAt < 0.6f ? Tee.m_Combo + 1 : 0;
			Tee.m_LastAttackAt = Tee.m_Time;
			// a united strike in its thrust is not interrupted
			bool UnitedBusy = false;
			for(int i = 0; i < Count; i++)
			{
				const CTentacle &T = Tee.m_aTentacles[i];
				if(T.m_StrikeT >= 0.0f && T.m_StrikeKind == KIND_UNITED && T.m_StrikeT < 0.3f)
					UnitedBusy = true;
			}
			const int Kind = PickAttackKind(Tee, Params, Count);
			if(!UnitedBusy)
			{
				const float Reach = 76.0f * In.m_Scale * std::sqrt(Params.m_Length);
				int aPick[MAX_TENTACLES] = {};
				int Picked = 0;
				if(Kind == KIND_UNITED)
				{
					for(int i = 0; i < Count; i++)
						aPick[Picked++] = i;
				}
				else
				{
					// the next tentacles that are not in a strike right now (else the next ones anyway)
					const int Want = Kind == KIND_DOUBLE ? 2 : 1;
					for(int Pass = 0; Pass < 2 && Picked < Want; Pass++)
						for(int n = 0; n < Count && Picked < Want; n++)
						{
							const int i = (Tee.m_NextStriker + n) % Count;
							const CTentacle &T = Tee.m_aTentacles[i];
							const bool Busy = T.m_StrikeT >= 0.0f && T.m_StrikeT - T.m_StrikeDelay < 0.2f;
							bool Already = false;
							for(int m = 0; m < Picked; m++)
								Already |= aPick[m] == i;
							if(!Already && (Pass == 1 || !Busy))
								aPick[Picked++] = i;
						}
					Tee.m_NextStriker = (aPick[Picked - 1] + 1) % Count;
				}
				for(int n = 0; n < Picked; n++)
				{
					CTentacle &T = Tee.m_aTentacles[aPick[n]];
					// keep the way around of a tentacle that is still in a strike
					T.m_StrikeTurnSet = T.m_StrikeT >= 0.0f && T.m_StrikeTurnSet;
					T.m_StrikeKind = Kind;
					T.m_StrikeT = 0.0f;
					T.m_StrikeDelay = 0.0f;
					T.m_StrikeHit = false;
					T.m_BloodDone = false;
					T.m_StrikeSlot = n;
					T.m_StrikeSlots = Picked;
					T.m_StrikeAim = In.m_Aim;
					T.m_StrikeReach = Reach;
					if(Kind == KIND_DOUBLE)
						T.m_StrikeAim = rotate(In.m_Aim, (n == 0 ? -1.0f : 1.0f) * 5.0f);
					else if(Kind == KIND_UNITED)
						T.m_StrikeReach = Reach * 1.18f;
				}
			}
			Tee.m_Flare = 1.0f;
		}
	}

	for(int i = 0; i < Count; i++)
	{
		CTentacle &T = Tee.m_aTentacles[i];

		// now and then one twitches like a muscle: a soft wave, not a kick
		T.m_TwitchIn -= Dt;
		if(T.m_TwitchT >= 0.0f)
		{
			T.m_TwitchT += Dt;
			if(T.m_TwitchT > 0.55f)
				T.m_TwitchT = -1.0f;
		}
		if(T.m_TwitchIn <= 0.0f)
		{
			T.m_TwitchIn = RandRange(Tee.m_Seed, In.m_Resting ? 2.5f : 1.2f, In.m_Resting ? 6.0f : 4.0f);
			T.m_TwitchT = 0.0f;
			T.m_TwitchAmp = (Rand01(Tee.m_Seed) < 0.5f ? -1.0f : 1.0f) * Rad(RandRange(Tee.m_Seed, 5.0f, 9.0f));
		}

		float aRest[LINKS];
		float RestSeg;
		RestPose(Tee, i, Params, In, Grow, aRest, RestSeg);
		T.m_RestLength = RestSeg * LINKS;

		// the lagging tee: the outer part trails behind the motion
		{
			float LastW = 0.0f;
			for(int j = 0; j < LINKS; j++)
			{
				const float s = (j + 1.0f) / LINKS;
				const float w = std::pow(s, 1.4f);
				const vec2 d = direction(aRest[j]) * RestSeg + Lag * (w - LastW);
				LastW = w;
				const float a = angle(d);
				aRest[j] += WrapPi(a - aRest[j]);
			}
		}

		float aTarget[LINKS];
		float Seg = RestSeg;
		float Weight = 0.0f;
		if(T.m_StrikeT >= 0.0f)
		{
			T.m_StrikeT += Dt;
			const float St = T.m_StrikeT - T.m_StrikeDelay;
			const CStrikeCurve Curve = StrikeCurve(T.m_StrikeKind, St);
			if(St > Curve.m_End)
			{
				T.m_StrikeT = -1.0f;
				T.m_StrikeTurnSet = false;
			}
			else if(St >= 0.0f)
			{
				Weight = Clamp01(Curve.m_Weight) * Clamp01(Tee.m_Presence * 2.0f);
				float aStrike[LINKS];
				const float StrikeSeg = StrikePose(Tee, i, In, T, T.m_StrikeReach * Curve.m_Reach, aStrike);
				// blend the two poses link by link in angles: each link turns from the rest angle towards the
				// strike angle, the first one keeps its way around for the whole strike, the others follow it
				Seg = mix(RestSeg, std::max(RestSeg * 0.45f, StrikeSeg), Weight);
				float Turn = WrapPi(aStrike[0] - aRest[0]);
				if(T.m_StrikeTurnSet)
					Turn = T.m_StrikeTurn + WrapPi(Turn - T.m_StrikeTurn);
				T.m_StrikeTurn = Turn;
				T.m_StrikeTurnSet = true;
				for(int j = 0; j < LINKS; j++)
				{
					if(j > 0)
						Turn += WrapPi((aStrike[j] - aRest[j]) - Turn);
					aTarget[j] = aRest[j] + Turn * Weight;
				}
				if(St >= Curve.m_HitAt && !T.m_StrikeHit)
				{
					// the hit: sparks fly on along the stab, a flash; the united spear hits once, harder
					T.m_StrikeHit = true;
					if(T.m_StrikeKind != KIND_UNITED || T.m_StrikeSlot == 0)
					{
						const vec2 Hit = In.m_Pos + T.m_StrikeAim * T.m_StrikeReach * Curve.m_Reach;
						StrikeSparks(Particles, Params, In, Hit, T.m_StrikeAim, T.m_StrikeKind == KIND_UNITED);
					}
				}
			}
		}
		T.m_StrikeWeight = Weight;
		if(Weight <= 0.0f)
		{
			for(int j = 0; j < LINKS; j++)
				aTarget[j] = aRest[j];
		}

		// follow the targets: unwrap them against last frame (they move continuously), then glide
		for(int j = 0; j < LINKS; j++)
		{
			if(Snap)
			{
				T.m_aGoal[j] = aTarget[j];
				T.m_aAngle[j] = aTarget[j];
				T.m_aAngleVel[j] = 0.0f;
				continue;
			}
			const float Goal = T.m_aGoal[j] + WrapPi(aTarget[j] - T.m_aGoal[j]);
			T.m_aGoal[j] = Goal;
			const float s = (j + 0.5f) / LINKS;
			// the root follows fast, the tip slower: motion runs down the tentacle; in a strike all of it is quick
			const float Time = mix(mix(0.04f, 0.12f, s), 0.009f, Weight);
			T.m_aAngle[j] = SmoothDamp(T.m_aAngle[j], Goal, T.m_aAngleVel[j], Time, Dt);
			// keep the numbers small, angle and goal move together
			if(std::fabs(T.m_aGoal[j]) > 8.0f * PI)
			{
				const float Shift = 2.0f * PI * std::round(T.m_aGoal[j] / (2.0f * PI));
				T.m_aGoal[j] -= Shift;
				T.m_aAngle[j] -= Shift;
			}
		}

		// the points: from the root along the smoothed angles, the length never changes
		for(int j = 0; j < SIM_POINTS; j++)
			T.m_aPrev[j] = T.m_aPos[j];
		T.m_aPos[0] = RootOf(Tee, i, Count, In);
		for(int j = 1; j < SIM_POINTS; j++)
			T.m_aPos[j] = T.m_aPos[j - 1] + direction(T.m_aAngle[j - 1]) * Seg;
		if(Snap)
		{
			for(int j = 0; j < SIM_POINTS; j++)
				T.m_aPrev[j] = T.m_aPos[j];
		}

		// the motion trail of a strike: the tip is remembered 180 times a second while it strikes, the trail
		// shortens again after
		if(Snap)
			T.m_TrailNum = 0;
		T.m_TrailAcc += Dt;
		while(T.m_TrailAcc >= 1.0f / 180.0f)
		{
			T.m_TrailAcc -= 1.0f / 180.0f;
			if(Weight > 0.25f)
			{
				if(T.m_TrailNum == 8)
				{
					for(int k = 0; k < 7; k++)
						T.m_aTrail[k] = T.m_aTrail[k + 1];
					T.m_TrailNum = 7;
				}
				T.m_aTrail[T.m_TrailNum++] = T.m_aPos[SIM_POINTS - 1];
			}
			else if(T.m_TrailNum > 0)
			{
				for(int k = 0; k + 1 < T.m_TrailNum; k++)
					T.m_aTrail[k] = T.m_aTrail[k + 1];
				T.m_TrailNum--;
			}
		}
	}
	Tee.m_Root = RootOf(Tee, 0, Count, In);

	// red cells drift off the tentacles
	if(Tee.m_Presence > 0.5f && Params.m_Particles > 0.0f && Dt > 0.0f)
	{
		Tee.m_MoteAcc += Dt * 3.2f * Count * Params.m_Particles;
		while(Tee.m_MoteAcc >= 1.0f)
		{
			Tee.m_MoteAcc -= 1.0f;
			CMote *pMote = Particles.New();
			if(!pMote)
				break;
			const CTentacle &T = Tee.m_aTentacles[(int)RandRange(Particles.m_Seed, 0.0f, Count - 0.01f)];
			const int j = (int)RandRange(Particles.m_Seed, 2.0f, SIM_POINTS - 0.01f);
			pMote->m_Pos = mix(T.m_aPos[j - 1], T.m_aPos[j], Rand01(Particles.m_Seed));
			pMote->m_Vel = (T.m_aPos[j] - T.m_aPrev[j]) / Dt * 0.3f + vec2(RandRange(Particles.m_Seed, -20.0f, 20.0f), RandRange(Particles.m_Seed, -30.0f, 5.0f)) * In.m_Scale;
			pMote->m_LifeSpan = RandRange(Particles.m_Seed, 0.5f, 1.1f);
			pMote->m_Size = RandRange(Particles.m_Seed, 1.4f, 3.0f) * In.m_Scale;
			pMote->m_Tone = Rand01(Particles.m_Seed);
		}
	}
	else
		Tee.m_MoteAcc = 0.0f;
}

// ---- blood ----

// a striking tentacle touching a tee (circle Center, Radius): the touching point, from the tip backwards
inline bool StrikeContact(const CTentacle &T, vec2 Center, float Radius, vec2 &Contact)
{
	for(int j = SIM_POINTS - 1; j >= SIM_POINTS - 4; j--)
	{
		// the point and the middle of its link (the tip moves fast, links are ~10 units long)
		const vec2 aAt[2] = {T.m_aPos[j], (T.m_aPos[j] + T.m_aPos[j - 1]) * 0.5f};
		for(const vec2 &At : aAt)
		{
			if(distance(At, Center) < Radius)
			{
				Contact = At;
				return true;
			}
		}
	}
	return false;
}

// a wound: drops of blood burst out along the strike and fall, they stick where they hit the map
inline void BloodBurst(CParticles &Particles, float Amount, vec2 At, vec2 Dir, float Scale, bool Big)
{
	if(Amount <= 0.0f)
		return;
	const int Num = (int)((Big ? 42.0f : 22.0f) * Amount);
	for(int n = 0; n < Num; n++)
	{
		CMote *pDrop = Particles.New();
		if(!pDrop)
			break;
		const vec2 Out = rotate(Dir, RandRange(Particles.m_Seed, -60.0f, 60.0f));
		const float Speed = RandRange(Particles.m_Seed, 120.0f, Big ? 680.0f : 520.0f);
		pDrop->m_Pos = At + vec2(RandRange(Particles.m_Seed, -4.0f, 4.0f), RandRange(Particles.m_Seed, -4.0f, 4.0f)) * Scale;
		pDrop->m_Vel = (Out * Speed + vec2(0.0f, -RandRange(Particles.m_Seed, 60.0f, 220.0f))) * Scale;
		pDrop->m_LifeSpan = RandRange(Particles.m_Seed, 1.0f, 1.6f);
		pDrop->m_Size = (Rand01(Particles.m_Seed) < 0.15f ? RandRange(Particles.m_Seed, 2.8f, 3.8f) : RandRange(Particles.m_Seed, 1.2f, 2.6f)) * Scale;
		pDrop->m_Tone = Rand01(Particles.m_Seed);
		pDrop->m_Drag = 0.7f;
		pDrop->m_Rise = -1500.0f; // they fall
		pDrop->m_Stretch = 0.016f;
		pDrop->m_Front = true;
		pDrop->m_Blood = true;
	}
	// a fine spray: soft dark red puffs that spread and fade
	const int Mist = Big ? 4 : 2;
	for(int n = 0; n < Mist; n++)
	{
		CMote *pMist = Particles.New();
		if(!pMist)
			break;
		pMist->m_Pos = At;
		pMist->m_Vel = rotate(Dir, RandRange(Particles.m_Seed, -35.0f, 35.0f)) * RandRange(Particles.m_Seed, 60.0f, 160.0f) * Scale;
		pMist->m_LifeSpan = RandRange(Particles.m_Seed, 0.3f, 0.5f);
		pMist->m_Size = RandRange(Particles.m_Seed, 9.0f, 14.0f) * Scale * (Big ? 1.3f : 1.0f);
		pMist->m_Tone = Rand01(Particles.m_Seed);
		pMist->m_Drag = 4.0f;
		pMist->m_Rise = -60.0f;
		pMist->m_Front = true;
		pMist->m_Blood = true;
		pMist->m_Mist = true;
	}
}

// the wound keeps bleeding a little
inline void BloodDrip(CParticles &Particles, vec2 At, vec2 Dir, float Scale)
{
	CMote *pDrop = Particles.New();
	if(!pDrop)
		return;
	pDrop->m_Pos = At + vec2(RandRange(Particles.m_Seed, -10.0f, 10.0f), RandRange(Particles.m_Seed, -8.0f, 10.0f)) * Scale;
	pDrop->m_Vel = (Dir * RandRange(Particles.m_Seed, 20.0f, 90.0f) + vec2(RandRange(Particles.m_Seed, -30.0f, 30.0f), RandRange(Particles.m_Seed, -40.0f, 20.0f))) * Scale;
	pDrop->m_LifeSpan = RandRange(Particles.m_Seed, 0.9f, 1.4f);
	pDrop->m_Size = RandRange(Particles.m_Seed, 1.0f, 2.0f) * Scale;
	pDrop->m_Tone = Rand01(Particles.m_Seed);
	pDrop->m_Drag = 0.7f;
	pDrop->m_Rise = -1400.0f;
	pDrop->m_Stretch = 0.016f;
	pDrop->m_Front = true;
	pDrop->m_Blood = true;
}

// ---- mesh ----

// the outline along the length, per kind; the same for every frame, so it is computed once per sample count
inline const float *TaperTable(int Num, int Style)
{
	static float s_aaaTable[NUM_STYLES][MAX_SAMPLES + 1][MAX_SAMPLES];
	static bool s_aaReady[NUM_STYLES][MAX_SAMPLES + 1] = {};
	Style = std::clamp(Style, 0, NUM_STYLES - 1);
	if(!s_aaReady[Style][Num])
	{
		for(int m = 0; m < Num; m++)
		{
			const float s = m / (Num - 1.0f);
			float w;
			if(Style == STYLE_KAKUJA) // thick armour to late, then a claw tip
				w = (0.8f + 0.2f * Smooth(0.0f, 0.3f, s)) * std::pow(Clamp01((1.0f - s) / 0.45f), 0.7f);
			else if(Style == STYLE_UKAKU) // a shard: widest near the shoulder, a straight taper
				w = (0.55f + 0.45f * Smooth(0.0f, 0.14f, s)) * std::pow(std::max(0.0f, 1.0f - s), 0.9f);
			else if(Style == STYLE_BONE) // a tube of segments, a short spike tip
				w = (1.0f - 0.25f * s) * std::pow(Clamp01((1.0f - s) / 0.2f), 0.8f);
			else // a blade: widest in the first half, a long taper to a sharp point
				w = (0.72f + 0.28f * Smooth(0.0f, 0.3f, s)) * std::pow(Clamp01((1.0f - s) / 0.62f), 0.8f);
			s_aaaTable[Style][Num][m] = w;
		}
		s_aaReady[Style][Num] = true;
	}
	return s_aaaTable[Style][Num];
}

// the smooth centre line, its normals, widths and lights, for this frame
inline void BuildSamples(CTee &Tee, const CParams &Params, float Scale)
{
	const CStyle &Style = StyleOf(Tee.m_Style);
	// segmented kinds need more samples for their joints
	const int Num = std::clamp(Style.m_Plates > 0 ? Params.m_Samples * 3 / 2 : Params.m_Samples, 8, MAX_SAMPLES);
	const float Width = 9.0f * Style.m_Width * Params.m_Width * Scale * std::sqrt(std::max(0.0f, Tee.m_Presence));
	const float *pTaper = TaperTable(Num, Tee.m_Style);
	const float Now = Tee.m_Time;
	const float Ds = 1.0f / (Num - 1.0f);
	for(int i = 0; i < Tee.m_Count; i++)
	{
		const CTentacle &T = Tee.m_aTentacles[i];
		CSamples &S = Tee.m_aSamples[i];
		S.m_Num = Num;
		S.m_URepeat = std::max(0.5f, T.m_RestLength / (TEX_UNITS * Scale));
		S.m_UOff = 0.37f * i;
		const auto P = [&](int j) {
			if(j < 0)
				return T.m_aPos[0] * 2.0f - T.m_aPos[1];
			if(j >= SIM_POINTS)
				return T.m_aPos[SIM_POINTS - 1] * 2.0f - T.m_aPos[SIM_POINTS - 2];
			return T.m_aPos[j];
		};
		for(int m = 0; m < Num; m++)
		{
			const float x = m / (Num - 1.0f) * (SIM_POINTS - 1);
			const int j = std::min((int)x, SIM_POINTS - 2);
			const float t = x - j;
			const vec2 P0 = P(j - 1), P1 = P(j), P2 = P(j + 1), P3 = P(j + 2);
			const float t2 = t * t, t3 = t2 * t;
			S.m_aPos[m] = (P1 * 2.0f + (P2 - P0) * t + (P0 * 2.0f - P1 * 5.0f + P2 * 4.0f - P3) * t2 + (P1 * 3.0f - P0 - P2 * 3.0f + P3) * t3) * 0.5f;
			S.m_aS[m] = m / (Num - 1.0f);
		}
		const float Phase = T.m_Phase;
		// two pulses of light flowing along the veins to the tip; the light hue; the vein color
		CWave BandA(2.0f * PI * (-Now * 0.7f) + Phase, 2.0f * PI * 1.1f * Ds);
		CWave BandB(2.0f * PI * (-Now * 0.45f) + Phase * 1.9f + 1.3f, 2.0f * PI * 0.6f * Ds);
		CWave Hue(2.0f * PI * (-Now * 0.23f) + Phase, 2.0f * PI * 0.6f * Ds);
		CWave Mix(2.0f * PI * (-Now * 0.35f) + i, 2.0f * PI * 0.7f * Ds);
		// a slow swelling running down the tentacle like a pulse (soft armour and bone barely)
		const float Peri = Tee.m_Style == STYLE_RINKAKU ? 0.05f : (Tee.m_Style == STYLE_UKAKU ? 0.03f : 0.015f);
		CWave Pulse(-2.3f * Now + Phase * 2.3f, 2.0f * PI * 1.4f * Ds);
		vec2 LastNormal = vec2(0.0f, 1.0f);
		float Down = 0.0f;
		for(int m = 0; m < Num; m++)
		{
			const vec2 Along = S.m_aPos[std::min(m + 1, Num - 1)] - S.m_aPos[std::max(m - 1, 0)];
			const float Len = length(Along);
			S.m_aNormal[m] = Len > 0.0001f ? vec2(-Along.y, Along.x) / Len : LastNormal;
			LastNormal = S.m_aNormal[m];
			float w = Width * pTaper[m];
			if(Style.m_Plates > 0)
			{
				// the segments: plates with narrow joints (bone) or plates growing to their end (armour)
				const float f = Fract((S.m_aS[m] * S.m_URepeat + S.m_UOff) * Style.m_Plates);
				if(Tee.m_Style == STYLE_BONE)
					w *= Style.m_Joint + (1.0f - Style.m_Joint) * Smooth(0.04f, 0.16f, f) * (1.0f - Smooth(0.84f, 0.96f, f));
				else
					w *= Style.m_Joint + (1.0f - Style.m_Joint) * Smooth(0.0f, 0.92f, f);
			}
			w *= 1.0f + Peri * Pulse.Sin() * Smooth(0.05f, 0.3f, S.m_aS[m]);
			Pulse.Next();
			S.m_aWidth[m] = w;
			S.m_aBand[m] = Pow10(std::max(0.0f, BandA.Sin())) + 0.6f * Pow14(std::max(0.0f, BandB.Sin()));
			S.m_aHue[m] = Hue.Sin();
			S.m_aMix[m] = 0.5f + 0.5f * Mix.Sin();
			Down += S.m_aNormal[m].y;
			BandA.Next();
			BandB.Next();
			Hue.Next();
			Mix.Next();
		}
		// the bend at every sample (sine of the turn between the links around it), smoothed over its neighbours
		{
			float aRaw[MAX_SAMPLES];
			for(int m = 0; m < Num; m++)
			{
				if(m == 0 || m == Num - 1)
				{
					aRaw[m] = 0.0f;
					continue;
				}
				const vec2 A = S.m_aPos[m] - S.m_aPos[m - 1], B = S.m_aPos[m + 1] - S.m_aPos[m];
				const float La = length(A), Lb = length(B);
				aRaw[m] = La > 0.0001f && Lb > 0.0001f ? (A.x * B.y - A.y * B.x) / (La * Lb) : 0.0f;
			}
			for(int m = 0; m < Num; m++)
			{
				const float Sum = aRaw[std::max(m - 1, 0)] + 2.0f * aRaw[m] + aRaw[std::min(m + 1, Num - 1)];
				S.m_aBend[m] = std::clamp(Sum * 0.25f * (Num - 1) * 0.3f, -1.0f, 1.0f);
			}
		}
		// the side facing down (with a margin, so the teeth do not jump between the sides)
		if(Down > 0.25f * Num)
			S.m_Down = 1.0f;
		else if(Down < -0.25f * Num)
			S.m_Down = -1.0f;
	}
}

struct CVertex
{
	vec2 m_Pos;
	float m_U;
	float m_V;
	ColorRGBA m_Color;
};

inline float Heartbeat(float t)
{
	const float x = Fract(t / 1.15f);
	return 0.82f + 0.18f * (std::exp(-std::pow(x / 0.05f, 2.0f)) + 0.7f * std::exp(-std::pow((x - 0.17f) / 0.06f, 2.0f)));
}

// additive, behind the bodies: a soft glow around every tentacle and on the back where they grow out
template<class FQuad>
void EmitGlow(const CTee &Tee, const CPalette &Pal, float Alpha, float Scale, FQuad &&Quad)
{
	if(Tee.m_Presence <= 0.0f)
		return;
	const float Beat = Heartbeat(Tee.m_Time);
	const float Strength = StyleOf(Tee.m_Style).m_Glow;
	for(int i = 0; i < Tee.m_Count; i++)
	{
		const CSamples &S = Tee.m_aSamples[i];
		CVertex aV[4];
		for(int m = 0; m + 1 < S.m_Num; m++)
		{
			for(int e = 0; e < 2; e++)
			{
				const int n = m + e;
				const float s = S.m_aS[n];
				const float w = S.m_aWidth[n] * 2.2f + 3.0f * Scale * (1.0f - s);
				const float a = Strength * Alpha * Beat * (0.75f + 0.5f * S.m_aBand[n]) * (1.0f - 0.55f * s);
				const ColorRGBA Col = Pal.m_Glow.WithAlpha(a);
				aV[e * 2 + 0] = {S.m_aPos[n] - S.m_aNormal[n] * w, SOFT_U, SOFT_V0, Col};
				aV[e * 2 + 1] = {S.m_aPos[n] + S.m_aNormal[n] * w, SOFT_U, SOFT_V1, Col};
			}
			Quad(aV);
		}
	}
	// the kakuhou: light on the lower back where the tentacles break out
	const float r = 26.0f * Scale * (0.6f + 0.4f * Tee.m_Presence);
	const ColorRGBA Col = Pal.m_Glow.WithAlpha(1.6f * Strength * Alpha * Beat);
	const CVertex aV[4] = {
		{Tee.m_Root + vec2(-r, -r), GLOW_UV.m_U0, GLOW_UV.m_V0, Col},
		{Tee.m_Root + vec2(r, -r), GLOW_UV.m_U1, GLOW_UV.m_V0, Col},
		{Tee.m_Root + vec2(-r, r), GLOW_UV.m_U0, GLOW_UV.m_V1, Col},
		{Tee.m_Root + vec2(r, r), GLOW_UV.m_U1, GLOW_UV.m_V1, Col},
	};
	Quad(aV);
}

// normal blend, one tentacle after another (the front one covers the back one completely): tendrils behind,
// spikes, the body, the scales (rinkaku), the glowing veins / seams / eyes, the tendrils where they wind in front
template<class FQuad>
void EmitBody(const CTee &Tee, const CPalette &Pal, const CParams &Params, float Alpha, float Scale, FQuad &&Quad)
{
	if(Tee.m_Presence <= 0.0f)
		return;
	const CStyle &Style = StyleOf(Tee.m_Style);
	const float Ext = 1.0f / BODY_EDGE; // the geometry is a bit wider than the visible body
	for(int i = 0; i < Tee.m_Count; i++)
	{
		const CSamples &S = Tee.m_aSamples[i];
		const CTentacle &T = Tee.m_aTentacles[i];
		const float UScale = S.m_URepeat;
		const float UOff = S.m_UOff;
		float aTint[MAX_SAMPLES][2];
		CVertex aV[4];
		for(int n = 0; n < S.m_Num; n++)
		{
			// light from above on the upper side, the root darker where it comes out of the body
			const float s = S.m_aS[n];
			// (the rinkaku darkens softly to its point, deep red at the tip, like in the art)
			const float Root = (0.6f + 0.4f * Smooth(0.0f, 0.22f, s)) * (Tee.m_Style == STYLE_RINKAKU ? 1.0f - 0.45f * Smooth(0.6f, 1.0f, s) : 1.0f);
			const float LitA = 0.5f + 0.5f * dot(-S.m_aNormal[n], vec2(0.0f, -1.0f));
			aTint[n][0] = Root * (0.78f + 0.22f * LitA);
			aTint[n][1] = Root * (0.78f + 0.22f * (1.0f - LitA));
			// a bend squeezes its inner side (darker, a fold) and stretches the outer one (a bit lighter)
			const float Bend = S.m_aBend[n];
			aTint[n][1] *= 1.0f - 0.32f * std::max(0.0f, Bend) + 0.06f * std::max(0.0f, -Bend);
			aTint[n][0] *= 1.0f - 0.32f * std::max(0.0f, -Bend) + 0.06f * std::max(0.0f, Bend);
		}
		const auto Strip = [&](float V0, float V1, float WidthK, auto &&Color) {
			for(int m = 0; m + 1 < S.m_Num; m++)
			{
				bool Any = false;
				for(int e = 0; e < 2; e++)
				{
					const int n = m + e;
					const vec2 Off = S.m_aNormal[n] * (S.m_aWidth[n] * WidthK);
					const float u = S.m_aS[n] * UScale + UOff;
					aV[e * 2 + 0] = {S.m_aPos[n] - Off, u, V0, Color(n, 0)};
					aV[e * 2 + 1] = {S.m_aPos[n] + Off, u, V1, Color(n, 1)};
					Any |= aV[e * 2].m_Color.a > 0.003f;
				}
				if(Any)
					Quad(aV);
			}
		};

		// the tendrils (kakuja) and streamers (ukaku): thin soft strands winding around the tentacle; behind the
		// body where they go round the back, in front where they come round the front
		const auto Tendrils = [&](bool Front) {
			for(int k = 0; k < Style.m_Tendrils; k++)
			{
				const float Turns = Tee.m_Style == STYLE_UKAKU ? 1.3f + 0.4f * k : 2.2f;
				const float Speed = Tee.m_Style == STYLE_UKAKU ? 2.2f : 1.4f;
				const float Amp = Tee.m_Style == STYLE_UKAKU ? 1.3f + 0.4f * k : 1.2f;
				const float Ph = T.m_Phase * 1.7f + k * 2.4f;
				const ColorRGBA Base = Tee.m_Style == STYLE_UKAKU ? MixColor(Pal.m_Bright, Pal.m_Hot, 0.5f) : Pal.m_Hot;
				vec2 aPos[MAX_SAMPLES];
				float aSide[MAX_SAMPLES];
				for(int n = 0; n < S.m_Num; n++)
				{
					const float a = 2.0f * PI * Turns * S.m_aS[n] - Tee.m_Time * Speed + Ph;
					// the strand stays outside the narrow tip
					const float r = std::max(S.m_aWidth[n], 2.5f * Scale * (1.0f - S.m_aS[n])) * Amp;
					aPos[n] = S.m_aPos[n] + S.m_aNormal[n] * (std::sin(a) * r);
					aSide[n] = std::cos(a);
				}
				for(int m = 0; m + 1 < S.m_Num; m++)
				{
					bool Any = false;
					for(int e = 0; e < 2; e++)
					{
						const int n = m + e;
						const vec2 Along = aPos[std::min(n + 1, S.m_Num - 1)] - aPos[std::max(n - 1, 0)];
						const float Len = length(Along);
						const vec2 Nrm = Len > 0.0001f ? vec2(-Along.y, Along.x) / Len : S.m_aNormal[n];
						const float w = Style.m_TendrilWidth * Scale * (1.0f - 0.7f * S.m_aS[n]);
						const float Vis = Front ? Smooth(0.0f, 0.35f, aSide[n]) : 1.0f;
						const float a = Alpha * Vis * Smooth(0.0f, 0.06f, S.m_aS[n]) * (1.0f - Smooth(0.9f, 1.0f, S.m_aS[n]));
						// the inner half of the soft band: solid in the middle, soft at the edges
						aV[e * 2 + 0] = {aPos[n] - Nrm * w, SOFT_U, SOFT_V0 + (SOFT_V1 - SOFT_V0) * 0.25f, Base.WithAlpha(a)};
						aV[e * 2 + 1] = {aPos[n] + Nrm * w, SOFT_U, SOFT_V1 - (SOFT_V1 - SOFT_V0) * 0.25f, Base.WithAlpha(a)};
						Any |= a > 0.003f;
					}
					if(Any)
						Quad(aV);
				}
			}
		};
		// a soft shadow down right: lies on the tentacles drawn before (behind), gives the bundle its depth
		{
			const vec2 ShadowOff = vec2(2.4f, 3.4f) * Scale;
			for(int m = 0; m + 1 < S.m_Num; m += 2)
			{
				const int m2 = std::min(m + 2, S.m_Num - 1);
				const int aN[2] = {m, m2};
				for(int e = 0; e < 2; e++)
				{
					const int n = aN[e];
					const float w = S.m_aWidth[n] * 1.2f + 1.6f * Scale;
					const ColorRGBA Col(0.0f, 0.0f, 0.0f, 0.24f * Alpha * (1.0f - 0.6f * S.m_aS[n]));
					aV[e * 2 + 0] = {S.m_aPos[n] + ShadowOff - S.m_aNormal[n] * w, SOFT_U, SOFT_V0, Col};
					aV[e * 2 + 1] = {S.m_aPos[n] + ShadowOff + S.m_aNormal[n] * w, SOFT_U, SOFT_V1, Col};
				}
				Quad(aV);
			}
		}
		Tendrils(false);

		// spikes, claws and teeth: behind the body, their base hidden in it
		if(Style.m_Spikes > 0)
		{
			const ColorRGBA SpikeCol = Tee.m_Style == STYLE_BONE ? Pal.m_Bright : (Tee.m_Style == STYLE_UKAKU ? MixColor(Pal.m_Bright, Pal.m_Hot, 0.35f) : Ramp(Pal, 0.8f));
			const int First = (int)std::floor(UOff * Style.m_Spikes);
			for(int k = 0; k < 64; k++)
			{
				const int Index = First + k;
				const float u = (Index + 0.5f) / Style.m_Spikes;
				const float s = (u - UOff) / UScale;
				if(s < 0.06f)
					continue;
				if(s > 0.92f)
					break;
				const float x = s * (S.m_Num - 1);
				const int m = std::min((int)x, S.m_Num - 2);
				const float t = x - m;
				const vec2 Pos = mix(S.m_aPos[m], S.m_aPos[m + 1], t);
				vec2 Nrm = mix(S.m_aNormal[m], S.m_aNormal[m + 1], t);
				Nrm = length(Nrm) > 0.0001f ? normalize(Nrm) : S.m_aNormal[m];
				const vec2 Along = vec2(Nrm.y, -Nrm.x);
				const float Width = mix(S.m_aWidth[m], S.m_aWidth[m + 1], t);
				const float Len = Style.m_SpikeLength * Width;
				const float BaseHalf = Width * (Tee.m_Style == STYLE_UKAKU ? 0.3f : 0.5f);
				for(int Side = -1; Side <= 1; Side += 2)
				{
					if(Style.m_SpikeSides == 1 && Side != ((Index & 1) ? 1 : -1))
						continue;
					if(Style.m_SpikeSides == 3 && (float)Side != S.m_Down)
						continue;
					const vec2 Base = Pos + Nrm * (Side * Width * 0.7f);
					const vec2 Tip = Pos + Nrm * (Side * (Width + Len)) + Along * (Style.m_SpikeLean * Len);
					const ColorRGBA Col = SpikeCol.WithAlpha(Alpha);
					aV[0] = {Base - Along * BaseHalf, SPIKE_UV.m_U0, SPIKE_UV.m_V0, Col};
					aV[1] = {Base + Along * BaseHalf, SPIKE_UV.m_U0, SPIKE_UV.m_V1, Col};
					aV[2] = {Tip - Along * (BaseHalf * 0.07f), SPIKE_UV.m_U1, SPIKE_UV.m_V0, Col};
					aV[3] = {Tip + Along * (BaseHalf * 0.07f), SPIKE_UV.m_U1, SPIKE_UV.m_V1, Col};
					Quad(aV);
				}
			}
		}

		Strip(BODY_V0, BODY_V1, Ext, [&](int n, int Side) {
			const float v = aTint[n][Side];
			if(Tee.m_Style == STYLE_UKAKU)
			{
				// violet at the shoulder, pink to the points
				const float k = Smooth(0.0f, 0.75f, S.m_aS[n]);
				return ColorRGBA(v * (0.68f + 0.32f * k), v * (0.55f + 0.45f * k), v, Alpha);
			}
			if(Tee.m_Style != STYLE_RINKAKU)
				return ColorRGBA(v, v, v, Alpha);
			return ColorRGBA(v, v * (0.95f + 0.05f * S.m_aHue[n]), v * (0.93f - 0.07f * S.m_aHue[n]), Alpha);
		});
		// the scales: towards the tip on every tentacle, a faint patch near the root on every other one
		if(Style.m_Scales)
		{
			const float Center = 0.66f + 0.05f * std::sin(T.m_Phase);
			Strip(SCALE_V0, SCALE_V1, Ext, [&](int n, int Side) {
				const float s = S.m_aS[n];
				float a = Smooth(Center - 0.24f, Center - 0.08f, s) * (1.0f - Smooth(Center + 0.18f, Center + 0.3f, s));
				if(i % 2 == 1)
					a = std::max(a, 0.55f * Smooth(0.08f, 0.16f, s) * (1.0f - Smooth(0.24f, 0.34f, s)));
				if(Tee.m_Style == STYLE_RINKAKU) // the faint chevrons evenly along, none on the point (it stays bright)
					a = 0.6f * Smooth(0.08f, 0.18f, s) * (1.0f - Smooth(0.7f, 0.85f, s));
				const float v = aTint[n][Side];
				return ColorRGBA(v, v, v, a * Alpha);
			});
		}
		// the light: always faintly lit, brighter where the light flows
		if(Params.m_Shimmer > 0.0f)
		{
			const float Faint = Tee.m_Style == STYLE_RINKAKU ? 0.32f : 0.45f;
			Strip(VEIN_V0, VEIN_V1, Ext, [&](int n, int) {
				const float s = S.m_aS[n];
				const float Mix = S.m_aMix[n];
				const float TipFade = 0.85f;
				const float a = Clamp01((Faint + (1.0f - Faint) * S.m_aBand[n]) * Params.m_Shimmer) * Alpha * Smooth(0.0f, 0.1f, s) * (1.0f - Smooth(TipFade, 1.0f, s));
				if(Tee.m_Style == STYLE_RINKAKU) // saturated red cracks, white hot where the light flows
					return MixColor(Pal.m_Bright, Pal.m_Hot, std::min(1.0f, 0.15f + 0.5f * S.m_aBand[n] + 0.1f * Mix)).WithAlpha(a);
				return ColorRGBA(Pal.m_Hot.r + (Pal.m_Bright.r - Pal.m_Hot.r) * Mix * 0.5f, Pal.m_Hot.g + (Pal.m_Bright.g - Pal.m_Hot.g) * Mix * 0.5f,
					Pal.m_Hot.b + (Pal.m_Bright.b - Pal.m_Hot.b) * Mix * 0.5f, a);
			});
		}
		// the gloss: a light line on the side that faces the light (up left); it moves over the body as the
		// tentacle bends and turns, so the body reads round and wet (or polished for armour, crystal and bone)
		{
			const vec2 Light = normalize(vec2(-0.45f, -0.9f));
			float Gloss = 0.3f;
			ColorRGBA GlossCol = MixColor(Pal.m_Hot, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), 0.4f);
			if(Tee.m_Style == STYLE_KAKUJA)
			{
				Gloss = 0.5f;
				GlossCol = ColorRGBA(0.78f, 0.72f, 0.9f, 1.0f);
			}
			else if(Tee.m_Style == STYLE_UKAKU)
			{
				Gloss = 0.45f;
				GlossCol = ColorRGBA(1.0f, 0.86f, 0.97f, 1.0f);
			}
			else if(Tee.m_Style == STYLE_BONE)
			{
				Gloss = 0.4f;
				GlossCol = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
			}
			for(int m = 0; m + 1 < S.m_Num; m += 2)
			{
				const int m2 = std::min(m + 2, S.m_Num - 1);
				const int aN[2] = {m, m2};
				bool Any = false;
				for(int e = 0; e < 2; e++)
				{
					const int n = aN[e];
					const float d = dot(S.m_aNormal[n], Light);
					const float s = S.m_aS[n];
					const vec2 At = S.m_aPos[n] + S.m_aNormal[n] * (S.m_aWidth[n] * 0.42f * std::clamp(d * 2.5f, -1.0f, 1.0f));
					const float w = S.m_aWidth[n] * 0.2f + 0.6f * Scale;
					const float a = Gloss * Alpha * Smooth(0.15f, 0.8f, std::fabs(d)) * Smooth(0.0f, 0.1f, s) * (1.0f - Smooth(0.8f, 1.0f, s));
					aV[e * 2 + 0] = {At - S.m_aNormal[n] * w, SOFT_U, SOFT_V0, GlossCol.WithAlpha(a)};
					aV[e * 2 + 1] = {At + S.m_aNormal[n] * w, SOFT_U, SOFT_V1, GlossCol.WithAlpha(a)};
					Any |= a > 0.003f;
				}
				if(Any)
					Quad(aV);
			}
		}
		Tendrils(true);
	}
}

// additive, in front of the bodies (still behind the tee): a glint on stabbing tips
template<class FQuad>
void EmitLights(const CTee &Tee, const CPalette &Pal, float Alpha, float Scale, FQuad &&Quad)
{
	if(Tee.m_Presence <= 0.0f)
		return;
	for(int i = 0; i < Tee.m_Count; i++)
	{
		const CTentacle &T = Tee.m_aTentacles[i];
		// the motion trail: a soft streak of light along the path of the striking tip, thin and faint at its old end
		if(T.m_TrailNum > 0)
		{
			// from the tip back along the remembered points, at most 60 units: a smear behind the tip, not a line
			// across the whole swing
			vec2 aBack[9];
			int Back = 0;
			aBack[Back++] = T.m_aPos[SIM_POINTS - 1];
			float Left = 60.0f * Scale;
			for(int k = T.m_TrailNum - 1; k >= 0 && Left > 0.0f; k--)
			{
				const float d = distance(aBack[Back - 1], T.m_aTrail[k]);
				if(d < 1.5f * Scale)
					continue;
				const vec2 From = aBack[Back - 1];
				aBack[Back] = d > Left ? mix(From, T.m_aTrail[k], Left / d) : T.m_aTrail[k];
				Back++;
				Left -= d;
			}
			vec2 aPath[9];
			int Num = 0;
			for(int k = Back - 1; k >= 0; k--)
				aPath[Num++] = aBack[k];
			const ColorRGBA Col = MixColor(Pal.m_Glow, Pal.m_Hot, 0.4f);
			CVertex aV[4];
			for(int k = 0; k + 1 < Num; k++)
			{
				for(int e = 0; e < 2; e++)
				{
					const int n = k + e;
					const vec2 Along = aPath[std::min(n + 1, Num - 1)] - aPath[std::max(n - 1, 0)];
					const float Len = length(Along);
					const vec2 Nrm = Len > 0.0001f ? vec2(-Along.y, Along.x) / Len : vec2(0.0f, 1.0f);
					const float f = (n + 1.0f) / Num; // 0 old .. 1 the tip
					const float w = (2.0f + 7.0f * f) * Scale;
					const ColorRGBA C = Col.WithAlpha(0.4f * Alpha * f * f);
					aV[e * 2 + 0] = {aPath[n] - Nrm * w, SOFT_U, SOFT_V0, C};
					aV[e * 2 + 1] = {aPath[n] + Nrm * w, SOFT_U, SOFT_V1, C};
				}
				Quad(aV);
			}
		}
		if(T.m_StrikeWeight > 0.6f && (T.m_StrikeKind != KIND_UNITED || T.m_StrikeSlot == 0))
		{
			const vec2 Tip = T.m_aPos[SIM_POINTS - 1];
			const float r = (T.m_StrikeKind == KIND_UNITED ? 14.0f : 9.0f) * Scale * T.m_StrikeWeight;
			const ColorRGBA Col = Pal.m_Hot.WithAlpha(0.8f * Alpha * T.m_StrikeWeight);
			const CVertex aV[4] = {
				{Tip + vec2(-r, -r), STAR_UV.m_U0, STAR_UV.m_V0, Col},
				{Tip + vec2(r, -r), STAR_UV.m_U1, STAR_UV.m_V0, Col},
				{Tip + vec2(-r, r), STAR_UV.m_U0, STAR_UV.m_V1, Col},
				{Tip + vec2(r, r), STAR_UV.m_U1, STAR_UV.m_V1, Col},
			};
			Quad(aV);
		}
	}
}

// the particles of one layer (LAYER_*): red cells, sparks, flashes and shards are additive, blood is normal
template<class FQuad, class FSees>
void EmitParticles(const CParticles &Particles, const CPalette &Pal, int Layer, FSees &&Sees, FQuad &&Quad)
{
	for(int i = 0; i < Particles.m_Num; i++)
	{
		const CMote &M = Particles.m_aMotes[i];
		if(LayerOf(M) != Layer)
			continue;
		const float Progress = M.m_Life / M.m_LifeSpan;
		const float Fade = std::min(1.0f, M.m_Life / 0.06f) * (1.0f - Smooth(0.55f, 1.0f, Progress));
		ColorRGBA Col;
		float Size = M.m_Size * 2.0f;
		vec2 Right = vec2(Size * 0.5f, 0.0f);
		vec2 Center = M.m_Pos;
		vec2 Up = vec2(0.0f, -Size * 0.5f);
		CSpriteUv Uv = GLOW_UV;
		if(M.m_Blood)
		{
			// dark red drops, a few lighter (the sprites are shaded gray with a wet light spot, so the color is
			// brighter than the blood: the spot shows it); splats darker, uneven pools on floors, blots running
			// down walls; a fine spray of soft puffs
			Uv = DROP_UV;
			Col = MixColor(ColorRGBA(0.72f, 0.03f, 0.05f, 1.0f), ColorRGBA(1.0f, 0.09f, 0.1f, 1.0f), M.m_Tone);
			if(M.m_Mist)
			{
				Uv = GLOW_UV;
				const float r = M.m_Size * (0.5f + 0.9f * Progress);
				Right = vec2(r, 0.0f);
				Up = vec2(0.0f, -r);
				Col = ColorRGBA(0.45f, 0.02f, 0.03f, 0.4f * (1.0f - Progress) * std::min(1.0f, M.m_Life / 0.03f));
			}
			else if(M.m_Splat)
			{
				Col = MixColor(Col, ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f), 0.38f);
				Col.a = 0.95f * (1.0f - Smooth(0.6f, 1.0f, Progress));
				const float Grow = 0.55f + 0.45f * Smooth(0.0f, 0.12f, M.m_Life); // it spreads when it lands
				if(M.m_SplatFlat)
				{
					Uv = (M.m_Variant & 1) ? SPLAT2_UV : SPLAT_UV;
					Right = vec2(M.m_Size * 1.7f * Grow, 0.0f);
					Up = vec2(0.0f, -M.m_Size * 0.6f * Grow);
					// it lies on the surface: mostly above a floor, mostly under a ceiling
					Center = M.m_Pos + Up * ((M.m_Variant & 8) ? -0.3f : 0.3f);
				}
				else
				{
					// the blot sits where the drop hit, its run goes down
					Uv = DRIP_UV;
					Right = vec2(M.m_Size * 0.8f * Grow, 0.0f);
					Up = vec2(0.0f, -M.m_Size * 1.8f * Grow);
					Center = M.m_Pos - Up * 0.45f;
				}
				if(M.m_Variant & 4)
					std::swap(Uv.m_U0, Uv.m_U1);
			}
			else
			{
				Col.a = 1.0f - Smooth(0.75f, 1.0f, Progress);
				const float Speed = length(M.m_Vel);
				if(Speed > 1.0f)
				{
					const vec2 d = M.m_Vel / Speed;
					Right = d * ((M.m_Size + Speed * M.m_Stretch) * 0.5f);
					Up = vec2(d.y, -d.x) * (M.m_Size * 0.5f);
				}
				else
				{
					Right = vec2(M.m_Size * 0.5f, 0.0f);
					Up = vec2(0.0f, -M.m_Size * 0.5f);
				}
			}
		}
		else if(M.m_Shard)
		{
			// a crystal shard flying along its velocity: a spike, base behind, tip ahead
			const float Speed = length(M.m_Vel);
			const vec2 d = Speed > 1.0f ? M.m_Vel / Speed : vec2(1.0f, 0.0f);
			const vec2 Side = vec2(d.y, -d.x) * (M.m_Size * 0.55f);
			const vec2 Back = M.m_Pos - d * (M.m_Size * 3.2f);
			const vec2 Tip = M.m_Pos + d * (M.m_Size * 3.2f);
			Col = MixColor(Pal.m_Bright, Pal.m_Hot, M.m_Tone);
			Col.a = Fade;
			if(!Sees(M.m_Pos, M.m_Size * 4.0f))
				continue;
			const CVertex aV[4] = {
				{Back - Side, SPIKE_UV.m_U0, SPIKE_UV.m_V0, Col},
				{Back + Side, SPIKE_UV.m_U0, SPIKE_UV.m_V1, Col},
				{Tip - Side * 0.07f, SPIKE_UV.m_U1, SPIKE_UV.m_V0, Col},
				{Tip + Side * 0.07f, SPIKE_UV.m_U1, SPIKE_UV.m_V1, Col},
			};
			Quad(aV);
			continue;
		}
		else
		{
			const float Tone = M.m_Tone * M.m_Tone * M.m_Tone; // mostly red, a few pink ones
			Col = ColorRGBA(Pal.m_Spark.r + (Pal.m_Hot.r - Pal.m_Spark.r) * Tone, Pal.m_Spark.g + (Pal.m_Hot.g - Pal.m_Spark.g) * Tone,
				Pal.m_Spark.b + (Pal.m_Hot.b - Pal.m_Spark.b) * Tone, 1.0f);
			if(M.m_Flash)
			{
				Size = M.m_Size * (0.6f + 0.8f * Progress);
				Right = vec2(Size, 0.0f);
				Up = vec2(0.0f, -Size);
				Col = M.m_Star ? Pal.m_Hot : Pal.m_Glow;
				Col.a = 0.9f * (1.0f - Progress) * (1.0f - Progress);
				if(M.m_Star)
					Uv = STAR_UV;
			}
			else
			{
				Col.a = Fade;
				const float Speed = length(M.m_Vel);
				if(M.m_Stretch > 0.0f && Speed > 1.0f)
				{
					const vec2 d = M.m_Vel / Speed;
					Right = d * ((M.m_Size * 2.0f + Speed * M.m_Stretch) * 0.5f);
					Up = vec2(d.y, -d.x) * (M.m_Size * 0.5f);
				}
			}
		}
		if(!Sees(Center, std::max(length(Right), length(Up))))
			continue;
		const CVertex aV[4] = {
			{Center - Right + Up, Uv.m_U0, Uv.m_V0, Col},
			{Center + Right + Up, Uv.m_U1, Uv.m_V0, Col},
			{Center - Right - Up, Uv.m_U0, Uv.m_V1, Col},
			{Center + Right - Up, Uv.m_U1, Uv.m_V1, Col},
		};
		Quad(aV);
	}
}

// does the layer have anything
inline bool AnyIn(const CParticles &Particles, int Layer)
{
	for(int i = 0; i < Particles.m_Num; i++)
		if(LayerOf(Particles.m_aMotes[i]) == Layer)
			return true;
	return false;
}

// the area the kagune can cover this frame
inline float Reach(const CTee &Tee)
{
	float Max = 0.0f;
	for(int i = 0; i < Tee.m_Count; i++)
		for(int j = 0; j < SIM_POINTS; j++)
			Max = std::max(Max, distance(Tee.m_aTentacles[i].m_aPos[j], Tee.m_Root));
	return Max + 30.0f;
}
}

#endif
