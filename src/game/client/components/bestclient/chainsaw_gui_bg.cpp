/* ChainsawClient: painted backgrounds for the in-game menu. Everything is drawn here in code, there are no image files. */
#include "chainsaw_gui_bg.h"

#include <algorithm>
#include <cmath>

namespace
{
struct C3
{
	float r, g, b;
};
C3 operator+(C3 a, C3 b) { return {a.r + b.r, a.g + b.g, a.b + b.b}; }
C3 operator*(C3 a, float s) { return {a.r * s, a.g * s, a.b * s}; }
C3 Mix(C3 a, C3 b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
C3 Hex(unsigned v) { return {((v >> 16) & 255) / 255.0f, ((v >> 8) & 255) / 255.0f, (v & 255) / 255.0f}; }

constexpr float PI = 3.14159265f;

float Clamp01(float x) { return std::clamp(x, 0.0f, 1.0f); }
float Smooth(float a, float b, float x)
{
	const float t = Clamp01((x - a) / (b - a));
	return t * t * (3.0f - 2.0f * t);
}

unsigned HashU(unsigned x)
{
	x ^= x >> 16;
	x *= 0x7feb352dU;
	x ^= x >> 15;
	x *= 0x846ca68bU;
	x ^= x >> 16;
	return x;
}
float Hash1(int x, int Seed) { return (HashU((unsigned)x * 1664525U + (unsigned)Seed * 1013904223U) & 0xffffff) / (float)0xffffff; }
float Hash2(int x, int y, int Seed) { return Hash1(x * 73856093 ^ y * 19349663, Seed); }

// deterministic random numbers for placing things
struct CRng
{
	unsigned m_State;
	explicit CRng(unsigned Seed) :
		m_State(Seed * 2654435761U + 1U) {}
	float Next()
	{
		m_State = HashU(m_State + 0x9e3779b9U);
		return (m_State & 0xffffff) / (float)0xffffff;
	}
	float Range(float a, float b) { return a + (b - a) * Next(); }
};

float Noise1(float x, int Seed)
{
	const int i = (int)std::floor(x);
	const float f = x - i;
	const float u = f * f * (3.0f - 2.0f * f);
	return Hash1(i, Seed) * (1.0f - u) + Hash1(i + 1, Seed) * u;
}
float Fbm1(float x, int Seed, int Octaves)
{
	float Sum = 0.0f, Amp = 0.5f, Norm = 0.0f;
	for(int o = 0; o < Octaves; o++)
	{
		Sum += Noise1(x, Seed + o * 31) * Amp;
		Norm += Amp;
		x *= 2.03f;
		Amp *= 0.5f;
	}
	return Sum / Norm;
}
float Noise2(float x, float y, int Seed)
{
	const int ix = (int)std::floor(x), iy = (int)std::floor(y);
	const float fx = x - ix, fy = y - iy;
	const float ux = fx * fx * (3.0f - 2.0f * fx), uy = fy * fy * (3.0f - 2.0f * fy);
	const float a = Hash2(ix, iy, Seed), b = Hash2(ix + 1, iy, Seed), c = Hash2(ix, iy + 1, Seed), d = Hash2(ix + 1, iy + 1, Seed);
	return (a * (1 - ux) + b * ux) * (1 - uy) + (c * (1 - ux) + d * ux) * uy;
}
float Fbm2(float x, float y, int Seed, int Octaves)
{
	float Sum = 0.0f, Amp = 0.5f, Norm = 0.0f;
	for(int o = 0; o < Octaves; o++)
	{
		Sum += Noise2(x, y, Seed + o * 17) * Amp;
		Norm += Amp;
		x = x * 2.01f + 3.1f;
		y = y * 2.01f + 1.7f;
		Amp *= 0.5f;
	}
	return Sum / Norm;
}

struct CCanvas
{
	int m_W, m_H;
	float m_S; // scale: 1 at 540 pixels height
	std::vector<C3> m_vPix;
	CCanvas(int W, int H) :
		m_W(W), m_H(H), m_S(H / 540.0f), m_vPix((size_t)W * H, C3{0, 0, 0}) {}

	C3 &At(int x, int y) { return m_vPix[(size_t)y * m_W + x]; }
	void Blend(int x, int y, C3 c, float a)
	{
		if(x < 0 || y < 0 || x >= m_W || y >= m_H || a <= 0.0f)
			return;
		C3 &p = At(x, y);
		p = Mix(p, c, std::min(a, 1.0f));
	}
	void Add(int x, int y, C3 c, float a)
	{
		if(x < 0 || y < 0 || x >= m_W || y >= m_H || a <= 0.0f)
			return;
		C3 &p = At(x, y);
		p = p + c * a;
	}
	void Sky(const float *pStops, const unsigned *pColors, int Num)
	{
		for(int y = 0; y < m_H; y++)
		{
			const float t = y / (float)(m_H - 1);
			C3 c = Hex(pColors[0]);
			for(int i = 0; i + 1 < Num; i++)
				if(t >= pStops[i])
					c = Mix(Hex(pColors[i]), Hex(pColors[i + 1]), Smooth(pStops[i], pStops[i + 1], t));
			for(int x = 0; x < m_W; x++)
				At(x, y) = c;
		}
	}
	// soft additive light: falls off with the square of the distance
	void Glow(float cx, float cy, float r, C3 c, float Intensity)
	{
		const int x0 = std::max(0, (int)(cx - r)), x1 = std::min(m_W - 1, (int)(cx + r));
		const int y0 = std::max(0, (int)(cy - r)), y1 = std::min(m_H - 1, (int)(cy + r));
		for(int y = y0; y <= y1; y++)
			for(int x = x0; x <= x1; x++)
			{
				const float d = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy)) / r;
				if(d < 1.0f)
				{
					const float f = (1.0f - d) * (1.0f - d);
					Add(x, y, c, f * f * Intensity);
				}
			}
	}
	void Disc(float cx, float cy, float r, C3 c, float a)
	{
		const int x0 = std::max(0, (int)(cx - r - 1)), x1 = std::min(m_W - 1, (int)(cx + r + 1));
		const int y0 = std::max(0, (int)(cy - r - 1)), y1 = std::min(m_H - 1, (int)(cy + r + 1));
		for(int y = y0; y <= y1; y++)
			for(int x = x0; x <= x1; x++)
			{
				const float d = std::sqrt((x + 0.5f - cx) * (x + 0.5f - cx) + (y + 0.5f - cy) * (y + 0.5f - cy));
				Blend(x, y, c, a * Clamp01(r - d + 0.5f));
			}
	}
	// thick line with round ends
	void Line(float x0, float y0, float x1, float y1, float w0, float w1, C3 c, float a)
	{
		const float Len = std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
		const int Steps = std::max(1, (int)(Len / 0.8f));
		for(int i = 0; i <= Steps; i++)
		{
			const float t = i / (float)Steps;
			Disc(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, (w0 + (w1 - w0) * t) * 0.5f, c, a);
		}
	}
	void Fill(int x, int y0, int y1, C3 c, float a)
	{
		for(int y = std::max(0, y0); y < std::min(m_H, y1); y++)
			Blend(x, y, c, a);
	}
	// fog that fades from the line downwards (or upwards for negative heights)
	void Haze(const std::vector<float> &vTop, float Height, C3 c, float a)
	{
		for(int x = 0; x < m_W; x++)
		{
			const int y0 = (int)vTop[x];
			for(int dy = 0; dy < (int)std::fabs(Height); dy++)
			{
				const int y = Height > 0 ? y0 + dy : y0 - dy;
				Blend(x, y, c, a * (1.0f - dy / std::fabs(Height)));
			}
		}
	}
	void Stars(CRng &Rng, int Num, float MaxY, float Bright)
	{
		for(int i = 0; i < Num; i++)
		{
			const float x = Rng.Range(0, (float)m_W), y = Rng.Range(0, MaxY);
			const float b = std::pow(Rng.Next(), 3.0f) * Bright;
			const C3 c = Mix(Hex(0xcfe0ff), Hex(0xfff1d6), Rng.Next());
			Add((int)x, (int)y, c, b);
			if(b > 0.45f)
				Glow(x, y, 3.5f * m_S + b * 4.0f * m_S, c, b * 0.35f);
		}
	}
	void Vignette(float Amount)
	{
		for(int y = 0; y < m_H; y++)
			for(int x = 0; x < m_W; x++)
			{
				const float dx = (x / (float)m_W - 0.5f) * 1.6f, dy = (y / (float)m_H - 0.5f) * 1.6f;
				const float d = Clamp01(dx * dx + dy * dy);
				At(x, y) = At(x, y) * (1.0f - Amount * d);
			}
	}
};

