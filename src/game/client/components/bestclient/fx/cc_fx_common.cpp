/* ChainsawClient: shared helpers of the fx (see cc_fx_common.h) */
#include "cc_fx_common.h"

#include <base/log.h>
#include <base/math.h>
#include <base/time.h>

#include <engine/image.h>
#include <engine/shared/config.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
constexpr int ATLAS_W = 1024;
constexpr int ATLAS_H = 512;

// the same falloff as the fgf glow mask (fgf_glow.cpp)
float GlowFalloff(float Distance)
{
	if(Distance >= 1.0f)
		return 0.0f;
	const float Inv = 1.0f - Distance;
	return Inv * Inv * (0.35f + 0.65f * Inv);
}

// the same band as the fgf ring mask (fgf_glow.cpp)
float FgfRingAlpha(float Dx, float Dy)
{
	const float Distance = std::sqrt(Dx * Dx + Dy * Dy);
	if(Distance >= 1.0f)
		return 0.0f;
	const float Band = (Distance - 0.78f) / 0.09f;
	return std::exp(-Band * Band) * std::clamp((1.0f - Distance) / 0.1f, 0.0f, 1.0f);
}

struct CShared
{
	IGraphics *m_pGraphics = nullptr;
	bool m_Built = false;
	CcFx::CAtlas m_Atlas;
};
CShared s_Shared;

struct CStats
{
	int64_t m_aTime[CcFx::NUM_STATS] = {};
	int m_Frames = 0;
	int64_t m_Batches = 0;
	int64_t m_Quads = 0;
	int64_t m_LastPrint = 0;
};
CStats s_Stats;
int s_BatchQuads = 0;

void Blit(std::vector<uint8_t> &vDst, int X, int Y, const uint8_t *pSrc, int Width, int Height)
{
	for(int y = 0; y < Height; y++)
		for(int x = 0; x < Width; x++)
			vDst[(Y + y) * ATLAS_W + X + x] = pSrc[y * Width + x];
}

CcFx::CUv Rect(int X, int Y, int Width, int Height)
{
	return {X / (float)ATLAS_W, Y / (float)ATLAS_H, (X + Width) / (float)ATLAS_W, (Y + Height) / (float)ATLAS_H};
}

void BuildMask(std::vector<uint8_t> &vMask, int Size, float (*pAlpha)(float Dx, float Dy))
{
	vMask.assign(Size * Size, 0);
	for(int y = 0; y < Size; y++)
	{
		for(int x = 0; x < Size; x++)
		{
			const float Dx = (x + 0.5f) / Size * 2.0f - 1.0f;
			const float Dy = (y + 0.5f) / Size * 2.0f - 1.0f;
			vMask[y * Size + x] = (uint8_t)std::clamp(round_to_int(pAlpha(Dx, Dy) * 255.0f), 0, 255);
		}
	}
}
}

