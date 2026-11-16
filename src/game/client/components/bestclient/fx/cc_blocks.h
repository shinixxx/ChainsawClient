/* ChainsawClient: realistic blocks (bc_blocks, 10.11). The hookable/unhookable/freeze tiles of the map are drawn
   with photo textures and procedural details: earth with grass, rock with moss, water with caustics and refraction.
   The drawing itself is done by the OpenGL 3.3 renderer (engine/cc_blocks_params.h); this component only builds
   the material texture (one pixel per block: material, neighbours, depth, air). */
#ifndef GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_CC_BLOCKS_H
#define GAME_CLIENT_COMPONENTS_BESTCLIENT_FX_CC_BLOCKS_H

#include <base/vmath.h>

#include <engine/graphics.h>

#include <game/client/component.h>

#include <vector>

class CCcBlocks : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnMapLoad() override;
	void OnShutdown() override;
	void OnRender() override;

private:
	IGraphics::CTextureHandle m_MatTexture;
	int m_MapWidth = 0;
	int m_MapHeight = 0;
	IGraphics::CTextureHandle m_SoilATexture;
	IGraphics::CTextureHandle m_SoilNTexture;
	IGraphics::CTextureHandle m_RockATexture;
	IGraphics::CTextureHandle m_RockNTexture;
	IGraphics::CTextureHandle m_MossATexture;
	IGraphics::CTextureHandle m_MossNTexture;

	bool LoadPhotoTextures();

	// styles out of a texture pack (bc_blocks_style 1..): data/chainsaw/blocks/<name>/ soil.png, rock.png, cap.png, sprites.png, pack.txt
	IGraphics::CTextureHandle m_PackSoilTexture;
	IGraphics::CTextureHandle m_PackRockTexture;
	IGraphics::CTextureHandle m_PackCapTexture;
	IGraphics::CTextureHandle m_PackSpritesTexture;
	int m_PackStyle = 0; // the style the four textures above belong to (0 = none loaded)
	int m_PackFailedStyle = 0; // a style that failed to load is reported once, not every frame
	float m_PackPpb = 16.0f;
	int m_PackSharp = 1;
	float m_PackCapBase = 0.0f;
	float m_PackSprCellW = 1.0f;
	float m_PackSprCellH = 1.0f;
	float m_PackSprCols = 1.0f;
	float m_PackSprCount = 0.0f;
	bool LoadPack(int Style);
	void UnloadPack();

	// the decor texture of the pack styles (one pixel per block: grass strip and plants over it, see cc_blocks_core.h):
	// made out of the materials of the map and the number of plants of the pack, so it is rebuilt when either changes
	std::vector<unsigned char> m_vMaterials; // the materials of the map, kept for it
	IGraphics::CTextureHandle m_DecoTexture;
	int m_DecoStyle = 0; // the style the decor texture was built for (0 = none)
	bool EnsureDeco(int Style);

	// where the water is, one byte per 8x8 blocks: the copy of the screen is needed only with water in sight
	std::vector<unsigned char> m_vWaterChunks;
	int m_WaterChunksW = 0;
	int m_WaterChunksH = 0;
	bool WaterInView(float OriginX, float OriginY, float SizeX, float SizeY) const;
};

#endif
