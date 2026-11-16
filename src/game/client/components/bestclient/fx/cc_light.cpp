/* ChainsawClient: light and shadows over the map, see cc_light.h */
#include "cc_light.h"

#include "own_tee.h"

#include <base/color.h>
#include <base/math.h>
#include <base/io.h>
#include <base/str.h>

#include <engine/cc_light_params.h>
#include <engine/client.h>
#include <engine/image.h>
#include <engine/map.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>
#include <game/collision.h>
#include <game/mapitems.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

void CCcLight::OnConsoleInit()
{
	Console()->Register("cc_light_place", "", CFGFLAG_CLIENT, ConPlace, this, "Hang an own lamp at the cursor (light and shadows)");
	Console()->Register("cc_light_remove", "", CFGFLAG_CLIENT, ConRemove, this, "Take down the own lamp closest to you");
	Console()->Register("cc_light_clear", "", CFGFLAG_CLIENT, ConClear, this, "Take down all own lamps of this map");
}

void CCcLight::ConPlace(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CCcLight *>(pUserData)->PlaceLamp(true);
}

void CCcLight::ConRemove(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CCcLight *>(pUserData)->RemoveNearest();
}

void CCcLight::ConClear(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CCcLight *>(pUserData)->ClearLamps();
}

void CCcLight::OnShutdown()
{
	if(m_MapTexture.IsValid())
		Graphics()->UnloadTexture(&m_MapTexture);
}

// one pixel per block of the map: red = the block stops the light (hookable and unhookable blocks)
void CCcLight::OnMapLoad()
{
	if(m_MapTexture.IsValid())
		Graphics()->UnloadTexture(&m_MapTexture);
	m_MapWidth = m_MapHeight = 0;
	m_vOwn.clear();

	const int W = Collision()->GetWidth(), H = Collision()->GetHeight();
	if(W <= 0 || H <= 0 || W > 8192 || H > 8192)
		return;
	CImageInfo Image;
	Image.m_Width = W;
	Image.m_Height = H;
	Image.m_Format = CImageInfo::FORMAT_RGBA;
	Image.Allocate();
	for(int i = 0; i < W * H; i++)
	{
		const int Tile = Collision()->GetTileIndex(i);
		const bool Solid = Tile == TILE_SOLID || Tile == TILE_NOHOOK;
		Image.m_pData[i * 4 + 0] = Solid ? 255 : 0;
		Image.m_pData[i * 4 + 1] = 0;
		Image.m_pData[i * 4 + 2] = 0;
		Image.m_pData[i * 4 + 3] = 255;
	}
	m_MapTexture = Graphics()->LoadTextureRawMove(Image, 0, "cc_light_map");
	m_MapWidth = W;
	m_MapHeight = H;
	LoadLamps();
}

vec2 CCcLight::OwnPos() const
{
	const int Local = GameClient()->m_Snap.m_LocalClientId;
	if(Local >= 0 && Local < MAX_CLIENTS && GameClient()->m_Snap.m_aCharacters[Local].m_Active && !GameClient()->m_Snap.m_SpecInfo.m_Active)
		return GameClient()->m_aClients[Local].m_RenderPos;
	return GameClient()->m_Camera.m_Center;
}