const CcFx::CAtlas &CcFx::Atlas(IGraphics *pGraphics)
{
	if(s_Shared.m_Built && s_Shared.m_pGraphics == pGraphics)
		return s_Shared.m_Atlas;
	s_Shared = CShared();
	s_Shared.m_pGraphics = pGraphics;
	CAtlas &Atlas = s_Shared.m_Atlas;

	// layout (all masks have empty borders, so the filtering never mixes neighbours):
	// flames 512x384 at 0,0 | ring line 256 at 512,0 | ring core 256 at 768,0
	// ring halo 256 at 512,256 | fgf ring 128 at 768,256 | glow 64 at 896,256 | petal 32 at 960,256 | white 16 at 1008,256
	std::vector<uint8_t> vAlpha(ATLAS_W * ATLAS_H, 0);
	std::vector<uint8_t> vMask;

	vMask.assign(BcFireShapes::ATLAS_W * BcFireShapes::ATLAS_H, 0);
	BcFireShapes::BuildFlameAtlas(vMask.data());
	Blit(vAlpha, 0, 0, vMask.data(), BcFireShapes::ATLAS_W, BcFireShapes::ATLAS_H);
	for(int Variant = 0; Variant < BcFireShapes::VARIANTS; Variant++)
		for(int Layer = 0; Layer < BcFireShapes::LAYERS; Layer++)
			Atlas.m_aaFlame[Variant][Layer] = Rect(Variant * BcFireShapes::CELL_W, Layer * BcFireShapes::CELL_H, BcFireShapes::CELL_W, BcFireShapes::CELL_H);

	vMask.assign(256 * 256, 0);
	BcFireShapes::BuildRing(vMask.data(), 256, LINE_RADIUS, 0.022f, 1.0f / 128.0f);
	Blit(vAlpha, 512, 0, vMask.data(), 256, 256);
	Atlas.m_RingLine = Rect(512, 0, 256, 256);
	BcFireShapes::BuildRing(vMask.data(), 256, LINE_RADIUS, 0.012f, 1.0f / 128.0f);
	Blit(vAlpha, 768, 0, vMask.data(), 256, 256);
	Atlas.m_RingCore = Rect(768, 0, 256, 256);
	BcFireShapes::BuildRing(vMask.data(), 256, HALO_RADIUS, 0.1f, 0.0f);
	Blit(vAlpha, 512, 256, vMask.data(), 256, 256);
	Atlas.m_RingHalo = Rect(512, 256, 256, 256);

	BuildMask(vMask, 128, FgfRingAlpha);
	Blit(vAlpha, 768, 256, vMask.data(), 128, 128);
	Atlas.m_Ring = Rect(768, 256, 128, 128);
	BuildMask(vMask, 64, [](float Dx, float Dy) { return GlowFalloff(std::sqrt(Dx * Dx + Dy * Dy)); });
	Blit(vAlpha, 896, 256, vMask.data(), 64, 64);
	Atlas.m_Glow = Rect(896, 256, 64, 64);

	vMask.assign(32 * 32, 0);
	BcFireShapes::BuildPetal(vMask.data(), 32);
	Blit(vAlpha, 960, 256, vMask.data(), 32, 32);
	Atlas.m_Petal = Rect(960, 256, 32, 32);

	vMask.assign(16 * 16, 255);
	Blit(vAlpha, 1008, 256, vMask.data(), 16, 16);
	const float WhiteU = (1008 + 8) / (float)ATLAS_W;
	const float WhiteV = (256 + 8) / (float)ATLAS_H;
	Atlas.m_White = {WhiteU, WhiteV, WhiteU, WhiteV};

	CImageInfo Image;
	Image.m_Width = ATLAS_W;
	Image.m_Height = ATLAS_H;
	Image.m_Format = CImageInfo::FORMAT_RGBA;
	Image.Allocate();
	for(int i = 0; i < ATLAS_W * ATLAS_H; i++)
	{
		Image.m_pData[i * 4 + 0] = 255;
		Image.m_pData[i * 4 + 1] = 255;
		Image.m_pData[i * 4 + 2] = 255;
		Image.m_pData[i * 4 + 3] = vAlpha[i];
	}
	Atlas.m_Texture = pGraphics->LoadTextureRawMove(Image, 0, "cc_fx_atlas");
	s_Shared.m_Built = true;
	return Atlas;
}

CcFx::CView CcFx::View(IGraphics *pGraphics, float Margin)
{
	const CScreenRect Screen = pGraphics->GetScreen();
	CView View;
	View.m_X0 = Screen.m_TopLeft.x - Margin;
	View.m_Y0 = Screen.m_TopLeft.y - Margin;
	View.m_X1 = Screen.m_BottomRight.x + Margin;
	View.m_Y1 = Screen.m_BottomRight.y + Margin;
	return View;
}

void CcFx::Begin(IGraphics *pGraphics, IGraphics::CTextureHandle Texture, bool Additive)
{
	if(Additive)
		pGraphics->BlendAdditive();
	else
		pGraphics->BlendNormal();
	pGraphics->TextureSet(Texture);
	pGraphics->QuadsBegin();
	s_BatchQuads = 0;
}

void CcFx::End(IGraphics *pGraphics)
{
	pGraphics->QuadsEnd();
	pGraphics->BlendNormal();
	if(s_BatchQuads > 0)
	{
		s_Stats.m_Batches++;
		s_Stats.m_Quads += s_BatchQuads;
	}
	s_BatchQuads = 0;
}