// a mountain ridge line: sharp peaks from ridged noise
std::vector<float> Ridge(CCanvas &C, float Base, float Amp, float Freq, int Seed)
{
	std::vector<float> v(C.m_W);
	for(int x = 0; x < C.m_W; x++)
	{
		const float u = x / (float)C.m_W * Freq;
		float n = 0.0f, a = 0.6f, f = 1.0f;
		for(int o = 0; o < 5; o++)
		{
			const float r = 1.0f - std::fabs(Noise1(u * f, Seed + o * 7) * 2.0f - 1.0f);
			n += r * r * a;
			f *= 2.1f;
			a *= 0.45f;
		}
		v[x] = Base - n * Amp;
	}
	return v;
}

void FillBelow(CCanvas &C, const std::vector<float> &vTop, C3 Top, C3 Bottom)
{
	for(int x = 0; x < C.m_W; x++)
		for(int y = std::max(0, (int)vTop[x]); y < C.m_H; y++)
		{
			const float t = Clamp01((y - vTop[x]) / (C.m_H * 0.35f));
			// the top pixel is covered only partly: anti aliased edge
			C.Blend(x, y, Mix(Top, Bottom, t), y == (int)vTop[x] ? 1.0f - (vTop[x] - (int)vTop[x]) : 1.0f);
		}
}

