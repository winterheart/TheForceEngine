#include <TFE_Asset/modelAsset_Jedi.h>
#include <TFE_System/profiler.h>
// TODO: Either move level.h or fix it.
#include <TFE_Game/level.h>

#include "rsectorFixed.h"
#include "rwallFixed.h"
#include "rflatFixed.h"
#include "rlightingFixed.h"
#include "redgePairFixed.h"
#include "rcommonFixed.h"
#include "robj3d_fixed/robj3dFixed.h"
#include "../fixedPoint.h"
#include "../rmath.h"
#include "../rcommon.h"
#include "../robject.h"
#include "../rtexture.h"

#include <climits>
#include <cstring>

using namespace TFE_JediRenderer::RClassic_Fixed;

namespace TFE_JediRenderer
{
	namespace
	{
		s32 sortObjectsFixed(const void* r0, const void* r1)
		{
			SecObject* obj0 = *((SecObject**)r0);
			SecObject* obj1 = *((SecObject**)r1);

			if (obj0->type == OBJ_TYPE_3D && obj1->type == OBJ_TYPE_3D)
			{
				// Both objects are 3D.
				const fixed16_16 distSq0 = dotFixed(obj0->posVS, obj0->posVS);
				const fixed16_16 distSq1 = dotFixed(obj1->posVS, obj1->posVS);
				const fixed16_16 dist0 = fixedSqrt(distSq0);
				const fixed16_16 dist1 = fixedSqrt(distSq1);

				if (obj0->model->isBridge && obj1->model->isBridge)
				{
					return dist1 - dist0;
				}
				else if (obj0->model->isBridge == 1)
				{
					return -1;
				}
				else if (obj1->model->isBridge == 1)
				{
					return 1;
				}

				return dist1 - dist0;
			}
			else if (obj0->type == OBJ_TYPE_3D && obj0->model->isBridge)
			{
				return -1;
			}
			else if (obj1->type == OBJ_TYPE_3D && obj1->model->isBridge)
			{
				return 1;
			}

			// Default case:
			return obj1->posVS.z.f16_16 - obj0->posVS.z.f16_16;
		}

		s32 vec2ToAngle(fixed16_16 dx, fixed16_16 dz)
		{
			if (dx == 0 && dz == 0)
			{
				return 0;
			}

			const s32 signsDiff = (signV2A(dx) != signV2A(dz)) ? 1 : 0;
			// Splits the view into 4 quadrants, 0 - 3:
			// 1 | 0
			// -----
			// 2 | 3
			const s32 quadrant = (dz < 0 ? 2 : 0) + signsDiff;

			// Further splits the quadrants into sub-quadrants:
			// \2|1/
			// 3\|/0
			//---*---
			// 4/|\7
			// /5|6\
			//
			dx = abs(dx);
			dz = abs(dz);
			const s32 subquadrant = quadrant * 2 + ((dx < dz) ? (1 - signsDiff) : signsDiff);

			// true in sub-quadrants: 0, 3, 4, 7; where dz tends towards 0.
			if ((subquadrant - 1) & 2)
			{
				// The original code did the "3 xor" trick to swap dx and dz.
				std::swap(dx, dz);
			}

			// next compute |dx| / |dz|, which will be a value from 0.0 to 1.0
			fixed16_16 dXdZ = div16(dx, dz);
			if (subquadrant & 1)
			{
				// invert the ratio in sub-quadrants 1, 3, 5, 7 to maintain the correct direction.
				dXdZ = ONE_16 - dXdZ;
			}

			// subquadrantF is based on the sub-quadrant, essentially fixed16(subquadrant)
			// which has a range of 0 to 7.0 (see above).
			const fixed16_16 subquadrantF = intToFixed16(subquadrant);
			// angle = (int(2.0 - (zF + dXdZ)) * 2048) & 16383
			// this flips the angle so that straight up (dx = 0, dz > 0) is 0, right is 90, down is 180.
			const s32 angle = (2 * ONE_16 - (subquadrantF + dXdZ)) >> 5;
			// the final angle will be in the range of 0 - 16383
			return angle & 0x3fff;
		}

		s32 cullObjects(RSector* sector, SecObject** buffer)
		{
			s32 drawCount = 0;
			SecObject** obj = sector->objectList;
			s32 count = sector->objectCount;

			for (s32 i = count - 1; i >= 0 && drawCount < MAX_VIEW_OBJ_COUNT; i--, obj++)
			{
				// Search for the next allocated object.
				SecObject* curObj = *obj;
				while (!curObj)
				{
					obj++;
					curObj = *obj;
				}

				if (curObj->flags & OBJ_FLAG_RENDERABLE)
				{
					const s32 type = curObj->type;
					if (type == OBJ_TYPE_SPRITE || type == OBJ_TYPE_FRAME)
					{
						if (curObj->posVS.z.f16_16 >= ONE_16)
						{
							buffer[drawCount++] = curObj;
						}
					}
					else if (type == OBJ_TYPE_3D)
					{
						const fixed16_16 radius = curObj->model->radius;
						const fixed16_16 zMax = curObj->posVS.z.f16_16 + radius;
						// Near plane
						if (zMax < ONE_16) { continue; }

						// Left plane
						const fixed16_16 xMax = curObj->posVS.x.f16_16 + radius;
						if (xMax < -zMax) { continue; }

						// Right plane
						const fixed16_16 xMin = curObj->posVS.x.f16_16 - radius;
						if (xMin > zMax) { continue; }

						// The object straddles the near plane, so add it and move on.
						const fixed16_16 zMin = curObj->posVS.z.f16_16 - radius;
						if (zMin <= 0)
						{
							buffer[drawCount++] = curObj;
							continue;
						}

						// Cull against the current "window."
						const fixed16_16 z = curObj->posVS.z.f16_16;
						const s32 x0 = round16(div16(mul16(xMin, s_focalLength_Fixed), z)) + s_screenXMid;
						if (x0 > s_windowMaxX) { continue; }

						const s32 x1 = round16(div16(mul16(xMax, s_focalLength_Fixed), z)) + s_screenXMid;
						if (x1 < s_windowMinX) { continue; }

						// Finally add the object to render.
						buffer[drawCount++] = curObj;
					}
				}
			}

			return drawCount;
		}