void CcFx::CountQuads(int Num)
{
	s_BatchQuads += Num;
}

void CcFx::Quad(IGraphics *pGraphics, vec2 Center, vec2 Right, vec2 Up, const CUv &Uv, const ColorRGBA &Color)
{
	pGraphics->QuadsSetSubsetFree(Uv.m_U0, Uv.m_V0, Uv.m_U1, Uv.m_V0, Uv.m_U0, Uv.m_V1, Uv.m_U1, Uv.m_V1);
	pGraphics->SetColor(Color);
	const IGraphics::CFreeformItem Item(Center - Right + Up, Center + Right + Up, Center - Right - Up, Center + Right - Up);
	pGraphics->QuadsDrawFreeform(&Item, 1);
	s_BatchQuads++;
}

void CcFx::QuadRotated(IGraphics *pGraphics, vec2 Center, float Width, float Height, float Rotation, const CUv &Uv, const ColorRGBA &Color)
{
	// the engine turns the corners by Rotation around the center: x' = x cos - y sin, y' = x sin + y cos
	const float c = std::cos(Rotation);
	const float s = std::sin(Rotation);
	Quad(pGraphics, Center, vec2(c, s) * (Width * 0.5f), vec2(s, -c) * (Height * 0.5f), Uv, Color);
}

void CcFx::Shape(IGraphics *pGraphics, vec2 P0, vec2 P1, vec2 P2, vec2 P3, const ColorRGBA &Color)
{
	const CUv &White = s_Shared.m_Atlas.m_White;
	pGraphics->QuadsSetSubsetFree(White.m_U0, White.m_V0, White.m_U0, White.m_V0, White.m_U0, White.m_V0, White.m_U0, White.m_V0);
	pGraphics->SetColor(Color);
	const IGraphics::CFreeformItem Item(P0, P1, P2, P3);
	pGraphics->QuadsDrawFreeform(&Item, 1);
	s_BatchQuads++;
}

void CcFx::CSparks::CSpark::SetDrag(float Drag)
{
	m_LnDrag = Drag >= 1.0f ? 0.0f : (Drag > 0.0f ? std::log(Drag) : -30.0f);
}

CcFx::CSparks::CSpark *CcFx::CSparks::NewSpark()
{
	if(m_NumSparks >= MAX_SPARKS)
		return nullptr;
	CSpark *pSpark = &m_aSparks[m_NumSparks++];
	*pSpark = CSpark();
	return pSpark;
}

void CcFx::CSparks::AddRing(vec2 Pos, float LifeSpan, float StartSize, float EndSize, float Alpha, ColorRGBA Color)
{
	if(m_NumRings >= MAX_RINGS)
		return;
	CRing &Ring = m_aRings[m_NumRings++];
	Ring.m_Pos = Pos;
	Ring.m_Life = 0.0f;
	Ring.m_LifeSpan = LifeSpan;
	Ring.m_StartSize = StartSize;
	Ring.m_EndSize = EndSize;
	Ring.m_Alpha = Alpha;
	Ring.m_Color = Color;
}

void CcFx::CSparks::Update(float Passed)
{
	for(int i = 0; i < m_NumSparks;)
	{
		CSpark &Spark = m_aSparks[i];
		Spark.m_Life += Passed;
		if(Spark.m_Life >= Spark.m_LifeSpan)
		{
			Spark = m_aSparks[--m_NumSparks];
			continue;
		}
		if(Spark.m_Gravity != 0.0f)
			Spark.m_Vel.y += Spark.m_Gravity * Passed;
		if(Spark.m_LnDrag != 0.0f)
			Spark.m_Vel *= std::exp(Spark.m_LnDrag * Passed);
		Spark.m_Pos += Spark.m_Vel * Passed;
		i++;
	}
	for(int i = 0; i < m_NumRings;)
	{
		m_aRings[i].m_Life += Passed;
		if(m_aRings[i].m_Life >= m_aRings[i].m_LifeSpan)
		{
			m_aRings[i] = m_aRings[--m_NumRings];
			continue;
		}
		i++;
	}
}