void CCcLight::LampFile(char *pBuf, int BufSize) const
{
	// the name of the map without anything a file name can not have
	char aName[96];
	str_copy(aName, GameClient()->Map()->BaseName());
	for(char *p = aName; *p; p++)
	{
		const bool Ok = (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' || *p == '.' || *p == ' ' || (unsigned char)*p >= 128;
		if(!Ok)
			*p = '_';
	}
	str_format(pBuf, BufSize, "%s/%s.txt", FOLDER, aName);
}

void CCcLight::LoadLamps()
{
	m_vOwn.clear();
	char aPath[192];
	LampFile(aPath, sizeof(aPath));
	char *pText = Storage()->ReadFileStr(aPath, IStorage::TYPE_SAVE);
	if(!pText)
		return;
	const char *p = pText;
	while(*p && m_vOwn.size() < 256)
	{
		char *pEnd = nullptr;
		const float x = std::strtof(p, &pEnd);
		if(pEnd == p)
			break;
		p = pEnd;
		const float y = std::strtof(p, &pEnd);
		if(pEnd == p)
			break;
		p = pEnd;
		if(std::isfinite(x) && std::isfinite(y))
			m_vOwn.emplace_back(x, y);
	}
	free(pText);
}

void CCcLight::SaveLamps()
{
	Storage()->CreateFolder("chainsaw", IStorage::TYPE_SAVE);
	Storage()->CreateFolder(FOLDER, IStorage::TYPE_SAVE);
	char aPath[192];
	LampFile(aPath, sizeof(aPath));
	IOHANDLE File = Storage()->OpenFile(aPath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return;
	std::string Text;
	for(const vec2 &Pos : m_vOwn)
	{
		char aLine[64];
		std::snprintf(aLine, sizeof(aLine), "%.2f %.2f\n", Pos.x, Pos.y);
		Text += aLine;
	}
	io_write(File, Text.c_str(), Text.size());
	io_close(File);
}

void CCcLight::PlaceLamp(bool AtCursor)
{
	if(m_MapWidth <= 0 || (Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK))
		return;
	if(m_vOwn.size() >= 256)
	{
		GameClient()->Echo("ChainsawClient: too many lamps on this map (256)");
		return;
	}
	vec2 Pos = OwnPos();
	if(AtCursor && Client()->State() == IClient::STATE_ONLINE)
		Pos = GameClient()->m_Controls.m_aTargetPos[g_Config.m_ClDummy];
	m_vOwn.emplace_back(Pos.x / 32.0f, Pos.y / 32.0f);
	SaveLamps();
	if(!g_Config.m_BcLight)
		GameClient()->Echo("ChainsawClient: the lamp is hung, turn on \"Light and shadows\" to see it");
}

void CCcLight::RemoveNearest()
{
	if(m_vOwn.empty())
		return;
	const vec2 Pos = OwnPos() / 32.0f;
	size_t Best = 0;
	for(size_t i = 1; i < m_vOwn.size(); i++)
	{
		if(distance(m_vOwn[i], Pos) < distance(m_vOwn[Best], Pos))
			Best = i;
	}
	m_vOwn.erase(m_vOwn.begin() + Best);
	SaveLamps();
}

void CCcLight::ClearLamps()
{
	if(m_vOwn.empty())
		return;
	m_vOwn.clear();
	SaveLamps();
}

void CCcLight::OnRender()
{
	if(!g_Config.m_BcLight || g_Config.m_BcHeadless || !m_MapTexture.IsValid())
		return;
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(BcFxSuppressed(GameClient()))
		return;

	const vec2 Center = GameClient()->m_Camera.m_Center;
	const CScreenRect View = Graphics()->MapScreenToWorld(Center.x, Center.y, 100.0f, 100.0f, 100.0f,
		0.0f, 0.0f, Graphics()->ScreenAspect(), GameClient()->m_Camera.m_Zoom);
	const vec2 Origin = View.m_TopLeft / 32.0f;
	const vec2 Size = (View.m_BottomRight - View.m_TopLeft) / 32.0f;
	if(Size.x <= 0.0f || Size.y <= 0.0f)
		return;

	SCcLightParams Params;
	Params.m_MapTexture = m_MapTexture.Id();
	Params.m_OriginX = Origin.x;
	Params.m_OriginY = Origin.y;
	Params.m_SizeX = Size.x;
	Params.m_SizeY = Size.y;
	Params.m_Time = std::fmod(Client()->LocalTime(), 3600.0f);
	Params.m_Quality = g_Config.m_BcLightQuality;
	// zoomed far out a pixel of the light picture is a big part of a block and there are many lamps: go easier
	if(Size.x > 110.0f && Params.m_Quality > 1)
		Params.m_Quality = 1;

	// what is left without any lamp: from untouched (0) to a night with a little blue in it (100)
	const float Dark = g_Config.m_BcLightDark / 100.0f;
	const float Keep = std::pow(1.0f - Dark, 2.5f);
	const float aNight[3] = {0.030f, 0.034f, 0.058f};
	for(int i = 0; i < 3; i++)
		Params.m_aAmbient[i] = aNight[i] + (1.0f - aNight[i]) * Keep;
	Params.m_Haze = g_Config.m_BcLightHaze / 100.0f;
	Params.m_Dust = g_Config.m_BcLightDust / 100.0f;
	Params.m_Vignette = g_Config.m_BcLightVignette / 100.0f;

	const float Power = g_Config.m_BcLightPower / 100.0f;
	if(g_Config.m_BcLightSun)
	{
		const float Angle = g_Config.m_BcLightSunAngle * pi / 180.0f;
		const ColorRGBA Col = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_BcLightSunColor));
		const float Strength = 2.2f * Power * g_Config.m_BcLightSunPower / 100.0f;
		Params.m_Sun = true;
		Params.m_aSunDir[0] = std::sin(Angle);
		Params.m_aSunDir[1] = -std::cos(Angle);
		Params.m_SunReach = (float)g_Config.m_BcLightSunReach;
		Params.m_SunSoft = 1.0f;
		Params.m_aSunColor[0] = Col.r * Col.r * Strength;
		Params.m_aSunColor[1] = Col.g * Col.g * Strength;
		Params.m_aSunColor[2] = Col.b * Col.b * Strength;
	}

	// everything that could light the screen, the closest to its middle first
	struct SCandidate
	{
		SCcLamp m_Lamp;
		float m_Dist;
	};
	std::vector<SCandidate> vCandidates;
	const vec2 Middle = Origin + Size * 0.5f;
	const auto Add = [&](vec2 Pos, float Range, float LampSize, ColorRGBA Col, float Strength) {
		// lamps whose light does not reach the screen are left out
		if(Pos.x + Range < Origin.x || Pos.x - Range > Origin.x + Size.x || Pos.y + Range < Origin.y || Pos.y - Range > Origin.y + Size.y)
			return;
		SCandidate Candidate;
		Candidate.m_Lamp.m_X = Pos.x;
		Candidate.m_Lamp.m_Y = Pos.y;
		Candidate.m_Lamp.m_Range = Range;
		Candidate.m_Lamp.m_Size = LampSize;
		Candidate.m_Lamp.m_aColor[0] = Col.r * Col.r * Strength;
		Candidate.m_Lamp.m_aColor[1] = Col.g * Col.g * Strength;
		Candidate.m_Lamp.m_aColor[2] = Col.b * Col.b * Strength;
		Candidate.m_Dist = distance(Pos, Middle);
		vCandidates.push_back(Candidate);
	};

	if(g_Config.m_BcLightLamps)
	{
		const ColorRGBA Col = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_BcLightLampColor));
		for(const vec2 &Pos : m_vOwn)
			Add(Pos, (float)g_Config.m_BcLightLampRange, 0.9f, Col, 3.0f * Power);
	}
	if(g_Config.m_BcLightTee)
	{
		const ColorRGBA Own = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_BcLightTeeColor));
		const int Local = GameClient()->m_Snap.m_LocalClientId;
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(!GameClient()->m_Snap.m_aCharacters[i].m_Active || !GameClient()->m_aClients[i].m_Active)
				continue;
			const bool Mine = BcIsOwnTee(GameClient(), i) || i == Local;
			if(!Mine && (!g_Config.m_BcLightTeeOthers || (GameClient()->IsOtherTeam(i) && !g_Config.m_ClShowOthers)))
				continue;
			ColorRGBA Col = Own;
			if(g_Config.m_BcLightTeeSkin)
			{
				// the colour of the tee, pulled towards white so a dark tee still gives light
				const ColorRGBA Body = GameClient()->m_aClients[i].m_RenderInfo.m_ColorBody;
				const float Max = std::max({Body.r, Body.g, Body.b, 0.001f});
				Col = ColorRGBA(mix(1.0f, Body.r / Max, 0.75f), mix(1.0f, Body.g / Max, 0.75f), mix(1.0f, Body.b / Max, 0.75f), 1.0f);
			}
			const vec2 Pos = GameClient()->m_aClients[i].m_RenderPos / 32.0f;
			Add(Pos, (float)g_Config.m_BcLightTeeRange * (Mine ? 1.0f : 0.75f), 0.55f, Col, (Mine ? 2.0f : 1.4f) * Power);
		}
	}
	std::sort(vCandidates.begin(), vCandidates.end(), [](const SCandidate &a, const SCandidate &b) { return a.m_Dist < b.m_Dist; });
	static const int s_aMaxLamps[4] = {4, 6, 10, 16};
	const int Max = s_aMaxLamps[std::clamp(Params.m_Quality, 0, 3)];
	Params.m_NumLamps = std::min((int)vCandidates.size(), Max);
	for(int i = 0; i < Params.m_NumLamps; i++)
		Params.m_aLamps[i] = vCandidates[i].m_Lamp;

	Graphics()->CcLight(Params);
}
