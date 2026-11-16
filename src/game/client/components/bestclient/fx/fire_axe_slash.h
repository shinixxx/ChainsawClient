/* ChainsawClient: the elegant strikes of the golden axe (bc_fire_axe_anim_slash, 08.11) - engine free, so the offline
   preview (E:\BestClient-backups\axe_preview\slash) draws exactly this.
   Three kinds of strike (bc_fire_axe_anim_attack):
   - diagonal: a thin golden crescent sweeps in front of the tee, the sharp leading tip runs ahead, the long tail
     follows and melts away; a white cutting line on its outer edge, a soft halo. Every swing goes the other way.
   - horizontal: the crescent is a flat ellipse around the tee: from behind it (dim) round the front (bright) to the
     side the tee looks at - a wide horizontal sweep.
   - head bash: the axe head thrusts forward: speed lines along the aim, a small pressure wave in front of the head.
   Where the blade hits a tee: Cut() - two thin needles of light cross (an X) and a twinkle star; a bash: Star() - a
   burst of short rays.
   Everything is flat colored quads (the white point of the atlas), drawn additive: Emit(P0, P1, P2, P3, C0, C1, C2, C3)
   with the corners in the order of IGraphics::CFreeformItem (top left, top right, bottom left, bottom right). */
#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_FIRE_AXE_SLASH_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_FIRE_AXE_SLASH_H

#include <base/color.h>
#include <base/vmath.h>

#include <algorithm>
#include <cmath>