		void sprite_drawWax(s32 angle, SecObject* obj)
		{
			// Angles range from [0, 16384), divide by 512 to get 32 even buckets.
			s32 angleDiff = (angle - obj->yaw) >> 9;
			angleDiff &= 31;	// up to 32 views

			// Get the animation based on the object state.
			Wax* wax = obj->wax;
			u8* basePtr = (u8*)obj->wax;
			WaxAnim* anim = WAX_AnimPtr(basePtr, wax, obj->anim & 0x1f);
			if (anim)
			{
				// Then get the Sequence from the angle difference.
				WaxView* view = WAX_ViewPtr(basePtr, anim, 31 - angleDiff);
				// And finall the frame from the current sequence.
				WaxFrame* frame = WAX_FramePtr(basePtr, view, obj->frame & 0x1f);
				// Draw the frame.
				sprite_drawFrame(basePtr, frame, obj);
			}
		}
	}
		
	void TFE_Sectors_Fixed::draw(RSector* sector)
	{
		s_curSector = sector;
		s_sectorIndex++;
		s_adjoinIndex++;
		if (s_adjoinIndex > s_maxAdjoinIndex)
		{
			s_maxAdjoinIndex = s_adjoinIndex;
		}

		s32* winTop = &s_windowTop_all[(s_adjoinDepth - 1) * s_width];
		s32* winBot = &s_windowBot_all[(s_adjoinDepth - 1) * s_width];
		s32* winTopNext = &s_windowTop_all[s_adjoinDepth * s_width];
		s32* winBotNext = &s_windowBot_all[s_adjoinDepth * s_width];

		s_depth1d_Fixed = &s_depth1d_all_Fixed[(s_adjoinDepth - 1) * s_width];

		s32 startWall = s_curSector->startWall;
		s32 drawWallCount = s_curSector->drawWallCnt;

		s_sectorAmbient = round16(s_curSector->ambient.f16_16);
		s_scaledAmbient = (s_sectorAmbient >> 1) + (s_sectorAmbient >> 2) + (s_sectorAmbient >> 3);
		s_sectorAmbientFraction = s_sectorAmbient << 11;	// fraction of ambient compared to max.

		s_windowTop = winTop;
		s_windowBot = winBot;
		fixed16_16* depthPrev = nullptr;
		if (s_adjoinDepth > 1)
		{
			depthPrev = &s_depth1d_all_Fixed[(s_adjoinDepth - 2) * s_width];
			memcpy(&s_depth1d_Fixed[s_minScreenX], &depthPrev[s_minScreenX], s_width * 4);
		}

		s_wallMaxCeilY = s_windowMinY;
		s_wallMinFloorY = s_windowMaxY;

		if (s_drawFrame != s_curSector->prevDrawFrame)
		{
			TFE_ZONE_BEGIN(secXform, "Sector Vertex Transform");
				vec2* vtxWS = s_curSector->verticesWS;
				vec2* vtxVS = s_curSector->verticesVS;
				for (s32 v = 0; v < s_curSector->vertexCount; v++)
				{
					vtxVS->x.f16_16 = mul16(vtxWS->x.f16_16, s_cosYaw_Fixed) + mul16(vtxWS->z.f16_16, s_sinYaw_Fixed) + s_xCameraTrans_Fixed;
					vtxVS->z.f16_16 = mul16(vtxWS->x.f16_16, s_negSinYaw_Fixed) + mul16(vtxWS->z.f16_16, s_cosYaw_Fixed) + s_zCameraTrans_Fixed;
					vtxVS++;
					vtxWS++;
				}
			TFE_ZONE_END(secXform);

			TFE_ZONE_BEGIN(objXform, "Sector Object Transform");
				SecObject** obj = s_curSector->objectList;
				for (s32 i = s_curSector->objectCount - 1; i >= 0; i--, obj++)
				{
					SecObject* curObj = *obj;
					while (!curObj)
					{
						obj++;
						curObj = *obj;
					}

					if (curObj->flags & OBJ_FLAG_RENDERABLE)
					{
						transformPointByCameraFixed(&curObj->posWS, &curObj->posVS);
					}
				}
			TFE_ZONE_END(objXform);

			TFE_ZONE_BEGIN(wallProcess, "Sector Wall Process");
				startWall = s_nextWall;
				RWall* wall = s_curSector->walls;
				for (s32 i = 0; i < s_curSector->wallCount; i++, wall++)
				{
					wall_process(wall);
				}
				drawWallCount = s_nextWall - startWall;

				s_curSector->startWall = startWall;
				s_curSector->drawWallCnt = drawWallCount;
				s_curSector->prevDrawFrame = s_drawFrame;

				// Setup wall flags not from the original code, still to be replaced.
				setupWallDrawFlags(s_curSector);
			TFE_ZONE_END(wallProcess);
		}

		RWallSegment* wallSegment = &s_wallSegListDst[s_curWallSeg];
		s32 drawSegCnt = wall_mergeSort(wallSegment, MAX_SEG - s_curWallSeg, startWall, drawWallCount);
		s_curWallSeg += drawSegCnt;

		TFE_ZONE_BEGIN(wallQSort, "Wall QSort");
			qsort(wallSegment, drawSegCnt, sizeof(RWallSegment), wallSortX);
		TFE_ZONE_END(wallQSort);

		s32 flatCount = s_flatCount;
		EdgePair* flatEdge = &s_flatEdgeList[s_flatCount];
		s_flatEdge = flatEdge;

		s32 adjoinStart = s_adjoinSegCount;
		EdgePair* adjoinEdges = &s_adjoinEdgeList[adjoinStart];
		RWallSegment* adjoinList[MAX_ADJOIN_DEPTH];

		s_adjoinEdge = adjoinEdges;
		s_adjoinSegment = adjoinList;

		// Draw each wall segment in the sector.
		TFE_ZONE_BEGIN(secDrawWalls, "Draw Walls");
		for (s32 i = 0; i < drawSegCnt; i++, wallSegment++)
		{
			RWall* srcWall = wallSegment->srcWall;
			RSector* nextSector = srcWall->nextSector;

			// This will always be true for now.
			if (!nextSector)
			{
				wall_drawSolid(wallSegment);
			}
			else
			{
				const s32 df = srcWall->drawFlags;
				assert(df >= 0);
				if (df <= WDF_MIDDLE)
				{
					if (df == WDF_MIDDLE || (nextSector->flags1 & SEC_FLAGS1_EXT_FLOOR_ADJ))
					{
						wall_drawMask(wallSegment);
					}
					else
					{
						wall_drawBottom(wallSegment);
					}
				}
				else if (df == WDF_TOP)
				{
					if (nextSector->flags1 & SEC_FLAGS1_EXT_ADJ)
					{
						wall_drawMask(wallSegment);
					}
					else
					{
						wall_drawTop(wallSegment);
					}
				}
				else if (df == WDF_TOP_AND_BOT)
				{
					if ((nextSector->flags1 & SEC_FLAGS1_EXT_ADJ) && (nextSector->flags1 & SEC_FLAGS1_EXT_FLOOR_ADJ))
					{
						wall_drawMask(wallSegment);
					}
					else if (nextSector->flags1 & SEC_FLAGS1_EXT_ADJ)
					{
						wall_drawBottom(wallSegment);
					}
					else if (nextSector->flags1 & SEC_FLAGS1_EXT_FLOOR_ADJ)
					{
						wall_drawTop(wallSegment);
					}
					else
					{
						wall_drawTopAndBottom(wallSegment);
					}
				}
				else // WDF_BOT
				{
					if (nextSector->flags1 & SEC_FLAGS1_EXT_FLOOR_ADJ)
					{
						wall_drawMask(wallSegment);
					}
					else
					{
						wall_drawBottom(wallSegment);
					}
				}
			}
		}
		TFE_ZONE_END(secDrawWalls);

		TFE_ZONE_BEGIN(secDrawFlats, "Draw Flats");
			// Draw flats
			// Note: in the DOS code flat drawing functions are called through function pointers.
			// Since the function pointers always seem to be the same, the functions are called directly in this code.
			// Most likely this was used for testing or debug drawing and may be added back in the future.
			const s32 newFlatCount = s_flatCount - flatCount;
			if (s_curSector->flags1 & SEC_FLAGS1_EXTERIOR)
			{
				if (s_curSector->flags1 & SEC_FLAGS1_NOWALL_DRAW)
				{
					wall_drawSkyTopNoWall(s_curSector);
				}
				else
				{
					wall_drawSkyTop(s_curSector);
				}
			}
			else
			{
				flat_drawCeiling(s_curSector, flatEdge, newFlatCount);
			}
			if (s_curSector->flags1 & SEC_FLAGS1_PIT)
			{
				if (s_curSector->flags1 & SEC_FLAGS1_NOWALL_DRAW)
				{
					wall_drawSkyBottomNoWall(s_curSector);
				}
				else
				{
					wall_drawSkyBottom(s_curSector);
				}
			}
			else
			{
				flat_drawFloor(s_curSector, flatEdge, newFlatCount);
			}
		TFE_ZONE_END(secDrawFlats);

		// Adjoins
		s32 adjoinCount = s_adjoinSegCount - adjoinStart;
		if (adjoinCount && s_adjoinDepth < MAX_ADJOIN_DEPTH)
		{
			adjoin_setupAdjoinWindow(winBot, winBotNext, winTop, winTopNext, adjoinEdges, adjoinCount);
			RWallSegment** seg = adjoinList;
			RWallSegment* prevAdjoinSeg = nullptr;
			RWallSegment* curAdjoinSeg  = nullptr;

			s32 adjoinEnd = adjoinCount - 1;
			for (s32 i = 0; i < adjoinCount; i++, seg++, adjoinEdges++)
			{
				prevAdjoinSeg = curAdjoinSeg;
				curAdjoinSeg = *seg;

				RWall* srcWall = curAdjoinSeg->srcWall;
				RWallSegment* nextAdjoin = (i < adjoinEnd) ? *(seg + 1) : nullptr;
				RSector* nextSector = srcWall->nextSector;
				if (s_adjoinDepth < MAX_ADJOIN_DEPTH && s_adjoinDepth < s_maxDepthCount)
				{
					s32 index = s_adjoinDepth - 1;
					saveValues(index);

					adjoin_computeWindowBounds(adjoinEdges);
					s_adjoinDepth++;
					if (s_adjoinDepth > s_maxAdjoinDepth)
					{
						s_maxAdjoinDepth = s_adjoinDepth;
					}

					srcWall->drawFrame = s_drawFrame;
					s_windowTop = winTopNext;
					s_windowBot = winBotNext;
					if (prevAdjoinSeg != 0)
					{
						if (prevAdjoinSeg->wallX1 + 1 == curAdjoinSeg->wallX0)
						{
							s_windowX0 = s_windowMinX;
						}
					}
					if (nextAdjoin)
					{
						if (curAdjoinSeg->wallX1 == nextAdjoin->wallX0 - 1)
						{
							s_windowX1 = s_windowMaxX;
						}
					}

					s_windowMinZ_Fixed = min(curAdjoinSeg->z0.f16_16, curAdjoinSeg->z1.f16_16);
					draw(nextSector);
					
					if (s_adjoinDepth)
					{
						s32 index = s_adjoinDepth - 2;
						s_adjoinDepth--;
						restoreValues(index);
					}
					srcWall->drawFrame = 0;
					if (srcWall->flags1 & WF1_ADJ_MID_TEX)
					{
						TFE_ZONE("Draw Transparent Walls");
						wall_drawTransparent(curAdjoinSeg, adjoinEdges);
					}
				}
			}
		}

		if (!(s_curSector->flags1 & SEC_FLAGS1_SUBSECTOR) && depthPrev && s_drawFrame != s_prevSector->prevDrawFrame2)
		{
			memcpy(&depthPrev[s_windowMinX], &s_depth1d_Fixed[s_windowMinX], (s_windowMaxX - s_windowMinX + 1) * sizeof(fixed16_16));
		}

		// Objects
		TFE_ZONE_BEGIN(secDrawObjects, "Draw Objects");
		const s32 objCount = cullObjects(s_curSector, s_objBuffer);
		if (objCount > 0)
		{
			// Which top and bottom edges are we going to use to clip objects?
			s_objWindowTop = s_windowTop;
			if (s_windowMinY < s_heightInPixels || s_windowMaxCeil < s_heightInPixels)
			{
				if (s_prevSector && s_prevSector->ceilingHeight.f16_16 <= s_curSector->ceilingHeight.f16_16)
				{
					s_objWindowTop = s_windowTopPrev;
				}
			}
			s_objWindowBot = s_windowBot;
			if (s_windowMaxY > s_heightInPixels || s_windowMinFloor > s_heightInPixels)
			{
				if (s_prevSector && s_prevSector->floorHeight.f16_16 >= s_curSector->floorHeight.f16_16)
				{
					s_objWindowBot = s_windowBotPrev;
				}
			}

			// Sort objects in viewspace (generally back to front but there are special cases).
			qsort(s_objBuffer, objCount, sizeof(SecObject*), sortObjectsFixed);

			// Draw objects in order.
			for (s32 i = 0; i < objCount; i++)
			{
				SecObject* obj = s_objBuffer[i];
				const s32 type = obj->type;
				if (type == OBJ_TYPE_SPRITE)
				{
					TFE_ZONE("Draw WAX");

					fixed16_16 dz = s_cameraPosZ_Fixed - obj->posWS.z.f16_16;
					fixed16_16 dx = s_cameraPosX_Fixed - obj->posWS.x.f16_16;
					s32 angle = vec2ToAngle(dx, dz);

					sprite_drawWax(angle, obj);
				}
				else if (type == OBJ_TYPE_3D)
				{
					TFE_ZONE("Draw 3DO");

					robj3d_draw(obj, obj->model);
				}
				else if (type == OBJ_TYPE_FRAME)
				{
					TFE_ZONE("Draw Frame");

					sprite_drawFrame((u8*)obj->fme, obj->fme, obj);
				}
			}
		}
		TFE_ZONE_END(secDrawObjects);

		s_curSector->flags1 |= SEC_FLAGS1_RENDERED;
		s_curSector->prevDrawFrame2 = s_drawFrame;
	}
			
