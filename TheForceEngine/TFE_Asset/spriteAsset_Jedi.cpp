#include "spriteAsset_Jedi.h"
#include <TFE_System/system.h>
#include <TFE_Asset/assetSystem.h>
#include <TFE_Archive/archive.h>
// TODO: dependency on JediRenderer, this should be refactored...
#include <TFE_JediRenderer/fixedPoint.h>
#include <TFE_JediRenderer/rlimits.h>
//
#include <assert.h>
#include <algorithm>
#include <cstring>
#include <vector>
#include <string>
#include <map>

using namespace TFE_JediRenderer;

namespace TFE_Sprite_Jedi
{
	typedef std::map<std::string, JediFrame*> FrameMap;
	typedef std::map<std::string, JediWax*> SpriteMap;

	static FrameMap  s_frames;
	static SpriteMap s_sprites;
	static std::vector<u8> s_buffer;
	static const char* c_defaultGob = "SPRITES.GOB";
		
	JediFrame* getFrame(const char* name)
	{
		FrameMap::iterator iFrame = s_frames.find(name);
		if (iFrame != s_frames.end())
		{
			return iFrame->second;
		}

		// It doesn't exist yet, try to load the frame.
		if (!TFE_AssetSystem::readAssetFromArchive(c_defaultGob, ARCHIVE_GOB, name, s_buffer))
		{
			return nullptr;
		}

		const u8* data = s_buffer.data();

		// Determine ahead of time how much we need to allocate.
		const WaxFrame* base_frame = (WaxFrame*)data;
		const WaxCell* base_cell = WAX_CellPtr(data, base_frame);
		const u32 columnSize = base_cell->sizeX * sizeof(u32);

		// This is a "load in place" format in the original code.
		// We are going to allocate new memory and copy the data.
		u8* assetPtr = (u8*)malloc(s_buffer.size() + columnSize + sizeof(JediFrame));
		JediFrame* asset = (JediFrame*)assetPtr;
		
		asset->basePtr = assetPtr + sizeof(JediFrame);
		asset->frame = (WaxFrame*)asset->basePtr;
		memcpy(asset->basePtr, data, s_buffer.size());

		WaxFrame* frame = asset->frame;
		WaxCell* cell = WAX_CellPtr(asset->basePtr, frame);

		// After load, the frame data has to be fixed up before rendering.
		// frame sizes remain in fixed point.
		frame->widthWS  = div16(intToFixed16(cell->sizeX), SPRITE_SCALE_FIXED);
		frame->heightWS = div16(intToFixed16(cell->sizeY), SPRITE_SCALE_FIXED);

		const s32 offsetX = -intToFixed16(frame->offsetX);
		const s32 offsetY =  intToFixed16(cell->sizeY) + intToFixed16(frame->offsetY);
		frame->offsetX = div16(offsetX, SPRITE_SCALE_FIXED);
		frame->offsetY = div16(offsetY, SPRITE_SCALE_FIXED);

		if (cell->compressed == 1)
		{
			// Update the column offset, it starts right after the cell data.
			cell->columnOffset = frame->cellOffset + sizeof(WaxCell);
		}
		else
		{
			u32* columns = (u32*)(asset->basePtr + s_buffer.size());
			// Local pointer.
			cell->columnOffset = u32((u8*)columns - asset->basePtr);
			// Calculate column offsets.
			for (s32 c = 0; c < cell->sizeX; c++)
			{
				columns[c] = cell->sizeY * c;
			}
		}
		
		s_frames[name] = asset;
		return asset;
	}

	static std::vector<u32> s_cellOffsets;

	bool isUniqueCell(u32 offset)
	{
		const size_t count = s_cellOffsets.size();
		const u32* offsetList = s_cellOffsets.data();
		for (u32 i = 0; i < count; i++)
		{
			if (offsetList[i] == offset) { return false; }
		}
		s_cellOffsets.push_back(offset);

		return true;
	}

