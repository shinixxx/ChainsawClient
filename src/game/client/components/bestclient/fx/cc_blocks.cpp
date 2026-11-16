/* ChainsawClient: realistic blocks (bc_blocks, 10.11), see cc_blocks.h */
#include "cc_blocks.h"

#include <base/color.h>
#include <base/log.h>

#include <engine/cc_blocks_core.h>
#include <engine/cc_blocks_params.h>
#include <engine/client.h>
#include <engine/client/cc_blocks_cmd.h>
#include <engine/graphics.h>
#include <engine/image.h>
#include <engine/map.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>
#include <game/collision.h>
#include <game/mapitems.h>

#include <cmath>
#include <cstdio>
#include <string>

void CCcBlocks::OnShutdown()
{
	if(m_MatTexture.IsValid())
		Graphics()->UnloadTexture(&m_MatTexture);
	if(m_SoilATexture.IsValid())
		Graphics()->UnloadTexture(&m_SoilATexture);
	if(m_SoilNTexture.IsValid())
		Graphics()->UnloadTexture(&m_SoilNTexture);
	if(m_RockATexture.IsValid())
		Graphics()->UnloadTexture(&m_RockATexture);
	if(m_RockNTexture.IsValid())
		Graphics()->UnloadTexture(&m_RockNTexture);
	if(m_MossATexture.IsValid())
		Graphics()->UnloadTexture(&m_MossATexture);
	if(m_MossNTexture.IsValid())
		Graphics()->UnloadTexture(&m_MossNTexture);
	UnloadPack();
	if(m_DecoTexture.IsValid())
		Graphics()->UnloadTexture(&m_DecoTexture);
}

// build material texture: one pixel per block (material, neighbours, depth, air)
void CCcBlocks::OnMapLoad()
{
	if(m_MatTexture.IsValid())
		Graphics()->UnloadTexture(&m_MatTexture);
	m_MapWidth = m_MapHeight = 0;
	// the decor texture belongs to the old map: EnsureDeco builds a new one
	m_DecoStyle = 0;
	m_vMaterials.clear();

	const int W = Collision()->GetWidth(), H = Collision()->GetHeight();
	if(W <= 0 || H <= 0 || W > 8192 || H > 8192)
		return;

	// classify every block of the map
	std::vector<unsigned char> vMat(W * H);
	for(int i = 0; i < W * H; i++)
	{
		const int Tile = Collision()->GetTileIndex(i);
		if(Tile == TILE_SOLID)
			vMat[i] = CcBlocks::MAT_GROUND;
		else if(Tile == TILE_NOHOOK)
			vMat[i] = CcBlocks::MAT_STONE;
		else if(Tile == TILE_FREEZE)
			vMat[i] = CcBlocks::MAT_WATER;
		else
			vMat[i] = CcBlocks::MAT_AIR;
	}

	// build the material texture
	CImageInfo Image;
	Image.m_Width = W;
	Image.m_Height = H;
	Image.m_Format = CImageInfo::FORMAT_RGBA;
	Image.Allocate();
	CcBlocks::BuildMaterialTexture(vMat.data(), W, H, Image.m_pData);
	m_MatTexture = Graphics()->LoadTextureRawMove(Image, 0, "cc_blocks_mat");
	m_MapWidth = W;
	m_MapHeight = H;

	// where the water is, coarsely (see WaterInView)
	m_WaterChunksW = (W + 7) / 8;
	m_WaterChunksH = (H + 7) / 8;
	m_vWaterChunks.assign((size_t)m_WaterChunksW * m_WaterChunksH, 0);
	for(int y = 0; y < H; y++)
	{
		for(int x = 0; x < W; x++)
		{
			if(vMat[y * W + x] == CcBlocks::MAT_WATER)
				m_vWaterChunks[(size_t)(y / 8) * m_WaterChunksW + x / 8] = 1;
		}
	}

	m_vMaterials = vMat;

	// the textures are read only when the blocks are on (here, or in OnRender when turned on later): the photo ones are 24 MB
	if(g_Config.m_BcBlocks && !g_Config.m_BcHeadless)
	{
		if(g_Config.m_BcBlocksStyle == 0)
			LoadPhotoTextures();
		else
			LoadPack(g_Config.m_BcBlocksStyle);
	}
}