	void TFE_Sectors_Fixed::setupWallDrawFlags(RSector* sector)
	{
		RWall* wall = sector->walls;
		for (s32 w = 0; w < sector->wallCount; w++, wall++)
		{
			if (wall->nextSector)
			{
				RSector* wSector = wall->sector;
				fixed16_16 wFloorHeight = wSector->floorHeight.f16_16;
				fixed16_16 wCeilHeight = wSector->ceilingHeight.f16_16;

				RWall* mirror = wall->mirrorWall;
				RSector* mSector = mirror->sector;
				fixed16_16 mFloorHeight = mSector->floorHeight.f16_16;
				fixed16_16 mCeilHeight = mSector->ceilingHeight.f16_16;

				wall->drawFlags = 0;
				mirror->drawFlags = 0;

				if (wCeilHeight < mCeilHeight)
				{
					wall->drawFlags |= WDF_TOP;
				}
				if (wFloorHeight > mFloorHeight)
				{
					wall->drawFlags |= WDF_BOT;
				}
				if (mCeilHeight < wCeilHeight)
				{
					mirror->drawFlags |= WDF_TOP;
				}
				if (mFloorHeight > wFloorHeight)
				{
					mirror->drawFlags |= WDF_BOT;
				}
				wall_computeTexelHeights(wall->mirrorWall);
			}
			wall_computeTexelHeights(wall);
		}
	}