void PaintSunset(CCanvas &C)
{
	const float aStops[] = {0.0f, 0.3f, 0.5f, 0.64f, 0.74f};
	const unsigned aColors[] = {0x170c33, 0x40195a, 0x9e3360, 0xf06a45, 0xffbd73};
	C.Sky(aStops, aColors, 5);
	const float W = (float)C.m_W, H = (float)C.m_H;
	// thin glowing clouds
	for(int y = (int)(H * 0.22f); y < (int)(H * 0.6f); y++)
		for(int x = 0; x < C.m_W; x++)
		{
			const float n = Fbm2(x / W * 2.2f, y / H * 16.0f, 11, 5);
			const float a = Smooth(0.56f, 0.72f, n) * 0.45f;
			const float Near = Clamp01(1.0f - std::fabs(x / W - 0.66f) * 1.4f);
			C.Blend(x, y, Mix(Hex(0x8c3a6a), Hex(0xffb38a), Near * Smooth(0.25f, 0.6f, y / H)), a);
		}
	C.Glow(W * 0.66f, H * 0.55f, H * 0.75f, Hex(0xff7a3c), 0.5f);
	C.Glow(W * 0.66f, H * 0.55f, H * 0.22f, Hex(0xffd59a), 0.6f);
	C.Disc(W * 0.66f, H * 0.55f, H * 0.07f, Hex(0xfff3d2), 1.0f);
	const unsigned aFar[] = {0x9a4a72, 0x6e2d5e, 0x4a1d4a, 0x2a1030, 0x0f0716};
	for(int i = 0; i < 5; i++)
	{
		const std::vector<float> vTop = Ridge(C, H * (0.66f + i * 0.075f), H * (0.12f + i * 0.04f), 2.0f + i * 0.9f, 40 + i * 13);
		FillBelow(C, vTop, Hex(aFar[i]), Mix(Hex(aFar[i]), Hex(0x050208), 0.6f));
		if(i < 4)
			C.Haze(vTop, H * 0.06f, Mix(Hex(0xff9a6a), Hex(aFar[i]), 0.4f + i * 0.12f), 0.35f - i * 0.05f);
	}
}