// bc_blocks_style -> folder in data/chainsaw/blocks/ (0 = the photo textures, no folder)
static const char *const s_apCcBlocksStyles[] = {nullptr, "pixel"};

// Water of the loaded pack (lines "water", "wstrips", "waterdeco" of pack.txt). Kept here, not in the class, for the
// same reason as the open tops of the rock below.
static IGraphics::CTextureHandle s_CcBlocksPackWaterTexture; // seamless fill of the body
static IGraphics::CTextureHandle s_CcBlocksPackWaterStripsTexture; // surface strip, under it the bed strip
static float s_CcBlocksPackWaterTopRows = 0.0f; // 0 = the pack has no water of its own
static float s_CcBlocksPackWaterTopBase = 0.0f;
static float s_CcBlocksPackWaterBedRows = 0.0f;
static int s_CcBlocksPackWaterSprFirst = 0; // the sprites that float on the water
static int s_CcBlocksPackWaterSprCount = 0;
// things that float on this share of the blocks of an open surface of the water, percent
static const int s_CcBlocksWaterPlantPercent = 22;

// Things that hang from the ceilings (vines, moss) of the loaded pack: line "hang" of pack.txt, sheet hang.png. Kept here
// for the same reason.
static IGraphics::CTextureHandle s_CcBlocksPackHangTexture;
static std::vector<unsigned char> s_vCcBlocksPackHangSizes; // what every sprite needs (CcBlocks::MeasureHangSprites); empty = the pack has none
static float s_CcBlocksPackHangCellW = 1.0f;
static float s_CcBlocksPackHangCellH = 1.0f;
static float s_CcBlocksPackHangCols = 0.0f;
static float s_CcBlocksPackHangOverlap = 0.0f;
static int s_CcBlocksPackHangFirst = 0; // their numbers in the decor texture go on after the cells of sprites.png
// a thing hangs from this share of the blocks of a ceiling at bc_blocks_vines 100, percent (they also keep apart from one another)
static const int s_CcBlocksHangPercentMax = 68;
static int s_CcBlocksDecoVines = -1; // bc_blocks_vines the decor texture was built with

void CCcBlocks::UnloadPack()
{
	if(s_CcBlocksPackWaterTexture.IsValid())
		Graphics()->UnloadTexture(&s_CcBlocksPackWaterTexture);
	if(s_CcBlocksPackWaterStripsTexture.IsValid())
		Graphics()->UnloadTexture(&s_CcBlocksPackWaterStripsTexture);
	s_CcBlocksPackWaterTopRows = 0.0f;
	s_CcBlocksPackWaterSprCount = 0;
	if(s_CcBlocksPackHangTexture.IsValid())
		Graphics()->UnloadTexture(&s_CcBlocksPackHangTexture);
	s_vCcBlocksPackHangSizes.clear();
	s_CcBlocksPackHangCols = 0.0f;
	IGraphics::CTextureHandle *apHandles[4] = {&m_PackSoilTexture, &m_PackRockTexture, &m_PackCapTexture, &m_PackSpritesTexture};
	for(IGraphics::CTextureHandle *pHandle : apHandles)
	{
		if(pHandle->IsValid())
			Graphics()->UnloadTexture(pHandle);
	}
	m_PackStyle = 0;
	m_DecoStyle = 0; // the number of plants comes from the pack
}

// plants on this share of the open tops of the earth, percent
static const int s_CcBlocksPlantPercent = 34;
// stones, crystals and plants on this share of the open tops of the rock, percent
static const int s_CcBlocksRockPlantPercent = 30;
// Open tops of the rock of the loaded pack (line "stonetop" of pack.txt). Kept here, not in the class: the component
// exists once, and its header is read by the whole client (a new member would rebuild all of it).
static float s_CcBlocksPackCapRows = 1.0f; // strips in cap.png: 1 grass; 2 + moss of the rock tops; 3 + uneven rim of the rock
static float s_CcBlocksPackSprRigid = 1000.0f; // the sprites from this number on stand still in the wind
static int s_CcBlocksPackRockFirst = 0; // the sprites that stand on the rock
static int s_CcBlocksPackRockCount = 0;

