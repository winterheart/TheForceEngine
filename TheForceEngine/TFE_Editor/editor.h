#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Renderer/renderer.h>

namespace TFE_Editor
{
	void enable(TFE_Renderer* renderer);
	void disable();
	bool update(bool consoleOpen = false);
	bool render();

	void showPerf(u32 frame);
}