void Spruce(CCanvas &C, float x, float Base, float h, float w, C3 c)
{
	const float Top = Base - h;
	for(int y = std::max(0, (int)Top); y < std::min(C.m_H, (int)Base + 1); y++)
	{
		const float t = (y - Top) / h;
		const float Tier = std::fmod(t * 5.0f, 1.0f);
		const float hw = w * (0.12f + 0.88f * t) * (0.5f + 0.5f * Tier) * 0.5f;
		for(int xx = (int)(x - hw - 1); xx <= (int)(x + hw + 1); xx++)
			C.Blend(xx, y, c, Clamp01(hw - std::fabs(xx + 0.5f - x) + 0.5f));
	}
	C.Line(x, Base - h * 0.1f, x, Base + 4.0f * C.m_S, 2.5f * C.m_S, 3.0f * C.m_S, c, 1.0f);
}

void PaintForest(CCanvas &C)
{
	const float aStops[] = {0.0f, 0.5f, 0.8f, 1.0f};
	const unsigned aColors[] = {0x02050e, 0x0a1834, 0x17345a, 0x24486a};
	C.Sky(aStops, aColors, 4);
	const float W = (float)C.m_W, H = (float)C.m_H;
	CRng Rng(5);
	C.Stars(Rng, (int)(700 * C.m_S * C.m_S), H * 0.7f, 1.0f);
	C.Glow(W * 0.24f, H * 0.23f, H * 0.5f, Hex(0x6f8fc8), 0.35f);
	C.Disc(W * 0.24f, H * 0.23f, H * 0.065f, Hex(0xf2efe2), 1.0f);
	for(int i = 0; i < 7; i++)
		C.Disc(W * 0.24f + Rng.Range(-0.04f, 0.04f) * H, H * 0.23f + Rng.Range(-0.04f, 0.04f) * H, Rng.Range(0.006f, 0.014f) * H, Hex(0xc9c6b8), 0.35f);
	const unsigned aLayer[] = {0x1d3656, 0x13263f, 0x0a1628, 0x03070d};
	for(int L = 0; L < 4; L++)
	{
		const float Base = H * (0.72f + L * 0.08f);
		std::vector<float> vGround(C.m_W);
		for(int x = 0; x < C.m_W; x++)
			vGround[x] = Base + (Fbm1(x / W * 3.0f, 70 + L, 3) - 0.5f) * H * 0.05f;
		FillBelow(C, vGround, Hex(aLayer[L]), Hex(aLayer[L]) * 0.7f);
		const float Step = (14.0f + L * 9.0f) * C.m_S;
		for(float x = -Step; x < W + Step; x += Step * Rng.Range(0.55f, 1.1f))
		{
			const int xi = std::clamp((int)x, 0, C.m_W - 1);
			const float h = (0.12f + L * 0.06f) * H * Rng.Range(0.6f, 1.25f);
			Spruce(C, x, vGround[xi] + 2.0f, h, h * Rng.Range(0.32f, 0.42f), Hex(aLayer[L]));
		}
		if(L < 3)
			C.Haze(vGround, -H * 0.08f, Hex(0x3c5f86), 0.22f);
	}
	// a few warm fireflies in front
	for(int i = 0; i < 26; i++)
	{
		const float x = Rng.Range(0, W), y = Rng.Range(H * 0.62f, H * 0.98f);
		C.Glow(x, y, Rng.Range(6, 14) * C.m_S, Hex(0xffd36a), Rng.Range(0.4f, 0.9f));
	}
}

