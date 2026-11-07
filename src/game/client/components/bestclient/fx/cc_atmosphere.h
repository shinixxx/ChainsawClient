/* ChainsawClient: atmosphere around the tee - fireflies, shooting stars, a thunderstorm, wind with leaves, snow,
   fog and rising embers. Particles live in a box around the camera view; far ones (small depth) follow the camera
   more, which gives parallax. Drawn after the foreground of the map in two batches (normal, additive). */
#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_CC_ATMOSPHERE_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_CC_ATMOSPHERE_H

#include "cc_fx_common.h"
#include "cc_weather_core.h"

#include <engine/sound.h>

#include <game/client/component.h>

#include <vector>

class CCcAtmosphere : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnReset() override;
	void OnShutdown() override;
	void OnRender() override;

	void TestLightning(); // menu: strike now

private:
	struct SPart
	{
		vec2 m_Pos;
		vec2 m_Vel;
		float m_Z; // depth 0..1, 1 = in the world, smaller = further away (follows the camera more)
		float m_Life;
		float m_LifeSpan;
		float m_Size;
		float m_Rot;
		float m_RotSpeed;
		float m_Phase;
		ColorRGBA m_Color;
		bool m_World = false; // physical rain / snow: flies in the world and hits the blocks
	};
	struct SStar
	{
		vec2 m_Pos;
		vec2 m_Dir;
		float m_Speed;
		float m_Length;
		float m_Life;
		float m_LifeSpan;
	};
	struct SBolt
	{
		std::vector<vec2> m_vMain;
		std::vector<std::vector<vec2>> m_vvBranches;
		float m_Life = 0.0f;
	};

	void Resize(std::vector<SPart> &vParts, int Target, int Kind);
	void Spawn(SPart &Part, int Kind, bool Anywhere);
	void Wrap(SPart &Part, float Margin);
	void Update(float Dt, vec2 CameraDelta);
	void Draw();
	void Strike();
	void UpdateSounds();
	void DrawWeatherNormal(const CcFx::CAtlas &Atlas);
	void DrawWeatherAdditive(const CcFx::CAtlas &Atlas);
	static bool SolidTile(void *pUser, int Tx, int Ty);
	void MakeWorldly(SPart &Part, bool Physics);
	void StopSounds();

	CcFx::CView m_View;
	vec2 m_LastCenter = vec2(0.0f, 0.0f);
	bool m_HaveCenter = false;
	float m_Time = 0.0f;
	float m_LastTime = 0.0f;
	float m_Quality = 1.0f;
	float m_WindX = 0.0f;

	std::vector<SPart> m_vRain;
	std::vector<SPart> m_vSnow;
	std::vector<SPart> m_vFlies;
	std::vector<SPart> m_vLeaves;
	std::vector<SPart> m_vStreaks;
	std::vector<SPart> m_vFog;
	std::vector<SPart> m_vEmbers;
	std::vector<SStar> m_vStars;
	std::vector<SBolt> m_vBolts;
	CcWeather::CWorld m_Weather; // puddles, snow piles, splashes

	float m_StarTimer = 1.0f;
	float m_BoltTimer = 5.0f;
	float m_Flash = 0.0f;
	float m_FlashAge = 10.0f;
	float m_ThunderIn = -1.0f;
	float m_ThunderVolume = 1.0f;

	int m_RainSample = -1;
	int m_WindSample = -1;
	int m_ThunderSample = -1;
	ISound::CVoiceHandle m_RainVoice;
	ISound::CVoiceHandle m_WindVoice;
};

#endif