	void TFE_Sectors_Fixed::adjustHeights(RSector* sector, decimal floorOffset, decimal ceilOffset, decimal secondHeightOffset)
	{
		// Adjust objects.
		if (sector->objectCount)
		{
			fixed16_16 heightOffset = secondHeightOffset.f16_16 + floorOffset.f16_16;
			for (s32 i = 0; i < sector->objectCapacity; i++)
			{
				SecObject* obj = sector->objectList[i];
				if (obj)
				{
					// TODO: Adjust sector objects.
				}
			}
		}
		// Adjust sector heights.
		sector->ceilingHeight.f16_16 += ceilOffset.f16_16;
		sector->floorHeight.f16_16 += floorOffset.f16_16;
		sector->secHeight.f16_16 += secondHeightOffset.f16_16;

		// Update wall data.
		s32 wallCount = sector->wallCount;
		RWall* wall = sector->walls;
		for (s32 w = 0; w < wallCount; w++, wall++)
		{
			if (wall->nextSector)
			{
				wall_setupAdjoinDrawFlags(wall);
				wall_computeTexelHeights(wall->mirrorWall);
			}
			wall_computeTexelHeights(wall);
		}

		// Update collision data.
		fixed16_16 floorHeight = sector->floorHeight.f16_16;
		if (sector->flags1 & SEC_FLAGS1_PIT)
		{
			floorHeight += 100 * ONE_16;
		}
		fixed16_16 ceilHeight = sector->ceilingHeight.f16_16;
		if (sector->flags1 & SEC_FLAGS1_EXTERIOR)
		{
			ceilHeight -= 100 * ONE_16;
		}
		fixed16_16 secHeight = sector->floorHeight.f16_16 + sector->secHeight.f16_16;
		if (sector->secHeight.f16_16 >= 0 && floorHeight > secHeight)
		{
			secHeight = floorHeight;
		}

		sector->colFloorHeight.f16_16 = floorHeight;
		sector->colCeilHeight.f16_16 = ceilHeight;
		sector->colSecHeight.f16_16 = secHeight;
		sector->colMinHeight.f16_16 = ceilHeight;
	}