void PaintSakura(CCanvas &C)
{
	const float aStops[] = {0.0f, 0.42f, 0.74f, 1.0f};
	const unsigned aColors[] = {0x120a24, 0x37194c, 0x93437a, 0xe995b3};
	C.Sky(aStops, aColors, 4);
	const float W = (float)C.m_W, H = (float)C.m_H;
	CRng Rng(9);
	C.Stars(Rng, (int)(260 * C.m_S * C.m_S), H * 0.45f, 0.7f);
	C.Glow(W * 0.72f, H * 0.34f, H * 0.6f, Hex(0xff9ac0), 0.35f);
	C.Disc(W * 0.72f, H * 0.34f, H * 0.11f, Hex(0xfff0f4), 1.0f);
	for(int i = 0; i < 2; i++)
	{
		const std::vector<float> vTop = Ridge(C, H * (0.8f + i * 0.08f), H * (0.07f + i * 0.04f), 1.6f + i, 90 + i);
		FillBelow(C, vTop, Hex(i ? 0x2a1236 : 0x5a2a5e), Hex(0x120814));
		C.Haze(vTop, H * 0.05f, Hex(0xe28aac), 0.25f);
	}
	// the branch from the top left corner with smaller branches, blossoms along all of them
	const C3 Wood = Hex(0x1a0b14);
	struct CSeg
	{
		float x0, y0, x1, y1, w0, w1;
	};
	std::vector<CSeg> vSegs;
	auto Grow = [&](auto &&Self, float x, float y, float Angle, float Len, float Width, int Depth) -> void {
		const int Steps = 10;
		for(int s = 0; s < Steps; s++)
		{
			Angle += Rng.Range(-0.18f, 0.18f);
			const float nx = x + std::cos(Angle) * Len / Steps, ny = y + std::sin(Angle) * Len / Steps;
			const float w1 = Width * (1.0f - (s + 1) / (float)Steps * 0.6f);
			vSegs.push_back({x, y, nx, ny, Width * (1.0f - s / (float)Steps * 0.6f), w1});
			if(Depth < 2 && Rng.Next() < 0.28f)
				Self(Self, nx, ny, Angle + Rng.Range(0.4f, 0.9f) * (Rng.Next() < 0.6f ? 1.0f : -1.0f), Len * Rng.Range(0.3f, 0.45f), w1 * 0.6f, Depth + 1);
			x = nx;
			y = ny;
		}
	};
	Grow(Grow, -0.04f * W, 0.08f * H, 0.22f, W * 0.62f, 16.0f * C.m_S, 0);
	for(const CSeg &S : vSegs)
		C.Line(S.x0, S.y0, S.x1, S.y1, S.w0, S.w1, Wood, 1.0f);
	const unsigned aPetal[] = {0xffc6d9, 0xff9fc2, 0xffe4ee, 0xf78ab0};
	for(const CSeg &S : vSegs)
	{
		if(Rng.Next() < 0.35f)
			continue;
		const int Num = (int)Rng.Range(2, 6);
		for(int i = 0; i < Num; i++)
		{
			const float x = S.x1 + Rng.Range(-10, 10) * C.m_S, y = S.y1 + Rng.Range(-9, 9) * C.m_S;
			const float r = Rng.Range(3.0f, 6.5f) * C.m_S;
			C.Glow(x, y, r * 3.0f, Hex(0xff8fb8), 0.12f);
			C.Disc(x, y, r, Hex(aPetal[(int)Rng.Range(0, 3.99f)]), 0.95f);
			C.Disc(x, y, r * 0.3f, Hex(0xc24a7a), 0.8f);
		}
	}
	// falling petals
	for(int i = 0; i < 140; i++)
	{
		const float x = Rng.Range(0, W), y = Rng.Range(0, H);
		const float r = Rng.Range(1.5f, 3.5f) * C.m_S, a = Rng.Range(0, PI);
		C.Line(x - std::cos(a) * r, y - std::sin(a) * r, x + std::cos(a) * r, y + std::sin(a) * r, r, r * 0.6f, Hex(aPetal[i % 4]), Rng.Range(0.4f, 0.85f));
	}
}

