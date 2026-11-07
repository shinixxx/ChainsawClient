/* ChainsawClient: atmosphere, see cc_atmosphere.h. */
#include "cc_atmosphere.h"

#include "own_tee.h"

#include <base/math.h>

#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/components/sounds.h>
#include <game/client/gameclient.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace
{
enum
{
	KIND_RAIN = 0,
	KIND_SNOW,
	KIND_FLY,
	KIND_LEAF,
	KIND_STREAK,
	KIND_FOG,
	KIND_EMBER,
};

ColorRGBA Cfg(unsigned Packed) { return color_cast<ColorRGBA>(ColorHSLA(Packed)).WithAlpha(1.0f); }

float Smooth(float a, float b, float x)
{
	const float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

ColorRGBA LeafColor(int Kind)
{
	static const ColorRGBA s_aAutumn[] = {ColorRGBA(0.93f, 0.45f, 0.12f, 1.0f), ColorRGBA(0.85f, 0.22f, 0.1f, 1.0f), ColorRGBA(0.97f, 0.72f, 0.2f, 1.0f), ColorRGBA(0.62f, 0.3f, 0.12f, 1.0f)};
	static const ColorRGBA s_aSummer[] = {ColorRGBA(0.35f, 0.7f, 0.25f, 1.0f), ColorRGBA(0.22f, 0.55f, 0.2f, 1.0f), ColorRGBA(0.55f, 0.8f, 0.3f, 1.0f), ColorRGBA(0.3f, 0.62f, 0.38f, 1.0f)};
	static const ColorRGBA s_aSakura[] = {ColorRGBA(1.0f, 0.78f, 0.86f, 1.0f), ColorRGBA(1.0f, 0.65f, 0.78f, 1.0f), ColorRGBA(1.0f, 0.88f, 0.93f, 1.0f), ColorRGBA(0.97f, 0.55f, 0.72f, 1.0f)};
	const ColorRGBA *pSet = Kind == 1 ? s_aSummer : (Kind == 2 ? s_aSakura : s_aAutumn);
	return pSet[(int)random_float(0.0f, 3.99f)];
}
} // namespace

void CCcAtmosphere::OnInit()
{
	if(Sound()->IsSoundEnabled())
	{
		// read directly: the sound pack logic of LoadOpus skips files that are not in the pack
		const auto Load = [&](const char *pPath) {
			void *pData = nullptr;
			unsigned DataSize = 0;
			if(!Storage()->ReadFile(pPath, IStorage::TYPE_ALL, &pData, &DataSize))
				return -1;
			const int Id = Sound()->LoadOpusFromMem(pData, DataSize, false, pPath);
			free(pData);
			return Id;
		};
		m_RainSample = Load("chainsaw/sounds/atm_rain.opus");
		m_WindSample = Load("chainsaw/sounds/atm_wind.opus");
		m_ThunderSample = Load("chainsaw/sounds/atm_thunder.opus");
	}
	m_Weather.SetSolid(SolidTile, this);
	OnReset();
}

void CCcAtmosphere::OnReset()
{
	m_vRain.clear();
	m_vSnow.clear();
	m_vFlies.clear();
	m_vLeaves.clear();
	m_vStreaks.clear();
	m_vFog.clear();
	m_vEmbers.clear();
	m_vStars.clear();
	m_vBolts.clear();
	m_Weather.Clear();
	m_HaveCenter = false;
	m_Flash = 0.0f;
	m_ThunderIn = -1.0f;
	StopSounds();
}

void CCcAtmosphere::OnShutdown()
{
	StopSounds();
}

void CCcAtmosphere::StopSounds()
{
	if(m_RainVoice.IsValid())
		Sound()->StopVoice(m_RainVoice);
	if(m_WindVoice.IsValid())
		Sound()->StopVoice(m_WindVoice);
	m_RainVoice = ISound::CVoiceHandle();
	m_WindVoice = ISound::CVoiceHandle();
}

void CCcAtmosphere::UpdateSounds()
{
	const float Volume = g_Config.m_BcAtmSounds ? g_Config.m_BcAtmSoundVolume / 100.0f : 0.0f;
	const float RainVolume = g_Config.m_BcAtmStorm ? Volume * std::clamp(g_Config.m_BcAtmRainAmount / 100.0f, 0.0f, 1.5f) * 0.8f : 0.0f;
	const float WindVolume = g_Config.m_BcAtmWind ? Volume * (0.3f + 0.7f * g_Config.m_BcAtmWindStrength / 100.0f) * 0.7f : 0.0f;
	const auto Keep = [&](ISound::CVoiceHandle &Voice, int Sample, float Vol) {
		if(Vol <= 0.001f || Sample < 0)
		{
			if(Voice.IsValid())
				Sound()->StopVoice(Voice);
			Voice = ISound::CVoiceHandle();
			return;
		}
		if(!Voice.IsValid())
			Voice = Sound()->Play(CSounds::CHN_GLOBAL, Sample, ISound::FLAG_LOOP, Vol);
		else
			Sound()->SetVoiceVolume(Voice, Vol);
	};
	Keep(m_RainVoice, m_RainSample, RainVolume);
	Keep(m_WindVoice, m_WindSample, WindVolume);
}

void CCcAtmosphere::Spawn(SPart &P, int Kind, bool Anywhere)
{
	const float W = m_View.m_X1 - m_View.m_X0, H = m_View.m_Y1 - m_View.m_Y0;
	P = SPart();
	P.m_Pos = vec2(random_float(m_View.m_X0, m_View.m_X1), random_float(m_View.m_Y0, m_View.m_Y1));
	P.m_Phase = random_float(0.0f, 2.0f * pi);
	P.m_Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	switch(Kind)
	{
	case KIND_RAIN:
		P.m_Z = random_float(0.35f, 1.0f);
		P.m_Size = random_float(26.0f, 44.0f);
		P.m_Vel = vec2(0.0f, random_float(1300.0f, 1800.0f));
		if(!Anywhere)
			P.m_Pos.y = m_View.m_Y0 - random_float(0.0f, H * 0.2f);
		MakeWorldly(P, g_Config.m_BcAtmRainPhysics != 0);
		break;
	case KIND_SNOW:
		P.m_Z = random_float(0.3f, 1.0f);
		P.m_Size = random_float(2.5f, 5.0f);
		P.m_Vel = vec2(0.0f, random_float(40.0f, 90.0f));
		if(!Anywhere)
			P.m_Pos.y = m_View.m_Y0 - random_float(0.0f, H * 0.1f);
		MakeWorldly(P, g_Config.m_BcAtmSnowPhysics != 0);
		break;
	case KIND_FLY:
		P.m_Z = random_float(0.55f, 1.0f);
		P.m_Size = random_float(0.8f, 1.2f);
		P.m_Vel = direction(random_float(0.0f, 2.0f * pi)) * 20.0f;
		break;
	case KIND_LEAF:
		P.m_Z = random_float(0.5f, 1.0f);
		P.m_Size = random_float(7.0f, 12.0f);
		P.m_Rot = random_float(0.0f, 2.0f * pi);
		P.m_RotSpeed = random_float(-4.0f, 4.0f);
		P.m_Color = LeafColor(g_Config.m_BcAtmWindLeafKind);
		if(!Anywhere)
			P.m_Pos.x = m_WindX >= 0.0f ? m_View.m_X0 - random_float(0.0f, W * 0.1f) : m_View.m_X1 + random_float(0.0f, W * 0.1f);
		break;
	case KIND_STREAK:
		P.m_Z = random_float(0.6f, 1.0f);
		P.m_Size = random_float(120.0f, 260.0f);
		P.m_LifeSpan = random_float(0.7f, 1.5f);
		P.m_Life = Anywhere ? random_float(0.0f, P.m_LifeSpan) : 0.0f;
		break;
	case KIND_FOG:
		P.m_Z = random_float(0.6f, 1.0f);
		P.m_Size = H * random_float(0.45f, 0.8f);
		if(g_Config.m_BcAtmFogLow)
			P.m_Pos.y = m_View.m_Y1 - H * random_float(-0.05f, 0.3f);
		break;
	case KIND_EMBER:
		P.m_Z = random_float(0.4f, 1.0f);
		P.m_Size = random_float(1.5f, 3.2f);
		P.m_Vel = vec2(random_float(-15.0f, 15.0f), -random_float(40.0f, 110.0f));
		P.m_Color = Cfg(g_Config.m_BcAtmEmbersColor);
		if(!Anywhere)
			P.m_Pos.y = m_View.m_Y1 + random_float(0.0f, H * 0.1f);
		break;
	}
}

// physical rain / snow: most of the drops fly in the world itself and hit the blocks; not where a block hangs above
// (caves, roofs) - there only the far ones fall, as before
void CCcAtmosphere::MakeWorldly(SPart &P, bool Physics)
{
	P.m_World = false;
	if(!Physics || random_float(0.0f, 1.0f) >= 0.6f)
		return;
	if(m_Weather.Covered(P.m_Pos) || m_Weather.SolidAt(P.m_Pos))
	{
		P.m_Z = random_float(0.35f, 0.8f);
		return;
	}
	P.m_Z = 1.0f;
	P.m_World = true;
}

bool CCcAtmosphere::SolidTile(void *pUser, int Tx, int Ty)
{
	const CCcAtmosphere *pSelf = static_cast<const CCcAtmosphere *>(pUser);
	return pSelf->Collision()->CheckPoint(Tx * 32.0f + 16.0f, Ty * 32.0f + 16.0f);
}

void CCcAtmosphere::Resize(std::vector<SPart> &vParts, int Target, int Kind)
{
	Target = std::max(0, (int)(Target * m_Quality));
	if((int)vParts.size() > Target)
		vParts.resize(Target);
	while((int)vParts.size() < Target)
	{
		vParts.emplace_back();
		Spawn(vParts.back(), Kind, true);
	}
}

void CCcAtmosphere::Wrap(SPart &P, float Margin)
{
	const float x0 = m_View.m_X0 - Margin, x1 = m_View.m_X1 + Margin;
	const float y0 = m_View.m_Y0 - Margin, y1 = m_View.m_Y1 + Margin;
	const float W = x1 - x0, H = y1 - y0;
	if(W <= 0.0f || H <= 0.0f)
		return;
	if(P.m_Pos.x < x0)
		P.m_Pos.x += W * std::ceil((x0 - P.m_Pos.x) / W);
	else if(P.m_Pos.x > x1)
		P.m_Pos.x -= W * std::ceil((P.m_Pos.x - x1) / W);
	if(P.m_Pos.y < y0)
		P.m_Pos.y += H * std::ceil((y0 - P.m_Pos.y) / H);
	else if(P.m_Pos.y > y1)
		P.m_Pos.y -= H * std::ceil((P.m_Pos.y - y1) / H);
}

void CCcAtmosphere::Strike()
{
	m_Flash = 1.0f;
	m_FlashAge = 0.0f;
	m_ThunderIn = random_float(0.25f, 1.6f);
	m_ThunderVolume = random_float(0.6f, 1.0f);
	if(!g_Config.m_BcAtmLightningBolts)
		return;
	const float W = m_View.m_X1 - m_View.m_X0, H = m_View.m_Y1 - m_View.m_Y0;
	SBolt Bolt;
	vec2 At(random_float(m_View.m_X0 + W * 0.1f, m_View.m_X1 - W * 0.1f), m_View.m_Y0 - 10.0f);
	const float EndY = m_View.m_Y0 + H * random_float(0.35f, 0.7f);
	const int Steps = 26;
	const float Lean = random_float(-0.5f, 0.5f);
	Bolt.m_vMain.push_back(At);
	for(int i = 1; i <= Steps; i++)
	{
		At.y = m_View.m_Y0 - 10.0f + (EndY - m_View.m_Y0 + 10.0f) * i / Steps;
		At.x += random_float(-1.0f, 1.0f) * H * 0.03f + Lean * H * 0.012f;
		Bolt.m_vMain.push_back(At);
	}
	const int NumBranches = (int)random_float(2.0f, 4.99f);
	for(int b = 0; b < NumBranches; b++)
	{
		const int From = (int)random_float(3.0f, Steps - 3.0f);
		vec2 P = Bolt.m_vMain[From];
		const float Side = random_float(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f;
		std::vector<vec2> vBranch = {P};
		const int Len = (int)random_float(4.0f, 9.0f);
		for(int i = 0; i < Len; i++)
		{
			P += vec2(Side * random_float(0.3f, 1.0f), random_float(0.6f, 1.2f)) * H * 0.025f;
			vBranch.push_back(P);
		}
		Bolt.m_vvBranches.push_back(vBranch);
	}
	m_vBolts.push_back(Bolt);
}

void CCcAtmosphere::TestLightning()
{
	Strike();
}

void CCcAtmosphere::Update(float Dt, vec2 CameraDelta)
{
	const float W = m_View.m_X1 - m_View.m_X0, H = m_View.m_Y1 - m_View.m_Y0;
	const auto Parallax = [&](SPart &P) { P.m_Pos += CameraDelta * (1.0f - P.m_Z); };

	// storm: rain falls, wind tilts it
	if(g_Config.m_BcAtmStorm)
	{
		Resize(m_vRain, std::min(900, (int)(380 * g_Config.m_BcAtmRainAmount / 100.0f)), KIND_RAIN);
		const bool Physics = g_Config.m_BcAtmRainPhysics != 0;
		const float Splash = g_Config.m_BcAtmRainSplash / 100.0f, Puddles = g_Config.m_BcAtmRainPuddles / 100.0f;
		for(SPart &P : m_vRain)
		{
			Parallax(P);
			const vec2 Old = P.m_Pos;
			P.m_Pos += vec2(m_WindX * 450.0f, P.m_Vel.y) * P.m_Z * Dt;
			if(P.m_World && Physics && m_Weather.RainStep(Old, P.m_Pos, Splash, Puddles, m_WindX))
			{
				Spawn(P, KIND_RAIN, false);
				continue;
			}
			if(P.m_Pos.y > m_View.m_Y1 + 60.0f)
			{
				Spawn(P, KIND_RAIN, false);
				continue;
			}
			Wrap(P, 60.0f);
		}
		// lightning
		if(g_Config.m_BcAtmLightning > 0)
		{
			m_BoltTimer -= Dt;
			if(m_BoltTimer <= 0.0f)
			{
				const float f = g_Config.m_BcAtmLightning / 100.0f;
				const float Mean = 2.5f + 30.0f * (1.0f - f) * (1.0f - f);
				m_BoltTimer = Mean * random_float(0.4f, 1.6f);
				Strike();
			}
		}
	}
	else
		m_vRain.clear();

	if(g_Config.m_BcAtmSnow)
	{
		Resize(m_vSnow, std::min(600, (int)(250 * g_Config.m_BcAtmSnowAmount / 100.0f)), KIND_SNOW);
		const float Speed = g_Config.m_BcAtmSnowSpeed / 100.0f;
		const bool Physics = g_Config.m_BcAtmSnowPhysics != 0;
		const float PileMax = 8.0f * g_Config.m_BcAtmSnowPile / 100.0f;
		for(SPart &P : m_vSnow)
		{
			Parallax(P);
			const float Sway = std::sin(m_Time * 1.3f + P.m_Phase) * 22.0f;
			const vec2 Old = P.m_Pos;
			P.m_Pos += vec2(m_WindX * 140.0f + Sway, P.m_Vel.y * Speed) * P.m_Z * Dt;
			if(P.m_World && Physics && m_Weather.SnowStep(Old, P.m_Pos, 2.4f, PileMax) != CcWeather::FLAKE_FLYING)
			{
				Spawn(P, KIND_SNOW, false);
				continue;
			}
			if(P.m_Pos.y > m_View.m_Y1 + 20.0f)
			{
				Spawn(P, KIND_SNOW, false);
				continue;
			}
			Wrap(P, 20.0f);
		}
	}
	else
		m_vSnow.clear();

	if(g_Config.m_BcAtmFireflies)
	{
		Resize(m_vFlies, std::min(150, (int)(40 * g_Config.m_BcAtmFirefliesAmount / 100.0f)), KIND_FLY);
		const float Speed = g_Config.m_BcAtmFirefliesSpeed / 100.0f;
		for(SPart &P : m_vFlies)
		{
			Parallax(P);
			// wander: the heading turns smoothly, sometimes faster
			const float Turn = std::sin(m_Time * 0.7f + P.m_Phase * 3.0f) * 1.6f + std::sin(m_Time * 1.9f + P.m_Phase) * 0.8f;
			P.m_Vel = rotate(P.m_Vel, Turn * Dt * 57.2958f);
			const float Len = length(P.m_Vel);
			P.m_Vel = Len > 0.001f ? P.m_Vel / Len * 26.0f : vec2(26.0f, 0.0f);
			P.m_Pos += (P.m_Vel * Speed + vec2(m_WindX * 40.0f, 0.0f)) * P.m_Z * Dt;
			Wrap(P, 40.0f);
		}
	}
	else
		m_vFlies.clear();

	if(g_Config.m_BcAtmWind)
	{
		const float Strength = 0.3f + 0.7f * g_Config.m_BcAtmWindStrength / 100.0f;
		Resize(m_vLeaves, std::min(120, (int)(24 * g_Config.m_BcAtmWindLeaves / 100.0f)), KIND_LEAF);
		for(SPart &P : m_vLeaves)
		{
			Parallax(P);
			const float Flutter = std::sin(m_Time * 2.3f + P.m_Phase);
			P.m_Pos += vec2(m_WindX * (220.0f + 160.0f * Strength) + Flutter * 40.0f, 55.0f + Flutter * 70.0f) * P.m_Z * Dt;
			P.m_Rot += P.m_RotSpeed * Dt * (1.0f + Strength);
			const bool Gone = m_WindX >= 0.0f ? P.m_Pos.x > m_View.m_X1 + 40.0f : P.m_Pos.x < m_View.m_X0 - 40.0f;
			if(Gone || P.m_Pos.y > m_View.m_Y1 + 40.0f)
			{
				Spawn(P, KIND_LEAF, false);
				P.m_Pos.y = random_float(m_View.m_Y0 - H * 0.1f, m_View.m_Y1 - H * 0.2f);
				continue;
			}
			Wrap(P, 60.0f);
		}
		Resize(m_vStreaks, std::min(50, (int)(14 * g_Config.m_BcAtmWindStreaks / 100.0f)), KIND_STREAK);
		for(SPart &P : m_vStreaks)
		{
			Parallax(P);
			P.m_Life += Dt;
			if(P.m_Life >= P.m_LifeSpan)
			{
				Spawn(P, KIND_STREAK, false);
				continue;
			}
			P.m_Pos.x += m_WindX * (700.0f + 500.0f * Strength) * P.m_Z * Dt;
			Wrap(P, 300.0f);
		}
	}
	else
	{
		m_vLeaves.clear();
		m_vStreaks.clear();
	}

	if(g_Config.m_BcAtmFog)
	{
		Resize(m_vFog, 12, KIND_FOG);
		const float Speed = g_Config.m_BcAtmFogSpeed / 100.0f;
		for(SPart &P : m_vFog)
		{
			Parallax(P);
			P.m_Pos.x += (14.0f + m_WindX * 90.0f) * Speed * P.m_Z * Dt;
			if(g_Config.m_BcAtmFogLow)
			{
				// keep the low fog low when the camera moves up and down
				const float Low = m_View.m_Y1 - H * 0.12f;
				P.m_Pos.y += (Low - P.m_Pos.y) * std::min(1.0f, Dt * 0.6f) * 0.5f;
			}
			Wrap(P, P.m_Size);
		}
	}
	else
		m_vFog.clear();

	if(g_Config.m_BcAtmEmbers)
	{
		Resize(m_vEmbers, std::min(200, (int)(60 * g_Config.m_BcAtmEmbersAmount / 100.0f)), KIND_EMBER);
		const float Speed = g_Config.m_BcAtmEmbersSpeed / 100.0f;
		for(SPart &P : m_vEmbers)
		{
			Parallax(P);
			const float Sway = std::sin(m_Time * 1.7f + P.m_Phase) * 18.0f;
			P.m_Pos += vec2(P.m_Vel.x + Sway + m_WindX * 120.0f, P.m_Vel.y * Speed) * P.m_Z * Dt;
			if(P.m_Pos.y < m_View.m_Y0 - 20.0f)
			{
				Spawn(P, KIND_EMBER, false);
				continue;
			}
			Wrap(P, 20.0f);
		}
	}
	else
		m_vEmbers.clear();

	// shooting stars live in the sky: they move with the camera
	if(g_Config.m_BcAtmStars)
	{
		m_StarTimer -= Dt;
		if(m_StarTimer <= 0.0f && m_vStars.size() < 6)
		{
			m_StarTimer = 60.0f / g_Config.m_BcAtmStarsRate * random_float(0.4f, 1.6f);
			SStar Star;
			const float Side = random_float(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f;
			Star.m_Pos = vec2(Side > 0.0f ? random_float(m_View.m_X0, m_View.m_X0 + W * 0.7f) : random_float(m_View.m_X0 + W * 0.3f, m_View.m_X1), m_View.m_Y0 + random_float(0.0f, H * 0.3f));
			Star.m_Dir = normalize(vec2(Side * random_float(0.75f, 1.0f), random_float(0.3f, 0.6f)));
			Star.m_Speed = random_float(900.0f, 1500.0f) * (H / 600.0f);
			Star.m_Length = random_float(140.0f, 220.0f) * g_Config.m_BcAtmStarsLength / 100.0f * (H / 600.0f);
			Star.m_LifeSpan = random_float(0.55f, 1.0f);
			Star.m_Life = 0.0f;
			m_vStars.push_back(Star);
		}
	}
	for(auto It = m_vStars.begin(); It != m_vStars.end();)
	{
		It->m_Life += Dt;
		It->m_Pos += CameraDelta + It->m_Dir * It->m_Speed * Dt;
		if(It->m_Life >= It->m_LifeSpan)
			It = m_vStars.erase(It);
		else
			++It;
	}

	// puddles, snow piles and splashes: droplets fly, puddles dry, snow settles and melts
	m_Weather.m_DropletBudget = m_Quality;
	m_Weather.Update(Dt, g_Config.m_BcAtmStorm && g_Config.m_BcAtmRainPhysics, g_Config.m_BcAtmSnow && g_Config.m_BcAtmSnowPhysics, g_Config.m_BcAtmSnowMelt / 100.0f * 0.1f);

	// flash and bolts fade, thunder comes later
	m_FlashAge += Dt;
	m_Flash = std::max(0.0f, 1.0f - m_FlashAge / 0.45f);
	for(auto It = m_vBolts.begin(); It != m_vBolts.end();)
	{
		It->m_Life += Dt;
		for(vec2 &P : It->m_vMain)
			P += CameraDelta * 0.85f;
		for(auto &vBranch : It->m_vvBranches)
			for(vec2 &P : vBranch)
				P += CameraDelta * 0.85f;
		if(It->m_Life > 0.4f)
			It = m_vBolts.erase(It);
		else
			++It;
	}
	if(m_ThunderIn >= 0.0f)
	{
		m_ThunderIn -= Dt;
		if(m_ThunderIn < 0.0f && g_Config.m_BcAtmSounds && m_ThunderSample >= 0)
			Sound()->Play(CSounds::CHN_GLOBAL, m_ThunderSample, 0, m_ThunderVolume * g_Config.m_BcAtmSoundVolume / 100.0f);
	}
}

void CCcAtmosphere::Draw()
{
	const CcFx::CAtlas &Atlas = CcFx::Atlas(Graphics());
	const float W = m_View.m_X1 - m_View.m_X0, H = m_View.m_Y1 - m_View.m_Y0;
	const vec2 Center((m_View.m_X0 + m_View.m_X1) * 0.5f, (m_View.m_Y0 + m_View.m_Y1) * 0.5f);
	const float Px = H / 600.0f; // world units per menu pixel at this zoom, keeps sizes steady when zooming

	// 1 normal: the storm darkens the world, fog, rain, leaves
	const bool Dark = g_Config.m_BcAtmStorm && g_Config.m_BcAtmStormDark > 0;
	if(Dark || !m_vFog.empty() || !m_vRain.empty() || !m_vLeaves.empty() || m_Weather.HasAny())
	{
		CcFx::Begin(Graphics(), Atlas.m_Texture, false);
		if(Dark)
		{
			const ColorRGBA Shade(0.02f, 0.03f, 0.06f, g_Config.m_BcAtmStormDark / 100.0f);
			CcFx::Shape(Graphics(), vec2(m_View.m_X0, m_View.m_Y0), vec2(m_View.m_X1, m_View.m_Y0), vec2(m_View.m_X0, m_View.m_Y1), vec2(m_View.m_X1, m_View.m_Y1), Shade);
		}
		if(!m_vFog.empty())
		{
			const ColorRGBA Fog = Cfg(g_Config.m_BcAtmFogColor);
			const float Alpha = g_Config.m_BcAtmFogDensity / 100.0f * 0.32f;
			for(const SPart &P : m_vFog)
			{
				const float Breathe = 0.8f + 0.2f * std::sin(m_Time * 0.3f + P.m_Phase);
				CcFx::QuadAxis(Graphics(), P.m_Pos, P.m_Size * 2.2f, P.m_Size, Atlas.m_Glow, Fog.WithAlpha(Alpha * Breathe * P.m_Z));
			}
		}
		if(m_Weather.HasAny())
			DrawWeatherNormal(Atlas);
		if(!m_vRain.empty())
		{
			const float Opacity = g_Config.m_BcAtmRainOpacity / 100.0f;
			const float LenScale = g_Config.m_BcAtmRainLength / 100.0f;
			const float Angle = std::atan2(1.0f, m_WindX * 450.0f / 1500.0f);
			for(const SPart &P : m_vRain)
			{
				const ColorRGBA c(0.72f, 0.8f, 0.95f, Opacity * (0.35f + 0.65f * P.m_Z));
				CcFx::QuadRotated(Graphics(), P.m_Pos, P.m_Size * LenScale * P.m_Z * Px * 1.6f, 2.4f * P.m_Z * Px, Angle, Atlas.m_Glow, c);
			}
		}
		if(!m_vLeaves.empty())
			for(const SPart &P : m_vLeaves)
			{
				const float Flip = std::fabs(std::sin(m_Time * 3.0f + P.m_Phase)); // the leaf turns over while it flies
				CcFx::QuadRotated(Graphics(), P.m_Pos, P.m_Size * P.m_Z * Px * 1.4f, P.m_Size * P.m_Z * Px * (0.35f + 0.65f * Flip), P.m_Rot, Atlas.m_Petal, P.m_Color.WithAlpha(0.9f));
			}
		CcFx::End(Graphics());
	}

	// 2 additive: snow, fireflies, embers, wind streaks, shooting stars, lightning
	const bool AnyLight = !m_vSnow.empty() || !m_vFlies.empty() || !m_vEmbers.empty() || !m_vStreaks.empty() || !m_vStars.empty() || !m_vBolts.empty() || m_Flash > 0.0f ||
			      !m_Weather.m_vCrowns.empty() || !m_Weather.m_vPuddles.empty();
	if(!AnyLight)
		return;
	CcFx::Begin(Graphics(), Atlas.m_Texture, true);
	if(!m_Weather.m_vCrowns.empty() || !m_Weather.m_vPuddles.empty())
		DrawWeatherAdditive(Atlas);
	if(!m_vSnow.empty())
	{
		const ColorRGBA Snow = Cfg(g_Config.m_BcAtmSnowColor);
		const float Size = g_Config.m_BcAtmSnowSize / 100.0f;
		for(const SPart &P : m_vSnow)
		{
			const float s = P.m_Size * P.m_Z * Size * Px;
			CcFx::QuadAxis(Graphics(), P.m_Pos, s * 3.0f, s * 3.0f, Atlas.m_Glow, Snow.WithAlpha(0.35f * P.m_Z));
			CcFx::QuadAxis(Graphics(), P.m_Pos, s * 1.2f, s * 1.2f, Atlas.m_Glow, Snow.WithAlpha(0.8f * P.m_Z));
		}
	}
	if(!m_vFlies.empty())
	{
		const ColorRGBA Fly = Cfg(g_Config.m_BcAtmFirefliesColor);
		const float Size = g_Config.m_BcAtmFirefliesSize / 100.0f, Glow = g_Config.m_BcAtmFirefliesGlow / 100.0f;
		for(const SPart &P : m_vFlies)
		{
			float Light = 1.0f;
			if(g_Config.m_BcAtmFirefliesBlink)
			{
				const float s = std::max(0.0f, std::sin(m_Time * (0.9f + 0.5f * std::fabs(std::sin(P.m_Phase))) + P.m_Phase));
				Light = 0.15f + 0.85f * s * s * s;
			}
			const float s = 5.0f * P.m_Size * P.m_Z * Size * Px;
			if(Glow > 0.0f)
				CcFx::QuadAxis(Graphics(), P.m_Pos, s * 7.0f, s * 7.0f, Atlas.m_Glow, Fly.WithAlpha(0.35f * Glow * Light * P.m_Z));
			CcFx::QuadAxis(Graphics(), P.m_Pos, s * 1.6f, s * 1.6f, Atlas.m_Glow, Fly.WithAlpha(Light));
			CcFx::QuadAxis(Graphics(), P.m_Pos, s * 0.7f, s * 0.7f, Atlas.m_Glow, ColorRGBA(1.0f, 1.0f, 0.9f, Light * 0.8f));
		}
	}
	if(!m_vEmbers.empty())
	{
		const float Size = g_Config.m_BcAtmEmbersSize / 100.0f;
		for(const SPart &P : m_vEmbers)
		{
			const float Flicker = 0.6f + 0.4f * std::sin(m_Time * 11.0f + P.m_Phase * 5.0f);
			const float s = P.m_Size * P.m_Z * Size * Px;
			CcFx::QuadAxis(Graphics(), P.m_Pos, s * 7.0f, s * 7.0f, Atlas.m_Glow, P.m_Color.WithAlpha(0.3f * Flicker * P.m_Z));
			CcFx::QuadAxis(Graphics(), P.m_Pos, s * 1.8f, s * 1.8f, Atlas.m_Glow, ColorRGBA(1.0f, 0.9f, 0.7f, 0.9f * Flicker));
		}
	}
	if(!m_vStreaks.empty())
	{
		for(const SPart &P : m_vStreaks)
		{
			const float a = std::sin(pi * P.m_Life / P.m_LifeSpan) * 0.16f * P.m_Z;
			CcFx::QuadRotated(Graphics(), P.m_Pos, P.m_Size * Px, 1.6f * Px, std::sin(m_Time + P.m_Phase) * 0.04f, Atlas.m_Glow, ColorRGBA(0.9f, 0.95f, 1.0f, a));
		}
	}
	if(!m_vStars.empty())
	{
		const ColorRGBA Star = Cfg(g_Config.m_BcAtmStarsColor);
		const float Bright = g_Config.m_BcAtmStarsBright / 100.0f;
		for(const SStar &S : m_vStars)
		{
			const float t = S.m_Life / S.m_LifeSpan;
			const float a = std::min(1.0f, t / 0.12f) * (1.0f - Smooth(0.6f, 1.0f, t)) * Bright;
			const float Len = S.m_Length * std::min(1.0f, t / 0.25f);
			const vec2 TailCenter = S.m_Pos - S.m_Dir * Len * 0.5f;
			const float Angle = angle(S.m_Dir);
			CcFx::QuadRotated(Graphics(), TailCenter, Len, 3.0f * Px, Angle, Atlas.m_Glow, Star.WithAlpha(0.7f * a));
			CcFx::QuadRotated(Graphics(), S.m_Pos - S.m_Dir * Len * 0.2f, Len * 0.4f, 1.6f * Px, Angle, Atlas.m_Glow, ColorRGBA(1.0f, 1.0f, 1.0f, 0.8f * a));
			CcFx::QuadAxis(Graphics(), S.m_Pos, 14.0f * Px, 14.0f * Px, Atlas.m_Glow, Star.WithAlpha(a));
		}
	}
	if(!m_vBolts.empty())
	{
		const ColorRGBA Bolt(0.75f, 0.85f, 1.0f, 1.0f);
		for(const SBolt &B : m_vBolts)
		{
			const float a = (1.0f - B.m_Life / 0.4f) * (0.75f + 0.25f * std::sin(B.m_Life * 70.0f));
			const auto Line = [&](const std::vector<vec2> &vPoints, float Width) {
				for(size_t i = 1; i < vPoints.size(); i++)
				{
					const vec2 a0 = vPoints[i - 1], a1 = vPoints[i];
					const vec2 d = a1 - a0;
					const float Len = length(d);
					if(Len < 0.01f)
						continue;
					const vec2 n = vec2(-d.y, d.x) / Len;
					CcFx::QuadRotated(Graphics(), (a0 + a1) * 0.5f, Len + Width, Width * 6.0f, angle(d), Atlas.m_Glow, Bolt.WithAlpha(0.25f * a));
					CcFx::Shape(Graphics(), a0 - n * Width * 0.5f, a0 + n * Width * 0.5f, a1 - n * Width * 0.5f, a1 + n * Width * 0.5f, ColorRGBA(1.0f, 1.0f, 1.0f, a));
				}
			};
			Line(B.m_vMain, 3.0f * Px);
			for(const auto &vBranch : B.m_vvBranches)
				Line(vBranch, 1.6f * Px);
		}
	}
	if(m_Flash > 0.0f)
	{
		// a double flicker, the way lightning lights up the sky
		const float Second = m_FlashAge > 0.12f ? std::max(0.0f, 1.0f - (m_FlashAge - 0.12f) / 0.25f) : 0.0f;
		const float Strength = (m_Flash * m_Flash * 0.7f + Second * 0.5f) * g_Config.m_BcAtmLightningFlash / 100.0f;
		const ColorRGBA Light(0.7f, 0.78f, 1.0f, std::min(1.0f, Strength * 0.6f));
		CcFx::Shape(Graphics(), vec2(m_View.m_X0, m_View.m_Y0), vec2(m_View.m_X1, m_View.m_Y0), vec2(m_View.m_X0, m_View.m_Y1), vec2(m_View.m_X1, m_View.m_Y1), Light);
	}
	CcFx::End(Graphics());
	(void)W;
	(void)Center;
}

// puddles and snow piles lie on the tops of blocks, in world units; droplets fly in the world
void CCcAtmosphere::DrawWeatherNormal(const CcFx::CAtlas &Atlas)
{
	const float Margin = 40.0f;
	const auto Sees = [&](float x, float y) { return x > m_View.m_X0 - Margin && x < m_View.m_X1 + Margin && y > m_View.m_Y0 - Margin && y < m_View.m_Y1 + Margin; };
	for(const CcWeather::SPuddle &P : m_Weather.m_vPuddles)
	{
		const float Top = P.m_Ty * CcWeather::TILE;
		if(!Sees(P.m_X, Top))
			continue;
		const float Width = CcWeather::CWorld::PuddleWidth(P), L = P.m_Level, A = std::min(1.0f, L * 2.0f);
		CcFx::QuadAxis(Graphics(), vec2(P.m_X, Top - 0.3f), Width * 1.16f, 3.2f + 2.6f * L, Atlas.m_Glow, ColorRGBA(0.2f, 0.29f, 0.43f, 0.85f * A));
		CcFx::QuadAxis(Graphics(), vec2(P.m_X, Top - 0.2f), Width * 0.86f, 2.0f + 1.4f * L, Atlas.m_Glow, ColorRGBA(0.15f, 0.22f, 0.34f, 0.6f * A));
	}
	if(!m_Weather.m_vPiles.empty())
	{
		const ColorRGBA Snow = Cfg(g_Config.m_BcAtmSnowColor);
		const ColorRGBA Body(Snow.r * 0.65f + 0.78f * 0.35f, Snow.g * 0.65f + 0.84f * 0.35f, Snow.b * 0.65f + 0.95f * 0.35f, 0.97f);
		const ColorRGBA Rim(std::min(1.0f, Snow.r * 1.05f + 0.05f), std::min(1.0f, Snow.g * 1.05f + 0.05f), std::min(1.0f, Snow.b * 1.05f + 0.05f), 0.9f);
		constexpr float STEP = CcWeather::TILE / (CcWeather::PILE_SAMPLES - 1);
		for(const CcWeather::SPile &P : m_Weather.m_vPiles)
		{
			const float Left = P.m_Tx * CcWeather::TILE, Top = P.m_Ty * CcWeather::TILE + 0.6f;
			if(!Sees(Left + CcWeather::TILE * 0.5f, Top))
				continue;
			for(int i = 0; i + 1 < CcWeather::PILE_SAMPLES; i++)
			{
				const float h0 = P.m_aH[i], h1 = P.m_aH[i + 1];
				if(h0 < 0.15f && h1 < 0.15f)
					continue;
				const float x0 = Left + i * STEP, x1 = x0 + STEP;
				CcFx::Shape(Graphics(), vec2(x0, Top - h0), vec2(x1, Top - h1), vec2(x0, Top + 1.2f), vec2(x1, Top + 1.2f), Body);
				const float r0 = std::min(h0, 1.1f), r1 = std::min(h1, 1.1f); // a bright rim where the light falls
				CcFx::Shape(Graphics(), vec2(x0, Top - h0), vec2(x1, Top - h1), vec2(x0, Top - h0 + r0), vec2(x1, Top - h1 + r1), Rim);
			}
		}
	}
	for(const CcWeather::SDroplet &D : m_Weather.m_vDroplets)
	{
		if(!Sees(D.m_Pos.x, D.m_Pos.y))
			continue;
		const float t = D.m_Life / D.m_LifeSpan;
		const float Speed = length(D.m_Vel);
		const float Len = D.m_Size * (1.4f + std::min(1.6f, Speed / 220.0f));
		CcFx::QuadRotated(Graphics(), D.m_Pos, Len * 1.6f, D.m_Size * 1.5f, angle(D.m_Vel), Atlas.m_Glow, ColorRGBA(0.72f, 0.8f, 0.95f, 0.9f * (1.0f - t * t)));
	}
}

// sky light on the puddles (brighter in a lightning flash), impact flashes and rings on the water
void CCcAtmosphere::DrawWeatherAdditive(const CcFx::CAtlas &Atlas)
{
	const float Margin = 40.0f;
	const auto Sees = [&](float x, float y) { return x > m_View.m_X0 - Margin && x < m_View.m_X1 + Margin && y > m_View.m_Y0 - Margin && y < m_View.m_Y1 + Margin; };
	for(const CcWeather::SPuddle &P : m_Weather.m_vPuddles)
	{
		const float Top = P.m_Ty * CcWeather::TILE;
		if(!Sees(P.m_X, Top))
			continue;
		const float Width = CcWeather::CWorld::PuddleWidth(P), L = P.m_Level;
		const float Shine = (0.22f + 0.6f * m_Flash) * L * (0.85f + 0.15f * std::sin(m_Time * 1.7f + P.m_Seed));
		CcFx::QuadAxis(Graphics(), vec2(P.m_X - Width * 0.08f, Top - 0.8f - 0.5f * L), Width * 0.62f, 1.2f, Atlas.m_Glow, ColorRGBA(0.75f, 0.85f, 1.0f, Shine));
	}
	for(const CcWeather::SCrown &C : m_Weather.m_vCrowns)
	{
		if(!Sees(C.m_Pos.x, C.m_Pos.y))
			continue;
		const float t = C.m_Life / C.m_LifeSpan;
		if(C.m_Ripple)
		{
			const float s = C.m_Size * (0.3f + 0.7f * t);
			CcFx::QuadAxis(Graphics(), C.m_Pos, s * 2.0f, s * 0.56f, Atlas.m_RingLine, ColorRGBA(0.8f, 0.9f, 1.0f, 0.3f * (1.0f - t)));
		}
		else
			CcFx::QuadAxis(Graphics(), C.m_Pos, C.m_Size * (1.2f + 1.2f * t), C.m_Size * 0.7f, Atlas.m_Glow, ColorRGBA(0.7f, 0.8f, 1.0f, 0.45f * (1.0f - t)));
	}
}

void CCcAtmosphere::OnRender()
{
	const bool Any = g_Config.m_BcAtmFireflies || g_Config.m_BcAtmStars || g_Config.m_BcAtmStorm || g_Config.m_BcAtmWind || g_Config.m_BcAtmSnow || g_Config.m_BcAtmFog || g_Config.m_BcAtmEmbers || m_Flash > 0.0f || !m_vBolts.empty() || m_ThunderIn >= 0.0f || m_Weather.HasAny();
	const bool Online = Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK;
	if(!Any || !Online || BcFxSuppressed(GameClient()) || g_Config.m_BcHeadless)
	{
		if(!m_vRain.empty() || !m_vSnow.empty() || !m_vFlies.empty() || !m_vLeaves.empty() || !m_vFog.empty() || !m_vEmbers.empty() || !m_vStars.empty() || m_RainVoice.IsValid() || m_WindVoice.IsValid() || m_Weather.HasAny())
			OnReset();
		return;
	}

	const float Now = LocalTime();
	const float Dt = std::clamp(Now - m_LastTime, 0.0f, 0.1f);
	m_LastTime = Now;
	m_Time += Dt;

	// fewer particles when the frame rate drops
	const float FrameTime = Client()->RenderFrameTime();
	const float TargetQuality = FrameTime > 1.0f / 40.0f ? 0.5f : (FrameTime > 1.0f / 90.0f ? 0.75f : 1.0f);
	m_Quality += (TargetQuality - m_Quality) * std::min(1.0f, Dt * 1.5f);

	m_View = CcFx::View(Graphics(), 0.0f);
	const vec2 Center((m_View.m_X0 + m_View.m_X1) * 0.5f, (m_View.m_Y0 + m_View.m_Y1) * 0.5f);
	vec2 CameraDelta(0.0f, 0.0f);
	const float ViewW = m_View.m_X1 - m_View.m_X0;
	if(m_HaveCenter && distance(Center, m_LastCenter) < ViewW * 2.0f)
		CameraDelta = Center - m_LastCenter;
	m_LastCenter = Center;
	m_HaveCenter = true;

	m_WindX = g_Config.m_BcAtmWind ? (0.25f + 0.75f * g_Config.m_BcAtmWindStrength / 100.0f) * (g_Config.m_BcAtmWindDir ? 1.0f : -1.0f) : 0.0f;

	Update(Dt, CameraDelta);
	UpdateSounds();
	Draw();
	Graphics()->QuadsSetRotation(0.0f);
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}