bool CCcBlocks::EnsureDeco(int Style)
{
	// how many things hang from the ceilings is a setting: the texture is built again when it changes
	const int Vines = g_Config.m_BcBlocksVines;
	if(m_DecoStyle == Style && m_DecoTexture.IsValid() && s_CcBlocksDecoVines == Vines)
		return true;
	if(m_DecoTexture.IsValid())
		Graphics()->UnloadTexture(&m_DecoTexture);
	m_DecoStyle = 0;
	if(m_MapWidth <= 0 || m_MapHeight <= 0 || m_vMaterials.size() != (size_t)m_MapWidth * m_MapHeight)
		return false;
	CImageInfo Image;
	Image.m_Width = m_MapWidth;
	Image.m_Height = m_MapHeight;
	Image.m_Format = CImageInfo::FORMAT_RGBA;
	Image.Allocate();
	CcBlocks::BuildDecoTexture(m_vMaterials.data(), m_MapWidth, m_MapHeight, (int)m_PackSprCount, s_CcBlocksPlantPercent, Image.m_pData,
		s_CcBlocksPackCapRows >= 2.0f, s_CcBlocksPackRockFirst, s_CcBlocksPackRockCount, s_CcBlocksRockPlantPercent,
		s_CcBlocksPackWaterSprFirst, s_CcBlocksPackWaterTopRows > 0.0f ? s_CcBlocksPackWaterSprCount : 0, s_CcBlocksWaterPlantPercent,
		s_CcBlocksPackHangFirst, s_vCcBlocksPackHangSizes.data(), (int)s_vCcBlocksPackHangSizes.size(), Vines * s_CcBlocksHangPercentMax / 100);
	m_DecoTexture = Graphics()->LoadTextureRawMove(Image, 0, "cc_blocks_deco");
	if(!m_DecoTexture.IsValid())
		return false;
	m_DecoStyle = Style;
	s_CcBlocksDecoVines = Vines;
	return true;
}