void PaintStars(CCanvas &C)
{
	const float aStops[] = {0.0f, 0.6f, 1.0f};
	const unsigned aColors[] = {0x020309, 0x0a0f25, 0x1c1734};
	C.Sky(aStops, aColors, 3);
	const float W = (float)C.m_W, H = (float)C.m_H;
	// the milky way: a band from the lower left to the upper right with dust lanes
	const float ax = 0.0f, ay = H * 0.9f, bx = W, by = H * 0.05f;
	const float lx = bx - ax, ly = by - ay, ll = std::sqrt(lx * lx + ly * ly);
	for(int y = 0; y < C.m_H; y++)
		for(int x = 0; x < C.m_W; x++)
		{
			const float d = std::fabs((x - ax) * ly - (y - ay) * lx) / ll;
			const float Band = std::exp(-std::pow(d / (H * 0.17f), 2.0f));
			if(Band < 0.01f)
				continue;
			const float n = Fbm2(x / W * 5.0f, y / H * 5.0f, 21, 6);
			const float Lane = Smooth(0.45f, 0.7f, Fbm2(x / W * 9.0f + 4.0f, y / H * 9.0f, 33, 5));
			const C3 c = Mix(Mix(Hex(0x4a3f9a), Hex(0xc88ad8), n), Hex(0xffcf9e), Smooth(0.6f, 0.8f, n) * Band);
			C.Add(x, y, c, Band * (0.15f + 0.6f * n) * (1.0f - 0.75f * Lane));
		}
	CRng Rng(3);
	C.Stars(Rng, (int)(2400 * C.m_S * C.m_S), H, 1.1f);
	for(int i = 0; i < 5; i++)
	{
		const float x = Rng.Range(0, W), y = Rng.Range(0, H * 0.7f), s = Rng.Range(10, 22) * C.m_S;
		const C3 c = Hex(0xe6eeff);
		C.Glow(x, y, s * 0.8f, c, 0.8f);
		C.Line(x - s, y, x + s, y, 1.2f * C.m_S, 1.2f * C.m_S, c, 0.35f);
		C.Line(x, y - s, x, y + s, 1.2f * C.m_S, 1.2f * C.m_S, c, 0.35f);
	}
	const std::vector<float> vTop = Ridge(C, H * 0.95f, H * 0.12f, 2.5f, 77);
	C.Haze(vTop, -H * 0.12f, Hex(0x3a2a62), 0.4f);
	FillBelow(C, vTop, Hex(0x07050c), Hex(0x020103));
}

void PaintNeon(CCanvas &C)
{
	const float aStops[] = {0.0f, 0.38f, 0.6f, 0.68f};
	const unsigned aColors[] = {0x08001a, 0x23063e, 0x64105c, 0xff2f72};
	C.Sky(aStops, aColors, 4);
	const float W = (float)C.m_W, H = (float)C.m_H;
	const float Horizon = H * 0.68f;
	CRng Rng(13);
	C.Stars(Rng, (int)(300 * C.m_S * C.m_S), H * 0.45f, 0.8f);
	// the striped sun
	const float sx = W * 0.5f, sy = H * 0.47f, sr = H * 0.17f;
	C.Glow(sx, sy, sr * 3.0f, Hex(0xff3d8a), 0.45f);
	for(int y = (int)(sy - sr); y <= (int)(sy + sr); y++)
	{
		const float t = (y - (sy - sr)) / (2.0f * sr);
		if(t > 0.5f)
		{
			const float Band = (t - 0.5f) * 2.0f; // stripes get thicker towards the bottom
			if(std::fmod((t - 0.5f) * 14.0f, 1.0f) < 0.18f + 0.55f * Band)
				continue;
		}
		const C3 c = Mix(Hex(0xffe27a), Hex(0xff3d8a), t);
		const float hw = std::sqrt(std::max(0.0f, sr * sr - (y - sy) * (y - sy)));
		for(int x = (int)(sx - hw); x <= (int)(sx + hw); x++)
			C.Blend(x, y, c, Clamp01(hw - std::fabs(x + 0.5f - sx) + 0.5f));
	}
	// skyline, two layers, lit windows on the front one
	for(int L = 0; L < 2; L++)
	{
		const C3 c = Hex(L ? 0x0b0316 : 0x2b0f46);
		float x = 0.0f;
		while(x < W)
		{
			const float bw = Rng.Range(14, 46) * C.m_S, bh = Rng.Range(0.04f, L ? 0.22f : 0.16f) * H;
			for(int xx = (int)x; xx < (int)(x + bw) && xx < C.m_W; xx++)
				C.Fill(xx, (int)(Horizon - bh), (int)Horizon, c, 1.0f);
			if(L == 1)
				for(float wy = Horizon - bh + 5 * C.m_S; wy < Horizon - 4 * C.m_S; wy += 6 * C.m_S)
					for(float wx = x + 3 * C.m_S; wx < x + bw - 3 * C.m_S; wx += 5 * C.m_S)
						if(Rng.Next() < 0.16f)
						{
							const unsigned aLit[] = {0x46f0ff, 0xff4fb0, 0xffd36a};
							const C3 Lit = Hex(aLit[(int)Rng.Range(0, 2.99f)]);
							for(int py = 0; py < (int)(2 * C.m_S + 1); py++)
								for(int px = 0; px < (int)(2 * C.m_S + 1); px++)
									C.Blend((int)wx + px, (int)wy + py, Lit, 0.9f);
						}
			x += bw + Rng.Range(0, 4) * C.m_S;
		}
	}
	// the floor with the glowing grid running to the horizon
	for(int y = (int)Horizon; y < C.m_H; y++)
		for(int x = 0; x < C.m_W; x++)
			C.At(x, y) = Mix(Hex(0x1a0430), Hex(0x05010c), (y - Horizon) / (H - Horizon));
	const C3 Grid = Hex(0xff2fa0);
	for(int k = 1; k < 14; k++)
	{
		const float t = k / 13.0f;
		const float y = Horizon + (H - Horizon) * t * t;
		C.Line(0, y, W, y, 1.4f * C.m_S, 1.4f * C.m_S, Grid, 0.25f + 0.6f * t);
	}
	for(int k = -12; k <= 12; k++)
		C.Line(sx + k * W * 0.02f, Horizon, sx + k * W * 0.16f, H, 1.0f * C.m_S, 2.2f * C.m_S, Grid, 0.55f);
	C.Glow(sx, Horizon, W * 0.5f, Hex(0xff2f72), 0.2f);
}