	void TFE_Sectors_Fixed::computeBounds(RSector* sector)
	{
		RWall* wall = sector->walls;
		vec2* w0 = wall->w0;
		fixed16_16 maxX = w0->x.f16_16;
		fixed16_16 maxZ = w0->z.f16_16;
		fixed16_16 minX = maxX;
		fixed16_16 minZ = maxZ;

		for (s32 i = 1; i < sector->wallCount; i++, wall++)
		{
			w0 = wall->w0;

			minX = min(minX, w0->x.f16_16);
			minZ = min(minZ, w0->z.f16_16);

			maxX = max(maxX, w0->x.f16_16);
			maxZ = max(maxZ, w0->z.f16_16);
		}

		sector->minX.f16_16 = minX;
		sector->maxX.f16_16 = maxX;
		sector->minZ.f16_16 = minZ;
		sector->maxZ.f16_16 = maxZ;

		// Setup when needed.
		//s_minX = minX;
		//s_maxX = maxX;
		//s_minZ = minZ;
		//s_maxZ = maxZ;
	}
		
	// Uses the "Winding Number" test for a point in polygon.
	// The point is considered inside if the winding number is greater than 0.
	// Note that this is different than DF's "crossing" algorithm.
	// TODO: Maybe? Replace algorithms.
	bool TFE_Sectors_Fixed::pointInSectorFixed(RSector* sector, f32 x, f32 z)
	{
		RWall* wall = sector->walls;
		s32 wallCount = sector->wallCount;
		s32 wn = 0;

		const Vec2f point = { x, z };
		for (s32 w = 0; w < wallCount; w++, wall++)
		{
			vec2* w1 = wall->w0;
			vec2* w0 = wall->w1;

			Vec2f p0 = { fixed16ToFloat(w0->x.f16_16), fixed16ToFloat(w0->z.f16_16) };
			Vec2f p1 = { fixed16ToFloat(w1->x.f16_16), fixed16ToFloat(w1->z.f16_16) };

			if (p0.z <= z)
			{
				// Upward crossing, if the point is left of the edge than it intersects.
				if (p1.z > z && isLeft(p0, p1, point) > 0)
				{
					wn++;
				}
			}
			else
			{
				// Downward crossing, if point is right of the edge it intersects.
				if (p1.z <= z && isLeft(p0, p1, point) < 0)
				{
					wn--;
				}
			}
		}

		// The point is only outside if the winding number is less than or equal to 0.
		return wn > 0;
	}

