/* ChainsawClient: realistic blocks parameters (ground/stone/water), sent to the renderer once per frame.
   The hookable/unhookable/freeze tiles of the map are drawn with photo textures and procedural details:
   earth with grass, rock with moss, water with caustics. Positions are in blocks. */
#ifndef ENGINE_CC_BLOCKS_PARAMS_H
#define ENGINE_CC_BLOCKS_PARAMS_H

struct SCcBlocksParams
{
	int m_MatTexture = -1; // material texture: one pixel per block, RGBA = material, neighbours, depth, air (see cc_blocks_core.h)
	int m_SceneTexture = -1; // what was drawn before the blocks (for water refraction)
	int m_SoilATexture = -1; // soil: rgb color, a height
	int m_SoilNTexture = -1; // soil: rg normal, b shadow
	int m_RockATexture = -1;
	int m_RockNTexture = -1;
	int m_MossATexture = -1;
	int m_MossNTexture = -1;
	float m_OriginX = 0.0f; // top left corner of the screen, in blocks
	float m_OriginY = 0.0f;
	float m_SizeX = 1.0f; // size of the screen in blocks
	float m_SizeY = 1.0f;
	float m_Time = 0.0f;
	int m_Quality = 2; // 0 low .. 3 ultra: details of grass/moss/water
	int m_Decor = 1; // grass blades and flowers above the blocks

	// styles out of a texture pack (data/chainsaw/blocks/<style>/): the pictures give the look, one shader for all of them
	int m_Style = 0; // 0 = photo textures (the fields above), 1.. = a pack (the fields below)
	int m_PackSoilTexture = -1; // seamless fill of the earth
	int m_PackRockTexture = -1; // seamless fill of the rock
	int m_PackCapTexture = -1; // strip of grass along the open tops, repeats horizontally
	int m_PackSpritesTexture = -1; // sheet of plants, one per cell
	int m_DecoTexture = -1; // per block: which grass and which plants lie over it (BuildDecoTexture in cc_blocks_core.h)
	float m_PackPpb = 16.0f; // texels of the pack per block
	int m_PackSharp = 1; // 1 = pixel art (crisp texels), 0 = painted (smooth)
	float m_PackCapBase = 0.0f; // row of the cap that lies on the top of the block
	float m_PackSprCellW = 1.0f; // cell of the sheet, texels
	float m_PackSprCellH = 1.0f;
	float m_PackSprCols = 1.0f; // cells across the sheet
	float m_PackSprCount = 0.0f;
	float m_PackCapRows = 1.0f; // strips in the cap texture: 1 grass; 2 + moss of the open tops of the rock; 3 + uneven rim of the rock
	float m_PackSprRigid = 1000.0f; // the sprites from this number on stand still in the wind
	int m_NeedScene = 1; // water on the screen: the copy of the screen is made only then
	// water of the pack (optional; without it the shader draws the water by a formula)
	int m_PackWaterTexture = -1; // seamless fill of the body of the water
	int m_PackWaterStripsTexture = -1; // two strips that repeat horizontally: the surface, under it the bed (pebbles, water plants)
	float m_PackWaterTopRows = 0.0f; // rows of the surface strip (0 = the pack has no water)
	float m_PackWaterTopBase = 0.0f; // row of the surface strip that lies on the top of the block
	float m_PackWaterBedRows = 0.0f; // rows of the bed strip
	float m_PackWaterSprFirst = 0.0f; // the sprites from this number on float on the water (0 = none)
	// things that hang from the ceilings (optional sheet hang.png of the pack)
	int m_PackHangTexture = -1;
	float m_PackHangCellW = 1.0f; // cell of the sheet, texels
	float m_PackHangCellH = 1.0f;
	float m_PackHangCols = 0.0f; // cells across the sheet (0 = the pack has none, or they are turned off)
	float m_PackHangFirst = 0.0f; // the number of the first of them in the decor texture
	float m_PackHangOverlap = 0.0f; // rows of a cell that lie over the block of the ceiling
	float m_PackOutline = 1.0f; // 1 = dark outline around the earth and the rock, 0 = none
	float m_PackWaterClear = 0.5f; // 0..1: how well the things behind the water show through it
};

#endif