bool CCcBlocks::LoadPack(int Style)
{
	if(Style <= 0 || Style >= (int)(sizeof(s_apCcBlocksStyles) / sizeof(s_apCcBlocksStyles[0])))
		return false;
	if(m_PackStyle == Style)
		return true;
	if(m_PackFailedStyle == Style)
		return false;
	UnloadPack();
	const char *pName = s_apCcBlocksStyles[Style];

	// pack.txt: "ppb N", "sharp 0|1", "soil W H", "rock W H", "cap W H BASE", "sprites W H CELLW CELLH COUNT" (COUNT: the plants
	// of the earth, the first cells of the sheet), optional "stonetop STRIPS FIRST COUNT RIGID": STRIPS more strips in cap.png
	// under the grass (1 moss of the open tops of the rock, 2 + the uneven rim of the rock), FIRST COUNT the sprites that
	// stand on the rock, RIGID the first sprite the wind does not move; optional water of the pack: "water W H" (water.png,
	// the seamless body), "wstrips W H TOP BASE BED" (wstrips.png: the surface strip of TOP rows, its row BASE lies on the top
	// of the block, under it the bed strip of BED rows), "waterdeco FIRST COUNT" (the sprites that float on the water);
	// optional things that hang from the ceilings: "hang W H CELLW CELLH COUNT OVERLAP" (hang.png: COUNT cells, the top
	// OVERLAP rows of a cell lie over the block of the ceiling)
	char aPath[192];
	str_format(aPath, sizeof(aPath), "chainsaw/blocks/%s/pack.txt", pName);
	void *pData = nullptr;
	unsigned Size = 0;
	if(!Storage()->ReadFile(aPath, IStorage::TYPE_ALL, &pData, &Size) || !pData)
	{
		log_error("chainsaw", "blocks: %s not found", aPath);
		m_PackFailedStyle = Style;
		return false;
	}
	const std::string Text((const char *)pData, Size);
	free(pData);
	int Ppb = 0, Sharp = 1, CapBase = 0, SheetW = 0, SheetH = 0, CellW = 0, CellH = 0, Count = 0;
	int HangCellW = 0, HangCellH = 0, HangCount = 0, HangOverlap = 0;
	int RockStrips = 0, RockFirst = 0, RockCount = 0, Rigid = 1000;
	bool HasWater = false;
	int StripsH = 0, WaterTop = 0, WaterBase = 0, WaterBed = 0, WaterFirst = 0, WaterCount = 0;
	for(const char *pLine = Text.c_str(); *pLine;)
	{
		char aKey[32] = "";
		int a[6] = {0, 0, 0, 0, 0, 0};
		const int Got = sscanf(pLine, "%31s %d %d %d %d %d %d", aKey, &a[0], &a[1], &a[2], &a[3], &a[4], &a[5]);
		if(Got >= 2 && str_comp(aKey, "ppb") == 0)
			Ppb = a[0];
		else if(Got >= 2 && str_comp(aKey, "sharp") == 0)
			Sharp = a[0];
		else if(Got >= 4 && str_comp(aKey, "cap") == 0)
			CapBase = a[2];
		else if(Got >= 6 && str_comp(aKey, "sprites") == 0)
		{
			SheetW = a[0];
			SheetH = a[1];
			CellW = a[2];
			CellH = a[3];
			Count = a[4];
		}
		else if(Got >= 5 && str_comp(aKey, "stonetop") == 0)
		{
			RockStrips = a[0];
			RockFirst = a[1];
			RockCount = a[2];
			Rigid = a[3];
		}
		else if(Got >= 3 && str_comp(aKey, "water") == 0)
			HasWater = a[0] > 0 && a[1] > 0;
		else if(Got >= 6 && str_comp(aKey, "wstrips") == 0)
		{
			StripsH = a[1];
			WaterTop = a[2];
			WaterBase = a[3];
			WaterBed = a[4];
		}
		else if(Got >= 3 && str_comp(aKey, "waterdeco") == 0)
		{
			WaterFirst = a[0];
			WaterCount = a[1];
		}
		else if(Got >= 7 && str_comp(aKey, "hang") == 0)
		{
			HangCellW = a[2];
			HangCellH = a[3];
			HangCount = a[4];
			HangOverlap = a[5];
		}
		const char *pNext = str_find(pLine, "\n");
		if(!pNext)
			break;
		pLine = pNext + 1;
	}
	if(Ppb < 1 || Ppb > 512 || CapBase < 0 || CellW < 1 || CellH < 1 || SheetW < CellW || Count < 0)
	{
		log_error("chainsaw", "blocks: %s has wrong numbers", aPath);
		m_PackFailedStyle = Style;
		return false;
	}

	const char *const apFiles[4] = {"soil", "rock", "cap", "sprites"};
	IGraphics::CTextureHandle *apHandles[4] = {&m_PackSoilTexture, &m_PackRockTexture, &m_PackCapTexture, &m_PackSpritesTexture};
	for(int i = 0; i < 4; i++)
	{
		str_format(aPath, sizeof(aPath), "chainsaw/blocks/%s/%s.png", pName, apFiles[i]);
		*apHandles[i] = Graphics()->LoadTexture(aPath, IStorage::TYPE_ALL);
		if(!apHandles[i]->IsValid() || apHandles[i]->IsNullTexture())
		{
			// the null texture belongs to the engine: forget it, do not unload it
			apHandles[i]->Invalidate();
			log_error("chainsaw", "blocks: texture %s failed to load", aPath);
			UnloadPack();
			m_PackFailedStyle = Style;
			return false;
		}
	}
	m_PackPpb = (float)Ppb;
	m_PackSharp = Sharp != 0 ? 1 : 0;
	m_PackCapBase = (float)CapBase;
	m_PackSprCellW = (float)CellW;
	m_PackSprCellH = (float)CellH;
	m_PackSprCols = (float)(SheetW / CellW);
	m_PackSprCount = (float)Count;
	s_CcBlocksPackCapRows = RockStrips >= 2 ? 3.0f : (RockStrips == 1 ? 2.0f : 1.0f);
	s_CcBlocksPackRockFirst = RockFirst < 0 ? 0 : RockFirst;
	s_CcBlocksPackRockCount = RockCount < 0 ? 0 : RockCount;
	s_CcBlocksPackSprRigid = (float)(Rigid < 0 ? 0 : Rigid);
	// the water of the pack: without it (or with a broken one) the pack still works, the shader draws the water by its formula
	if(HasWater && WaterTop >= 2 && WaterBase >= 0 && WaterBase < WaterTop && WaterBed >= 0 && WaterTop + WaterBed <= StripsH)
	{
		str_format(aPath, sizeof(aPath), "chainsaw/blocks/%s/water.png", pName);
		s_CcBlocksPackWaterTexture = Graphics()->LoadTexture(aPath, IStorage::TYPE_ALL);
		str_format(aPath, sizeof(aPath), "chainsaw/blocks/%s/wstrips.png", pName);
		s_CcBlocksPackWaterStripsTexture = Graphics()->LoadTexture(aPath, IStorage::TYPE_ALL);
		IGraphics::CTextureHandle *apWater[2] = {&s_CcBlocksPackWaterTexture, &s_CcBlocksPackWaterStripsTexture};
		bool Loaded = true;
		for(IGraphics::CTextureHandle *pHandle : apWater)
		{
			if(!pHandle->IsValid() || pHandle->IsNullTexture())
			{
				pHandle->Invalidate(); // the null texture belongs to the engine
				Loaded = false;
			}
		}
		if(Loaded)
		{
			s_CcBlocksPackWaterTopRows = (float)WaterTop;
			s_CcBlocksPackWaterTopBase = (float)WaterBase;
			s_CcBlocksPackWaterBedRows = (float)WaterBed;
			s_CcBlocksPackWaterSprFirst = WaterFirst < 0 ? 0 : WaterFirst;
			s_CcBlocksPackWaterSprCount = WaterCount < 0 ? 0 : WaterCount;
		}
		else
		{
			log_error("chainsaw", "blocks: the water textures of %s failed to load, the water is drawn without them", pName);
			for(IGraphics::CTextureHandle *pHandle : apWater)
			{
				if(pHandle->IsValid())
					Graphics()->UnloadTexture(pHandle);
			}
		}
	}
	// the things that hang from the ceilings: without their sheet (or with a broken one) the pack works without them
	const int HangFirst = (SheetW / CellW) * (SheetH / CellH); // their numbers go on after the cells of sprites.png
	if(HangCount > 0 && HangCellW >= 1 && HangCellH >= 1 && HangOverlap >= 0 && HangOverlap < HangCellH && HangFirst >= 1)
	{
		str_format(aPath, sizeof(aPath), "chainsaw/blocks/%s/hang.png", pName);
		CImageInfo Image;
		if(Graphics()->LoadPng(Image, aPath, IStorage::TYPE_ALL) && Image.m_Format == CImageInfo::FORMAT_RGBA && Image.m_pData &&
			(int)Image.m_Width >= HangCellW && (int)Image.m_Height >= HangCellH)
		{
			const int Cols = (int)Image.m_Width / HangCellW;
			const int Cells = Cols * ((int)Image.m_Height / HangCellH);
			if(HangCount > Cells)
				HangCount = Cells;
			if(HangFirst + HangCount > 254) // the number of a sprite + 1 is one byte of the decor texture
				HangCount = 254 - HangFirst;
			if(HangCount > 0)
			{
				// the picture itself says what every thing needs: how far down it reaches, whether it is wider than a block
				s_vCcBlocksPackHangSizes.resize(HangCount);
				CcBlocks::MeasureHangSprites(Image.m_pData, (int)Image.m_Width, (int)Image.m_Height, HangCellW, HangCellH, HangCount, HangOverlap, Ppb, s_vCcBlocksPackHangSizes.data());
				s_CcBlocksPackHangTexture = Graphics()->LoadTextureRawMove(Image, 0, aPath);
				if(s_CcBlocksPackHangTexture.IsValid() && !s_CcBlocksPackHangTexture.IsNullTexture())
				{
					s_CcBlocksPackHangCellW = (float)HangCellW;
					s_CcBlocksPackHangCellH = (float)HangCellH;
					s_CcBlocksPackHangCols = (float)Cols;
					s_CcBlocksPackHangOverlap = (float)HangOverlap;
					s_CcBlocksPackHangFirst = HangFirst;
				}
				else
				{
					s_CcBlocksPackHangTexture.Invalidate(); // the null texture belongs to the engine
					s_vCcBlocksPackHangSizes.clear();
				}
			}
		}
		else
		{
			log_error("chainsaw", "blocks: %s failed to load (it has to be a PNG with transparency), nothing hangs from the ceilings", aPath);
			Image.Free();
		}
	}
	m_PackStyle = Style;
	return true;
}