	// Use the floating point point inside polygon algorithm.
	RSector* TFE_Sectors_Fixed::which3D(decimal& dx, decimal& dy, decimal& dz)
	{
		s32 ix = dx.f16_16;
		s32 iz = dz.f16_16;
		f32 x = fixed16ToFloat(ix);
		f32 z = fixed16ToFloat(iz);
		fixed16_16 y = dy.f16_16;

		RSector* sector = s_rsectors;
		RSector* foundSector = nullptr;
		s32 sectorUnitArea = 0;
		s32 prevSectorUnitArea = INT_MAX;

		for (u32 i = 0; i < s_sectorCount; i++, sector++)
		{
			if (y >= sector->ceilingHeight.f16_16 && y <= sector->floorHeight.f16_16)
			{
				const fixed16_16 sectorMaxX = sector->maxX.f16_16;
				const fixed16_16 sectorMinX = sector->minX.f16_16;
				const fixed16_16 sectorMaxZ = sector->maxZ.f16_16;
				const fixed16_16 sectorMinZ = sector->minZ.f16_16;

				const s32 dxInt = ((sectorMaxX - sectorMinX) >> 16) + 1;
				const s32 dzInt = ((sectorMaxZ - sectorMinZ) >> 16) + 1;
				sectorUnitArea = dzInt * dxInt;

				s32 insideBounds = 0;
				if (ix >= sectorMinX && ix <= sectorMaxX && iz >= sectorMinZ && iz <= sectorMaxZ && pointInSectorFixed(sector, x, z))
				{
					// pick the containing sector with the smallest area.
					if (sectorUnitArea < prevSectorUnitArea)
					{
						prevSectorUnitArea = sectorUnitArea;
						foundSector = sector;
					}
				}
			}
		}

		return foundSector;
	}

	void TFE_Sectors_Fixed::copy(RSector* out, const Sector* sector, const SectorWall* walls, const Vec2f* vertices, Texture** textures)
	{
		out->vertexCount = sector->vtxCount;
		out->wallCount = sector->wallCount;

		// Initial setup.
		if (!out->verticesWS)
		{
			out->verticesWS = (vec2*)s_memPool->allocate(sizeof(vec2) * out->vertexCount);
			out->verticesVS = (vec2*)s_memPool->allocate(sizeof(vec2) * out->vertexCount);
			out->walls = (RWall*)s_memPool->allocate(sizeof(RWall) * out->wallCount);

			out->startWall = 0;
			out->drawWallCnt = 0;

			RWall* wall = out->walls;
			for (s32 w = 0; w < out->wallCount; w++, wall++)
			{
				wall->sector = out;
				wall->drawFrame = 0;
				wall->drawFlags = WDF_MIDDLE;
				wall->topTexelHeight.f16_16 = 0;
				wall->botTexelHeight.f16_16 = 0;

				wall->w0 = &out->verticesWS[walls[w].i0];
				wall->w1 = &out->verticesWS[walls[w].i1];
				wall->v0 = &out->verticesVS[walls[w].i0];
				wall->v1 = &out->verticesVS[walls[w].i1];
			}

			out->prevDrawFrame = 0;
			out->prevDrawFrame2 = 0;
		}

		update(sector->id);
	}
			
