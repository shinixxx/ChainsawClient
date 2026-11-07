/* ChainsawClient: shared helpers of the fx (fire ring, fire chain, kagune).
   Every QuadsBegin/QuadsEnd is one draw call with its own vertex upload, which is what costs frames, so:
   one atlas texture for all fire masks, few batches per effect, quads with precomputed corners (no rotation work
   in the engine), particles off screen are skipped, sparks are drawn into the caller's batch. */
#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_CC_FX_COMMON_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_CC_FX_COMMON_H

#include <base/color.h>
#include <base/vmath.h>

#include <engine/graphics.h>

#include "fire_ice_shapes.h"

#include <cstdint>

namespace CcFx
{
struct CUv
{
	float m_U0 = 0.0f;
	float m_V0 = 0.0f;
	float m_U1 = 1.0f;
	float m_V1 = 1.0f;
	CUv Flipped() const { return {m_U1, m_V0, m_U0, m_V1}; }
};

constexpr float LINE_RADIUS = 0.9f; // radius of the band inside the ring line mask
constexpr float HALO_RADIUS = 0.8f; // radius of the band inside the ring halo mask
constexpr float RING_BAND = 0.78f; // radius of the band inside the fgf ring mask

// every white mask of the fire effects in one texture (white, the mask in alpha)
struct CAtlas
{
	IGraphics::CTextureHandle m_Texture;
	CUv m_aaFlame[BcFireShapes::VARIANTS][BcFireShapes::LAYERS]; // tongue cells: columns = variants, rows = layers
	CUv m_RingLine; // thin sharp circle
	CUv m_RingCore; // even thinner core of the line
	CUv m_RingHalo; // soft wide circle
	CUv m_Ring; // soft band like the fgf ring (shockwaves, spark rings)
	CUv m_Glow; // soft round light like the fgf glow
	CUv m_Petal;
	CUv m_White; // one white point for flat colored shapes
};
// built once, shared by every effect
const CAtlas &Atlas(IGraphics *pGraphics);

// the visible world with a margin
struct CView
{
	float m_X0 = 0.0f;
	float m_Y0 = 0.0f;
	float m_X1 = 0.0f;
	float m_Y1 = 0.0f;
	bool Sees(vec2 Pos, float Radius) const
	{
		return Pos.x + Radius >= m_X0 && Pos.x - Radius <= m_X1 && Pos.y + Radius >= m_Y0 && Pos.y - Radius <= m_Y1;
	}
};
CView View(IGraphics *pGraphics, float Margin);

// one batch = one texture, one blend mode, one draw call
void Begin(IGraphics *pGraphics, IGraphics::CTextureHandle Texture, bool Additive);
void End(IGraphics *pGraphics);
// for quads drawn by hand inside a batch (bc_fx_stats)
void CountQuads(int Num);

// a quad by its center and half axes: Right points to u1, Up points to v0 (the top of the image)
void Quad(IGraphics *pGraphics, vec2 Center, vec2 Right, vec2 Up, const CUv &Uv, const ColorRGBA &Color);
// not rotated
inline void QuadAxis(IGraphics *pGraphics, vec2 Center, float Width, float Height, const CUv &Uv, const ColorRGBA &Color)
{
	Quad(pGraphics, Center, vec2(Width * 0.5f, 0.0f), vec2(0.0f, -Height * 0.5f), Uv, Color);
}
// the same as QuadsSetRotation(Rotation) + QuadsDraw(CQuadItem(Center, Width, Height))
void QuadRotated(IGraphics *pGraphics, vec2 Center, float Width, float Height, float Rotation, const CUv &Uv, const ColorRGBA &Color);
// a flat colored freeform quad (the engine draws the triangles P0 P1 P3 and P0 P3 P2); needs the atlas texture
void Shape(IGraphics *pGraphics, vec2 P0, vec2 P1, vec2 P2, vec2 P3, const ColorRGBA &Color);
// the corners of a fire tongue cell standing on Base and pointing along Dir: the quad the old DrawTongue drew
// with QuadsSetRotation(angle(Dir) + pi / 2) and the size Size x 2 Size
inline void TongueQuad(vec2 Base, vec2 Dir, float Size, vec2 &Center, vec2 &Right, vec2 &Up)
{
	const float Height = Size * 2.0f;
	Center = Base + Dir * (Height * (0.5f - BcFireShapes::BASE_FROM_BOTTOM));
	Right = vec2(-Dir.y, Dir.x) * (Size * 0.5f);
	Up = Dir * (Height * 0.5f);
}

// sparks and rings like CGlowParticles, but drawn into the caller's batch with the atlas and skipped off screen
class CSparks
{
public:
	struct CSpark
	{
		vec2 m_Pos = vec2(0.0f, 0.0f);
		vec2 m_Vel = vec2(0.0f, 0.0f);
		float m_Gravity = 0.0f;
		float m_LnDrag = 0.0f; // log of the speed kept per second
		float m_Stretch = 0.0f;
		float m_Life = 0.0f;
		float m_LifeSpan = 1.0f;
		float m_StartSize = 1.0f;
		float m_EndSize = 1.0f;
		float m_StartAlpha = 1.0f;
		float m_EndAlpha = 0.0f;
		ColorRGBA m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		ColorRGBA m_EndColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		bool m_ShiftColor = false;
		// the part of the speed kept after one second, like CGlowParticles::m_Drag
		void SetDrag(float Drag);
	};

	void Clear()
	{
		m_NumSparks = 0;
		m_NumRings = 0;
	}
	bool HasAny() const { return m_NumSparks > 0 || m_NumRings > 0; }
	int Count() const { return m_NumSparks; }
	CSpark *NewSpark(); // nullptr when full
	void AddRing(vec2 Pos, float LifeSpan, float StartSize, float EndSize, float Alpha, ColorRGBA Color);
	void Update(float Passed);
	// rings first, then the sparks; between Begin/End with the atlas texture
	void Draw(IGraphics *pGraphics, const CAtlas &Atlas, const CView &View, float AlphaScale = 1.0f) const;

private:
	static constexpr int MAX_SPARKS = 1024;
	static constexpr int MAX_RINGS = 32;

	struct CRing
	{
		vec2 m_Pos;
		float m_Life;
		float m_LifeSpan;
		float m_StartSize;
		float m_EndSize;
		float m_Alpha;
		ColorRGBA m_Color;
	};

	CSpark m_aSparks[MAX_SPARKS];
	int m_NumSparks = 0;
	CRing m_aRings[MAX_RINGS];
	int m_NumRings = 0;
};

// bc_fx_stats: time spent in each effect, printed to the console once per second
enum
{
	STAT_FIRE_RING = 0,
	STAT_FIRE_FRONT,
	STAT_FIRE_CHAIN,
	STAT_KAGUNE,
	NUM_STATS
};
void AddTime(int Stat, int64_t Nanoseconds);
// once per frame, from one component
void StatsFrame();

class CTimer
{
public:
	explicit CTimer(int Stat);
	~CTimer();

private:
	int m_Stat;
	int64_t m_Start;
};
}

#endif
