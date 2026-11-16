/* ChainsawClient: realistic blocks, the part without the engine (the preview stand uses it too).
   Every block of the map gets 4 bytes the shader reads:
   R  material (MAT_*)
   G  neighbours of the same material: 1 up, 2 right, 4 down, 8 left, 16 up-right, 32 down-right, 64 down-left, 128 up-left
   B  ground: blocks below the grass (0 = the block with the grass); water: blocks below the surface;
      air: rock next to the block: 1 up, 2 right, 4 down, 8 left (its uneven rim sticks out into this block),
      16 a thing that hangs from a ceiling reaches the block
   A  air next to the block: 1 up, 2 right, 4 down, 8 left; 16 grass reaches the block,
      32 air: air over water; earth and rock: a thing that hangs from this ceiling or the one beside it lies over the block,
      64 moss or a thing of an open top of the rock reaches the block (see below),
      128 a thing that floats on an open surface of the water reaches the block */
#ifndef ENGINE_CC_BLOCKS_CORE_H
#define ENGINE_CC_BLOCKS_CORE_H

namespace CcBlocks
{
enum
{
	MAT_AIR = 0,
	MAT_GROUND, // hookable: earth with grass
	MAT_STONE, // unhookable: rock
	MAT_WATER, // freeze: water
	NUM_MATS
};

enum
{
	MAX_HANG = 4, // blocks a thing that hangs from a ceiling reaches down at most
	HANG_WIDE = 16, // bit of the size of a hanging sprite (MeasureHangSprites): it is wider than one block
};

// pMat: W*H materials, pOut: W*H*4 bytes. Outside of the map the border blocks go on (like the game does).
inline void BuildMaterialTexture(const unsigned char *pMat, int W, int H, unsigned char *pOut)
{
	const auto At = [&](int x, int y) -> int {
		x = x < 0 ? 0 : (x >= W ? W - 1 : x);
		y = y < 0 ? 0 : (y >= H ? H - 1 : y);
		return pMat[y * W + x];
	};
	static const int s_aDx[8] = {0, 1, 0, -1, 1, 1, -1, -1};
	static const int s_aDy[8] = {-1, 0, 1, 0, -1, 1, 1, -1};
	for(int y = 0; y < H; y++)
	{
		for(int x = 0; x < W; x++)
		{
			const int Mat = At(x, y);
			unsigned char *pPix = &pOut[(y * W + x) * 4];
			int Same = 0, Air = 0, Rock = 0;
			for(int i = 0; i < 8; i++)
			{
				const int Other = At(x + s_aDx[i], y + s_aDy[i]);
				if(Other == Mat)
					Same |= 1 << i;
				if(i < 4 && Other == MAT_AIR)
					Air |= 1 << i;
				if(i < 4 && Other == MAT_STONE)
					Rock |= 1 << i;
			}
			int Depth = 0;
			if(Mat == MAT_GROUND || Mat == MAT_WATER)
			{
				// columns are filled top to bottom, so the block above is ready
				const int Above = y > 0 ? pMat[(y - 1) * W + x] : Mat;
				if(Above == Mat)
				{
					Depth = y > 0 ? pOut[((y - 1) * W + x) * 4 + 2] + 1 : 4;
					if(Depth > 255)
						Depth = 255;
				}
				else if(Mat == MAT_GROUND && Above != MAT_AIR)
					Depth = 3; // earth under rock or water: no grass, already deep
			}
			pPix[0] = (unsigned char)Mat;
			pPix[1] = (unsigned char)Same;
			pPix[2] = (unsigned char)(Mat == MAT_AIR ? Rock : Depth);
			pPix[3] = (unsigned char)Air;
		}
	}
	// what may be drawn over a block by its neighbours (the pack shader looks around only where one of these is set):
	// A 16: grass or a plant of an open top of the earth reaches this block (the top is here, beside, or up to two blocks below)
	// A 32: air right over an open surface of the water (the wave rises into it)
	// A 64: the same as 16 for an open top of the rock (moss, stones, crystals: only the packs that have them draw them)
	const auto GrassTop = [&](int x, int y) {
		return At(x, y) == MAT_GROUND && At(x, y - 1) == MAT_AIR;
	};
	const auto RockTop = [&](int x, int y) {
		return At(x, y) == MAT_STONE && At(x, y - 1) == MAT_AIR;
	};
	// a ceiling a thing can hang from: earth or rock of the map itself with air right under it
	const auto Ceiling = [&](int x, int y) {
		if(x < 0 || y < 0 || x >= W || y + 1 >= H)
			return false;
		const int m = pMat[y * W + x];
		return (m == MAT_GROUND || m == MAT_STONE) && pMat[(y + 1) * W + x] == MAT_AIR;
	};
	for(int y = 0; y < H; y++)
	{
		for(int x = 0; x < W; x++)
		{
			unsigned char *pPix = &pOut[(y * W + x) * 4];
			// a hanging thing is up to 3 blocks wide, lies a little over its ceiling and reaches up to MAX_HANG blocks down
			if(pPix[0] == MAT_AIR)
			{
				bool Hang = false;
				for(int Row = 1; Row <= MAX_HANG && !Hang; Row++)
					Hang = Ceiling(x - 1, y - Row) || Ceiling(x, y - Row) || Ceiling(x + 1, y - Row);
				if(Hang)
					pPix[2] |= 16;
			}
			else if((pPix[0] == MAT_GROUND || pPix[0] == MAT_STONE) && (Ceiling(x - 1, y) || Ceiling(x, y) || Ceiling(x + 1, y)))
				pPix[3] |= 32;
			bool Green = false, Mossy = false, Wet = false;
			for(int Row = 0; Row <= 2; Row++)
			{
				for(int Dx = -1; Dx <= 1; Dx++)
				{
					if(y + Row >= H || x + Dx < 0 || x + Dx >= W)
						continue;
					Green = Green || GrassTop(x + Dx, y + Row);
					Mossy = Mossy || RockTop(x + Dx, y + Row);
					Wet = Wet || (At(x + Dx, y + Row) == MAT_WATER && At(x + Dx, y + Row - 1) == MAT_AIR);
				}
			}
			if(Green)
				pPix[3] |= 16;
			if(Mossy)
				pPix[3] |= 64;
			if(Wet)
				pPix[3] |= 128;
			if(pPix[0] == MAT_AIR && y + 1 < H && pMat[(y + 1) * W + x] == MAT_WATER)
				pPix[3] |= 32;
		}
	}
}
// Second texture of the pack styles, one pixel per block: what of the grass and the plants lies over the block.
// The processor decides it once per map, the shader reads one pixel instead of looking at all the neighbours.
//   R  strip (cap) of an open top: 1 this block is the top, 2 the top is right below,
//      4 / 8 the block to the left / right is a top (its grass hangs over the side into this block),
//      16 / 32 the block down-left / down-right is a top (the grass standing on it leans into this block),
//      64 the strips of this row (bits 1, 4, 8) are the moss of the rock, not the grass of the earth,
//      128 the same for the row below (bits 2, 16, 32). In one row over one block the grass wins over the moss.
//   G, B  up to two plants over the block: number of the sprite + 1 (0 = none)
//   A  where the plant stands: low 4 bits for G, high 4 bits for B; value = (dx + 1) + 3 * dy, the foot block is at (x + dx, y + dy).
//      A thing that hangs (its number is HangFirst or more): the block of the ceiling it hangs from is at (x + dx, y - dy).
inline unsigned DecoHash(int x, int y, unsigned Salt)
{
	unsigned h = (unsigned)x * 0x9E3779B1u ^ (unsigned)y * 0x85EBCA77u ^ Salt * 0xC2B2AE3Du;
	h ^= h >> 15;
	h *= 0x2C1B3C6Du;
	h ^= h >> 12;
	h *= 0x297A2D39u;
	h ^= h >> 15;
	return h;
}

// What the sprites of a sheet of hanging things need (the sheet: Count cells of CellW x CellH texels, left to right and top to
// bottom, the top Overlap rows of a cell lie over the block of the ceiling). pRgba: W*H*4 bytes, Ppb: texels per block.
// pSizes[i]: blocks sprite i reaches down from its ceiling (1..MAX_HANG, 0 = the cell is empty or the thing is too long),
// + HANG_WIDE if it does not fit into one block with room to sway.
inline void MeasureHangSprites(const unsigned char *pRgba, int W, int H, int CellW, int CellH, int Count, int Overlap, int Ppb, unsigned char *pSizes)
{
	const int Cols = CellW > 0 ? W / CellW : 0;
	for(int i = 0; i < Count; i++)
	{
		pSizes[i] = 0;
		if(Cols <= 0 || CellH <= 0 || Ppb <= 0 || (i / Cols + 1) * CellH > H)
			continue;
		const int Cx = (i % Cols) * CellW, Cy = (i / Cols) * CellH;
		int Left = CellW, Right = -1, Bottom = -1;
		for(int y = 0; y < CellH; y++)
		{
			for(int x = 0; x < CellW; x++)
			{
				if(pRgba[((Cy + y) * W + Cx + x) * 4 + 3] < 128)
					continue;
				Left = x < Left ? x : Left;
				Right = x > Right ? x : Right;
				Bottom = y;
			}
		}
		if(Bottom < 0)
			continue;
		const int Rows = Bottom + 1 - Overlap; // texels under the ceiling
		const int Blocks = Rows <= 0 ? 1 : (Rows + Ppb - 1) / Ppb;
		if(Blocks > MAX_HANG)
			continue;
		// the thing is drawn around the middle of its cell, the middle of the cell is the middle of the block
		const int Mid2 = CellW; // twice the middle
		const int Half2 = (Mid2 - 2 * Left) > (2 * (Right + 1) - Mid2) ? (Mid2 - 2 * Left) : (2 * (Right + 1) - Mid2); // twice the reach from the middle
		pSizes[i] = (unsigned char)(Blocks | (Half2 > Ppb - 4 ? HANG_WIDE : 0));
	}
}

// pMat: W*H materials, pOut: W*H*4 bytes. SpriteCount: plants in the sheet of the pack, Percent: tops that get a plant.
// Open tops of the rock (only with a pack that has them): RockCap = the pack has a strip of moss for them,
// RockFirst / RockCount = the sprites of the sheet that stand on them, RockPercent = tops that get one.
// Open surface of the water: WaterFirst / WaterCount = the sprites that float on it, WaterPercent = blocks of the surface that get one.
// Ceilings (earth or rock with air under it): HangFirst = the number of the first thing that hangs, pHangSizes / HangCount = what
// each of them needs (MeasureHangSprites), HangPercent = blocks of a ceiling that get one.
inline void BuildDecoTexture(const unsigned char *pMat, int W, int H, int SpriteCount, int Percent, unsigned char *pOut, bool RockCap = false, int RockFirst = 0, int RockCount = 0, int RockPercent = 0, int WaterFirst = 0, int WaterCount = 0, int WaterPercent = 0,
	int HangFirst = 0, const unsigned char *pHangSizes = nullptr, int HangCount = 0, int HangPercent = 0)
{
	const auto Mat = [&](int x, int y) -> int {
		return x < 0 || y < 0 || x >= W || y >= H ? -1 : pMat[y * W + x];
	};
	// open top with a strip: 0 none, 1 earth (grass), 2 rock (moss)
	const auto Top = [&](int x, int y) -> int {
		if(Mat(x, y - 1) != MAT_AIR)
			return 0;
		const int m = Mat(x, y);
		return m == MAT_GROUND ? 1 : (m == MAT_STONE && RockCap ? 2 : 0);
	};
	const auto Solid = [&](int x, int y) {
		const int m = Mat(x, y);
		return m == MAT_GROUND || m == MAT_STONE || m < 0;
	};
	for(int i = 0; i < W * H * 4; i++)
		pOut[i] = 0;
	for(int y = 0; y < H; y++)
	{
		for(int x = 0; x < W; x++)
		{
			unsigned char *pPix = &pOut[(y * W + x) * 4];
			const int Own = Top(x, y);
			if(Own)
				pPix[0] |= Own == 2 ? (1 | 64) : 1;
			if(Solid(x, y))
				continue;
			// the strips of one row over one block are of one kind: the grass wins over the moss
			const int Left = Top(x - 1, y), Right = Top(x + 1, y);
			const int Kind0 = (Left == 1 || Right == 1) ? 1 : ((Left || Right) ? 2 : 0);
			if(Kind0)
			{
				if(Left == Kind0)
					pPix[0] |= 4;
				if(Right == Kind0)
					pPix[0] |= 8;
				if(Kind0 == 2)
					pPix[0] |= 64;
			}
			const int Below = Top(x, y + 1), BelowLeft = Top(x - 1, y + 1), BelowRight = Top(x + 1, y + 1);
			const int Kind1 = Below ? Below : ((BelowLeft == 1 || BelowRight == 1) ? 1 : ((BelowLeft || BelowRight) ? 2 : 0));
			if(Kind1)
			{
				if(Below)
					pPix[0] |= 2;
				if(BelowLeft == Kind1)
					pPix[0] |= 16;
				if(BelowRight == Kind1)
					pPix[0] |= 32;
				if(Kind1 == 2)
					pPix[0] |= 128;
			}
		}
	}
	// the number of a sprite is stored + 1 in one byte
	if(SpriteCount < 0)
		SpriteCount = 0;
	if(SpriteCount > 254)
		SpriteCount = 254;
	if(RockFirst < 0 || RockFirst > 253 || RockCount < 0)
		RockCount = 0;
	if(RockFirst + RockCount > 254)
		RockCount = 254 - RockFirst;
	if(WaterFirst < 0 || WaterFirst > 253 || WaterCount < 0)
		WaterCount = 0;
	if(WaterFirst + WaterCount > 254)
		WaterCount = 254 - WaterFirst;
	if(HangFirst < 1 || HangFirst > 253 || HangCount < 0 || !pHangSizes)
		HangCount = 0;
	if(HangFirst + HangCount > 254)
		HangCount = 254 - HangFirst;
	if(SpriteCount <= 0 && RockCount <= 0 && WaterCount <= 0 && HangCount <= 0)
		return;
	const auto Surface = [&](int x, int y) {
		return Mat(x, y) == MAT_WATER && Mat(x, y - 1) == MAT_AIR;
	};
	// rows top to bottom, blocks left to right: two plants over one block are stored in the same order everywhere
	for(int y = 0; y < H; y++)
	{
		for(int x = 0; x < W; x++)
		{
			if(Mat(x, y - 1) != MAT_AIR)
				continue;
			const int Ground = Mat(x, y);
			if(Ground == MAT_WATER)
			{
				// a thing that floats: only with open water on both sides (it is up to 3 blocks wide), never two side by side
				if(WaterCount <= 0 || !Surface(x - 1, y) || !Surface(x + 1, y) || (int)(DecoHash(x, y, 5) % 100u) >= WaterPercent)
					continue;
				if(Surface(x - 2, y) && Surface(x - 3, y) && (int)(DecoHash(x - 1, y, 5) % 100u) < WaterPercent)
					continue;
				const int Sprite = WaterFirst + (int)(DecoHash(x, y, 6) % (unsigned)WaterCount) + 1;
				for(int Dy = 0; Dy <= 2; Dy++)
				{
					for(int Dx = -1; Dx <= 1; Dx++)
					{
						const int Bx = x + Dx, By = y - Dy;
						if(Bx < 0 || By < 0 || Bx >= W || Solid(Bx, By))
							continue;
						unsigned char *pPix = &pOut[(By * W + Bx) * 4];
						const int Where = (-Dx + 1) + 3 * Dy;
						if(pPix[1] == 0)
						{
							pPix[1] = (unsigned char)Sprite;
							pPix[3] = (unsigned char)((pPix[3] & 0xF0) | Where);
						}
						else if(pPix[2] == 0)
						{
							pPix[2] = (unsigned char)Sprite;
							pPix[3] = (unsigned char)((pPix[3] & 0x0F) | (Where << 4));
						}
					}
				}
				continue;
			}
			if(Ground != MAT_GROUND && Ground != MAT_STONE)
				continue;
			const int First = Ground == MAT_GROUND ? 0 : RockFirst;
			const int Count = Ground == MAT_GROUND ? SpriteCount : RockCount;
			if(Count <= 0 || (int)(DecoHash(x, y, 1) % 100u) >= (Ground == MAT_GROUND ? Percent : RockPercent))
				continue;
			// a stone or a crystal cut in half by a wall or a ceiling shows: the things of the rock need room around them
			if(Ground == MAT_STONE && (Solid(x - 1, y - 1) || Solid(x + 1, y - 1) || Solid(x, y - 2)))
				continue;
			const int Sprite = First + (int)(DecoHash(x, y, 2) % (unsigned)Count) + 1;
			// a plant is up to 3 blocks wide and reaches 2 blocks up from its foot block
			for(int Dy = 0; Dy <= 2; Dy++)
			{
				for(int Dx = -1; Dx <= 1; Dx++)
				{
					const int Bx = x + Dx, By = y - Dy;
					if(Bx < 0 || By < 0 || Bx >= W || (Solid(Bx, By) && (Dx != 0 || Dy != 0)))
						continue;
					unsigned char *pPix = &pOut[(By * W + Bx) * 4];
					const int Where = (-Dx + 1) + 3 * Dy;
					if(pPix[1] == 0)
					{
						pPix[1] = (unsigned char)Sprite;
						pPix[3] = (unsigned char)((pPix[3] & 0xF0) | Where);
					}
					else if(pPix[2] == 0)
					{
						pPix[2] = (unsigned char)Sprite;
						pPix[3] = (unsigned char)((pPix[3] & 0x0F) | (Where << 4));
					}
				}
			}
		}
	}
	// the things that hang from the ceilings: after the plants (a block has room for two things, a plant on the floor of a low
	// passage keeps its place). They never lie over one another and end at least one block above what is below.
	for(int y = 0; y < H && HangCount > 0; y++)
	{
		int LastX = -9;
		bool LastWide = false;
		for(int x = 0; x < W; x++)
		{
			const int Roof = Mat(x, y);
			if((Roof != MAT_GROUND && Roof != MAT_STONE) || Mat(x, y + 1) != MAT_AIR)
				continue;
			if((int)(DecoHash(x, y, 7) % 100u) >= HangPercent)
				continue;
			int Free = 0; // blocks of air under the ceiling
			while(Free <= MAX_HANG && Mat(x, y + 1 + Free) == MAT_AIR)
				Free++;
			// a wide thing needs three blocks of ceiling in a row and air beside it all the way down
			int FreeWide = 0;
			if(Solid(x - 1, y) && Mat(x - 1, y) >= 0 && Solid(x + 1, y) && Mat(x + 1, y) >= 0)
			{
				while(FreeWide < Free && Mat(x - 1, y + 1 + FreeWide) == MAT_AIR && Mat(x + 1, y + 1 + FreeWide) == MAT_AIR)
					FreeWide++;
			}
			const bool RoomNarrow = x - LastX >= (LastWide ? 3 : 2);
			const bool RoomWide = x - LastX >= 3;
			const auto Fits = [&](int i) {
				const int Blocks = pHangSizes[i] & 15;
				if(Blocks < 1 || Blocks > MAX_HANG)
					return false;
				if((pHangSizes[i] & HANG_WIDE) != 0)
					return RoomWide && Blocks <= FreeWide && Blocks + 1 <= Free;
				return RoomNarrow && Blocks + 1 <= Free;
			};
			int Num = 0;
			for(int i = 0; i < HangCount; i++)
				Num += Fits(i) ? 1 : 0;
			if(Num == 0)
				continue;
			int Pick = (int)(DecoHash(x, y, 8) % (unsigned)Num), Chosen = 0;
			for(int i = 0; i < HangCount; i++)
			{
				if(Fits(i) && Pick-- == 0)
				{
					Chosen = i;
					break;
				}
			}
			const int Blocks = pHangSizes[Chosen] & 15;
			const int Sprite = HangFirst + Chosen + 1;
			LastX = x;
			LastWide = (pHangSizes[Chosen] & HANG_WIDE) != 0;
			// row 0 is the ceiling itself (the thing lies a little over it), below it only the air takes the thing
			for(int Dy = 0; Dy <= Blocks; Dy++)
			{
				for(int Dx = -1; Dx <= 1; Dx++)
				{
					const int Bx = x + Dx, By = y + Dy;
					if(Bx < 0 || Bx >= W || By >= H)
						continue;
					if(Dy == 0 ? !Solid(Bx, By) : Mat(Bx, By) != MAT_AIR)
						continue;
					unsigned char *pPix = &pOut[(By * W + Bx) * 4];
					const int Where = (-Dx + 1) + 3 * Dy;
					if(pPix[1] == 0)
					{
						pPix[1] = (unsigned char)Sprite;
						pPix[3] = (unsigned char)((pPix[3] & 0xF0) | Where);
					}
					else if(pPix[2] == 0)
					{
						pPix[2] = (unsigned char)Sprite;
						pPix[3] = (unsigned char)((pPix[3] & 0x0F) | (Where << 4));
					}
				}
			}
		}
	}
}
}

#endif