	// In the future, renderer sectors can be changed directly by INF, but for now just copy from the level data.
	// TODO: Currently all sector data is updated - get the "dirty" flag to work reliably so only partial data needs to be updated (textures).
	// TODO: Properly handle switch textures (after reverse-engineering of switch rendering is done).
	void TFE_Sectors_Fixed::update(u32 sectorId, u32 updateFlags)
	{
		TFE_ZONE("Sector Update");
		if (updateFlags == 0) { return; }

		LevelData* level = TFE_LevelAsset::getLevelData();
		Texture** textures = level->textures.data();

		Sector* sector = &level->sectors[sectorId];
		SectorWall* walls = level->walls.data() + sector->wallOffset;
		Vec2f* vertices = level->vertices.data() + sector->vtxOffset;

		RSector* out = &s_rsectors[sectorId];
		out->vertexCount = sector->vtxCount;
		out->wallCount = sector->wallCount;

		const SectorBaseHeight* baseHeight = TFE_Level::getBaseSectorHeight(sectorId);
		fixed16_16 ceilDelta  = floatToFixed16(8.0f * (sector->ceilAlt - baseHeight->ceilAlt));
		fixed16_16 floorDelta = floatToFixed16(8.0f * (sector->floorAlt - baseHeight->floorAlt));

		out->ambient.f16_16       = intToFixed16(sector->ambient);
		out->floorHeight.f16_16   = floatToFixed16(sector->floorAlt);
		out->ceilingHeight.f16_16 = floatToFixed16(sector->ceilAlt);
		out->secHeight.f16_16     = floatToFixed16(sector->secAlt);
		out->floorOffsetX.f16_16  = floatToFixed16(sector->floorTexture.offsetX);
		out->floorOffsetZ.f16_16  = floatToFixed16(sector->floorTexture.offsetY);
		out->ceilOffsetX.f16_16   = floatToFixed16(sector->ceilTexture.offsetX);
		out->ceilOffsetZ.f16_16   = floatToFixed16(sector->ceilTexture.offsetY);

		out->flags1        = sector->flags[0];
		out->flags2        = sector->flags[1];
		out->flags3        = sector->flags[2];
		out->floorTex      = texture_getFrame(textures[sector->floorTexture.texId]);
		out->ceilTex       = texture_getFrame(textures[sector->ceilTexture.texId]);
		
		TFE_ZONE_BEGIN(secVtx, "Sector Update Vertices");
		if (updateFlags & SEC_UPDATE_GEO)
		{
			for (s32 v = 0; v < out->vertexCount; v++)
			{
				out->verticesWS[v].x.f16_16 = floatToFixed16(vertices[v].x);
				out->verticesWS[v].z.f16_16 = floatToFixed16(vertices[v].z);
			}
		}
		TFE_ZONE_END(secVtx);
		
		TFE_ZONE_BEGIN(secWall, "Sector Update Walls");
		RWall* wall = out->walls;
		const fixed16_16 midTexelHeight = mul16(intToFixed16(8), floatToFixed16(sector->floorAlt - sector->ceilAlt));
		for (s32 w = 0; w < out->wallCount; w++, wall++)
		{
			wall->nextSector = (walls[w].adjoin >= 0) ? &s_rsectors[walls[w].adjoin] : nullptr;
			wall->mirror = walls[w].mirror;
			wall->mirrorWall = wall->nextSector ? &wall->nextSector->walls[wall->mirror] : nullptr;
			
			wall->topTex  = texture_getFrame(walls[w].top.texId  >= 0 ? textures[walls[w].top.texId]  : nullptr);
			wall->midTex  = texture_getFrame(walls[w].mid.texId  >= 0 ? textures[walls[w].mid.texId]  : nullptr);
			wall->botTex  = texture_getFrame(walls[w].bot.texId  >= 0 ? textures[walls[w].bot.texId]  : nullptr);
			wall->signTex = texture_getFrame(walls[w].sign.texId >= 0 ? textures[walls[w].sign.texId] : nullptr, walls[w].sign.frame);

			if (updateFlags & SEC_UPDATE_GEO)
			{
				const Vec2f offset = { vertices[walls[w].i1].x - vertices[walls[w].i0].x, vertices[walls[w].i1].z - vertices[walls[w].i0].z };
				wall->texelLength.f16_16 = floatToFixed16(8.0f * sqrtf(offset.x*offset.x + offset.z*offset.z));
			}

			// Prime with mid texture height, other heights will be computed as needed.
			wall->midTexelHeight.f16_16 = midTexelHeight;

			// Texture Offsets
			wall->topUOffset.f16_16 = floatToFixed16(8.0f * walls[w].top.offsetX);
			wall->topVOffset.f16_16 = floatToFixed16(8.0f * walls[w].top.offsetY);
			wall->midUOffset.f16_16 = floatToFixed16(8.0f * walls[w].mid.offsetX);
			wall->midVOffset.f16_16 = floatToFixed16(8.0f * walls[w].mid.offsetY);
			wall->botUOffset.f16_16 = floatToFixed16(8.0f * walls[w].bot.offsetX);
			wall->botVOffset.f16_16 = floatToFixed16(8.0f * walls[w].bot.offsetY);
			wall->signUOffset.f16_16 = floatToFixed16(8.0f * walls[w].sign.offsetX);
			wall->signVOffset.f16_16 = floatToFixed16(8.0f * walls[w].sign.offsetY);

			if (walls[w].flags[0] & WF1_TEX_ANCHORED)
			{
				wall->midVOffset.f16_16 -= floorDelta;
				wall->botVOffset.f16_16 -= floorDelta;
				
				const s32 nextId = walls[w].adjoin;
				const SectorBaseHeight* baseHeightNext = (nextId >= 0) ? TFE_Level::getBaseSectorHeight(nextId) : nullptr;
				const Sector* nextSrc = (nextId >= 0) ? &level->sectors[nextId] : nullptr;
				if (nextSrc)
				{
					// Handle next sector moving.
					wall->botVOffset.f16_16 -= floatToFixed16(8.0f * (baseHeightNext->floorAlt - nextSrc->floorAlt));
				}
				wall->topVOffset.f16_16 = -wall->topVOffset.f16_16 + (wall->topTex ? wall->topTex->height : 0);
			}
			if (walls[w].flags[0] & WF1_SIGN_ANCHORED)
			{
				wall->signVOffset.f16_16 -= floorDelta;
				const s32 nextId = walls[w].adjoin;
				const SectorBaseHeight* baseHeightNext = (nextId >= 0) ? TFE_Level::getBaseSectorHeight(nextId) : nullptr;
				const Sector* nextSrc = (nextId >= 0) ? &level->sectors[nextId] : nullptr;
				if (nextSrc)
				{
					// Handle next sector moving.
					wall->signVOffset.f16_16 -= floatToFixed16(8.0f * (baseHeightNext->floorAlt - nextSrc->floorAlt));
				}
			}

			wall->flags1 = walls[w].flags[0];
			wall->flags2 = walls[w].flags[1];
			wall->flags3 = walls[w].flags[2];

			wall->wallLight = walls[w].light;
		}
		TFE_ZONE_END(secWall);
	}
		