// the screen (in blocks) with one block around it: the wave rises into the block over the water
bool CCcBlocks::WaterInView(float OriginX, float OriginY, float SizeX, float SizeY) const
{
	if(m_vWaterChunks.empty())
		return false;
	// outside of the map the border blocks go on, so the view is brought back onto the map
	const auto Chunk = [](float Block, int Max) {
		const int c = (int)std::floor(Block / 8.0f);
		return c < 0 ? 0 : (c > Max ? Max : c);
	};
	const int x0 = Chunk(OriginX - 1.0f, m_WaterChunksW - 1), x1 = Chunk(OriginX + SizeX + 1.0f, m_WaterChunksW - 1);
	const int y0 = Chunk(OriginY - 1.0f, m_WaterChunksH - 1), y1 = Chunk(OriginY + SizeY + 1.0f, m_WaterChunksH - 1);
	for(int y = y0; y <= y1; y++)
	{
		for(int x = x0; x <= x1; x++)
		{
			if(m_vWaterChunks[(size_t)y * m_WaterChunksW + x])
				return true;
		}
	}
	return false;
}

// a missing or broken texture file is reported once, not every frame
static bool s_CcBlocksPhotoFailed = false;

bool CCcBlocks::LoadPhotoTextures()
{
	if(s_CcBlocksPhotoFailed)
		return false;
	// photo textures: soil (Ground048), rock (Rock058), moss (Ground037) from ambientCG, CC0
	// packed as *_a.raw (1024x1024 RGBA: rgb color, a height) and *_n.raw (rg normal, b shadow)
	const char *const apNames[6] = {"soil_a", "soil_n", "rock_a", "rock_n", "moss_a", "moss_n"};
	IGraphics::CTextureHandle *apHandles[6] = {&m_SoilATexture, &m_SoilNTexture, &m_RockATexture, &m_RockNTexture, &m_MossATexture, &m_MossNTexture};
	for(int i = 0; i < 6; i++)
	{
		if(apHandles[i]->IsValid())
			continue; // already loaded
		char aPath[192];
		str_format(aPath, sizeof(aPath), "chainsaw/blocks/%s.raw", apNames[i]);
		void *pData = nullptr;
		unsigned Size = 0;
		if(!Storage()->ReadFile(aPath, IStorage::TYPE_ALL, &pData, &Size) || !pData)
		{
			log_error("chainsaw", "blocks: texture %s not found", aPath);
			s_CcBlocksPhotoFailed = true;
			return false;
		}
		if(Size != 1024u * 1024u * 4u)
		{
			// the renderer reads 1024x1024x4 bytes: a shorter file would be read past its end
			log_error("chainsaw", "blocks: texture %s has a wrong size (%u bytes)", aPath, Size);
			free(pData);
			s_CcBlocksPhotoFailed = true;
			return false;
		}
		CImageInfo Image;
		Image.m_Width = 1024;
		Image.m_Height = 1024;
		Image.m_Format = CImageInfo::FORMAT_RGBA;
		Image.m_pData = (unsigned char *)pData;
		*apHandles[i] = Graphics()->LoadTextureRaw(Image, 0, aPath);
		free(pData);
		if(!apHandles[i]->IsValid())
		{
			log_error("chainsaw", "blocks: texture %s failed to upload", aPath);
			s_CcBlocksPhotoFailed = true;
			return false;
		}
	}
	return true;
}

