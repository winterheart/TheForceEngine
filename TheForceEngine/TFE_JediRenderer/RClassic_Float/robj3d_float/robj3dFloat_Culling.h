#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Asset/modelAsset_Jedi.h>

namespace TFE_JediRenderer
{
	namespace RClassic_Float
	{
		extern Polygon* s_visPolygons[MAX_POLYGON_COUNT_3DO];
		s32 robj3d_backfaceCull(JediModel* model);
	}
}