	JediWax* getWax(const char* name)
	{
		SpriteMap::iterator iSprite = s_sprites.find(name);
		if (iSprite != s_sprites.end())
		{
			return iSprite->second;
		}

		// It doesn't exist yet, try to load the frame.
		if (!TFE_AssetSystem::readAssetFromArchive(c_defaultGob, ARCHIVE_GOB, name, s_buffer))
		{
			return nullptr;
		}

		const u8* data = s_buffer.data();
		const Wax* srcWax = (Wax*)data;
		
		// every animation is filled out until the end, so no animations = no wax.
		if (!srcWax->animOffsets[0])
		{
			return nullptr;
		}
		s_cellOffsets.clear();

		// First determine the size to allocate (note that this will overallocate a bit because cells are shared).
		u32 sizeToAlloc = sizeof(JediWax) + (u32)s_buffer.size();
		const s32* animOffset = srcWax->animOffsets;
		for (s32 animIdx = 0; animIdx < 32 && animOffset[animIdx]; animIdx++)
		{
			WaxAnim* anim = (WaxAnim*)(data + animOffset[animIdx]);
			const s32* viewOffsets = anim->viewOffsets;
			for (s32 v = 0; v < 32; v++)
			{
				const WaxView* view = (WaxView*)(data + viewOffsets[v]);
				const s32* frameOffset = view->frameOffsets;
				for (s32 f = 0; f < 32 && frameOffset[f]; f++)
				{
					const WaxFrame* frame = (WaxFrame*)(data + frameOffset[f]);
					const WaxCell* cell = frame->cellOffset ? (WaxCell*)(data + frame->cellOffset) : nullptr;
					if (cell && cell->compressed == 0 && isUniqueCell(frame->cellOffset))
					{
						sizeToAlloc += cell->sizeX * sizeof(u32);
					}
				}
			}
		}

		// Allocate and copy the data (this is a "copy in place" format... mostly.
		JediWax* asset = (JediWax*)malloc(sizeToAlloc);
		asset->basePtr = (u8*)asset + sizeof(JediWax);
		Wax* dstWax = asset->wax = (Wax*)asset->basePtr;
		memcpy(dstWax, srcWax, s_buffer.size());

		// Loop through animation list until we reach 32 (maximum count) or a null animation.
		// This means that animations are contiguous.
		fixed16_16 scaledWidth, scaledHeight;

		u32 cellOffsetPtr = 0;
		for (s32 animIdx = 0; animIdx < 32 && animOffset[animIdx]; animIdx++)
		{
			WaxAnim* dstAnim = (WaxAnim*)(asset->basePtr + animOffset[animIdx]);

			if (animIdx == 0)
			{
				scaledWidth  = div16(SPRITE_SCALE_FIXED, dstAnim->worldWidth);
				scaledHeight = div16(SPRITE_SCALE_FIXED, dstAnim->worldHeight);

				dstWax->xScale = scaledWidth;
				dstWax->yScale = scaledHeight;
			}

			const s32* viewOffsets = dstAnim->viewOffsets;
			for (s32 v = 0; v < 32; v++)
			{
				const WaxView* dstView = (WaxView*)(asset->basePtr + viewOffsets[v]);
				const s32* frameOffset = dstView->frameOffsets;
				for (s32 f = 0; f < 32 && frameOffset[f]; f++)
				{
					const WaxFrame* srcFrame = (WaxFrame*)(data + frameOffset[f]);
					WaxFrame* dstFrame = (WaxFrame*)(asset->basePtr + frameOffset[f]);

					// Some frames are shared between animations, so we need to read from the source, unmodified data.
					dstFrame->offsetX = round16(mul16(dstAnim->worldWidth,  intToFixed16(srcFrame->offsetX)));
					dstFrame->offsetY = round16(mul16(dstAnim->worldHeight, intToFixed16(srcFrame->offsetY)));

					WaxCell* dstCell = dstFrame->cellOffset ? (WaxCell*)(asset->basePtr + dstFrame->cellOffset) : nullptr;
					if (dstCell)
					{
						dstFrame->widthWS  = div16(intToFixed16(dstCell->sizeX), scaledWidth);
						dstFrame->heightWS = div16(intToFixed16(dstCell->sizeY), scaledHeight);

						if (dstCell->columnOffset == 0)
						{
							if (dstCell->compressed == 1)
							{
								// Update the column offset, it starts right after the cell data.
								dstCell->columnOffset = dstFrame->cellOffset + sizeof(WaxCell);
							}
							else
							{
								u32* columns = (u32*)(asset->basePtr + s_buffer.size() + cellOffsetPtr);
								cellOffsetPtr += dstCell->sizeX * sizeof(u32);

								// Local pointer.
								dstCell->columnOffset = u32((u8*)columns - asset->basePtr);
								// Calculate column offsets.
								for (s32 c = 0; c < dstCell->sizeX; c++)
								{
									columns[c] = dstCell->sizeY * c;
								}
							}
						}

						dstFrame->offsetX = div16(-intToFixed16(dstFrame->offsetX), SPRITE_SCALE_FIXED);
						const s32 adjOffsetY = mul16(intToFixed16(dstCell->sizeY), dstAnim->worldHeight) + intToFixed16(dstFrame->offsetY);
						dstFrame->offsetY = div16(adjOffsetY, SPRITE_SCALE_FIXED);
					}
				}
			}
		}

		s_sprites[name] = asset;
		return asset;
	}

	void freeAll()
	{
		FrameMap::iterator iFrame = s_frames.begin();
		for (; iFrame != s_frames.end(); ++iFrame)
		{
			free(iFrame->second);
		}
		s_frames.clear();

		SpriteMap::iterator iSprite = s_sprites.begin();
		for (; iSprite != s_sprites.end(); ++iSprite)
		{
			free(iSprite->second);
		}
		s_sprites.clear();
	}
}