void CCcBlocks::OnRender()
{
	if(!g_Config.m_BcBlocks || g_Config.m_BcHeadless || !m_MatTexture.IsValid())
		return;
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	const int Style = g_Config.m_BcBlocksStyle;
	if(Style == 0)
	{
		// all six are needed (the last one is loaded last): turned on in the middle of a map they are read here
		if(!m_MossNTexture.IsValid() && !LoadPhotoTextures())
			return;
		if(!m_SoilATexture.IsValid() || !m_SoilNTexture.IsValid() || !m_RockATexture.IsValid() || !m_RockNTexture.IsValid() || !m_MossATexture.IsValid() || !m_MossNTexture.IsValid())
			return;
	}
	else if(!LoadPack(Style) || !EnsureDeco(Style)) // a style out of a texture pack; a change of the style in the middle of a map is read here
		return;

	const vec2 Center = GameClient()->m_Camera.m_Center;
	const CScreenRect View = Graphics()->MapScreenToWorld(Center.x, Center.y, 100.0f, 100.0f, 100.0f,
		0.0f, 0.0f, Graphics()->ScreenAspect(), GameClient()->m_Camera.m_Zoom);
	const vec2 Origin = View.m_TopLeft / 32.0f;
	const vec2 Size = (View.m_BottomRight - View.m_TopLeft) / 32.0f;
	if(Size.x <= 0.0f || Size.y <= 0.0f)
		return;

	SCommand_CcBlocks Cmd;
	SCcBlocksParams &P = Cmd.m_Params;
	P.m_MatTexture = m_MatTexture.Id();
	// SceneTexture is filled by the backend before the block pass (copy of the screen)
	P.m_SceneTexture = -1;
	P.m_SoilATexture = m_SoilATexture.Id();
	P.m_SoilNTexture = m_SoilNTexture.Id();
	P.m_RockATexture = m_RockATexture.Id();
	P.m_RockNTexture = m_RockNTexture.Id();
	P.m_MossATexture = m_MossATexture.Id();
	P.m_MossNTexture = m_MossNTexture.Id();
	P.m_OriginX = Origin.x;
	P.m_OriginY = Origin.y;
	P.m_SizeX = Size.x;
	P.m_SizeY = Size.y;
	P.m_Time = std::fmod(Client()->LocalTime(), 3600.0f);
	P.m_Quality = g_Config.m_BcBlocksQuality;
	P.m_Decor = g_Config.m_BcBlocksDecor;
	if(Style != 0)
	{
		P.m_Style = Style;
		P.m_PackSoilTexture = m_PackSoilTexture.Id();
		P.m_PackRockTexture = m_PackRockTexture.Id();
		P.m_PackCapTexture = m_PackCapTexture.Id();
		P.m_PackSpritesTexture = m_PackSpritesTexture.Id();
		P.m_DecoTexture = m_DecoTexture.Id();
		P.m_PackPpb = m_PackPpb;
		P.m_PackSharp = m_PackSharp;
		P.m_PackCapBase = m_PackCapBase;
		P.m_PackSprCellW = m_PackSprCellW;
		P.m_PackSprCellH = m_PackSprCellH;
		P.m_PackSprCols = m_PackSprCols;
		P.m_PackSprCount = m_PackSprCount;
		P.m_PackCapRows = s_CcBlocksPackCapRows;
		P.m_PackSprRigid = s_CcBlocksPackSprRigid;
		P.m_NeedScene = WaterInView(Origin.x, Origin.y, Size.x, Size.y) ? 1 : 0;
		P.m_PackOutline = g_Config.m_BcBlocksOutline ? 1.0f : 0.0f;
		P.m_PackWaterClear = g_Config.m_BcBlocksWaterClear / 100.0f;
		if(g_Config.m_BcBlocksVines > 0 && !s_vCcBlocksPackHangSizes.empty() && s_CcBlocksPackHangTexture.IsValid())
		{
			P.m_PackHangTexture = s_CcBlocksPackHangTexture.Id();
			P.m_PackHangCellW = s_CcBlocksPackHangCellW;
			P.m_PackHangCellH = s_CcBlocksPackHangCellH;
			P.m_PackHangCols = s_CcBlocksPackHangCols;
			P.m_PackHangFirst = (float)s_CcBlocksPackHangFirst;
			P.m_PackHangOverlap = s_CcBlocksPackHangOverlap;
		}
		if(s_CcBlocksPackWaterTopRows > 0.0f && s_CcBlocksPackWaterTexture.IsValid() && s_CcBlocksPackWaterStripsTexture.IsValid())
		{
			P.m_PackWaterTexture = s_CcBlocksPackWaterTexture.Id();
			P.m_PackWaterStripsTexture = s_CcBlocksPackWaterStripsTexture.Id();
			P.m_PackWaterTopRows = s_CcBlocksPackWaterTopRows;
			P.m_PackWaterTopBase = s_CcBlocksPackWaterTopBase;
			P.m_PackWaterBedRows = s_CcBlocksPackWaterBedRows;
			P.m_PackWaterSprFirst = s_CcBlocksPackWaterSprCount > 0 ? (float)s_CcBlocksPackWaterSprFirst : 0.0f;
		}
	}

	Graphics()->CcBlocks(P);
}