void PaintEmbers(CCanvas &C)
{
	const float W = (float)C.m_W, H = (float)C.m_H;
	for(int y = 0; y < C.m_H; y++)
		for(int x = 0; x < C.m_W; x++)
		{
			const float dx = x / W - 0.5f, dy = y / H - 1.05f;
			const float d = std::sqrt(dx * dx * 1.2f + dy * dy);
			C3 c = Mix(Hex(0x4a0f06), Hex(0x060203), Smooth(0.0f, 1.0f, d));
			// smoke, warped noise
			const float wx = Fbm2(x / W * 3.0f, y / H * 3.0f, 5, 4);
			const float n = Fbm2(x / W * 4.0f + wx * 2.0f, y / H * 3.0f - wx, 8, 5);
			c = c + Hex(0x3a1a12) * (Smooth(0.45f, 0.8f, n) * 0.6f * (1.0f - y / H * 0.4f));
			C.At(x, y) = c;
		}
	CRng Rng(17);
	for(int i = 0; i < 22; i++)
		C.Glow(Rng.Range(0, W), Rng.Range(0, H), Rng.Range(30, 90) * C.m_S, Hex(0xff6a1a), Rng.Range(0.08f, 0.2f));
	for(int i = 0; i < 180; i++)
	{
		const float x = Rng.Range(0, W), y = Rng.Range(0, H);
		const float Heat = Rng.Next();
		const C3 c = Mix(Hex(0xff5a12), Hex(0xffd27a), Heat);
		const float s = Rng.Range(1.0f, 2.6f) * C.m_S;
		C.Glow(x, y, s * 7.0f, c, 0.45f + Heat * 0.5f);
		C.Disc(x, y, s * 0.7f, Mix(c, Hex(0xfff2d0), 0.5f), 0.9f);
		if(Rng.Next() < 0.4f)
			C.Line(x, y, x + Rng.Range(-2, 2) * C.m_S, y + Rng.Range(3, 7) * C.m_S, s * 0.8f, s * 0.2f, c, 0.3f);
	}
}
} // namespace

const char *CcMenuBg::PresetName(int Preset)
{
	switch(Preset)
	{
	case BG_NONE: return "Без картинки";
	case BG_SUNSET: return "Закат в горах";
	case BG_FOREST: return "Ночной лес";
	case BG_SAKURA: return "Сакура";
	case BG_STARS: return "Звёздное небо";
	case BG_NEON: return "Неоновый город";
	case BG_EMBERS: return "Пепел и угли";
	default: return "";
	}
}

