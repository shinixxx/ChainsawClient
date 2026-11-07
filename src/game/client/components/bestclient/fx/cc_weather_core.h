/* ChainsawClient: physical rain and snow for the atmosphere (no engine inside, so an offline preview can use it too).
   Rain drops that fly in the world (not the far ones) hit the blocks: on the top of a block a drop breaks into a few
   little droplets and fills a small puddle, on a side it only splashes. Puddles dry when the rain stops.
   Snowflakes land on the tops of blocks and on the snow already lying there: every block top keeps a few heights,
   which slide down to a gentle slope, fall off open edges and melt slowly, so tiny mounds grow.
   Cost: one or two tile lookups per world particle per frame; puddles, piles and droplets live in small capped pools. */
#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_CC_WEATHER_CORE_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_CC_WEATHER_CORE_H

#include <base/vmath.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace CcWeather
{
constexpr float TILE = 32.0f;
constexpr int PILE_SAMPLES = 7; // heights across one block top, 32 / 6 units apart
constexpr int MAX_PUDDLES = 256;
constexpr int MAX_PILES = 384;
constexpr int MAX_DROPLETS = 220;
constexpr int MAX_CROWNS = 60;

struct SDroplet
{
	vec2 m_Pos;
	vec2 m_Vel;
	float m_Life;
	float m_LifeSpan;
	float m_Size;
};

struct SCrown // the flash where a drop hits, or a ring on a puddle
{
	vec2 m_Pos;
	float m_Life;
	float m_LifeSpan;
	float m_Size;
	bool m_Ripple;
};

struct SPuddle
{
	int m_Tx, m_Ty; // the block it lies on
	float m_X; // center
	float m_MinX, m_MaxX; // how far it can spread (over the next block too when that one is a free top)
	float m_Level; // 0..1
	float m_Seed;
};

struct SPile
{
	int m_Tx, m_Ty;
	float m_aH[PILE_SAMPLES];
};

enum
{
	FLAKE_FLYING = 0,
	FLAKE_LANDED,
	FLAKE_GONE, // went into a block from the side or started inside one
};

class CWorld
{
public:
	using FSolid = bool (*)(void *pUser, int Tx, int Ty);

	void SetSolid(FSolid pfnSolid, void *pUser)
	{
		m_pfnSolid = pfnSolid;
		m_pSolidUser = pUser;
	}

	void Clear()
	{
		m_vDroplets.clear();
		m_vCrowns.clear();
		m_vPuddles.clear();
		m_vPiles.clear();
		m_PuddleIndex.clear();
		m_PileIndex.clear();
	}

	bool HasAny() const { return !m_vDroplets.empty() || !m_vCrowns.empty() || !m_vPuddles.empty() || !m_vPiles.empty(); }

	bool Solid(int Tx, int Ty) const { return m_pfnSolid && m_pfnSolid(m_pSolidUser, Tx, Ty); }
	static int TileOf(float v) { return (int)std::floor(v / TILE); }
	bool SolidAt(vec2 Pos) const { return Solid(TileOf(Pos.x), TileOf(Pos.y)); }

	// a block close above (caves, under roofs): the world drops start somewhere else. Close only: most DDNet maps are
	// closed boxes with a solid top, and drops that start above a roof hit the roof by themselves
	bool Covered(vec2 Pos, int Reach = 8) const
	{
		const int Tx = TileOf(Pos.x);
		int Ty = TileOf(Pos.y);
		for(int i = 0; i < Reach && Ty >= 0; i++, Ty--)
			if(Solid(Tx, Ty))
				return true;
		return false;
	}

	// rain drop moved from Old to New: true when it hit a block (then the caller sends it up again)
	bool RainStep(vec2 Old, vec2 New, float SplashAmount, float PuddleAmount, float WindX)
	{
		if(SolidAt(Old))
			return true; // started inside a block: gone without a splash
		const float Len = distance(Old, New);
		const int Steps = std::max(1, (int)std::ceil(Len / 14.0f));
		vec2 Air = Old;
		for(int s = 1; s <= Steps; s++)
		{
			const vec2 P = mix(Old, New, s / (float)Steps);
			if(!SolidAt(P))
			{
				Air = P;
				continue;
			}
			// find the surface between the last free point and this one
			vec2 In = P;
			for(int k = 0; k < 6; k++)
			{
				const vec2 Mid = (Air + In) * 0.5f;
				if(SolidAt(Mid))
					In = Mid;
				else
					Air = Mid;
			}
			const int Tx = TileOf(In.x), Ty = TileOf(In.y);
			const bool Top = TileOf(Air.y) < Ty && !Solid(Tx, Ty - 1);
			if(Top)
				HitTop(Tx, Ty, std::clamp(Air.x, Tx * TILE + 0.5f, Tx * TILE + TILE - 0.5f), SplashAmount, PuddleAmount, WindX);
			else
				HitSide(Air, Air.x < Tx * TILE ? -1.0f : 1.0f, SplashAmount);
			return true;
		}
		return false;
	}

	// snowflake moved from Old to New
	int SnowStep(vec2 Old, vec2 New, float PileAmount, float PileMax)
	{
		if(SolidAt(Old))
			return FLAKE_GONE;
		const int Tx = TileOf(New.x), Ty = TileOf(New.y);
		if(Solid(Tx, Ty))
		{
			if(!Solid(Tx, Ty - 1) && Old.y < Ty * TILE)
			{
				AddSnow(Tx, Ty, New.x, PileAmount, PileMax);
				return FLAKE_LANDED;
			}
			return FLAKE_GONE;
		}
		if(Solid(Tx, Ty + 1))
		{
			const float Surface = (Ty + 1) * TILE - PileHeight(Tx, Ty + 1, New.x);
			if(New.y >= Surface)
			{
				AddSnow(Tx, Ty + 1, New.x, PileAmount, PileMax);
				return FLAKE_LANDED;
			}
		}
		return FLAKE_FLYING;
	}

	float PileHeight(int Tx, int Ty, float x) const
	{
		const auto It = m_PileIndex.find(Key(Tx, Ty));
		if(It == m_PileIndex.end())
			return 0.0f;
		const SPile &Pile = m_vPiles[It->second];
		const float u = std::clamp((x - Tx * TILE) / TILE, 0.0f, 1.0f) * (PILE_SAMPLES - 1);
		const int i0 = std::min((int)u, PILE_SAMPLES - 2);
		const float f = u - i0;
		return Pile.m_aH[i0] * (1.0f - f) + Pile.m_aH[i0 + 1] * f;
	}

	// Raining / Snowing: new water and snow still comes. Melt: units per second the snow loses.
	void Update(float Dt, bool Raining, bool Snowing, float Melt)
	{
		// droplets fly in an arc and vanish in blocks
		for(size_t i = 0; i < m_vDroplets.size();)
		{
			SDroplet &D = m_vDroplets[i];
			D.m_Life += Dt;
			D.m_Vel.y += 1500.0f * Dt;
			D.m_Pos += D.m_Vel * Dt;
			if(D.m_Life >= D.m_LifeSpan || SolidAt(D.m_Pos))
			{
				D = m_vDroplets.back();
				m_vDroplets.pop_back();
				continue;
			}
			i++;
		}
		for(size_t i = 0; i < m_vCrowns.size();)
		{
			m_vCrowns[i].m_Life += Dt;
			if(m_vCrowns[i].m_Life >= m_vCrowns[i].m_LifeSpan)
			{
				m_vCrowns[i] = m_vCrowns.back();
				m_vCrowns.pop_back();
				continue;
			}
			i++;
		}

		// puddles dry, quicker without rain
		const float Dry = (Raining ? 0.006f : 0.04f) * Dt;
		for(size_t i = 0; i < m_vPuddles.size();)
		{
			m_vPuddles[i].m_Level -= Dry;
			if(m_vPuddles[i].m_Level <= 0.0f)
				ErasePuddle(i);
			else
				i++;
		}

		// snow slides to a gentle slope, falls off open edges, melts
		const float MeltNow = (Snowing ? Melt : std::max(Melt, 0.6f)) * Dt;
		constexpr float SLOPE = 1.9f; // most height difference between two samples (5.3 units apart)
		for(SPile &Pile : m_vPiles)
		{
			float *h = Pile.m_aH;
			for(int i = 0; i + 1 < PILE_SAMPLES; i++)
			{
				const float d = h[i] - h[i + 1];
				if(std::fabs(d) > SLOPE)
				{
					const float Move = (std::fabs(d) - SLOPE) * 0.5f * std::min(1.0f, Dt * 12.0f);
					h[i] -= d > 0.0f ? Move : -Move;
					h[i + 1] += d > 0.0f ? Move : -Move;
				}
			}
			// edges: shared with the next pile, or the snow falls down when the block ends there
			const auto Edge = [&](int Side, float &Mine, float Inner) {
				const int Nx = Pile.m_Tx + Side;
				if(Solid(Nx, Pile.m_Ty) && !Solid(Nx, Pile.m_Ty - 1))
				{
					const auto It = m_PileIndex.find(Key(Nx, Pile.m_Ty));
					if(It != m_PileIndex.end())
					{
						float &Theirs = m_vPiles[It->second].m_aH[Side < 0 ? PILE_SAMPLES - 1 : 0];
						const float Avg = (Mine + Theirs) * 0.5f;
						Mine = Theirs = Avg;
					}
					else
						Mine = std::min(Mine, Inner * 0.7f);
				}
				else
					Mine = std::min(Mine, Inner * 0.35f);
			};
			Edge(-1, h[0], h[1]);
			Edge(1, h[PILE_SAMPLES - 1], h[PILE_SAMPLES - 2]);
			for(int i = 0; i < PILE_SAMPLES; i++)
				h[i] = std::max(0.0f, h[i] - MeltNow);
		}
		// melted piles go after the pass, the index is used inside it
		for(size_t p = 0; p < m_vPiles.size();)
		{
			float Total = 0.0f;
			for(float h : m_vPiles[p].m_aH)
				Total += h;
			if(Total < 0.05f)
				ErasePile(p);
			else
				p++;
		}
	}

	// forget puddles and snow far away from the view (out of sight anyway), so the pools never fill up
	void Cull(float X0, float Y0, float X1, float Y1)
	{
		for(size_t i = 0; i < m_vPuddles.size();)
		{
			const float x = m_vPuddles[i].m_Tx * TILE, y = m_vPuddles[i].m_Ty * TILE;
			if(x < X0 || x > X1 || y < Y0 || y > Y1)
				ErasePuddle(i);
			else
				i++;
		}
		for(size_t i = 0; i < m_vPiles.size();)
		{
			const float x = m_vPiles[i].m_Tx * TILE, y = m_vPiles[i].m_Ty * TILE;
			if(x < X0 || x > X1 || y < Y0 || y > Y1)
				ErasePile(i);
			else
				i++;
		}
	}

	std::vector<SDroplet> m_vDroplets;
	std::vector<SCrown> m_vCrowns;
	std::vector<SPuddle> m_vPuddles;
	std::vector<SPile> m_vPiles;
	float m_DropletBudget = 1.0f; // the caller lowers it when the frame rate drops

private:
	static int64_t Key(int Tx, int Ty) { return ((int64_t)Tx << 32) ^ (uint32_t)Ty; }

	float Rand(float a, float b)
	{
		m_Rng ^= m_Rng << 13;
		m_Rng ^= m_Rng >> 17;
		m_Rng ^= m_Rng << 5;
		return a + (b - a) * ((m_Rng & 0xffffff) / (float)0x1000000);
	}

	void AddDroplet(vec2 Pos, vec2 Vel, float Size)
	{
		if((int)m_vDroplets.size() >= (int)(MAX_DROPLETS * m_DropletBudget))
			return;
		m_vDroplets.push_back({Pos, Vel, 0.0f, Rand(0.28f, 0.5f), Size});
	}

	void AddCrown(vec2 Pos, float Size, bool Ripple)
	{
		if((int)m_vCrowns.size() >= MAX_CROWNS)
			return;
		m_vCrowns.push_back({Pos, 0.0f, Ripple ? Rand(0.35f, 0.5f) : Rand(0.08f, 0.14f), Size, Ripple});
	}

	void HitTop(int Tx, int Ty, float x, float SplashAmount, float PuddleAmount, float WindX)
	{
		const float Top = Ty * TILE;
		SPuddle *pPuddle = PuddleAt(Tx, Ty);
		const bool InWater = pPuddle && pPuddle->m_Level > 0.15f && std::fabs(x - pPuddle->m_X) < PuddleWidth(*pPuddle) * 0.45f;
		if(InWater)
			AddCrown(vec2(x, Top - 0.8f), Rand(7.0f, 11.0f), true);
		else
			AddCrown(vec2(x, Top - 1.0f), Rand(4.0f, 6.5f), false);
		// fewer droplets when many are already flying (heavy rain on a block map)
		const float Busy = m_vDroplets.size() > MAX_DROPLETS / 2 ? 0.5f : 1.0f;
		const int Num = (int)(Rand(0.8f, 2.4f) * SplashAmount * Busy * (InWater ? 0.7f : 1.0f));
		for(int i = 0; i < Num; i++)
			AddDroplet(vec2(x + Rand(-1.5f, 1.5f), Top - 1.5f), vec2(Rand(-120.0f, 120.0f) + WindX * 90.0f, -Rand(110.0f, 260.0f)), Rand(1.1f, 2.0f));
		if(PuddleAmount > 0.0f)
			AddWater(Tx, Ty, x, PuddleAmount);
	}

	void HitSide(vec2 Pos, float Normal, float SplashAmount)
	{
		const int Num = (int)(Rand(0.6f, 2.0f) * SplashAmount);
		for(int i = 0; i < Num; i++)
			AddDroplet(Pos + vec2(Normal * 1.5f, 0.0f), vec2(Normal * Rand(40.0f, 130.0f), -Rand(10.0f, 90.0f)), Rand(1.0f, 1.7f));
	}

public:
	static float PuddleWidth(const SPuddle &Puddle)
	{
		return std::min(Puddle.m_MaxX - Puddle.m_MinX, 7.0f + 26.0f * Puddle.m_Level);
	}

private:
	SPuddle *PuddleAt(int Tx, int Ty)
	{
		const auto It = m_PuddleIndex.find(Key(Tx, Ty));
		return It == m_PuddleIndex.end() ? nullptr : &m_vPuddles[It->second];
	}

	void AddWater(int Tx, int Ty, float x, float Amount)
	{
		SPuddle *pPuddle = PuddleAt(Tx, Ty);
		if(!pPuddle)
		{
			if((int)m_vPuddles.size() >= MAX_PUDDLES)
			{
				// the driest one makes room
				size_t Driest = 0;
				for(size_t i = 1; i < m_vPuddles.size(); i++)
					if(m_vPuddles[i].m_Level < m_vPuddles[Driest].m_Level)
						Driest = i;
				ErasePuddle(Driest);
			}
			SPuddle Puddle;
			Puddle.m_Tx = Tx;
			Puddle.m_Ty = Ty;
			const float Left = Tx * TILE, Right = Left + TILE;
			const bool OpenLeft = Solid(Tx - 1, Ty) && !Solid(Tx - 1, Ty - 1);
			const bool OpenRight = Solid(Tx + 1, Ty) && !Solid(Tx + 1, Ty - 1);
			Puddle.m_MinX = OpenLeft ? Left - 10.0f : Left + 1.5f;
			Puddle.m_MaxX = OpenRight ? Right + 10.0f : Right - 1.5f;
			Puddle.m_X = std::clamp(x, Left + 6.0f, Right - 6.0f);
			Puddle.m_Level = 0.0f;
			Puddle.m_Seed = Rand(0.0f, 6.2832f);
			m_vPuddles.push_back(Puddle);
			m_PuddleIndex[Key(Tx, Ty)] = (int)m_vPuddles.size() - 1;
			pPuddle = &m_vPuddles.back();
		}
		pPuddle->m_Level = std::min(1.0f, pPuddle->m_Level + 0.012f * Amount);
		pPuddle->m_X += (x - pPuddle->m_X) * 0.04f;
		const float Half = PuddleWidth(*pPuddle) * 0.5f;
		pPuddle->m_X = std::clamp(pPuddle->m_X, pPuddle->m_MinX + Half, pPuddle->m_MaxX - Half);
	}

	// how high snow can lie at x (0.55..1): gentle hills about five blocks apart instead of a flat sheet
	static float Hill(float x)
	{
		const float a = std::sin(x * 0.041f + 1.3f), b = std::sin(x * 0.0137f + 4.1f);
		return 0.55f + 0.45f * (0.5f + 0.5f * a * (0.65f + 0.35f * b));
	}

	void AddSnow(int Tx, int Ty, float x, float Amount, float Max)
	{
		const int64_t K = Key(Tx, Ty);
		auto It = m_PileIndex.find(K);
		if(It == m_PileIndex.end())
		{
			if((int)m_vPiles.size() >= MAX_PILES)
				return;
			SPile Pile;
			Pile.m_Tx = Tx;
			Pile.m_Ty = Ty;
			for(float &h : Pile.m_aH)
				h = 0.0f;
			m_vPiles.push_back(Pile);
			It = m_PileIndex.emplace(K, (int)m_vPiles.size() - 1).first;
		}
		float *h = m_vPiles[It->second].m_aH;
		const float u = std::clamp((x - Tx * TILE) / TILE, 0.0f, 1.0f) * (PILE_SAMPLES - 1);
		const int i0 = std::min((int)u, PILE_SAMPLES - 2);
		const float f = u - i0;
		const float Step = TILE / (PILE_SAMPLES - 1);
		h[i0] = std::min(Max * Hill(Tx * TILE + i0 * Step), h[i0] + Amount * (1.0f - f));
		h[i0 + 1] = std::min(Max * Hill(Tx * TILE + (i0 + 1) * Step), h[i0 + 1] + Amount * f);
	}

	// remove in O(1): the last one moves into the hole, only its index entry changes
	void ErasePuddle(size_t i)
	{
		m_PuddleIndex.erase(Key(m_vPuddles[i].m_Tx, m_vPuddles[i].m_Ty));
		if(i + 1 != m_vPuddles.size())
		{
			m_vPuddles[i] = m_vPuddles.back();
			m_PuddleIndex[Key(m_vPuddles[i].m_Tx, m_vPuddles[i].m_Ty)] = (int)i;
		}
		m_vPuddles.pop_back();
	}

	void ErasePile(size_t i)
	{
		m_PileIndex.erase(Key(m_vPiles[i].m_Tx, m_vPiles[i].m_Ty));
		if(i + 1 != m_vPiles.size())
		{
			m_vPiles[i] = m_vPiles.back();
			m_PileIndex[Key(m_vPiles[i].m_Tx, m_vPiles[i].m_Ty)] = (int)i;
		}
		m_vPiles.pop_back();
	}

	FSolid m_pfnSolid = nullptr;
	void *m_pSolidUser = nullptr;
	uint32_t m_Rng = 0x9e3779b9u;
	std::unordered_map<int64_t, int> m_PuddleIndex;
	std::unordered_map<int64_t, int> m_PileIndex;
};
} // namespace CcWeather

#endif