namespace BcAxeSlash
{
enum
{
	KIND_DIAGONAL = 0,
	KIND_HORIZONTAL,
	KIND_BASH,
	NUM_KINDS
};

constexpr float SLASH_LIFE = 0.3f;
constexpr float CUT_LIFE = 0.36f;
constexpr float SPAN = 2.5f; // radians of the diagonal crescent
constexpr int SEGMENTS = 28;
constexpr float PI_F = 3.14159265f;

inline float Clamp01(float x) { return std::clamp(x, 0.0f, 1.0f); }
inline float SmoothStep(float a, float b, float x)
{
	const float t = Clamp01((x - a) / (b - a));
	return t * t * (3.0f - 2.0f * t);
}
inline ColorRGBA Whiten(ColorRGBA c, float w)
{
	return ColorRGBA(c.r + (1.0f - c.r) * w, c.g + (1.0f - c.g) * w, c.b + (1.0f - c.b) * w, c.a);
}

// the path of a crescent: angle Start + Dir * Span * s (s = 0..1) on an ellipse (Squash = its height / width) turned by Rot
struct CArc
{
	vec2 m_Center = vec2(0.0f, 0.0f);
	float m_Rot = 0.0f;
	float m_Start = 0.0f;
	float m_Dir = 1.0f;
	float m_Span = SPAN;
	float m_Radius = 46.0f;
	float m_Squash = 1.0f;
	float m_Open = 0.16f; // the radius grows by this along the path: a spiral
};

// a diagonal strike towards Aim (radians), sweeping one way (Side = +1) or the other
inline CArc DiagonalArc(vec2 Center, float Aim, float Side, float Radius)
{
	CArc Arc;
	Arc.m_Center = Center;
	Arc.m_Rot = Aim;
	Arc.m_Start = -Side * SPAN * 0.5f;
	Arc.m_Dir = Side;
	Arc.m_Radius = Radius * 0.9f;
	return Arc;
}

// a flat sweep round the tee: from behind (the far, upper half of the ellipse) round the front to the side it looks at
inline CArc HorizontalArc(vec2 Center, float Aim, float Radius)
{
	CArc Arc;
	Arc.m_Center = Center;
	const bool Right = std::cos(Aim) >= 0.0f;
	// tilted a little with the aim, never steep
	const float Tilt = std::clamp(std::sin(Aim) * 0.45f, -0.3f, 0.3f) * (Right ? 1.0f : -1.0f);
	Arc.m_Rot = Tilt;
	Arc.m_Span = 3.85f;
	Arc.m_Start = Right ? PI_F + 0.4f : -0.4f;
	Arc.m_Dir = Right ? -1.0f : 1.0f;
	Arc.m_Radius = Radius * 1.3f;
	Arc.m_Squash = 0.34f;
	Arc.m_Open = 0.08f;
	return Arc;
}

inline vec2 ArcPoint(const CArc &Arc, float s)
{
	const float a = Arc.m_Start + Arc.m_Dir * Arc.m_Span * s;
	const float r = Arc.m_Radius * (1.0f + Arc.m_Open * s);
	const vec2 Local(std::cos(a) * r, std::sin(a) * r * Arc.m_Squash);
	const float c = std::cos(Arc.m_Rot), sn = std::sin(Arc.m_Rot);
	return Arc.m_Center + vec2(Local.x * c - Local.y * sn, Local.x * sn + Local.y * c);
}

// in front of the tee (the lower half of a flat ellipse) = 1, behind it = dim
inline float ArcDepth(const CArc &Arc, float s)
{
	if(Arc.m_Squash > 0.9f)
		return 1.0f;
	const float a = Arc.m_Start + Arc.m_Dir * Arc.m_Span * s;
	return 0.3f + 0.7f * SmoothStep(-0.5f, 0.6f, std::sin(a));
}

// where along the crescent (0 = start, 1 = end) the leading tip and the tail are at time t
inline void SlashRange(float t, float &Tail, float &Head)
{
	const float h = Clamp01(t / 0.09f);
	Head = 1.0f - (1.0f - h) * (1.0f - h) * (1.0f - h);
	const float u = Clamp01((t - 0.035f) / 0.235f);
	Tail = u * u * (3.0f - 2.0f * u);
}

// one band along the crescent: a center line with the full alpha that fades to nothing at both sides
template<class FEmit>
void Band(const vec2 *pCenter, const vec2 *pOut, const float *pWidth, const float *pAlpha, int Num, float Offset, float Scale, ColorRGBA Color, FEmit &&Emit)
{
	for(int i = 0; i + 1 < Num; i++)
	{
		const float w0 = pWidth[i] * Scale, w1 = pWidth[i + 1] * Scale;
		if(w0 <= 0.01f && w1 <= 0.01f)
			continue;
		const vec2 C0 = pCenter[i] + pOut[i] * (pWidth[i] * Offset), C1 = pCenter[i + 1] + pOut[i + 1] * (pWidth[i + 1] * Offset);
		const ColorRGBA A0 = Color.WithAlpha(Color.a * pAlpha[i]), A1 = Color.WithAlpha(Color.a * pAlpha[i + 1]);
		const ColorRGBA Z0 = A0.WithAlpha(0.0f), Z1 = A1.WithAlpha(0.0f);
		// outer half, inner half
		Emit(C0 + pOut[i] * w0, C1 + pOut[i + 1] * w1, C0, C1, Z0, Z1, A0, A1);
		Emit(C0, C1, C0 - pOut[i] * w0, C1 - pOut[i + 1] * w1, A0, A1, Z0, Z1);
	}
}

// the crescent along Arc at time t; Thick: its width relative to the radius
template<class FEmit>
void Slash(const CArc &Arc, float t, ColorRGBA Gold, float Power, float Thick, FEmit &&Emit)
{
	if(t < 0.0f || t >= SLASH_LIFE)
		return;
	float Tail, Head;
	SlashRange(t, Tail, Head);
	if(Head - Tail < 0.01f)
		return;
	const float Fade = (1.0f - SmoothStep(0.14f, SLASH_LIFE, t)) * Power;
	vec2 aCenter[SEGMENTS + 1], aOut[SEGMENTS + 1];
	float aWidth[SEGMENTS + 1], aAlpha[SEGMENTS + 1];
	for(int i = 0; i <= SEGMENTS; i++)
	{
		const float x = i / (float)SEGMENTS; // 0 = the tail end, 1 = the leading tip
		const float s = Tail + (Head - Tail) * x;
		aCenter[i] = ArcPoint(Arc, s);
		// a long thin tail, the thickest a bit behind a sharp leading tip (max: sin(pi) is a hair below 0)
		const float Shape = std::pow(std::max(0.0f, std::sin(PI_F * std::pow(x, 1.6f))), 0.85f);
		aWidth[i] = Arc.m_Radius * Thick * Shape * (0.55f + 0.45f * Fade);
		aAlpha[i] = Fade * (0.35f + 0.65f * x) * ArcDepth(Arc, s);
	}
	// outwards = away from the center, square to the path
	for(int i = 0; i <= SEGMENTS; i++)
	{
		const vec2 Along = aCenter[std::min(i + 1, SEGMENTS)] - aCenter[std::max(i - 1, 0)];
		vec2 n = length(Along) > 1e-4f ? normalize(vec2(-Along.y, Along.x)) : normalize(aCenter[i] - Arc.m_Center + vec2(0.0f, 1e-4f));
		if(dot(n, aCenter[i] - Arc.m_Center) < 0.0f)
			n = -n;
		aOut[i] = n;
	}
	Band(aCenter, aOut, aWidth, aAlpha, SEGMENTS + 1, 0.0f, 2.6f, Gold.WithAlpha(0.3f), Emit); // halo
	Band(aCenter, aOut, aWidth, aAlpha, SEGMENTS + 1, 0.1f, 1.0f, Whiten(Gold, 0.08f).WithAlpha(0.75f), Emit); // golden crescent
	Band(aCenter, aOut, aWidth, aAlpha, SEGMENTS + 1, 0.45f, 0.2f, ColorRGBA(1.0f, 0.97f, 0.88f, 0.9f), Emit); // the cutting edge
}

// a thin needle of light from A to B, Width at its middle
template<class FEmit>
void Needle(vec2 A, vec2 B, float Width, ColorRGBA Color, FEmit &&Emit)
{
	const vec2 d = B - A;
	const float Len = length(d);
	if(Len < 0.01f || Width <= 0.01f || Color.a <= 0.003f)
		return;
	const vec2 n = vec2(-d.y, d.x) / Len * Width;
	const vec2 Mid = (A + B) * 0.5f;
	const ColorRGBA Edge = Color.WithAlpha(0.0f);
	// two halves: bright along the axis, clear at the sides
	Emit(A, B, Mid + n, Mid + n, Color, Color, Edge, Edge);
	Emit(A, B, Mid - n, Mid - n, Color, Color, Edge, Edge);
}

// the head bash: Center = the tee, Aim = the angle of the thrust, Reach = how far the head goes (world units)
constexpr float THRUST_LIFE = 0.3f;
template<class FEmit>
void Thrust(vec2 Center, float Aim, float Reach, float t, ColorRGBA Gold, FEmit &&Emit)
{
	if(t < 0.0f || t >= THRUST_LIFE)
		return;
	const vec2 D(std::cos(Aim), std::sin(Aim));
	const vec2 N(-D.y, D.x);
	// speed lines while the head flies forward (0.08 .. 0.2 s)
	const float Fly = SmoothStep(0.07f, 0.14f, t) * (1.0f - SmoothStep(0.15f, 0.24f, t));
	if(Fly > 0.01f)
	{
		const float Go = SmoothStep(0.07f, 0.16f, t);
		const vec2 Tip = Center + D * Reach * (0.55f + 0.5f * Go);
		const float aOff[3] = {-0.3f, 0.0f, 0.32f};
		const float aLen[3] = {0.7f, 1.1f, 0.6f};
		for(int k = 0; k < 3; k++)
		{
			const vec2 Front = Tip + N * (aOff[k] * Reach * 0.45f) - D * (std::fabs(aOff[k]) * Reach * 0.3f);
			const vec2 Back = Front - D * (Reach * aLen[k] * (0.4f + 0.6f * Fly));
			Needle(Back, Front, Reach * 0.05f, Gold.WithAlpha(0.6f * Fly), Emit);
			Needle(Back + D * (Reach * 0.1f), Front, Reach * 0.012f, ColorRGBA(1.0f, 0.97f, 0.88f, 0.8f * Fly), Emit);
		}
	}
	// the pressure wave: a small crescent opening in front of the head
	const float tw = t - 0.12f;
	if(tw > 0.0f && tw < 0.16f)
	{
		CArc Wave;
		Wave.m_Center = Center + D * (Reach * (0.75f + 0.9f * SmoothStep(0.0f, 0.16f, tw)));
		Wave.m_Rot = Aim;
		Wave.m_Start = -0.95f;
		Wave.m_Dir = 1.0f;
		Wave.m_Span = 1.9f;
		Wave.m_Radius = Reach * (0.3f + 0.38f * SmoothStep(0.0f, 0.16f, tw));
		Wave.m_Squash = 1.0f;
		Wave.m_Open = 0.0f;
		const float Fade = 1.0f - SmoothStep(0.03f, 0.16f, tw);
		// the whole wave at once (no sweep): the tail of the slash range is not used
		vec2 aCenter[13], aOut[13];
		float aWidth[13], aAlpha[13];
		for(int i = 0; i <= 12; i++)
		{
			const float s = i / 12.0f;
			aCenter[i] = ArcPoint(Wave, s);
			aOut[i] = normalize(aCenter[i] - Wave.m_Center);
			aWidth[i] = Reach * 0.09f * std::sin(PI_F * s);
			aAlpha[i] = Fade;
		}
		Band(aCenter, aOut, aWidth, aAlpha, 13, 0.0f, 2.2f, Gold.WithAlpha(0.35f), Emit);
		Band(aCenter, aOut, aWidth, aAlpha, 13, 0.2f, 0.35f, ColorRGBA(1.0f, 0.97f, 0.88f, 0.85f), Emit);
	}
}

// Pos: where the blade went in; Angle: the way the strike went; Size: in world units
template<class FEmit>
void Cut(vec2 Pos, float Angle, float Size, float t, ColorRGBA Gold, FEmit &&Emit)
{
	if(t < 0.0f || t >= CUT_LIFE)
		return;
	const float Grow = 1.0f - std::pow(1.0f - Clamp01(t / 0.07f), 3.0f);
	const float Fade = 1.0f - SmoothStep(0.1f, CUT_LIFE, t);
	const float Len = Size * Grow * (1.0f + 0.35f * t / CUT_LIFE);
	const float Thin = 1.0f - 0.7f * SmoothStep(0.05f, CUT_LIFE, t);
	for(int k = 0; k < 2; k++)
	{
		// the X: one long stroke along the strike, one a little shorter across it
		const float a = Angle + (k == 0 ? 0.55f : -0.75f);
		const float L = Len * (k == 0 ? 1.0f : 0.78f);
		const vec2 D(std::cos(a), std::sin(a));
		Needle(Pos - D * L, Pos + D * L, Size * 0.11f * Thin, Gold.WithAlpha(0.6f * Fade), Emit);
		Needle(Pos - D * L * 0.95f, Pos + D * L * 0.95f, Size * 0.028f * Thin, ColorRGBA(1.0f, 0.97f, 0.88f, 0.9f * Fade), Emit);
	}
	// the twinkle: a small four point star a moment later
	const float ts = t - 0.06f;
	if(ts > 0.0f)
	{
		const float Tw = std::sin(PI_F * Clamp01(ts / 0.24f));
		for(int k = 0; k < 2; k++)
		{
			const float a = Angle + PI_F * 0.25f + k * PI_F * 0.5f;
			const vec2 D(std::cos(a), std::sin(a));
			const float L = Size * 0.55f * Tw;
			Needle(Pos - D * L, Pos + D * L, Size * 0.03f, ColorRGBA(1.0f, 0.98f, 0.9f, 0.9f * Tw), Emit);
		}
	}
}

// the head bash hits a tee: a burst of short rays round the point, long ones along the thrust
template<class FEmit>
void Star(vec2 Pos, float Angle, float Size, float t, ColorRGBA Gold, FEmit &&Emit)
{
	if(t < 0.0f || t >= CUT_LIFE)
		return;
	const float Grow = 1.0f - std::pow(1.0f - Clamp01(t / 0.06f), 3.0f);
	const float Fade = 1.0f - SmoothStep(0.08f, CUT_LIFE, t);
	for(int k = 0; k < 8; k++)
	{
		const float a = Angle + k * PI_F * 0.25f;
		const vec2 D(std::cos(a), std::sin(a));
		const float L = Size * (k % 2 == 0 ? 1.0f : 0.55f) * (k == 0 ? 1.3f : (k == 4 ? 0.5f : 1.0f)) * Grow * (1.0f + 0.4f * t / CUT_LIFE);
		const vec2 From = Pos + D * (Size * 0.18f * Grow);
		Needle(From, Pos + D * L, Size * 0.07f, Gold.WithAlpha(0.6f * Fade), Emit);
		Needle(From, Pos + D * L * 0.9f, Size * 0.022f, ColorRGBA(1.0f, 0.97f, 0.88f, 0.9f * Fade), Emit);
	}
}
}

#endif