void CcMenuBg::Paint(int Preset, int W, int H, std::vector<uint8_t> &vRgba)
{
	CCanvas C(W, H);
	switch(Preset)
	{
	case BG_SUNSET: PaintSunset(C); break;
	case BG_FOREST: PaintForest(C); break;
	case BG_SAKURA: PaintSakura(C); break;
	case BG_STARS: PaintStars(C); break;
	case BG_NEON: PaintNeon(C); break;
	case BG_EMBERS: PaintEmbers(C); break;
	default: break;
	}
	C.Vignette(0.35f);
	vRgba.resize((size_t)W * H * 4);
	for(int y = 0; y < H; y++)
		for(int x = 0; x < W; x++)
		{
			const C3 &p = C.At(x, y);
			// a little dither against banding in the smooth skies
			const float Dither = (Hash2(x, y, 99) - 0.5f) / 255.0f;
			uint8_t *pOut = &vRgba[((size_t)y * W + x) * 4];
			pOut[0] = (uint8_t)std::lround(Clamp01(p.r + Dither) * 255.0f);
			pOut[1] = (uint8_t)std::lround(Clamp01(p.g + Dither) * 255.0f);
			pOut[2] = (uint8_t)std::lround(Clamp01(p.b + Dither) * 255.0f);
			pOut[3] = 255;
		}
}

void CcMenuBg::Blur(std::vector<uint8_t> &vRgba, int W, int H, int Radius)
{
	if(Radius <= 0 || W <= 0 || H <= 0)
		return;
	std::vector<uint8_t> vTmp(vRgba.size());
	const auto Pass = [&](const uint8_t *pSrc, uint8_t *pDst, int Len, int Count, int Stride, int Step) {
		// Count lines of Len pixels; Stride between lines, Step between pixels (in pixels)
		for(int l = 0; l < Count; l++)
		{
			const uint8_t *pLine = pSrc + (size_t)l * Stride * 4;
			uint8_t *pOutLine = pDst + (size_t)l * Stride * 4;
			for(int c = 0; c < 4; c++)
			{
				int Sum = 0;
				const auto Px = [&](int i) { return (int)pLine[(size_t)std::clamp(i, 0, Len - 1) * Step * 4 + c]; };
				for(int i = -Radius; i <= Radius; i++)
					Sum += Px(i);
				for(int i = 0; i < Len; i++)
				{
					pOutLine[(size_t)i * Step * 4 + c] = (uint8_t)(Sum / (2 * Radius + 1));
					Sum += Px(i + Radius + 1) - Px(i - Radius);
				}
			}
		}
	};
	for(int Iter = 0; Iter < 3; Iter++)
	{
		Pass(vRgba.data(), vTmp.data(), W, H, W, 1); // rows
		Pass(vTmp.data(), vRgba.data(), H, W, 1, W); // columns
	}
}

void CcMenuBg::Fit(std::vector<uint8_t> &vRgba, int &W, int &H, int MaxW, int MaxH)
{
	const float Scale = std::max(W / (float)MaxW, H / (float)MaxH);
	if(Scale <= 1.0f)
		return;
	const int NW = std::max(1, (int)(W / Scale)), NH = std::max(1, (int)(H / Scale));
	std::vector<uint8_t> vOut((size_t)NW * NH * 4);
	for(int y = 0; y < NH; y++)
	{
		const int y0 = (int)(y * Scale), y1 = std::max(y0 + 1, std::min(H, (int)((y + 1) * Scale)));
		for(int x = 0; x < NW; x++)
		{
			const int x0 = (int)(x * Scale), x1 = std::max(x0 + 1, std::min(W, (int)((x + 1) * Scale)));
			int aSum[4] = {0, 0, 0, 0};
			for(int sy = y0; sy < y1; sy++)
				for(int sx = x0; sx < x1; sx++)
					for(int c = 0; c < 4; c++)
						aSum[c] += vRgba[((size_t)sy * W + sx) * 4 + c];
			const int Num = (y1 - y0) * (x1 - x0);
			for(int c = 0; c < 4; c++)
				vOut[((size_t)y * NW + x) * 4 + c] = (uint8_t)(aSum[c] / Num);
		}
	}
	vRgba.swap(vOut);
	W = NW;
	H = NH;
}
