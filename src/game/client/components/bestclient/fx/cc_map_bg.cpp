/* ChainsawClient: own map background, see cc_map_bg.h. */
#include "cc_map_bg.h"

#include <game/client/components/bestclient/cc_image_wic.h>

#include <base/str.h>

#include <engine/client.h>
#include <engine/image.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>
#include <game/layers.h>

#include <algorithm>
#include <cmath>

static bool PictureName(const char *pName)
{
	const char *apExt[] = {".png", ".jpg", ".jpeg", ".webp", ".bmp", ".gif", ".heic", ".avif", ".tif", ".tiff"};
	for(const char *pExt : apExt)
		if(str_endswith_nocase(pName, pExt))
			return true;
	return false;
}

void CCcMapBg::OnInit()
{
	Storage()->CreateFolder("chainsaw", IStorage::TYPE_SAVE);
	Storage()->CreateFolder(FOLDER, IStorage::TYPE_SAVE);
	Rescan();
}

void CCcMapBg::OnShutdown()
{
	if(m_Texture.IsValid())
		Graphics()->UnloadTexture(&m_Texture);
}

void CCcMapBg::Rescan()
{
	m_vFiles.clear();
	Storage()->ListDirectory(
		IStorage::TYPE_ALL, FOLDER, [](const char *pName, int IsDir, int, void *pUser) {
			if(!IsDir && PictureName(pName))
				static_cast<std::vector<std::string> *>(pUser)->emplace_back(pName);
			return 0;
		},
		&m_vFiles);
	std::sort(m_vFiles.begin(), m_vFiles.end(), [](const std::string &a, const std::string &b) { return str_comp_nocase(a.c_str(), b.c_str()) < 0; });
	m_vFiles.erase(std::unique(m_vFiles.begin(), m_vFiles.end()), m_vFiles.end());
}

void CCcMapBg::Select(int Index)
{
	if(Index < 0 || Index >= (int)m_vFiles.size())
		return;
	str_copy(g_Config.m_BcMapBgFile, m_vFiles[Index].c_str());
	g_Config.m_BcMapBg = 1;
}

void CCcMapBg::Step(int Dir)
{
	Rescan();
	if(m_vFiles.empty())
		return;
	int At = -1;
	for(int i = 0; i < (int)m_vFiles.size(); i++)
		if(str_comp(m_vFiles[i].c_str(), g_Config.m_BcMapBgFile) == 0)
			At = i;
	const int Num = (int)m_vFiles.size();
	Select(At < 0 ? 0 : ((At + Dir) % Num + Num) % Num);
}

void CCcMapBg::PickFile()
{
	char aFolder[IO_MAX_PATH_LENGTH];
	Storage()->GetCompletePath(IStorage::TYPE_SAVE, FOLDER, aFolder, sizeof(aFolder));
	char aName[128];
	if(!CcImage::PickImageAndCopy(aFolder, aName, sizeof(aName)))
		return;
	Rescan();
	str_copy(g_Config.m_BcMapBgFile, aName);
	g_Config.m_BcMapBg = 1;
}

void CCcMapBg::OpenFolder()
{
	char aPath[IO_MAX_PATH_LENGTH];
	Storage()->GetCompletePath(IStorage::TYPE_SAVE, FOLDER, aPath, sizeof(aPath));
	Client()->ViewFile(aPath);
}

void CCcMapBg::Load()
{
	str_copy(m_aLoaded, g_Config.m_BcMapBgFile);
	m_Failed = true;
	if(m_Texture.IsValid())
		Graphics()->UnloadTexture(&m_Texture);
	if(!m_aLoaded[0])
	{
		str_copy(m_aStatus, "Картинка не выбрана");
		return;
	}
	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "%s/%s", FOLDER, m_aLoaded);
	void *pData = nullptr;
	unsigned DataSize = 0;
	if(!Storage()->ReadFile(aPath, IStorage::TYPE_ALL, &pData, &DataSize))
	{
		str_format(m_aStatus, sizeof(m_aStatus), "Нет файла «%s» в папке", m_aLoaded);
		return;
	}
	std::vector<uint8_t> vPixels;
	int W = 0, H = 0;
	char aWhy[128] = "";
	const bool Ok = CcImage::DecodeWic(pData, DataSize, 1920, 1080, vPixels, W, H, aWhy, sizeof(aWhy));
	free(pData);
	if(!Ok || W <= 0 || H <= 0)
	{
		str_format(m_aStatus, sizeof(m_aStatus), "Не открылась: %s", aWhy[0] ? aWhy : "неизвестный формат");
		return;
	}
	CImageInfo Image;
	Image.m_Width = W;
	Image.m_Height = H;
	Image.m_Format = CImageInfo::FORMAT_RGBA;
	Image.Allocate();
	mem_copy(Image.m_pData, vPixels.data(), (size_t)W * H * 4);
	m_Texture = Graphics()->LoadTextureRawMove(Image, 0, "cc_map_bg");
	m_Width = W;
	m_Height = H;
	m_Failed = !m_Texture.IsValid();
	str_format(m_aStatus, sizeof(m_aStatus), m_Failed ? "Не загрузилась" : "Фон: %s (%dx%d)", m_aLoaded, W, H);
}

bool CCcMapBg::Replaces()
{
	if(!g_Config.m_BcMapBg)
		return false;
	if(str_comp(m_aLoaded, g_Config.m_BcMapBgFile) != 0)
		Load();
	return !m_Failed && m_Texture.IsValid();
}

void CCcMapBg::Draw()
{
	// the whole screen, cover-fit, shifted a little with the camera across the map (parallax)
	const CScreenRect Old = Graphics()->GetScreen();
	Graphics()->MapScreenToSize(1.0f, 1.0f);
	const float ScreenAspect = Graphics()->ScreenAspect();
	const float ImageAspect = (float)m_Width / std::max(1, m_Height);
	float UW = 1.0f, VH = 1.0f;
	if(ImageAspect > ScreenAspect)
		UW = ScreenAspect / ImageAspect;
	else
		VH = ImageAspect / ScreenAspect;
	// a little zoom leaves room to move
	const float Parallax = g_Config.m_BcMapBgParallax / 100.0f;
	const float Room = 0.18f * Parallax;
	UW *= 1.0f - Room;
	VH *= 1.0f - Room;
	float FX = 0.5f, FY = 0.5f;
	const CLayers *pLayers = GameClient()->Layers();
	if(Parallax > 0.0f && pLayers && pLayers->GameLayer())
	{
		const vec2 Center = GameClient()->m_Camera.m_Center;
		FX = std::clamp(Center.x / std::max(1.0f, pLayers->GameLayer()->m_Width * 32.0f), 0.0f, 1.0f);
		FY = std::clamp(Center.y / std::max(1.0f, pLayers->GameLayer()->m_Height * 32.0f), 0.0f, 1.0f);
	}
	const float U0 = (1.0f - UW) * FX, V0 = (1.0f - VH) * FY;
	const float Bright = g_Config.m_BcMapBgBrightness / 100.0f;
	Graphics()->TextureSet(m_Texture);
	Graphics()->BlendNormal();
	Graphics()->QuadsBegin();
	Graphics()->QuadsSetSubset(U0, V0, U0 + UW, V0 + VH);
	Graphics()->SetColor(Bright, Bright, Bright, 1.0f);
	IGraphics::CQuadItem Quad(0.0f, 0.0f, 1.0f, 1.0f);
	Graphics()->QuadsDrawTL(&Quad, 1);
	Graphics()->QuadsEnd();
	Graphics()->MapScreen(Old);
}