void CcFx::CSparks::Draw(IGraphics *pGraphics, const CAtlas &Atlas, const CView &View, float AlphaScale) const
{
	for(int i = 0; i < m_NumRings; i++)
	{
		const CRing &Ring = m_aRings[i];
		const float Progress = Ring.m_Life / Ring.m_LifeSpan;
		const float Spread = 1.0f - (1.0f - Progress) * (1.0f - Progress);
		const float Size = mix(Ring.m_StartSize, Ring.m_EndSize, Spread);
		if(!View.Sees(Ring.m_Pos, Size * 0.5f))
			continue;
		QuadAxis(pGraphics, Ring.m_Pos, Size, Size, Atlas.m_Ring, Ring.m_Color.WithAlpha(Ring.m_Alpha * (1.0f - Progress) * AlphaScale));
	}
	for(int i = 0; i < m_NumSparks; i++)
	{
		const CSpark &Spark = m_aSparks[i];
		const float Progress = Spark.m_Life / Spark.m_LifeSpan;
		const float Size = mix(Spark.m_StartSize, Spark.m_EndSize, Progress);
		const float Speed = length(Spark.m_Vel);
		const float Width = Size + Speed * Spark.m_Stretch;
		if(!View.Sees(Spark.m_Pos, Width * 0.5f))
			continue;
		const float Alpha = mix(Spark.m_StartAlpha, Spark.m_EndAlpha, Progress) * std::min(1.0f, Spark.m_Life / 0.05f) * AlphaScale;
		ColorRGBA Color = Spark.m_Color;
		if(Spark.m_ShiftColor)
			Color = ColorRGBA(mix(Spark.m_Color.r, Spark.m_EndColor.r, Progress), mix(Spark.m_Color.g, Spark.m_EndColor.g, Progress), mix(Spark.m_Color.b, Spark.m_EndColor.b, Progress), 1.0f);
		const vec2 Dir = Speed > 0.01f ? Spark.m_Vel / Speed : vec2(1.0f, 0.0f);
		Quad(pGraphics, Spark.m_Pos, Dir * (Width * 0.5f), vec2(Dir.y, -Dir.x) * (Size * 0.5f), Atlas.m_Glow, Color.WithAlpha(Alpha));
	}
}

void CcFx::AddTime(int Stat, int64_t Nanoseconds)
{
	if(Stat >= 0 && Stat < NUM_STATS)
		s_Stats.m_aTime[Stat] += Nanoseconds;
}

void CcFx::StatsFrame()
{
	if(!g_Config.m_BcFxStats)
	{
		if(s_Stats.m_LastPrint != 0)
			s_Stats = CStats();
		return;
	}
	const int64_t Now = time_get_nanoseconds().count();
	if(s_Stats.m_LastPrint == 0)
	{
		s_Stats = CStats();
		s_Stats.m_LastPrint = Now;
		return;
	}
	s_Stats.m_Frames++;
	if(Now - s_Stats.m_LastPrint < 1000000000)
		return;
	const double Frames = std::max(1, s_Stats.m_Frames);
	const auto Ms = [&](int Stat) { return s_Stats.m_aTime[Stat] / Frames / 1000000.0; };
	log_info("ccfx", "per frame: ring %.3f ms, fire %.3f ms, chain %.3f ms, kagune %.3f ms | %.1f draw calls, %.0f quads | %d fps",
		Ms(STAT_FIRE_RING), Ms(STAT_FIRE_FRONT), Ms(STAT_FIRE_CHAIN), Ms(STAT_KAGUNE), s_Stats.m_Batches / Frames, s_Stats.m_Quads / Frames,
		(int)(s_Stats.m_Frames * 1000000000.0 / (Now - s_Stats.m_LastPrint)));
	s_Stats = CStats();
	s_Stats.m_LastPrint = Now;
}

CcFx::CTimer::CTimer(int Stat) :
	m_Stat(Stat), m_Start(g_Config.m_BcFxStats ? time_get_nanoseconds().count() : -1)
{
}

CcFx::CTimer::~CTimer()
{
	if(m_Start >= 0)
		AddTime(m_Stat, time_get_nanoseconds().count() - m_Start);
}