	void TFE_Sectors_Fixed::adjoin_setupAdjoinWindow(s32* winBot, s32* winBotNext, s32* winTop, s32* winTopNext, EdgePair* adjoinEdges, s32 adjoinCount)
	{
		TFE_ZONE("Setup Adjoin Window");

		// Note: This is pretty inefficient, especially at higher resolutions.
		// The column loops below can be adjusted to do the copy only in the required ranges.
		memcpy(&winTopNext[s_minScreenX], &winTop[s_minScreenX], s_width * 4);
		memcpy(&winBotNext[s_minScreenX], &winBot[s_minScreenX], s_width * 4);

		// Loop through each adjoin and setup the column range based on the edge pair and the parent
		// column range.
		for (s32 i = 0; i < adjoinCount; i++, adjoinEdges++)
		{
			const s32 x0 = adjoinEdges->x0;
			const s32 x1 = adjoinEdges->x1;

			const fixed16_16 ceil_dYdX = adjoinEdges->dyCeil_dx.f16_16;
			fixed16_16 y = adjoinEdges->yCeil0.f16_16;
			for (s32 x = x0; x <= x1; x++, y += ceil_dYdX)
			{
				s32 yPixel = round16(y);
				s32 yBot = winBotNext[x];
				s32 yTop = winTop[x];
				if (yPixel > yTop)
				{
					winTopNext[x] = (yPixel <= yBot) ? yPixel : yBot + 1;
				}
			}
			const fixed16_16 floor_dYdX = adjoinEdges->dyFloor_dx.f16_16;
			y = adjoinEdges->yFloor0.f16_16;
			for (s32 x = x0; x <= x1; x++, y += floor_dYdX)
			{
				s32 yPixel = round16(y);
				s32 yTop = winTop[x];
				s32 yBot = winBot[x];
				if (yPixel < yBot)
				{
					winBotNext[x] = (yPixel >= yTop) ? yPixel : yTop - 1;
				}
			}
		}
	}

	void TFE_Sectors_Fixed::adjoin_computeWindowBounds(EdgePair* adjoinEdges)
	{
		s32 yC = adjoinEdges->yPixel_C0;
		if (yC > s_windowMinY)
		{
			s_windowMinY = yC;
		}
		s32 yF = adjoinEdges->yPixel_F0;
		if (yF < s_windowMaxY)
		{
			s_windowMaxY = yF;
		}
		yC = adjoinEdges->yPixel_C1;
		if (yC > s_windowMaxCeil)
		{
			s_windowMaxCeil = yC;
		}
		yF = adjoinEdges->yPixel_F1;
		if (yF < s_windowMinFloor)
		{
			s_windowMinFloor = yF;
		}
		s_wallMaxCeilY = s_windowMinY - 1;
		s_wallMinFloorY = s_windowMaxY + 1;
		s_windowMinX = adjoinEdges->x0;
		s_windowMaxX = adjoinEdges->x1;
		s_windowTopPrev = s_windowTop;
		s_windowBotPrev = s_windowBot;
		s_prevSector = s_curSector;
	}

	void TFE_Sectors_Fixed::saveValues(s32 index)
	{
		SectorSaveValues* dst = &s_sectorStack[index];
		dst->curSector = s_curSector;
		dst->prevSector = s_prevSector;
		dst->depth1d.f16_16 = s_depth1d_Fixed;
		dst->windowX0 = s_windowX0;
		dst->windowX1 = s_windowX1;
		dst->windowMinY = s_windowMinY;
		dst->windowMaxY = s_windowMaxY;
		dst->windowMaxCeil = s_windowMaxCeil;
		dst->windowMinFloor = s_windowMinFloor;
		dst->wallMaxCeilY = s_wallMaxCeilY;
		dst->wallMinFloorY = s_wallMinFloorY;
		dst->windowMinX = s_windowMinX;
		dst->windowMaxX = s_windowMaxX;
		dst->windowTop = s_windowTop;
		dst->windowBot = s_windowBot;
		dst->windowTopPrev = s_windowTopPrev;
		dst->windowBotPrev = s_windowBotPrev;
		dst->sectorAmbient = s_sectorAmbient;
		dst->scaledAmbient = s_scaledAmbient;
		dst->sectorAmbientFraction = s_sectorAmbientFraction;
	}

	void TFE_Sectors_Fixed::restoreValues(s32 index)
	{
		const SectorSaveValues* src = &s_sectorStack[index];
		s_curSector = src->curSector;
		s_prevSector = src->prevSector;
		s_depth1d_Fixed = src->depth1d.f16_16;
		s_windowX0 = src->windowX0;
		s_windowX1 = src->windowX1;
		s_windowMinY = src->windowMinY;
		s_windowMaxY = src->windowMaxY;
		s_windowMaxCeil = src->windowMaxCeil;
		s_windowMinFloor = src->windowMinFloor;
		s_wallMaxCeilY = src->wallMaxCeilY;
		s_wallMinFloorY = src->wallMinFloorY;
		s_windowMinX = src->windowMinX;
		s_windowMaxX = src->windowMaxX;
		s_windowTop = src->windowTop;
		s_windowBot = src->windowBot;
		s_windowTopPrev = src->windowTopPrev;
		s_windowBotPrev = src->windowBotPrev;
		s_sectorAmbient = src->sectorAmbient;
		s_scaledAmbient = src->scaledAmbient;
		s_sectorAmbientFraction = src->sectorAmbientFraction;
	}

	// Switch from float to fixed.
	void TFE_Sectors_Fixed::subrendererChanged()
	{
		RSector* sector = s_rsectors;
		for (u32 i = 0; i < s_sectorCount; i++, sector++)
		{
			SecObject** obj = sector->objectList;
			for (s32 i = sector->objectCount - 1; i >= 0; i--, obj++)
			{
				SecObject* curObj = *obj;
				while (!curObj)
				{
					obj++;
					curObj = *obj;
				}

				// Convert from float to fixed.
				curObj->posWS.x.f16_16 = floatToFixed16(curObj->posWS.x.f32);
				curObj->posWS.y.f16_16 = floatToFixed16(curObj->posWS.y.f32);
				curObj->posWS.z.f16_16 = floatToFixed16(curObj->posWS.z.f32);
			}
		}
	}
}