#include "levelEditor.h"
#include "Help/helpWindow.h"
#include "levelEditorData.h"
#include <TFE_System/system.h>
#include <TFE_System/math.h>
#include <TFE_Renderer/renderer.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Archive/archive.h>
#include <TFE_Input/input.h>
#include <TFE_Game/geometry.h>
#include <TFE_Asset/assetSystem.h>
#include <TFE_Asset/imageAsset.h>
#include <TFE_Asset/levelAsset.h>
#include <TFE_Asset/infAsset.h>
#include <TFE_Asset/paletteAsset.h>
#include <TFE_Asset/colormapAsset.h>
#include <TFE_Asset/textureAsset.h>
#include <TFE_Asset/spriteAsset.h>
#include <TFE_Asset/fontAsset.h>
#include <TFE_Asset/modelAsset.h>
#include <TFE_Asset/gameMessages.h>
#include <TFE_Asset/levelList.h>
#include <TFE_Asset/levelObjectsAsset.h>
#include <TFE_Ui/markdown.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/fileutil.h>

//Help
#include "Help/helpWindow.h"

#include <TFE_Editor/Rendering/editorRender.h>
//Rendering 2d
#include <TFE_Editor/Rendering/lineDraw2d.h>
#include <TFE_Editor/Rendering/grid2d.h>
#include <TFE_Editor/Rendering/trianglesColor2d.h>
#include <TFE_Editor/Rendering/trianglesTextured2d.h>
//Rendering 3d
#include <TFE_Editor/Rendering/grid3d.h>
#include <TFE_Editor/Rendering/lineDraw3d.h>
#include <TFE_Editor/Rendering/trianglesColor3d.h>

// Triangulation
#include <TFE_Polygon/polygon.h>

// UI
#include <TFE_Ui/imGUI/imgui.h>

// Game
#include <TFE_Game/gameLoop.h>

#include <vector>
#include <algorithm>

// TODO: Add features to the file browser:
// 1. Filter does not exclude directories.
// 2. Ability to auto-set filter.
// 3. Ability to show only directories and select them.

using TrianglesColor3d::Tri3dTrans;

namespace LevelEditor
{
	#define TEXTURES_GOB_START_TEX 41
	
	enum EditorView
	{
		EDIT_VIEW_2D = 0,
		EDIT_VIEW_3D,
		EDIT_VIEW_3D_GAME,
		EDIT_VIEW_PLAY,
	};

	enum SectorDrawMode
	{
		SDM_WIREFRAME = 0,
		SDM_LIGHTING,
		SDM_TEXTURED_FLOOR,
		SDM_TEXTURED_CEIL,
		SDM_COUNT
	};

	enum LevelEditMode
	{
		LEDIT_DRAW = 1,
		LEDIT_VERTEX,
		LEDIT_WALL,
		LEDIT_SECTOR,
		LEDIT_ENTITY
	};

	enum BooleanMode
	{
		BOOL_SET = 0,
		BOOL_SUBTRACT,
		BOOL_INV_SUBTRACT,
		BOOL_ADD,
		BOOL_INTERSECT,
		BOOL_INV_INTERSECT,
		BOOL_COUNT
	};

	struct LevelFilePath
	{
		char gobName[64];
		char levelFilename[64];
		char levelName[64];
		char gobPath[TFE_MAX_PATH];
	};

	static s32 s_gridIndex = 5;
	static f32 c_gridSizeMap[] =
	{
		1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f, 64.0f, 128.0f, 256.0f
	};
	static const char* c_gridSizes[] =
	{
		"1",
		"2",
		"4",
		"8",
		"16",
		"32",
		"64",
		"128",
		"256",
	};

	static u32 s_recentCount = 16;	// 10 (just adding the rest for testing)
	static LevelFilePath s_recentLevels[16] =
	{
		{"DARK.GOB", "SECBASE.LEV",  "Secret Base",       "DARK.GOB"},
		{"DARK.GOB", "TALAY.LEV",    "Talay: Tak Base",   "DARK.GOB"},
		{"DARK.GOB", "SEWERS.LEV",   "Anoat City",        "DARK.GOB"},
		{"DARK.GOB", "TESTBASE.LEV", "Research Facility", "DARK.GOB"},
		{"DARK.GOB", "GROMAS.LEV",   "Gromas Mines",      "DARK.GOB"},
		{"DARK.GOB", "DTENTION.LEV", "Detention Center",  "DARK.GOB"},
		{"DARK.GOB", "RAMSHED.LEV",  "Ramsees Hed",       "DARK.GOB"},
		{"DARK.GOB", "ROBOTICS.LEV", "Robotics Facility", "DARK.GOB"},
		{"DARK.GOB", "NARSHADA.LEV", "Nar Shaddaa",       "DARK.GOB"},
		{"DARK.GOB", "JABSHIP.LEV",  "Jabba's Ship",      "DARK.GOB"},

		{"DARK.GOB", "IMPCITY.LEV",  "Imperial City",     "DARK.GOB"},
		{"DARK.GOB", "FUELSTAT.LEV", "Fuel Station",      "DARK.GOB"},
		{"DARK.GOB", "EXECUTOR.LEV", "The Executor",      "DARK.GOB"},
		{"DARK.GOB", "ARC.LEV",		 "The Arc Hammer",    "DARK.GOB"},

		{"DEMO1.GOB", "SECBASE.LEV", "Demo 1", "DEMO1.GOB"},
		{"DEMO3.GOB", "SECBASE.LEV", "Demo 3", "DEMO3.GOB"},
	};

	static const f32 c_gridInvisibleHeight = 10000.0f;
	
	static EditorView s_editView = EDIT_VIEW_2D;
	static bool s_showLowerLayers = true;
	static bool s_enableInfEditor = false;
		
	static char s_gobFile[TFE_MAX_PATH] = "";
	static char s_levelFile[TFE_MAX_PATH] = "";
	static char s_levelName[64] = "";
	static bool s_loadLevel = false;
	static bool s_showSectorColors = true;
	static TFE_Renderer* s_renderer;
	static void* s_editCtrlToolbarData = nullptr;
	static void* s_booleanToolbarData  = nullptr;
	static RenderTargetHandle s_view3d = nullptr;

	static EditorLevel* s_levelData = nullptr;

	static Vec2f s_editWinPos = { 0.0f, 67.0f };
	static Vec2f s_editWinSize;
	static Vec2f s_editWinMapCorner;

	static f32 s_gridSize = 32.0f;
	static f32 s_subGridSize = 32.0f;
	// 2D Camera
	static f32 s_zoom = 0.25f;
	static f32 s_zoomVisual = s_zoom;
	static f32 s_gridOpacity = 0.25f;
	static Vec2f s_offset = { 0 };
	static Vec2f s_offsetVis = { 0 };
	// 3D Camera
	struct Camera
	{
		Vec3f pos;
		Mat3  viewMtx;
		Mat4  projMtx;
		Mat4  invProj;

		f32 yaw = 0.0f;
		f32 pitch = 0.0f;
	};
	static Camera s_camera;

	static char s_layerMem[4 * 31];
	static char* s_layerStr[31];
	static const s32 s_layerMin = -15;
	static s32 s_layerIndex = 1 - s_layerMin;
	static s32 s_infoHeight = 300;

	// Editing
	static f32 s_gridHeight = 0.0f;
	static bool s_gridAutoAdjust = false;
	static bool s_showGridInSector = true;
	static bool s_drawStarted = false;
	static Vec3f s_drawPlaneNrm;
	static Vec3f s_drawPlaneOrg;
	static Vec2f s_drawBaseVtx[2];
	static bool s_moveVertex = false;
	static bool s_moveWall = false;
	static EditorSector s_newSector = {};
		
	// Sector Rendering
	static SectorDrawMode s_sectorDrawMode = SDM_WIREFRAME;
	static bool s_fullbright = false;

	static u32 s_editMode = LEDIT_DRAW;
	static u32 s_boolMode = BOOL_SET;
	static s32 s_selectedSector = -1;
	static s32 s_hoveredSector = -1;
	static s32 s_selectedWall = -1;
	static s32 s_hoveredWall = -1;
	static s32 s_selectedEntity = -1;
	static s32 s_hoveredEntity = -1;
	static s32 s_selectedEntitySector = -1;
	static s32 s_hoveredEntitySector = -1;
	static RayHitPart s_selectWallPart;
	static RayHitPart s_hoveredWallPart;
	static s32 s_selectedWallSector = -1;
	static s32 s_hoveredWallSector = -1;
	static s32 s_selectedVertex = -1;
	static s32 s_hoveredVertex = -1;
	static s32 s_selectedVertexSector = -1;
	static s32 s_hoveredVertexSector = -1;
	static RayHitPart s_selectVertexPart;
	static RayHitPart s_hoveredVertexPart;

	static Vec3f s_cursor3d = { 0 };
	static Vec3f s_cursorSnapped = { 0 };
	static Vec3f s_rayDir = { 0 };
	static s32 s_hideFrames = 0;
	static const Palette256* s_palette;
	static bool s_runLevel = false;

	static Archive* s_outGob = nullptr;

	// Error message
	static bool s_showError = false;
	static char s_errorMessage[TFE_MAX_PATH];

	void drawSectorPolygon(const EditorSector* sector, bool hover, u32 colorOverride = 0);
	void drawTexturedSectorPolygon(const EditorSector* sector, u32 color, bool floorTex);

	void loadLevel();
	void toolbarBegin();
	void toolbarEnd();
	void levelEditWinBegin();
	void levelEditWinEnd();
	void infoToolBegin(s32 height);
	void infoToolEnd();
	void browserBegin(s32 offset);
	void browserEnd();
	void messagePanel(ImVec2 pos);

	// Info panels
	void infoPanelMap();
	void infoPanelVertex();
	void infoPanelWall();
	void infoPanelInfWall(EditorSector* sector, u32 wallId);
	void infoPanelSector();
	void infoPanelEntity();
	// Browser panels
	void browseTextures();
	void browseEntities();

	void play(bool playFromDos);

	// Draw
	void drawSector3d(const EditorSector* sector, const SectorTriangles* poly, bool overlay = false, bool hover = false, bool lowerLayer = false);
	void drawSector3d_Lines(const EditorSector* sector, f32 width, u32 color, bool overlay = false, bool hover = false);
	void drawInfWalls3d(f32 width, u32 color);
	void drawWallColor(const EditorSector* sector, const Vec2f* vtx, const EditorWall* wall, const u32* color, bool blend = false, RayHitPart part = HIT_PART_UNKNOWN, bool showAllWallsOnBlend = true);

	// Error Handling
	bool isValidName(const char* name);
	void popupErrorMessage(const char* errorMessage);
	void showErrorPopup();

	// General viewport editing
	void deleteSector(EditorSector* sector);

	// 2D viewport editing
	void editSector2d(Vec2f worldPos, s32 sectorId);
	void editWalls2d(Vec2f worldPos);
	void editVertices2d(Vec2f worldPos);
	void editEntities2d(Vec2f worldPos);
	void editEntities2d(Vec2f worldPos);
	void editDrawSectors2d(Vec2f worldPos);
	bool editInsertVertex2d(Vec2f worldPos);
	bool editInsertVertex2d(Vec2f worldPos, s32 sectorId, s32 wallId);
	void clearNewSector();

	void splitSector(EditorSector* sector, Vec2f v0, Vec2f v1, u32 insideVertexCount = 0, const Vec2f* insideVtx = nullptr);

	void* loadGpuImage(const char* localPath)
	{
		char imagePath[TFE_MAX_PATH];
		TFE_Paths::appendPath(TFE_PathType::PATH_PROGRAM, localPath, imagePath, TFE_MAX_PATH);
		Image* image = TFE_Image::get(imagePath);
		if (image)
		{
			TextureGpu* editCtrlHandle = TFE_RenderBackend::createTexture(image->width, image->height, image->data);
			return TFE_RenderBackend::getGpuPtr(editCtrlHandle);
		}
		return nullptr;
	}
		
	void init(TFE_Renderer* renderer)
	{
		TFE_EditorRender::init();
		
		s_renderer = renderer;
		s_renderer->enableScreenClear(true);
				
		// Load UI images.
		s_editCtrlToolbarData = loadGpuImage("UI_Images/EditCtrl_32x6.png");
		s_booleanToolbarData  = loadGpuImage("UI_Images/Boolean_32x6.png");

		u32 idx = 0;
		for (s32 i = -15; i < 16; i++, idx += 4)
		{
			s_layerStr[i + 15] = &s_layerMem[idx];
			sprintf(s_layerStr[i + 15], "%d", i);
		}

		// Create a basic 3d camera.
		s_camera.pos = { 0.0f, -2.0f, 0.0f };
		s_camera.yaw = 0.0f;
		s_camera.pitch = 0.0f;
	}

	void disable()
	{
		Archive::deleteCustomArchive(s_outGob);
		TFE_EditorRender::destroy();

		s_outGob = nullptr;
	}
		
	bool render3dView()
	{
		if (s_runLevel)
		{
			GameTransition trans = TFE_GameLoop::update(false);
			TFE_GameLoop::draw();

			if (trans == TRANS_QUIT || trans == TRANS_TO_AGENT_MENU || trans == TRANS_NEXT_LEVEL)
			{
				s_runLevel = false;
				TFE_Input::enableRelativeMode(false);
				s_renderer->enableScreenClear(true);
			}

			return true;
		}
		return false;
	}

	bool isFullscreen()
	{
		return false;
	}

	bool drawToolbarButton(void* ptr, u32 imageId, bool highlight)
	{
		const f32 imageHeightScale = 1.0f / 192.0f;
		const f32 y0 = f32(imageId) * 32.0f;
		const f32 y1 = y0 + 32.0f;

		ImGui::PushID(imageId);
		bool res = ImGui::ImageButton(ptr, ImVec2(32, 32), ImVec2(0.0f, y0 * imageHeightScale), ImVec2(1.0f, y1 * imageHeightScale), 0, ImVec4(0, 0, 0, highlight ? 0.75f : 0.25f), ImVec4(1, 1, 1, 1));
		ImGui::PopID();

		return res;
	}

	void cameraControl2d(s32 mx, s32 my)
	{
		f32 moveSpd = s_zoomVisual * f32(960.0 * TFE_System::getDeltaTime());
		if (TFE_Input::keyDown(KEY_W))
		{
			s_offset.z -= moveSpd;
		}
		else if (TFE_Input::keyDown(KEY_S))
		{
			s_offset.z += moveSpd;
		}

		if (TFE_Input::keyDown(KEY_A))
		{
			s_offset.x -= moveSpd;
		}
		else if (TFE_Input::keyDown(KEY_D))
		{
			s_offset.x += moveSpd;
		}

		// Mouse scrolling.
		if (TFE_Input::mouseDown(MBUTTON_RIGHT))
		{
			s32 dx, dy;
			TFE_Input::getMouseMove(&dx, &dy);

			s_offset.x -= f32(dx) * s_zoomVisual;
			s_offset.z -= f32(dy) * s_zoomVisual;
		}

		s32 dx, dy;
		TFE_Input::getMouseWheel(&dx, &dy);
		if (dy != 0)
		{
			// We want to zoom into the mouse position.
			s32 relX = s32(mx - s_editWinMapCorner.x);
			s32 relY = s32(my - s_editWinMapCorner.z);
			// Old position in world units.
			Vec2f worldPos;
			worldPos.x = s_offset.x + f32(relX) * s_zoomVisual;
			worldPos.z = s_offset.z + f32(relY) * s_zoomVisual;

			s_zoom = std::max(s_zoom - f32(dy) * s_zoom * 0.1f, 0.001f);
			s_zoomVisual = floorf(s_zoom * 1000.0f) * 0.001f;

			// We want worldPos to stay put as we zoom
			Vec2f newWorldPos;
			newWorldPos.x = s_offset.x + f32(relX) * s_zoomVisual;
			newWorldPos.z = s_offset.z + f32(relY) * s_zoomVisual;
			s_offset.x += (worldPos.x - newWorldPos.x);
			s_offset.z += (worldPos.z - newWorldPos.z);
		}
	}

	// return true to use relative mode.
	bool cameraControl3d(s32 mx, s32 my)
	{
		// Movement
		f32 moveSpd = f32(16.0 * TFE_System::getDeltaTime());
		if (TFE_Input::keyDown(KEY_LSHIFT) || TFE_Input::keyDown(KEY_RSHIFT))
		{
			moveSpd *= 10.0f;
		}

		if (TFE_Input::keyDown(KEY_W))
		{
			s_camera.pos.x -= s_camera.viewMtx.m2.x * moveSpd;
			s_camera.pos.y -= s_camera.viewMtx.m2.y * moveSpd;
			s_camera.pos.z -= s_camera.viewMtx.m2.z * moveSpd;
		}
		else if (TFE_Input::keyDown(KEY_S))
		{
			s_camera.pos.x += s_camera.viewMtx.m2.x * moveSpd;
			s_camera.pos.y += s_camera.viewMtx.m2.y * moveSpd;
			s_camera.pos.z += s_camera.viewMtx.m2.z * moveSpd;
		}

		if (TFE_Input::keyDown(KEY_A))
		{
			s_camera.pos.x -= s_camera.viewMtx.m0.x * moveSpd;
			s_camera.pos.y -= s_camera.viewMtx.m0.y * moveSpd;
			s_camera.pos.z -= s_camera.viewMtx.m0.z * moveSpd;
		}
		else if (TFE_Input::keyDown(KEY_D))
		{
			s_camera.pos.x += s_camera.viewMtx.m0.x * moveSpd;
			s_camera.pos.y += s_camera.viewMtx.m0.y * moveSpd;
			s_camera.pos.z += s_camera.viewMtx.m0.z * moveSpd;
		}

		// Turning.
		if (TFE_Input::mouseDown(MBUTTON_RIGHT))
		{
			s32 dx, dy;
			TFE_Input::getMouseMove(&dx, &dy);

			const f32 turnSpeed = 0.01f;
			s_camera.yaw += f32(dx) * turnSpeed;
			s_camera.pitch -= f32(dy) * turnSpeed;

			if (s_camera.yaw < 0.0f) { s_camera.yaw += PI * 2.0f; }
			else { s_camera.yaw = fmodf(s_camera.yaw, PI * 2.0f); }

			if (s_camera.pitch < -1.55f) { s_camera.pitch = -1.55f; }
			else if (s_camera.pitch > 1.55f) { s_camera.pitch = 1.55f; }

			s_cursor3d = { 0 };
			return true;
		}
		else if (TFE_Input::relativeModeEnabled())
		{
			s_hideFrames = 3;
		}
		return false;
	}
		
	void editWinControls2d(s32 mx, s32 my)
	{
		// We want to zoom into the mouse position.
		s32 relX = s32(mx - s_editWinMapCorner.x);
		s32 relY = s32(my - s_editWinMapCorner.z);

		// Compute the cursor position in 3D space.
		Vec2f worldPos;
		worldPos.x =   s_offset.x + f32(relX) * s_zoomVisual;
		worldPos.z = -(s_offset.z + f32(relY) * s_zoomVisual);
		s32 curSector = LevelEditorData::findSector(s_layerIndex + s_layerMin, &worldPos);

		s_cursor3d.x = worldPos.x;
		s_cursor3d.y = (curSector >= 0) ? s_levelData->sectors[curSector].floorAlt : s_gridHeight;
		s_cursor3d.z = worldPos.z;
		
		// Editing
		if (s_editMode != LEDIT_SECTOR) { s_selectedSector = -1; }
		if (s_editMode != LEDIT_ENTITY) { s_selectedEntity = -1; }

		if (TFE_Input::keyPressed(KEY_INSERT))
		{
			editInsertVertex2d(worldPos);
		}

		editSector2d(worldPos, curSector);
		editWalls2d(worldPos);
		editVertices2d(worldPos);
		editEntities2d(worldPos);
		editDrawSectors2d(worldPos);

		// Compute the visual view offset.
		s_offsetVis.x = floorf(s_offset.x * 100.0f) * 0.01f;
		s_offsetVis.z = floorf(s_offset.z * 100.0f) * 0.01f;
	}

	Vec4f transform(const Mat4& mtx, const Vec4f& vec)
	{
		return { TFE_Math::dot(&mtx.m0, &vec), TFE_Math::dot(&mtx.m1, &vec), TFE_Math::dot(&mtx.m2, &vec), TFE_Math::dot(&mtx.m3, &vec) };
	}

	// Get the world direction from the current screen pixel coordinates.
	Vec3f getWorldDir(s32 x, s32 y, s32 rtWidth, s32 rtHeight)
	{
		// Given a plane at Z = 1.0, figure out the view space position of the pixel.
		const Vec4f posScreen =
		{
            f32(x) / f32(rtWidth)  * 2.0f - 1.0f,
		    f32(y) / f32(rtHeight) * 2.0f - 1.0f,
		   -1.0f, 1.0f
		};
		const Vec4f posView = transform(s_camera.invProj, posScreen);
		const Mat3  viewT   = TFE_Math::transpose(s_camera.viewMtx);

		const Vec3f posView3 = { posView.x, posView.y, posView.z };
		const Vec3f posRelWorld =	// world relative to the camera position.
		{
			TFE_Math::dot(&posView3, &viewT.m0),
			TFE_Math::dot(&posView3, &viewT.m1),
			TFE_Math::dot(&posView3, &viewT.m2)
		};
		return TFE_Math::normalize(&posRelWorld);
	}

	Vec3f rayGridPlaneHit(const Vec3f& origin, const Vec3f& rayDir)
	{
		Vec3f hit = { 0 };
		if (fabsf(rayDir.y) < FLT_EPSILON) { return hit; }

		f32 s = (s_gridHeight - origin.y) / rayDir.y;
		if (s <= 0) { return hit; }

		hit.x = origin.x + s*rayDir.x;
		hit.y = origin.y + s*rayDir.y;
		hit.z = origin.z + s*rayDir.z;
		return hit;
	}

	bool rayPlaneHit(const Vec3f& origin, const Vec3f& rayDir, Vec3f* hit)
	{
		const f32 den = TFE_Math::dot(&rayDir, &s_drawPlaneNrm);
		if (fabsf(den) <= FLT_EPSILON) { return false; }

		Vec3f offset = { s_drawPlaneOrg.x - origin.x, s_drawPlaneOrg.y - origin.y, s_drawPlaneOrg.z - origin.z };
		f32 s = TFE_Math::dot(&offset, &s_drawPlaneNrm) / den;
		if (s < 0.0f) { return false; }

		*hit = { origin.x + rayDir.x * s, origin.y + rayDir.y * s, origin.z + rayDir.z * s };
		return true;
	}

	static const f32 c_vertexMerge = 0.005f;

	bool vec2Equals(const Vec2f& a, const Vec2f& b)
	{
		return fabsf(a.x - b.x) < c_vertexMerge && fabsf(a.z - b.z) < c_vertexMerge;
	}

	// Is the position "near" a wall that it can snap to?
	bool snapToGeometry(Vec3f* pos, bool view2d)
	{
		if (view2d)
		{
			Vec2f worldPos = { pos->x, pos->z };
			s32 sectorId = LevelEditorData::findSector(s_layerIndex + s_layerMin, &worldPos);
			s32 wallId = LevelEditorData::findClosestWall(&sectorId, s_layerIndex + s_layerMin, &worldPos, s_zoomVisual * 16.0f);
			if (sectorId < 0 || wallId < 0) { return false; }

			const EditorSector* sector = &s_levelData->sectors[sectorId];
			const EditorWall* wall = &sector->walls[wallId];
			const Vec2f* vtx = sector->vertices.data();

			const Vec2f& v0 = vtx[wall->i0];
			const Vec2f& v1 = vtx[wall->i1];

			Vec2f linePos;
			Geometry::closestPointOnLineSegment(v0, v1, worldPos, &linePos);
			f32 maxDistSq = s_zoomVisual * 8.0f;
			maxDistSq *= maxDistSq;
			// First check the vertices
			if (TFE_Math::distanceSq(&v0, &worldPos) < maxDistSq)
			{
				s_cursorSnapped.x = v0.x;
				s_cursorSnapped.z = v0.z;
			}
			else if (TFE_Math::distanceSq(&v1, &worldPos) < maxDistSq)
			{
				s_cursorSnapped.x = v1.x;
				s_cursorSnapped.z = v1.z;
			}
			else
			{
				f32 minDistSq = maxDistSq;
				Vec2f minLinePos;

				// Holding Alt will disable grid line snapping.
				if (!TFE_Input::keyDown(KEY_LALT) && !TFE_Input::keyDown(KEY_RALT))
				{
					// Next check to see if we can snap to either X or Z while staying on the line.
					const f32 gridScale = s_gridSize / s_subGridSize;

					// Test the nearest grid line versus line intersections and see if they are close enough
					// to snap to.
					Vec2f posGridSpace = { worldPos.x / gridScale, worldPos.z / gridScale };
					posGridSpace.x = floorf(posGridSpace.x + 0.5f) * gridScale;
					posGridSpace.z = floorf(posGridSpace.z + 0.5f) * gridScale;

					const f32 dx = v1.x - v0.x;
					const f32 dz = v1.z - v0.z;
					if (fabsf(dx) > 0.005f)
					{
						const f32 s = (posGridSpace.x - v0.x) / dx;
						const Vec2f snappedPos = { v0.x + s * dx, v0.z + s * dz };
						const f32 distSq = TFE_Math::distanceSq(&snappedPos, &worldPos);
						if (distSq < minDistSq)
						{
							minDistSq = distSq;
							minLinePos = snappedPos;
						}
					}
					if (fabsf(dz) > 0.005f)
					{
						const f32 s = (posGridSpace.z - v0.z) / dz;
						const Vec2f snappedPos = { v0.x + s * dx, v0.z + s * dz };
						const f32 distSq = TFE_Math::distanceSq(&snappedPos, &worldPos);
						if (distSq < minDistSq)
						{
							minDistSq = distSq;
							minLinePos = snappedPos;
						}
					}
				}

				if (minDistSq < maxDistSq)
				{
					// Snap to an intersection of the grid and the line.
					s_cursorSnapped.x = minLinePos.x;
					s_cursorSnapped.z = minLinePos.z;
				}
				else
				{
					// Otherwise anywhere on the line.
					s_cursorSnapped.x = linePos.x;
					s_cursorSnapped.z = linePos.z;
				}
			}
			return true;
		}
		return false;
	}

	void snapToGrid(Vec3f* pos, bool view2d)
	{
		// Holding alt disables snap to grid.
		if (TFE_Input::keyDown(KEY_LALT) || TFE_Input::keyDown(KEY_RALT))
		{
			return;
		}

		if (view2d)
		{
			f32 gridScale = s_gridSize / s_subGridSize;

			Vec2f posGridSpace = { pos->x / gridScale, pos->z / gridScale };
			pos->x = floorf(posGridSpace.x + 0.5f) * gridScale;
			pos->z = floorf(posGridSpace.z + 0.5f) * gridScale;
		}
		else
		{
			Vec2f posGridSpace = { pos->x, pos->z };
			pos->x = floorf(posGridSpace.x + 0.5f);
			pos->z = floorf(posGridSpace.z + 0.5f);
		}
	}

	// Given a ray (origin + dir); snap the distance to the grid.
	// i.e. given the previous and next intersection points, find the closest.
	// TODO: Change to find where segment intersects with grid points while moving along the direction.
	f32 snapToGrid(const Vec3f* pos, const Vec3f* dir, f32 dist, bool view2d)
	{
		// Holding alt disables snap to grid.
		if (TFE_Input::keyDown(KEY_LALT) || TFE_Input::keyDown(KEY_RALT))
		{
			return dist;
		}

		f32 gridScale;
		if (view2d)
		{
			gridScale = s_gridSize / s_subGridSize;
		}
		else
		{
			gridScale = 1.0f;
		}

		// get nearest X intersection.
		f32 dX = FLT_MAX;
		f32 dZ = FLT_MAX;

		if (fabsf(dir->x) > FLT_EPSILON)
		{
			f32 curX = pos->x + dist * dir->x;
			f32 gridSpaceX = curX / gridScale;
			f32 snapX = floorf(gridSpaceX + 0.5f) * gridScale;

			dX = (snapX - pos->x) / dir->x;
		}
		if (fabsf(dir->z) > FLT_EPSILON)
		{
			f32 curZ = pos->z + dist * dir->z;
			f32 gridSpaceZ = curZ / gridScale;
			f32 snapZ = floorf(gridSpaceZ + 0.5f) * gridScale;

			dZ = (snapZ - pos->z) / dir->z;
		}

		if (dX < FLT_MAX && fabsf(dX - dist) < fabsf(dZ - dist))
		{
			return dX;
		}
		else if (dZ < FLT_MAX)
		{
			return dZ;
		}
		return dist;
	}

	void moveVertices(s32 sectorId, s32 wallIndex, const Vec2f& vtxPos, const Vec2f& newPos)
	{
		if (wallIndex < 0) { return; }

		EditorSector* sectorNext = &s_levelData->sectors[sectorId];
		sectorNext->needsUpdate = true;

		EditorWall& wall = sectorNext->walls[wallIndex];
		if (vec2Equals(vtxPos, sectorNext->vertices[wall.i0]))
		{
			sectorNext->vertices[wall.i0] = newPos;
		}
		else if (vec2Equals(vtxPos, sectorNext->vertices[wall.i1]))
		{
			sectorNext->vertices[wall.i1] = newPos;
		}
	}

	void handleWallMove(bool view2d)
	{
		if (s_selectedWall < 0)
		{
			s_moveWall = false;
			return;
		}

		EditorSector* sector = s_levelData->sectors.data() + s_selectedWallSector;
		const EditorWall* wall = sector->walls.data() + s_selectedWall;
		Vec2f* vtx = sector->vertices.data();
		sector->needsUpdate = true;
				
		Vec3f hit;
		if (view2d)
		{
			hit = s_cursor3d;
		}
		else
		{
			Vec3f dir = { s_cursor3d.x - s_camera.pos.x, s_cursor3d.y - s_camera.pos.y, s_cursor3d.z - s_camera.pos.z };
			dir = TFE_Math::normalize(&dir);
			if (!rayPlaneHit(s_camera.pos, dir, &hit)) { return; }
		}
		
		// Get the wall normal.
		const s32 i0 = wall->i0;
		const s32 i1 = wall->i1;
		Vec2f* v0 = &vtx[i0];
		Vec2f* v1 = &vtx[i1];
		
		Vec3f normal;
		normal.x = -(v1->z - v0->z);
		normal.y =   0.0f;
		normal.z =   v1->x - v0->x;
		normal = TFE_Math::normalize(&normal);

		// TODO: This snapping doesn't actually work in 2D - though it is fine in 3D, revisit.
		Vec3f offset = { hit.x - s_drawPlaneOrg.x, hit.y - s_drawPlaneOrg.y, hit.z - s_drawPlaneOrg.z };
		f32   dist   = TFE_Math::dot(&offset, &normal);
		dist = snapToGrid(&s_drawPlaneOrg, &normal, dist, view2d);

		Vec2f vtx0 = *v0;
		Vec2f vtx1 = *v1;

		// now compute the movement along the normal direction.
		*v0 = { s_drawBaseVtx[0].x + normal.x * dist, s_drawBaseVtx[0].z + normal.z * dist };
		*v1 = { s_drawBaseVtx[1].x + normal.x * dist, s_drawBaseVtx[1].z + normal.z * dist };

		// Next handle any sectors across adjoins that have been moved.
		const size_t wallCount = sector->walls.size();
		for (size_t w = 0; w < wallCount; w++)
		{
			if ((sector->walls[w].i0 == i0 || sector->walls[w].i1 == i0) && sector->walls[w].adjoin >= 0)
			{
				moveVertices(sector->walls[w].adjoin, sector->walls[w].mirror, vtx0, *v0);
			}
			if ((sector->walls[w].i0 == i1 || sector->walls[w].i1 == i1) && sector->walls[w].adjoin >= 0)
			{
				moveVertices(sector->walls[w].adjoin, sector->walls[w].mirror, vtx1, *v1);
			}
		}

		LevelEditorData::updateSectors();
	}

	void handleVertexMove(bool view2d)
	{
		if (s_selectedVertex < 0)
		{
			s_moveVertex = false;
			return;
		}

		// first determine the real hit location on the edit plane.
		Vec3f hit;
		if (view2d)
		{
			hit = s_cursor3d;
		}
		else
		{
			Vec3f dir = { s_cursor3d.x - s_camera.pos.x, s_cursor3d.y - s_camera.pos.y, s_cursor3d.z - s_camera.pos.z };
			dir = TFE_Math::normalize(&dir);
			if (!rayPlaneHit(s_camera.pos, dir, &hit)) { return; }
		}
		snapToGrid(&hit, view2d);

		EditorSector& sector = s_levelData->sectors[s_selectedVertexSector];
		sector.needsUpdate = true;

		Vec2f vtxPos = sector.vertices[s_selectedVertex];
		sector.vertices[s_selectedVertex] = { hit.x, hit.z };

		// Next handle any sectors across adjoins that have been moved.
		const size_t wallCount = sector.walls.size();
		for (size_t w = 0; w < wallCount; w++)
		{
			if ((sector.walls[w].i0 == s_selectedVertex || sector.walls[w].i1 == s_selectedVertex) && sector.walls[w].adjoin >= 0)
			{
				moveVertices(sector.walls[w].adjoin, sector.walls[w].mirror, vtxPos, { hit.x,hit.z });
			}
		}

		LevelEditorData::updateSectors();
	}

	void editWinControls3d(s32 mx, s32 my, s32 rtWidth, s32 rtHeight)
	{
		s_hoveredSector = -1;
		s_hoveredWall = -1;
		s_hoveredVertex = -1;
		s_hoveredVertexSector = -1;
		s_hoveredEntity = -1;
		if (!TFE_Input::relativeModeEnabled() && s_hideFrames == 0)
		{
			s_rayDir = getWorldDir(mx - (s32)s_editWinMapCorner.x, my - (s32)s_editWinMapCorner.z, rtWidth, rtHeight);

			const Ray ray = { s_camera.pos, s_rayDir, -1, 1000.0f, s_editMode == LEDIT_ENTITY, s_layerIndex + s_layerMin };
			RayHitInfoLE hitInfo;
			if (LevelEditorData::traceRay(&ray, &hitInfo))
			{
				s_cursor3d = hitInfo.hitPoint;
				
				if (s_editMode == LEDIT_SECTOR)
				{
					if (TFE_Input::mousePressed(MBUTTON_LEFT))
					{
						s_selectedSector = hitInfo.hitSectorId;
					}
					else
					{
						s_hoveredSector = hitInfo.hitSectorId;
					}
				}
				else if (s_editMode == LEDIT_WALL && hitInfo.hitWallId >= 0)
				{
					if (TFE_Input::mousePressed(MBUTTON_LEFT))
					{
						s_selectedWallSector = hitInfo.hitSectorId;
						s_selectedWall = hitInfo.hitWallId;
						s_selectWallPart = hitInfo.hitPart;

						if (!s_moveWall)
						{
							s_moveWall = true;
							// get the plane...
							s_drawPlaneNrm = { 0.0f, 1.0f, 0.0f };
							s_drawPlaneOrg = s_cursor3d;

							EditorWall* wall = &s_levelData->sectors[s_selectedWallSector].walls[s_selectedWall];
							s_drawBaseVtx[0] = s_levelData->sectors[s_selectedWallSector].vertices[wall->i0];
							s_drawBaseVtx[1] = s_levelData->sectors[s_selectedWallSector].vertices[wall->i1];
						}
					}
					else if (!s_moveWall)
					{
						s_hoveredWallSector = hitInfo.hitSectorId;
						s_hoveredWall = hitInfo.hitWallId;
						s_hoveredWallPart = hitInfo.hitPart;
					}
				}
				else if (s_editMode == LEDIT_WALL)
				{
					// In the last case, the ray hit a surface but not a wall.
					// Since this is wall edit mode, see if there is a "hidden" wall close enough to select (i.e. an adjoin with no mask wall or upper/lower part).
					const EditorSector* sector = s_levelData->sectors.data() + hitInfo.hitSectorId;
					const EditorWall* wall = sector->walls.data();
					const Vec2f* vtx = sector->vertices.data();
					const u32 wallCount = (u32)sector->walls.size();

					const Vec2f hitPoint2d = { hitInfo.hitPoint.x, hitInfo.hitPoint.z };
					const f32 distFromCam = TFE_Math::distance(&hitInfo.hitPoint, &s_camera.pos);
					const f32 maxDist = distFromCam * 64.0f / f32(rtHeight);
					const f32 maxDistSq = maxDist * maxDist;

					f32 closestDistSq = maxDistSq;
					s32 closestWall = -1;
					for (u32 w = 0; w < wallCount; w++, wall++)
					{
						if (wall->adjoin < 0) { continue; }
						// Test to see if we should select the wall parts themselves.
						EditorSector* next = s_levelData->sectors.data() + wall->adjoin;
						if ((next->floorAlt < sector->floorAlt - 0.1f && hitInfo.hitPart == HIT_PART_FLOOR) || (next->ceilAlt > sector->ceilAlt + 0.1f && hitInfo.hitPart == HIT_PART_CEIL)) { continue; }

						Vec2f closest;
						Geometry::closestPointOnLineSegment(vtx[wall->i0], vtx[wall->i1], hitPoint2d, &closest);
						const Vec2f diff = { closest.x - hitPoint2d.x, closest.z - hitPoint2d.z };
						const f32 distSq = TFE_Math::dot(&diff, &diff);
						if (distSq < closestDistSq)
						{
							closestDistSq = distSq;
							closestWall = s32(w);
						}
					}
					if (closestWall >= 0)
					{
						if (TFE_Input::mousePressed(MBUTTON_LEFT))
						{
							s_selectedWallSector = hitInfo.hitSectorId;
							s_selectedWall = closestWall;
							s_selectWallPart = HIT_PART_MID;
						}
						else
						{
							s_hoveredWallSector = hitInfo.hitSectorId;
							s_hoveredWall = closestWall;
							s_hoveredWallPart = HIT_PART_MID;
						}
					}
					else if (TFE_Input::mousePressed(MBUTTON_LEFT))
					{
						s_selectedWall = -1;
					}
				}
				else if (s_editMode == LEDIT_VERTEX)
				{
					s_hoveredVertexSector = hitInfo.hitSectorId;

					// Check for the nearest vertex in the current sector.
					// In the last case, the ray hit a surface but not a wall.
					// Since this is wall edit mode, see if there is a "hidden" wall close enough to select (i.e. an adjoin with no mask wall or upper/lower part).
					const EditorSector* sector = s_levelData->sectors.data() + hitInfo.hitSectorId;
					const Vec2f* vtx = sector->vertices.data();
					const u32 vtxCount = (u32)sector->vertices.size();

					const f32 distFromCam = TFE_Math::distance(&hitInfo.hitPoint, &s_camera.pos);
					const f32 maxDist = distFromCam * 64.0f / f32(rtHeight);
					f32 minDistSq = maxDist * maxDist;
					s32 closestVtx = -1;
					RayHitPart closestPart = HIT_PART_FLOOR;

					for (u32 v = 0; v < vtxCount; v++)
					{
						// Check against the floor and ceiling vertex of each vertex.
						const Vec3f floorVtx = { vtx[v].x, sector->floorAlt, vtx[v].z };
						const Vec3f ceilVtx  = { vtx[v].x, sector->ceilAlt,  vtx[v].z };

						const f32 floorDistSq = TFE_Math::distanceSq(&floorVtx, &hitInfo.hitPoint);
						const f32 ceilDistSq  = TFE_Math::distanceSq(&ceilVtx, &hitInfo.hitPoint);
						if (floorDistSq < minDistSq)
						{
							minDistSq = floorDistSq;
							closestVtx = v;
							closestPart = HIT_PART_FLOOR;
						}
						if (ceilDistSq < minDistSq)
						{
							minDistSq = ceilDistSq;
							closestVtx = v;
							closestPart = HIT_PART_CEIL;
						}
					}

					if (closestVtx >= 0)
					{
						if (TFE_Input::mousePressed(MBUTTON_LEFT))
						{
							s_selectedVertexSector = hitInfo.hitSectorId;
							s_selectedVertex = closestVtx;
							s_selectVertexPart = closestPart;
							s_moveVertex = true;
							// get the plane...
							s_drawPlaneNrm = { 0.0f, 1.0f, 0.0f };
							s_drawPlaneOrg = s_cursor3d;
						}
						else if (!s_moveVertex)
						{
							s_hoveredVertex = closestVtx;
							s_hoveredVertexPart = closestPart;
						}
					}
					else if (TFE_Input::mousePressed(MBUTTON_LEFT) && !s_moveVertex)
					{
						s_selectedVertex = -1;
					}
				}
				else if (s_editMode == LEDIT_ENTITY)
				{
					if (TFE_Input::mousePressed(MBUTTON_LEFT))
					{
						s_selectedEntitySector = hitInfo.hitSectorId;
						s_selectedEntity = hitInfo.hitObjectId;
					}
					s_hoveredEntity = hitInfo.hitObjectId;
					s_hoveredEntitySector = hitInfo.hitSectorId;
				}

				if (s_editMode != LEDIT_SECTOR) { s_selectedSector = -1; }
				if (s_editMode != LEDIT_WALL)   { s_selectedWall = -1;   }
				if (s_editMode != LEDIT_VERTEX) { s_selectedVertex = -1; }
				if (s_editMode != LEDIT_ENTITY) { s_selectedEntity = -1; }
			}
			else if (TFE_Input::mousePressed(MBUTTON_LEFT))
			{
				s_selectedSector = -1;
				s_selectedWall = -1;
				s_selectedVertex = -1;

				// Intersect the ray with the grid plane.
				s_cursor3d = rayGridPlaneHit(s_camera.pos, s_rayDir);
			}
			else
			{
				// Intersect the ray with the grid plane.
				s_cursor3d = rayGridPlaneHit(s_camera.pos, s_rayDir);
			}
			
			if (s_editMode == LEDIT_DRAW)
			{
				//handleDraw(s_cursor3d);
			}
		}

		if (s_hideFrames > 0) { s_hideFrames--; }
	}

	// Controls for the main level edit window.
	// This will only be called if the mouse is inside the area and its not covered by popups (like combo boxes).
	void editWinControls(s32 rtWidth, s32 rtHeight)
	{
		// Edit controls.
		s32 mx, my;
		TFE_Input::getMousePos(&mx, &my);
		if (!TFE_Input::relativeModeEnabled() && (mx < s_editWinPos.x || mx >= s_editWinPos.x + s_editWinSize.x || my < s_editWinPos.z || my >= s_editWinPos.z + s_editWinSize.z))
		{
			return;
		}

		bool relEnabled = false;
		if (s_editView == EDIT_VIEW_2D)
		{
			cameraControl2d(mx, my);
			editWinControls2d(mx, my);
		}
		else
		{
			relEnabled = cameraControl3d(mx, my);
			editWinControls3d(mx, my, rtWidth, rtHeight);
			TFE_Input::enableRelativeMode(relEnabled);
		}

		// Vertex move
		if (TFE_Input::mouseDown(MBUTTON_LEFT) && s_moveVertex && s_editMode == LEDIT_VERTEX && !relEnabled)
		{
			handleVertexMove(s_editView == EDIT_VIEW_2D);
		}
		else if (s_moveVertex)
		{
			LevelEditorData::updateSectors();
			s_moveVertex = false;
		}

		if (TFE_Input::mouseDown(MBUTTON_LEFT) && s_moveWall && s_editMode == LEDIT_WALL && !relEnabled)
		{
			handleWallMove(s_editView == EDIT_VIEW_2D);
		}
		else if (s_moveWall)
		{
			LevelEditorData::updateSectors();
			s_moveWall = false;
		}

		// Set the current grid height to the cursor height.
		if ((TFE_Input::keyDown(KEY_LCTRL) || TFE_Input::keyDown(KEY_RCTRL)) && TFE_Input::keyPressed(KEY_G))
		{
			s_gridHeight = s_cursor3d.y;
		}
	}

	void drawVertex(const Vec2f* vtx, const f32 scale, const u32* color)
	{
		Vec2f quad[8];
		// Outer Quad
		quad[0].x = vtx->x - scale;
		quad[0].z = vtx->z - scale;

		quad[1].x = quad[0].x + 2.0f * scale;
		quad[1].z = quad[0].z;

		quad[2].x = quad[0].x + 2.0f * scale;
		quad[2].z = quad[0].z + 2.0f * scale;

		quad[3].x = quad[0].x;
		quad[3].z = quad[0].z + 2.0f * scale;

		// Inner Quad
		quad[4].x = vtx->x - scale * 0.5f;
		quad[4].z = vtx->z - scale * 0.5f;

		quad[5].x = quad[4].x + 2.0f * scale * 0.5f;
		quad[5].z = quad[4].z;

		quad[6].x = quad[4].x + 2.0f * scale * 0.5f;
		quad[6].z = quad[4].z + 2.0f * scale * 0.5f;

		quad[7].x = quad[4].x;
		quad[7].z = quad[4].z + 2.0f * scale * 0.5f;

		Vec2f tri[12] = { quad[0], quad[1], quad[2], quad[0], quad[2], quad[3],
						  quad[4], quad[5], quad[6], quad[4], quad[6], quad[7] };
		TriColoredDraw2d::addTriangles(4, tri, color);
	}
		
	void highlightWall(bool selected)
	{
		if ((selected && s_selectedWall < 0) || (!selected && s_hoveredWall < 0)) { return; }

		const s32 wallId = s_selectedWall >= 0 && selected ? s_selectedWall : s_hoveredWall;
		const s32 sectorId = s_selectedWall >= 0 && selected ? s_selectedWallSector : s_hoveredWallSector;

		EditorSector* sector = s_levelData->sectors.data() + sectorId;
		const EditorWall* wall = sector->walls.data() + wallId;
		const Vec2f* vtx = sector->vertices.data();

		Vec2f line[] = { vtx[wall->i0], vtx[wall->i1] };
		u32 lineClr;
		if (s_selectedWall >= 0 && selected)
		{
			lineClr = wall->adjoin < 0 ? 0xffffA040 : 0xff805020;
		}
		else
		{
			lineClr = wall->adjoin < 0 ? 0xffffC080 : 0xff806040;
		}
		u32 color[] = { lineClr, lineClr };
		LineDraw2d::addLines(1, 2.0f, line, color);

		Vec2f nrm = { vtx[wall->i1].z - vtx[wall->i0].z, -(vtx[wall->i1].x - vtx[wall->i0].x) };
		nrm = TFE_Math::normalize(&nrm);
		const f32 nScale = 8.0f * s_zoomVisual;

		line[0] = { (vtx[wall->i0].x + vtx[wall->i1].x) * 0.5f, (vtx[wall->i0].z + vtx[wall->i1].z) * 0.5f };
		line[1] = { line[0].x + nrm.x * nScale, line[0].z + nrm.z * nScale };
		LineDraw2d::addLines(1, 2.0f, line, color);
	}

	void beginDraw2d(u32 rtWidth, u32 rtHeight)
	{
		TriColoredDraw2d::begin(rtWidth, rtHeight, s_zoomVisual, s_offsetVis.x, s_offsetVis.z);
		TriTexturedDraw2d::begin(rtWidth, rtHeight, s_zoomVisual, s_offsetVis.x, s_offsetVis.z);
		LineDraw2d::begin(rtWidth, rtHeight, s_zoomVisual, s_offsetVis.x, s_offsetVis.z);
	}

	void flushTriangles()
	{
		TriTexturedDraw2d::drawTriangles();
		TriColoredDraw2d::drawTriangles();
	}

	struct InfWall
	{
		const EditorSector* sector;
		u32 wallId;
		u32 triggerType;
	};
	static std::vector<InfWall> s_infWalls;
		
	void drawSectorWalls2d(s32 drawLayer, s32 layer, u8 infType)
	{
		if (infType != INF_NONE && infType != INF_ALL && drawLayer != layer) { return; }

		// First draw the non-INF affected walls.
		const u32 sectorCount = (u32)s_levelData->sectors.size();
		EditorSector* sector = s_levelData->sectors.data();
		for (u32 i = 0; i < sectorCount; i++, sector++)
		{
			if (sector->layer != drawLayer || s_selectedSector == i || s_hoveredSector == i) { continue; }
			if ((infType != INF_NONE || drawLayer == layer) && sector->infType != infType && infType != INF_ALL) { continue; }
			
			const u32 wallCount = (u32)sector->walls.size();
			const EditorWall* wall = sector->walls.data();
			const Vec2f* vtx = sector->vertices.data();

			for (u32 w = 0; w < wallCount; w++, wall++)
			{
				Vec2f line[] = { vtx[wall->i0], vtx[wall->i1] };
				u32 lineClr;
				
				if (drawLayer != layer)
				{
					u32 alpha = 0x40 / (layer - drawLayer);
					lineClr = 0x00808000 | (alpha << 24);
				}
				else
				{
					if (infType == INF_NONE || infType == INF_ALL) { lineClr = wall->adjoin < 0 ? 0xffffffff : 0xff808080; }
					else if (infType == INF_SELF) { lineClr = wall->adjoin < 0 ? 0xffff80ff : 0xff804080; }
					else if (infType == INF_OTHER) { lineClr = wall->adjoin < 0 ? 0xffffff80 : 0xff808020; }
				}

				u32 color[] = { lineClr, lineClr };
				LineDraw2d::addLines(1, 1.5f, line, color);

				if (s_showSectorColors && sector->layer == layer && wall->infType != INF_NONE)
				{
					s_infWalls.push_back({ sector, w, (u32)wall->triggerType });
				}
			}
		}
	}

	void drawLineToTarget2d(const Vec2f* p0, s32 sectorId, s32 wallId, f32 width, u32 color)
	{
		const EditorSector* sector = s_levelData->sectors.data() + sectorId;
		const Vec2f* vtx = sector->vertices.data();
		Vec2f target = { 0 };

		if (wallId >= 0)
		{
			const EditorWall* wall = sector->walls.data() + wallId;
			target.x = (vtx[wall->i0].x + vtx[wall->i1].x) * 0.5f;
			target.z = (vtx[wall->i0].z + vtx[wall->i1].z) * 0.5f;
		}
		else
		{
			const u32 vtxCount = (u32)sector->vertices.size();
			for (u32 v = 0; v < vtxCount; v++)
			{
				target.x += vtx[v].x;
				target.z += vtx[v].z;
			}
			const f32 scale = 1.0f / f32(vtxCount);
			target.x *= scale;
			target.z *= scale;
		}
		Vec2f line[] = { *p0, target };
		u32 colors[] = { color, color };
		LineDraw2d::addLine(width, line, colors);
	}

	// If the selected wall or sector has an INF script, draw lines towards
	// 1. Any objects it effects (targets and clients).
	// 2. Any slaves (sector).
	void drawTargetsAndClientLines2d(s32 sectorId, f32 width)
	{
		const EditorSector* sector = s_levelData->sectors.data() + sectorId;
		const EditorInfItem* item = sector->infItem;
		if (!item) { return; }

		Vec2f lineStart = { 0 };
		const Vec2f* vtx = sector->vertices.data();
		const u32 vtxCount = (u32)sector->vertices.size();
		for (u32 v = 0; v < vtxCount; v++)
		{
			lineStart.x += vtx[v].x;
			lineStart.z += vtx[v].z;
		}
		const f32 scale = 1.0f / f32(vtxCount);
		lineStart.x *= scale;
		lineStart.z *= scale;

		for (u32 c = 0; c < (u32)item->classData.size(); c++)
		{
			const EditorInfClassData* cdata = &item->classData[c];
			u32 stopCount = 0;
			if (cdata->iclass == INF_CLASS_TRIGGER)
			{
				stopCount = 1;
			}
			else if (cdata->iclass == INF_CLASS_ELEVATOR)
			{
				stopCount = (u32)cdata->stop.size();
			}

			for (u32 s = 0; s < stopCount; s++)
			{
				const u32 funcCount = (u32)cdata->stop[s].func.size();
				for (u32 f = 0; f < funcCount; f++)
				{
					// func | clientCount << 8 | argCount << 16  (top bot available).
					const u32 funcId = cdata->stop[s].func[f].funcId;
					const u32 clientCount = (u32)cdata->stop[s].func[f].client.size();
					for (u32 cl = 0; cl < clientCount; cl++)
					{
						const EditorSector* clientSector = LevelEditorData::getSector(cdata->stop[s].func[f].client[cl].sectorName.c_str());
						const s32 clientWallId = cdata->stop[s].func[f].client[cl].wallId;
						if (!clientSector) { continue; }

						if (clientWallId >= 0)
						{
							drawLineToTarget2d(&lineStart, clientSector->id, clientWallId, width, 0xffff2020);
						}
						else
						{
							drawLineToTarget2d(&lineStart, clientSector->id, -1, width, 0xffff2020);
						}
					}

					// Draw to the adjoining lines.
					if (funcId == INF_MSG_ADJOIN)
					{
						drawLineToTarget2d(&lineStart, LevelEditorData::getSector(cdata->stop[s].func[f].arg[0].sValue.c_str())->id, cdata->stop[s].func[f].arg[1].iValue, width, 0xff2020ff);
						drawLineToTarget2d(&lineStart, LevelEditorData::getSector(cdata->stop[s].func[f].arg[2].sValue.c_str())->id, cdata->stop[s].func[f].arg[3].iValue, width, 0xff2020ff);
					}
				}
			}
		}
	}

	void drawTargetsAndClientLines2d(s32 sectorId, s32 wallId, f32 width)
	{
		const EditorSector* sector = s_levelData->sectors.data() + sectorId;
		const Vec2f* vtx = sector->vertices.data();
		const EditorWall* wall = sector->walls.data() + wallId;
		const EditorInfItem* item = wall->infItem;
				
		Vec2f lineStart;
		lineStart.x = (vtx[wall->i0].x + vtx[wall->i1].x) * 0.5f;
		lineStart.z = (vtx[wall->i0].z + vtx[wall->i1].z) * 0.5f;

		for (u32 c = 0; c < (u32)item->classData.size(); c++)
		{
			const EditorInfClassData* cdata = &item->classData[c];
			u32 stopCount = 0;
			if (cdata->iclass == INF_CLASS_TRIGGER)
			{
				stopCount = 1;
			}
			else if (cdata->iclass == INF_CLASS_ELEVATOR)
			{
				stopCount = (u32)cdata->stop.size();
			}

			for (u32 s = 0; s < stopCount; s++)
			{
				const u32 funcCount = (u32)cdata->stop[s].func.size();
				for (u32 f = 0; f < funcCount; f++)
				{
					const u32 clientCount = (u32)cdata->stop[s].func[f].client.size();
					for (u32 cl = 0; cl < clientCount; cl++)
					{
						const EditorSector* clientSector = LevelEditorData::getSector(cdata->stop[s].func[f].client[cl].sectorName.c_str());
						const s32 clientWallId = cdata->stop[s].func[f].client[cl].wallId;
						if (!clientSector) { continue; }

						if (clientWallId >= 0)
						{
							drawLineToTarget2d(&lineStart, clientSector->id, clientWallId, width, 0xffff2020);
						}
						else
						{
							drawLineToTarget2d(&lineStart, clientSector->id, -1, width, 0xffff2020);
						}
					}
				}
			}
		}
	}

	void drawTargetsAndClientSlaves2d(s32 sectorId, f32 width)
	{
		const EditorSector* sector = s_levelData->sectors.data() + sectorId;
		const EditorInfItem* item = sector->infItem;
		if (!item) { return; }

		Vec2f lineStart = { 0 };
		const Vec2f* vtx = sector->vertices.data();
		const u32 vtxCount = (u32)sector->vertices.size();
		for (u32 v = 0; v < vtxCount; v++)
		{
			lineStart.x += vtx[v].x;
			lineStart.z += vtx[v].z;
		}
		const f32 scale = 1.0f / f32(vtxCount);
		lineStart.x *= scale;
		lineStart.z *= scale;

		for (u32 c = 0; c < (u32)item->classData.size(); c++)
		{
			const EditorInfClassData* cdata = &item->classData[c];
			for (u32 s = 0; s < (u32)cdata->slaves.size(); s++)
			{
				const EditorSector* slaveSector = LevelEditorData::getSector(cdata->slaves[s].c_str());
				drawLineToTarget2d(&lineStart, slaveSector->id, -1, width, 0xff20ff20);
			}
		}
	}
		
	void drawSectorOutline2d(const EditorSector* sector, u32 adjoinColor, u32 baseColor, bool closed)
	{
		const s32 wallCount = closed ? (s32)sector->walls.size() : (s32)sector->walls.size() - 1;
		const EditorWall* wall = sector->walls.data();
		const Vec2f* vtx = sector->vertices.data();

		for (s32 w = 0; w < wallCount; w++, wall++)
		{
			Vec2f line[] = { vtx[wall->i0], vtx[wall->i1] };
			u32 lineClr = wall->adjoin < 0 ? adjoinColor : baseColor;
			u32 color[] = { lineClr, lineClr };
			LineDraw2d::addLines(1, 2.0f, line, color);
		}
	}

	void drawLevel2d(u32 rtWidth, u32 rtHeight)
	{
		if (!s_levelData || s_sectorDrawMode == SDM_WIREFRAME || (s_layerIndex + s_layerMin < s_levelData->layerMin))
		{
			// Draw the grid.
			Grid2d::blitToScreen(rtWidth, rtHeight, s_gridSize, s_subGridSize, s_zoomVisual, s_offsetVis.x, s_offsetVis.z, s_gridOpacity);
		}

		// Draw the level.
		if (!s_levelData) { return; }

		const u32 sectorCount = (u32)s_levelData->sectors.size();
		const EditorSector* sector = s_levelData->sectors.data();
		s32 layer = s_layerIndex + s_layerMin;

		beginDraw2d(rtWidth, rtHeight);

		// Loop through all layers, bottom to current - only draw walls for all but the current layer.
		s_infWalls.clear();
		s32 startLayer = s_showLowerLayers ? s_levelData->layerMin : layer;
		for (s32 l = startLayer; l <= layer; l++)
		{
			// Draw the solid sectors before drawing walls.
			if (s_sectorDrawMode == SDM_LIGHTING && l == layer)
			{
				sector = s_levelData->sectors.data();
				for (u32 i = 0; i < sectorCount; i++, sector++)
				{
					if (sector->layer != layer) { continue; }

					u32 L = u32(sector->ambient);
					L = s_fullbright ? 255u : std::max(0u, std::min(CONV_5bitTo8bit(L), 255u));
					const u32 Lrg = L * 7 / 8;
					const u32 lighting = Lrg | (Lrg << 8u) | (L << 16u) | 0xff000000;
					drawSectorPolygon(sector, false, lighting);
				}
			}
			else if ((s_sectorDrawMode == SDM_TEXTURED_FLOOR || s_sectorDrawMode == SDM_TEXTURED_CEIL) && l == layer)
			{
				sector = s_levelData->sectors.data();
				for (u32 i = 0; i < sectorCount; i++, sector++)
				{
					if (sector->layer != layer) { continue; }

					u32 L = u32(sector->ambient);
					L = s_fullbright ? 255u : std::max(0u, std::min(CONV_5bitTo8bit(L), 255u));
					const u32 lighting = L | (L << 8u) | (L << 16u) | 0xff000000;
					drawTexturedSectorPolygon(sector, lighting, s_sectorDrawMode == SDM_TEXTURED_FLOOR);
				}
			}
						
			if (l == layer && s_sectorDrawMode != SDM_WIREFRAME)
			{
				// Draw the grid.
				flushTriangles();
				Grid2d::blitToScreen(rtWidth, rtHeight, s_gridSize, s_subGridSize, s_zoomVisual, s_offsetVis.x, s_offsetVis.z, s_gridOpacity);
			}

			// Draw the selected sector before drawing the walls.
			if (l == layer && (s_selectedSector >= 0 || s_hoveredSector >= 0))
			{
				if (s_selectedSector >= 0) { drawSectorPolygon(s_levelData->sectors.data() + s_selectedSector, false); }
				if (s_hoveredSector >= 0) { drawSectorPolygon(s_levelData->sectors.data() + s_hoveredSector, true); }
				flushTriangles();
			}
			else if (l == layer && s_editMode == LEDIT_DRAW)  // Draw the interior of the sector being drawn.
			{
				drawSectorPolygon(&s_newSector, false);
			}

			// Walls
			if (s_showSectorColors)
			{
				drawSectorWalls2d(l, layer, INF_NONE);
				
				// Draw INF walls.
				if (l == layer)
				{
					drawSectorWalls2d(l, layer, INF_SELF);
					drawSectorWalls2d(l, layer, INF_OTHER);

					const size_t iWallCount = s_infWalls.size();
					const InfWall* infWall = s_infWalls.data();
					for (size_t w = 0; w < iWallCount; w++, infWall++)
					{
						const EditorWall* wall = infWall->sector->walls.data() + infWall->wallId;
						const Vec2f* vtx = infWall->sector->vertices.data();

						Vec2f line[] = { vtx[wall->i0], vtx[wall->i1] };
						u32 lineClr = wall->adjoin < 0 ? 0xff2020ff : 0xff101080;
						u32 color[] = { lineClr, lineClr };
						LineDraw2d::addLines(1, 1.5f, line, color);

						Vec2f nrm = { vtx[wall->i1].z - vtx[wall->i0].z, -(vtx[wall->i1].x - vtx[wall->i0].x) };
						nrm = TFE_Math::normalize(&nrm);
						const f32 nScale = 4.0f * s_zoomVisual;

						line[0] = { (vtx[wall->i0].x + vtx[wall->i1].x) * 0.5f, (vtx[wall->i0].z + vtx[wall->i1].z) * 0.5f };
						line[1] = { line[0].x + nrm.x * nScale, line[0].z + nrm.z * nScale };
						LineDraw2d::addLines(1, 2.0f, line, color);
					}
				}
			}
			else
			{
				drawSectorWalls2d(l, layer, INF_ALL);
			}

			if (l == layer && s_selectedSector >= 0)
			{
				sector = s_levelData->sectors.data() + s_selectedSector;
				drawSectorOutline2d(sector, 0xffffA040, 0xff805020, true);
			}
			if (l == layer && s_hoveredSector >= 0)
			{
				sector = s_levelData->sectors.data() + s_hoveredSector;
				drawSectorOutline2d(sector, 0xffffC080, 0xff806040, true);
			}
			if (l == layer && s_editMode == LEDIT_DRAW)
			{
				drawSectorOutline2d(&s_newSector, 0xffffA040, 0xff805020, false);
			}
			// Draw the selected sector before drawing the walls.
			if (l == layer && (s_selectedWall >= 0 || s_hoveredWall >= 0))
			{
				highlightWall(true);
				highlightWall(false);
			}
			if (s_selectedSector >= 0 && s_levelData->sectors[s_selectedSector].infType != INF_NONE)
			{
				// If the selected wall or sector has an INF script, draw lines towards
				drawTargetsAndClientLines2d(s_selectedSector, 2.0f);
				drawTargetsAndClientSlaves2d(s_selectedSector, 2.0f);
			}
			else if (s_selectedWall >= 0 && s_levelData->sectors[s_selectedWallSector].walls[s_selectedWall].infType != INF_NONE)
			{
				drawTargetsAndClientLines2d(s_selectedWallSector, s_selectedWall, 2.0f);
			}
						
			LineDraw2d::drawLines();
		}

		// Objects
		const u32 alphaFg = s_editMode == LEDIT_ENTITY ? 0xff000000 : 0xA0000000;
		const u32 alphaBg = s_editMode == LEDIT_ENTITY ? 0xA0000000 : 0x60000000;
		sector = s_levelData->sectors.data();
		for (u32 i = 0; i < sectorCount; i++, sector++)
		{
			if (sector->layer != layer) { continue; }
			
			// For now just draw a quad + image.
			const u32 objCount = (u32)sector->objects.size();
			const EditorLevelObject* obj = sector->objects.data();
			for (u32 o = 0; o < objCount; o++, obj++)
			{
				const bool selected = s_selectedEntity == o && s_selectedEntitySector == i;
				const bool highlight = (s_hoveredEntity == o && s_hoveredEntitySector == i) || selected;
				const u32 clrBg = highlight ? (0xffae8653 | alphaBg) : (0x0051331a | alphaBg);

				if (obj->oclass == CLASS_3D)
				{
					const Vec2f pos = { obj->pos.x, obj->pos.z };
					TFE_EditorRender::drawModel2d_Bounds(obj->displayModel, &obj->pos, &obj->rotMtx, clrBg, highlight);
					TFE_EditorRender::drawModel2d(sector, obj->displayModel, &pos, &obj->rotMtx, s_palette->colors, alphaFg);
				}
				else
				{
					f32 width  = obj->display ? (f32)obj->display->width  : 1.0f;
					f32 height = obj->display ? (f32)obj->display->height : 1.0f;
					
					f32 x0 = -obj->worldExt.x, z0 = -obj->worldExt.z;
					f32 x1 =  obj->worldExt.x, z1 =  obj->worldExt.z;
					if (height > width)
					{
						const f32 dx = obj->worldExt.x * width / height;
						x0 = -dx;
						x1 = x0 + 2.0f * dx;
					}
					else if (width > height)
					{
						const f32 dz = obj->worldExt.z * height / width;
						z0 = -dz;
						z1 = z0 + 2.0f * dz;
					}

					const Vec2f vtxTex[6]=
					{
						{obj->worldCen.x + x0, obj->worldCen.z + z0},
						{obj->worldCen.x + x1, obj->worldCen.z + z0},
						{obj->worldCen.x + x1, obj->worldCen.z + z1},

						{obj->worldCen.x + x0, obj->worldCen.z + z0},
						{obj->worldCen.x + x1, obj->worldCen.z + z1},
						{obj->worldCen.x + x0, obj->worldCen.z + z1},
					};

					const f32 scale = highlight ? 1.25f : 1.0f;
					Vec2f vtxClr[6]=
					{
						{obj->worldCen.x - obj->worldExt.x*scale, obj->worldCen.z - obj->worldExt.z*scale},
						{obj->worldCen.x + obj->worldExt.x*scale, obj->worldCen.z - obj->worldExt.z*scale},
						{obj->worldCen.x + obj->worldExt.x*scale, obj->worldCen.z + obj->worldExt.z*scale},

						{obj->worldCen.x - obj->worldExt.x*scale, obj->worldCen.z - obj->worldExt.z*scale},
						{obj->worldCen.x + obj->worldExt.x*scale, obj->worldCen.z + obj->worldExt.z*scale},
						{obj->worldCen.x - obj->worldExt.x*scale, obj->worldCen.z + obj->worldExt.z*scale},
					};

					const Vec2f uv[6] =
					{
						{0.0f, 1.0f},
						{1.0f, 1.0f},
						{1.0f, 0.0f},

						{0.0f, 1.0f},
						{1.0f, 0.0f},
						{0.0f, 0.0f},
					};
					const u32 baseRGB = selected ? 0x00ffa0a0 : 0x00ffffff;
					const u32 colorFg[2] = { baseRGB | alphaFg, baseRGB | alphaFg };
					const u32 colorBg[2] = { clrBg, clrBg };

					TriColoredDraw2d::addTriangles(2, vtxClr, colorBg);
					if (obj->display) { TriTexturedDraw2d::addTriangles(2, vtxTex, uv, colorFg, obj->display->texture); }

					// Draw a direction.
					const f32 angle = obj->orientation.y * PI / 180.0f;
					Vec2f dir = { sinf(angle), cosf(angle) };

					// Make an arrow.
					f32 dx = dir.x * obj->worldExt.x;
					f32 dz = dir.z * obj->worldExt.z;
					f32 cx = obj->pos.x + dx;
					f32 cz = obj->pos.z + dz;
					f32 tx = -dir.z * obj->worldExt.x * 0.5f;
					f32 tz =  dir.x * obj->worldExt.z * 0.5f;

					Vec2f vtx[]=
					{
						{cx, cz}, {cx + tx - dx * 0.25f, cz + tz - dz * 0.25f},
						{cx, cz}, {cx - tx - dx * 0.25f, cz - tz - dz * 0.25f},
					};

					const u32 alphaArrow = s_editMode == LEDIT_ENTITY ? (highlight ? 0xA0000000 : 0x60000000) : 0x30000000;
					u32 baseRGBArrow = 0x00ffff00;
					u32 baseClrArrow = baseRGBArrow | alphaArrow;
					u32 color[] = { baseClrArrow, baseClrArrow, baseClrArrow, baseClrArrow };
					LineDraw2d::addLines(2, 2.0f, vtx, color);
				}
			}
		}
		TriColoredDraw2d::drawTriangles();
		TriTexturedDraw2d::drawTriangles();
		LineDraw2d::drawLines();

		// Vertices
		const u32 color[4] = { 0xffae8653, 0xffae8653, 0xff51331a, 0xff51331a };
		const u32 colorSelected[4] = { 0xffffc379, 0xffffc379, 0xff764a26, 0xff764a26 };

		const f32 scaleMax = s_zoomVisual * 4.0f;
		const f32 scaleMin = s_zoomVisual * 1.0f;
		const f32 scale = std::max(scaleMin, std::min(scaleMax, 0.5f));

		sector = s_levelData->sectors.data();
		for (u32 i = 0; i < sectorCount; i++, sector++)
		{
			if (sector->layer != layer) { continue; }

			const u32 wallCount = (u32)sector->walls.size();
			const EditorWall* wall = sector->walls.data();
			const Vec2f* vtx = sector->vertices.data();

			for (u32 w = 0; w < wallCount; w++, wall++)
			{
				drawVertex(&vtx[wall->i0], scale, color);
			}
		}
		if (s_hoveredVertex >= 0 && s_hoveredVertex < (s32)s_levelData->sectors[s_hoveredVertexSector].vertices.size())
		{
			const Vec2f* vtx = &s_levelData->sectors[s_hoveredVertexSector].vertices[s_hoveredVertex];
			drawVertex(vtx, scale * 1.5f, colorSelected);
		}
		if (s_selectedVertex >= 0 && s_selectedVertex < (s32)s_levelData->sectors[s_selectedVertexSector].vertices.size())
		{
			const Vec2f* vtx = &s_levelData->sectors[s_selectedVertexSector].vertices[s_selectedVertex];
			drawVertex(vtx, scale * 1.5f, colorSelected);
		}

		// Draw the 2d cursor in "Draw Mode"
		if (s_editMode == LEDIT_DRAW)
		{
			Vec2f pos = { s_cursorSnapped.x, s_cursorSnapped.z };
			drawVertex(&pos, scale * 1.5f, colorSelected);
		}

		flushTriangles();
	}

	void drawSectorFloorAndCeilColor(const EditorSector* sector, const SectorTriangles* poly, const u32* color, bool blend)
	{
		const u32 triCount = poly->count;
		const Vec2f* vtx = poly->vtx.data();
		for (u32 t = 0; t < triCount; t++, vtx += 3)
		{
			const Vec3f floorCeilVtx[6] =
			{
				{vtx[0].x, sector->floorAlt, vtx[0].z},
				{vtx[2].x, sector->floorAlt, vtx[2].z},
				{vtx[1].x, sector->floorAlt, vtx[1].z},

				{vtx[0].x, sector->ceilAlt, vtx[0].z},
				{vtx[1].x, sector->ceilAlt, vtx[1].z},
				{vtx[2].x, sector->ceilAlt, vtx[2].z},
			};
			const Vec2f floorUv[6] =
			{
				{vtx[0].x, vtx[0].z},
				{vtx[2].x, vtx[2].z},
				{vtx[1].x, vtx[1].z},

				{vtx[0].x, vtx[0].z},
				{vtx[1].x, vtx[1].z},
				{vtx[2].x, vtx[2].z},
			};
			TrianglesColor3d::addTriangles(2, floorCeilVtx, floorUv, color, blend);
		}
	}

	void drawSectorFloorAndCeilTextured(const EditorSector* sector, const SectorTriangles* poly, u32 color)
	{
		const bool hasFloorTex = sector->floorTexture.tex != nullptr;
		const bool hasCeilTex  = sector->ceilTexture.tex  != nullptr;

		const f32* floorOff = &sector->floorTexture.offsetX;
		const f32* ceilOff = &sector->ceilTexture.offsetX;
		const TextureGpu* floorTex = hasFloorTex ? sector->floorTexture.tex->texture : nullptr;
		const TextureGpu* ceilTex  = hasCeilTex  ? sector->ceilTexture.tex->texture  : nullptr;
		const f32 scaleFloorX = hasFloorTex ? -8.0f / f32(sector->floorTexture.tex->width)  : 1.0f;
		const f32 scaleFloorZ = hasFloorTex ? -8.0f / f32(sector->floorTexture.tex->height) : 1.0f;
		const f32 scaleCeilX  = hasCeilTex  ? -8.0f / f32(sector->ceilTexture.tex->width)   : 1.0f;
		const f32 scaleCeilZ  = hasCeilTex  ? -8.0f / f32(sector->ceilTexture.tex->height)  : 1.0f;

		const u32 triCount = poly->count;
		
		// Two passes, one for floor and ceiling to avoid changing textures too often...
		const Vec2f* vtx = poly->vtx.data();
		for (u32 t = 0; t < triCount; t++, vtx += 3)
		{
			const Vec3f floorCeilVtx[6] =
			{
				{vtx[0].x, sector->floorAlt, vtx[0].z},
				{vtx[2].x, sector->floorAlt, vtx[2].z},
				{vtx[1].x, sector->floorAlt, vtx[1].z},
			};
			const Vec2f floorUv[6] =
			{
				{vtx[0].x, vtx[0].z},
				{vtx[2].x, vtx[2].z},
				{vtx[1].x, vtx[1].z},
			};
			const Vec2f floorUv1[6] =
			{
				{(vtx[0].x - floorOff[0]) * scaleFloorX, (vtx[0].z - floorOff[1]) * scaleFloorZ},
				{(vtx[2].x - floorOff[0]) * scaleFloorX, (vtx[2].z - floorOff[1]) * scaleFloorZ},
				{(vtx[1].x - floorOff[0]) * scaleFloorX, (vtx[1].z - floorOff[1]) * scaleFloorZ},
			};

			TrianglesColor3d::addTexturedTriangle(floorCeilVtx, floorUv, floorUv1, color, floorTex);
		}

		vtx = poly->vtx.data();
		for (u32 t = 0; t < triCount; t++, vtx += 3)
		{
			const Vec3f floorCeilVtx[6] =
			{
				{vtx[0].x, sector->ceilAlt, vtx[0].z},
				{vtx[1].x, sector->ceilAlt, vtx[1].z},
				{vtx[2].x, sector->ceilAlt, vtx[2].z},
			};
			const Vec2f floorUv[6] =
			{
				{vtx[0].x, vtx[0].z},
				{vtx[1].x, vtx[1].z},
				{vtx[2].x, vtx[2].z},
			};
			const Vec2f floorUv1[6] =
			{
				{(vtx[0].x - ceilOff[0]) * scaleCeilX, (vtx[0].z - ceilOff[1]) * scaleCeilZ},
				{(vtx[1].x - ceilOff[0]) * scaleCeilX, (vtx[1].z - ceilOff[1]) * scaleCeilZ},
				{(vtx[2].x - ceilOff[0]) * scaleCeilX, (vtx[2].z - ceilOff[1]) * scaleCeilZ},
			};

			TrianglesColor3d::addTexturedTriangle(floorCeilVtx, floorUv, floorUv1, color, ceilTex);
		}
	}

	f32 wallLength(const EditorWall* wall, const EditorSector* sector)
	{
		const Vec2f* vtx = sector->vertices.data();
		const Vec2f offset = { vtx[wall->i1].x - vtx[wall->i0].x, vtx[wall->i1].z - vtx[wall->i0].z };
		return sqrtf(offset.x * offset.x + offset.z * offset.z);
	}

	void buildWallVertices(f32 floorAlt, f32 ceilAlt, const Vec2f* v0, const Vec2f* v1, Vec3f* wallVertex, Vec2f* wallUv)
	{
		const f32 dx = fabsf(v1->x - v0->x);
		const f32 dz = fabsf(v1->z - v0->z);

		wallVertex[0] = { v0->x, ceilAlt, v0->z };
		wallVertex[1] = { v1->x, ceilAlt, v1->z };
		wallVertex[2] = { v1->x, floorAlt, v1->z };

		wallVertex[3] = { v0->x, ceilAlt, v0->z };
		wallVertex[4] = { v1->x, floorAlt, v1->z };
		wallVertex[5] = { v0->x, floorAlt, v0->z };

		wallUv[0] = { (dx >= dz) ? v0->x : v0->z, ceilAlt };
		wallUv[1] = { (dx >= dz) ? v1->x : v1->z, ceilAlt };
		wallUv[2] = { (dx >= dz) ? v1->x : v1->z, floorAlt };

		wallUv[3] = { (dx >= dz) ? v0->x : v0->z, ceilAlt };
		wallUv[4] = { (dx >= dz) ? v1->x : v1->z, floorAlt };
		wallUv[5] = { (dx >= dz) ? v0->x : v0->z, floorAlt };
	}

	void buildWallTextureVertices(const EditorSectorTexture* stex, f32 wallLen, f32 h, bool flipHorz, u32 texWidth, u32 texHeight, Vec2f* wallUv1)
	{
		f32 u0 = (stex->offsetX) * c_worldToTexelScale;
		f32 u1 = u0 + wallLen;
		f32 v0 = (-stex->offsetY - h) * c_worldToTexelScale;
		f32 v1 = (-stex->offsetY) * c_worldToTexelScale;

		const f32 textureScaleX = 1.0f / f32(texWidth);
		const f32 textureScaleY = 1.0f / f32(texHeight);
		u0 = u0 * textureScaleX;
		u1 = u1 * textureScaleX;
		v0 = v0 * textureScaleY;
		v1 = v1 * textureScaleY;

		if (flipHorz) { u0 = 1.0f - u0; u1 = 1.0f - u1; }

		wallUv1[0] = { u0, v0 };
		wallUv1[1] = { u1, v0 };
		wallUv1[2] = { u1, v1 };

		wallUv1[3] = { u0, v0 };
		wallUv1[4] = { u1, v1 };
		wallUv1[5] = { u0, v1 };
	}

	void buildSignTextureVertices(const EditorSectorTexture* sbaseTex, const EditorSectorTexture* sTex, f32 wallLen, f32 h, u32 texWidth, u32 texHeight, Vec2f* wallUv1)
	{
		const f32 offsetX = sbaseTex->offsetX - sTex->offsetX;
		// The way the base offset Y is factored in is quite strange but has been arrived at by looking at the data...
		const f32 offsetY = -TFE_Math::fract(std::max(sbaseTex->offsetY, 0.0f)) + sTex->offsetY;

		f32 u0 = offsetX * c_worldToTexelScale;
		f32 u1 = u0 + wallLen;
		f32 v0 = (-offsetY - h) * c_worldToTexelScale;
		f32 v1 = (-offsetY) * c_worldToTexelScale;

		const f32 textureScaleX = 1.0f / f32(texWidth);
		const f32 textureScaleY = 1.0f / f32(texHeight);
		u0 = u0 * textureScaleX;
		u1 = u1 * textureScaleX;
		v0 = 1.0f + v0 * textureScaleY;
		v1 = 1.0f + v1 * textureScaleY;

		wallUv1[0] = { u0, v0 };
		wallUv1[1] = { u1, v0 };
		wallUv1[2] = { u1, v1 };

		wallUv1[3] = { u0, v0 };
		wallUv1[4] = { u1, v1 };
		wallUv1[5] = { u0, v1 };
	}

	void drawWallColor(const EditorSector* sector, const Vec2f* vtx, const EditorWall* wall, const u32* color, bool blend, RayHitPart part, bool showAllWallsOnBlend)
	{
		const Vec2f* v0 = &vtx[wall->i0];
		const Vec2f* v1 = &vtx[wall->i1];

		Vec3f wallVertex[6];
		Vec2f wallUv[6];
		if (wall->adjoin < 0 && (part == HIT_PART_UNKNOWN || part == HIT_PART_MID))
		{
			// No adjoin - a single quad.
			buildWallVertices(sector->floorAlt, sector->ceilAlt, v0, v1, wallVertex, wallUv);
			TrianglesColor3d::addTriangles(2, wallVertex, wallUv, color, blend);
		}
		else
		{
			// lower
			f32 nextFloorAlt = s_levelData->sectors[wall->adjoin].floorAlt;
			if (nextFloorAlt < sector->floorAlt && (part == HIT_PART_UNKNOWN || part == HIT_PART_BOT))
			{
				buildWallVertices(sector->floorAlt, nextFloorAlt, v0, v1, wallVertex, wallUv);
				TrianglesColor3d::addTriangles(2, wallVertex, wallUv, color, blend);
			}

			// upper
			f32 nextCeilAlt = s_levelData->sectors[wall->adjoin].ceilAlt;
			if (nextCeilAlt > sector->ceilAlt && (part == HIT_PART_UNKNOWN || part == HIT_PART_TOP))
			{
				buildWallVertices(nextCeilAlt, sector->ceilAlt, v0, v1, wallVertex, wallUv);
				TrianglesColor3d::addTriangles(2, wallVertex, wallUv, color, blend);
			}

			// mask wall or highlight.
			if (((wall->flags[0] & WF1_ADJ_MID_TEX) || (blend && showAllWallsOnBlend)) && (part == HIT_PART_UNKNOWN || part == HIT_PART_MID))
			{
				const f32 ceilAlt = std::max(nextCeilAlt, sector->ceilAlt);
				const f32 floorAlt = std::min(nextFloorAlt, sector->floorAlt);

				buildWallVertices(floorAlt, ceilAlt, v0, v1, wallVertex, wallUv);
				TrianglesColor3d::addTriangles(2, wallVertex, wallUv, color, blend);
			}
		}
	}
				
	void drawWallTexture(const EditorSector* sector, const Vec2f* vtx, const EditorWall* wall, u32 wallIndex, const u32* color)
	{
		const Vec2f* v0 = &vtx[wall->i0];
		const Vec2f* v1 = &vtx[wall->i1];
		const f32 wallLen = wallLength(wall, sector) * c_worldToTexelScale;
		const bool flipHorz = (wall->flags[0] & WF1_FLIP_HORIZ) != 0u;

		const u32 fullbright[] = { 0xffffffff, 0xffffffff };

		Vec3f wallVertex[6];
		Vec2f wallUv[6];
		Vec2f wallUv1[6];
		if (wall->adjoin < 0)
		{
			const f32 h = sector->floorAlt - sector->ceilAlt;

			buildWallVertices(sector->floorAlt, sector->ceilAlt, v0, v1, wallVertex, wallUv);
			buildWallTextureVertices(&wall->mid, wallLen, h, flipHorz, wall->mid.tex->width, wall->mid.tex->height, wallUv1);
			TrianglesColor3d::addTexturedTriangles(2, wallVertex, wallUv, wallUv1, color, wall->mid.tex->texture);

			if (wall->sign.tex)
			{
				const u32* sgnColor = (wall->flags[0] & WF1_ILLUM_SIGN) ? fullbright : color;
				buildSignTextureVertices(&wall->mid, &wall->sign, wallLen, h, wall->sign.tex->width, wall->sign.tex->height, wallUv1);
				TrianglesColor3d::addTexturedTriangles(2, wallVertex, wallUv, wallUv1, sgnColor, wall->sign.tex->texture, Tri3dTrans::TRANS_CLAMP);
			}
		}
		else
		{
			const EditorSector* next = s_levelData->sectors.data() + wall->adjoin;

			// lower
			if (next->floorAlt < sector->floorAlt && wall->bot.tex)
			{
				const f32 h = sector->floorAlt - next->floorAlt;

				buildWallVertices(sector->floorAlt, next->floorAlt, v0, v1, wallVertex, wallUv);
				buildWallTextureVertices(&wall->bot, wallLen, h, flipHorz, wall->bot.tex->width, wall->bot.tex->height, wallUv1);
				TrianglesColor3d::addTexturedTriangles(2, wallVertex, wallUv, wallUv1, color, wall->bot.tex->texture);

				if (wall->sign.tex)
				{
					const u32* sgnColor = (wall->flags[0] & WF1_ILLUM_SIGN) ? fullbright : color;
					buildSignTextureVertices(&wall->bot, &wall->sign, wallLen, h, wall->sign.tex->width, wall->sign.tex->height, wallUv1);
					TrianglesColor3d::addTexturedTriangles(2, wallVertex, wallUv, wallUv1, sgnColor, wall->sign.tex->texture, Tri3dTrans::TRANS_CLAMP);
				}
			}

			// upper
			if (next->ceilAlt > sector->ceilAlt && wall->top.tex)
			{
				const f32 h = next->ceilAlt - sector->ceilAlt;

				buildWallVertices(next->ceilAlt, sector->ceilAlt, v0, v1, wallVertex, wallUv);
				buildWallTextureVertices(&wall->top, wallLen, h, flipHorz, wall->top.tex->width, wall->top.tex->height, wallUv1);
				TrianglesColor3d::addTexturedTriangles(2, wallVertex, wallUv, wallUv1, color, wall->top.tex->texture);

				// no upper sign if a lower sign was rendered.
				if (wall->sign.tex && next->floorAlt >= sector->floorAlt)
				{
					const u32* sgnColor = (wall->flags[0] & WF1_ILLUM_SIGN) ? fullbright : color;
					buildSignTextureVertices(&wall->top, &wall->sign, wallLen, h, wall->sign.tex->width, wall->sign.tex->height, wallUv1);
					TrianglesColor3d::addTexturedTriangles(2, wallVertex, wallUv, wallUv1, sgnColor, wall->sign.tex->texture, Tri3dTrans::TRANS_CLAMP);
				}
			}

			// mask wall
			if (wall->flags[0] & WF1_ADJ_MID_TEX)
			{
				const f32 ceilAlt  = std::max(next->ceilAlt,  sector->ceilAlt);
				const f32 floorAlt = std::min(next->floorAlt, sector->floorAlt);
				const f32 h = floorAlt - ceilAlt;

				buildWallVertices(floorAlt, ceilAlt, v0, v1, wallVertex, wallUv);
				buildWallTextureVertices(&wall->mid, wallLen, h, flipHorz, wall->mid.tex->width, wall->mid.tex->height, wallUv1);
				TrianglesColor3d::addTexturedTriangles(2, wallVertex, wallUv, wallUv1, color, wall->mid.tex->texture, Tri3dTrans::TRANS_CUTOUT);
			}
		}
	}

	static f32 c_editorCameraFov = 1.57079632679489661923f;

	void drawScreenQuad(const Vec3f* center, f32 side, u32 color)
	{
		const f32 w = side * 0.5f;
		const Vec3f r = { s_camera.viewMtx.m0.x * w, s_camera.viewMtx.m0.y * w, s_camera.viewMtx.m0.z * w };
		const Vec3f u = { s_camera.viewMtx.m1.x * w, s_camera.viewMtx.m1.y * w, s_camera.viewMtx.m1.z * w };

		const Vec3f vtx[]=
		{
			{center->x - r.x - u.x, center->y - r.y - u.y, center->z - r.z - u.z},
			{center->x + r.x - u.x, center->y + r.y - u.y, center->z + r.z - u.z},
			{center->x + r.x + u.x, center->y + r.y + u.y, center->z + r.z + u.z},

			{center->x - r.x - u.x, center->y - r.y - u.y, center->z - r.z - u.z},
			{center->x + r.x + u.x, center->y + r.y + u.y, center->z + r.z + u.z},
			{center->x - r.x + u.x, center->y - r.y + u.y, center->z - r.z + u.z},
		};

		u32 colors[2];
		for (u32 i = 0; i < 2; i++)
		{
			colors[i] = color;
		}

		TrianglesColor3d::addTriangles(2, vtx, nullptr, colors, true);
	}

	void drawScreenQuadOutline(const Vec3f* center, f32 side, f32 lineWidth, u32 color)
	{
		const f32 w = side * 0.5f;
		const Vec3f r = { s_camera.viewMtx.m0.x * w, s_camera.viewMtx.m0.y * w, s_camera.viewMtx.m0.z * w };
		const Vec3f u = { s_camera.viewMtx.m1.x * w, s_camera.viewMtx.m1.y * w, s_camera.viewMtx.m1.z * w };

		const Vec3f lines[] =
		{
			{center->x - r.x - u.x, center->y - r.y - u.y, center->z - r.z - u.z}, {center->x + r.x - u.x, center->y + r.y - u.y, center->z + r.z - u.z},
			{center->x + r.x - u.x, center->y + r.y - u.y, center->z + r.z - u.z}, {center->x + r.x + u.x, center->y + r.y + u.y, center->z + r.z + u.z},
			{center->x + r.x + u.x, center->y + r.y + u.y, center->z + r.z + u.z}, {center->x - r.x + u.x, center->y - r.y + u.y, center->z - r.z + u.z},
			{center->x - r.x + u.x, center->y - r.y + u.y, center->z - r.z + u.z}, {center->x - r.x - u.x, center->y - r.y - u.y, center->z - r.z - u.z},
		};

		u32 colors[4];
		for (u32 i = 0; i < 4; i++)
		{
			colors[i] = color;
		}

		LineDraw3d::addLines(4, lineWidth, lines, colors);
	}

	void drawBox(const Vec3f* center, f32 side, f32 lineWidth, u32 color)
	{
		const f32 w = side * 0.5f;
		const Vec3f lines[]=
		{
			{center->x - w, center->y - w, center->z - w}, {center->x + w, center->y - w, center->z - w},
			{center->x + w, center->y - w, center->z - w}, {center->x + w, center->y - w, center->z + w},
			{center->x + w, center->y - w, center->z + w}, {center->x - w, center->y - w, center->z + w},
			{center->x - w, center->y - w, center->z + w}, {center->x - w, center->y - w, center->z - w},

			{center->x - w, center->y + w, center->z - w}, {center->x + w, center->y + w, center->z - w},
			{center->x + w, center->y + w, center->z - w}, {center->x + w, center->y + w, center->z + w},
			{center->x + w, center->y + w, center->z + w}, {center->x - w, center->y + w, center->z + w},
			{center->x - w, center->y + w, center->z + w}, {center->x - w, center->y + w, center->z - w},

			{center->x - w, center->y - w, center->z - w}, {center->x - w, center->y + w, center->z - w},
			{center->x + w, center->y - w, center->z - w}, {center->x + w, center->y + w, center->z - w},
			{center->x + w, center->y - w, center->z + w}, {center->x + w, center->y + w, center->z + w},
			{center->x - w, center->y - w, center->z + w}, {center->x - w, center->y + w, center->z + w},
		};
		u32 colors[12];
		for (u32 i = 0; i < 12; i++)
		{
			colors[i] = color;
		}

		LineDraw3d::addLines(12, lineWidth, lines, colors);
	}

	void drawBounds(const Vec3f* center, const Vec3f* ext, f32 lineWidth, u32 color)
	{
		const Vec3f lines[] =
		{
			{center->x - ext->x, center->y + ext->y, center->z - ext->z}, {center->x + ext->x, center->y + ext->y, center->z - ext->z},
			{center->x + ext->x, center->y + ext->y, center->z - ext->z}, {center->x + ext->x, center->y + ext->y, center->z + ext->z},
			{center->x + ext->x, center->y + ext->y, center->z + ext->z}, {center->x - ext->x, center->y + ext->y, center->z + ext->z},
			{center->x - ext->x, center->y + ext->y, center->z + ext->z}, {center->x - ext->x, center->y + ext->y, center->z - ext->z},

			{center->x - ext->x, center->y - ext->y, center->z - ext->z}, {center->x + ext->x, center->y - ext->y, center->z - ext->z},
			{center->x + ext->x, center->y - ext->y, center->z - ext->z}, {center->x + ext->x, center->y - ext->y, center->z + ext->z},
			{center->x + ext->x, center->y - ext->y, center->z + ext->z}, {center->x - ext->x, center->y - ext->y, center->z + ext->z},
			{center->x - ext->x, center->y - ext->y, center->z + ext->z}, {center->x - ext->x, center->y - ext->y, center->z - ext->z},

			{center->x - ext->x, center->y + ext->y, center->z - ext->z}, {center->x - ext->x, center->y - ext->y, center->z - ext->z},
			{center->x + ext->x, center->y + ext->y, center->z - ext->z}, {center->x + ext->x, center->y - ext->y, center->z - ext->z},
			{center->x + ext->x, center->y + ext->y, center->z + ext->z}, {center->x + ext->x, center->y - ext->y, center->z + ext->z},
			{center->x - ext->x, center->y + ext->y, center->z + ext->z}, {center->x - ext->x, center->y - ext->y, center->z + ext->z},
		};
		u32 colors[12];
		for (u32 i = 0; i < 12; i++)
		{
			colors[i] = color;
		}

		LineDraw3d::addLines(12, lineWidth, lines, colors);
	}
		
	void drawSector3d(const EditorSector* sector, const SectorTriangles* poly, bool overlay, bool hover, bool lowerLayer)
	{
		// Draw the floor and ceiling polygons.
		const u32 triCount = poly->count;
		const Vec2f* vtx = poly->vtx.data();

		u32 color[4];
		const bool textured = s_sectorDrawMode == SDM_TEXTURED_FLOOR || s_sectorDrawMode == SDM_TEXTURED_CEIL;
		const bool blend = overlay;// || (!textured && lowerLayer);

		if (lowerLayer)
		{
			for (u32 t = 0; t < 4; t++) { color[t] = textured ? 0x80ffff20 : 0xa0ffa020; }
		}
		else if (overlay)
		{
			for (u32 t = 0; t < 4; t++) { color[t] = hover ? 0x40ff8020 : 0x40ff4020; }
		}
		else if (s_sectorDrawMode != SDM_WIREFRAME)
		{
			u32 L = u32(sector->ambient);
			L = s_fullbright ? 255u : std::max(0u, std::min(CONV_5bitTo8bit(L), 255u));
			const u32 Lrg = s_sectorDrawMode == SDM_LIGHTING ? L * 7 / 8 : L;
			const u32 lighting = Lrg | (Lrg << 8u) | (L << 16u) | 0xff000000;

			for (u32 t = 0; t < 4; t++) { color[t] = lighting; }
		}
		else
		{
			for (u32 t = 0; t < 4; t++) { color[t] = 0xff1a0f0d; }
		}

		if (s_sectorDrawMode == SDM_WIREFRAME || s_sectorDrawMode == SDM_LIGHTING || overlay)
		{
			drawSectorFloorAndCeilColor(sector, poly, color, blend);
		}
		else
		{
			drawSectorFloorAndCeilTextured(sector, poly, color[0]);
		}

		// Draw quads for each wall.
		const u32 wallCount = (u32)sector->walls.size();
		const EditorWall* wall = sector->walls.data();
		vtx = sector->vertices.data();
		for (u32 w = 0; w < wallCount; w++, wall++)
		{
			if (lowerLayer)
			{
				for (u32 t = 0; t < 4; t++) { color[t] = textured ? 0x80ffff20 : 0x80ffa020; }
			}
			else if (overlay)
			{
				for (u32 t = 0; t < 4; t++) { color[t] = hover ? 0x40ff8020 : 0x40ff4020; }
			}
			else if (s_sectorDrawMode != SDM_WIREFRAME)
			{
				const s32 wallLight = sector->ambient < 31 ? wall->light : 0;

				u32 L = u32(std::max(sector->ambient + wallLight, 0));
				L = s_fullbright ? 255u : std::max(0u, std::min(CONV_5bitTo8bit(L), 255u));
				const u32 Lrg = s_sectorDrawMode == SDM_LIGHTING ? L * 7 / 8 : L;
				const u32 lighting = Lrg | (Lrg << 8u) | (L << 16u) | 0xff000000;

				for (u32 t = 0; t < 4; t++) { color[t] = lighting; }
			}
			else
			{
				for (u32 t = 0; t < 4; t++) { color[t] = 0xff1a0f0d; }
			}

			if (s_sectorDrawMode == SDM_WIREFRAME || s_sectorDrawMode == SDM_LIGHTING || overlay)
			{
				drawWallColor(sector, vtx, wall, color, blend, HIT_PART_UNKNOWN, !lowerLayer);
			}
			else
			{
				drawWallTexture(sector, vtx, wall, w, color);
			}
		}
	}

	void highlightWall3d(bool selected, f32 width)
	{
		if ((selected && s_selectedWall < 0) || (!selected && s_hoveredWall < 0)) { return; }

		s32 sectorId = selected ? s_selectedWallSector : s_hoveredWallSector;
		s32 wallId = selected ? s_selectedWall : s_hoveredWall;
		RayHitPart part = selected ? s_selectWallPart : s_hoveredWallPart;
		const u32 color = selected ? 0xffffA040 : 0xffffC080;
		const u32 colors[] = { color, color, color, color };

		const EditorSector* sector = s_levelData->sectors.data() + sectorId;
		const EditorWall* wall = sector->walls.data() + wallId;
		const Vec2f* vtx = sector->vertices.data();

		const EditorSector* next = wall->adjoin >= 0 ? s_levelData->sectors.data() + wall->adjoin : nullptr;
		f32 floorHeight, ceilHeight;
		if (part == HIT_PART_MID || part == HIT_PART_UNKNOWN || !next)
		{
			floorHeight = next ? std::min(sector->floorAlt, next->floorAlt) : sector->floorAlt;
			ceilHeight = next ? std::max(sector->ceilAlt, next->ceilAlt) : sector->ceilAlt;
		}
		else if (part == HIT_PART_BOT)
		{
			floorHeight = sector->floorAlt;
			ceilHeight = next->floorAlt;
		}
		else if (part == HIT_PART_TOP)
		{
			floorHeight = next->ceilAlt;
			ceilHeight = sector->ceilAlt;
		}

		const Vec2f* v0 = &vtx[wall->i0];
		const Vec2f* v1 = &vtx[wall->i1];

		Vec3f lines[8]=
		{
			{v0->x, ceilHeight, v0->z}, {v1->x, ceilHeight, v1->z},
			{v1->x, ceilHeight, v1->z}, {v1->x, floorHeight, v1->z},
			{v1->x, floorHeight, v1->z}, {v0->x, floorHeight, v0->z},
			{v0->x, floorHeight, v0->z}, {v0->x, ceilHeight, v0->z}
		};
		LineDraw3d::addLines(4, width, lines, colors);
	}

	void drawInfWalls3d(f32 width, u32 color)
	{
		const u32 colors[] = { color, color, color, color };

		const u32 count = (u32)s_infWalls.size();
		const InfWall* infWall = s_infWalls.data();
		for (u32 w = 0; w < count; w++, infWall++)
		{
			const EditorSector* sector = infWall->sector;
			const EditorWall* wall = sector->walls.data() + infWall->wallId;
			const Vec2f* vtx = sector->vertices.data();

			// TODO: Draw based on trigger type?
			f32 y0, y1;
			if (infWall->triggerType == TRIGGER_LINE && wall->adjoin >= 0)
			{
				const EditorSector* next = s_levelData->sectors.data() + wall->adjoin;
				y0 = std::min(next->floorAlt, sector->floorAlt);
				y1 = std::max(next->ceilAlt, sector->ceilAlt);
			}
			else
			{
				// Switch of some type.
				if (wall->adjoin < 0)
				{
					const f32 h = sector->floorAlt - sector->ceilAlt;
					y0 = sector->floorAlt;
					y1 = sector->ceilAlt;
				}
				else
				{
					const EditorSector* next = s_levelData->sectors.data() + wall->adjoin;

					if (next->floorAlt < sector->floorAlt)  // lower
					{
						y0 = next->floorAlt;
						y1 = sector->floorAlt;
					}
					else if (next->ceilAlt > sector->ceilAlt)	// upper
					{
						y0 = next->ceilAlt;
						y1 = sector->ceilAlt;
					}
					else // mid
					{
						y0 = sector->floorAlt;
						y1 = sector->ceilAlt;
					}
				}
			}
			Vec3f lines[8];
			lines[0] = { vtx[wall->i0].x, y0, vtx[wall->i0].z };
			lines[1] = { vtx[wall->i1].x, y0, vtx[wall->i1].z };

			lines[2] = { vtx[wall->i1].x, y0, vtx[wall->i1].z };
			lines[3] = { vtx[wall->i1].x, y1, vtx[wall->i1].z };

			lines[4] = { vtx[wall->i1].x, y1, vtx[wall->i1].z };
			lines[5] = { vtx[wall->i0].x, y1, vtx[wall->i0].z };

			lines[6] = { vtx[wall->i0].x, y1, vtx[wall->i0].z };
			lines[7] = { vtx[wall->i0].x, y0, vtx[wall->i0].z };

			LineDraw3d::addLines(4, width, lines, colors);
		}
	}

	void drawSector3d_Lines(const EditorSector* sector, f32 width, u32 color, bool overlay, bool hover)
	{
		const u32 colors[] = { color, color, color, color, color };

		// Draw the walls
		const u32 wallCount = (u32)sector->walls.size();
		const EditorWall* wall = sector->walls.data();
		const Vec2f* vtx = sector->vertices.data();
		for (u32 w = 0; w < wallCount; w++, wall++)
		{
			if (s_showSectorColors && wall->infType != INF_NONE)
			{
				// Only add the wall if we are on the "correct" side.
				Vec2f dir = { vtx[wall->i1].x - vtx[wall->i0].x, vtx[wall->i1].z - vtx[wall->i0].z };
				Vec2f camDir = { s_camera.pos.x - vtx[wall->i0].x, s_camera.pos.z - vtx[wall->i0].z };
				Vec2f nrm = { -dir.z, dir.x };
				if (nrm.x*camDir.x + nrm.z*camDir.z < 0.0f)
				{
					s_infWalls.push_back({ sector, w, (u32)wall->triggerType });
				}
			}

			// Draw the floor piece.
			Vec3f lines[10];
			// floor
			lines[0] = { vtx[wall->i0].x, sector->floorAlt, vtx[wall->i0].z };
			lines[1] = { vtx[wall->i1].x, sector->floorAlt, vtx[wall->i1].z };
			// ceiling
			u32 count = 1;
			if (wall->adjoin < 0 || s_levelData->sectors[wall->adjoin].ceilAlt != sector->ceilAlt || s_camera.pos.y > sector->ceilAlt)
			{
				lines[2] = { vtx[wall->i0].x, sector->ceilAlt, vtx[wall->i0].z };
				lines[3] = { vtx[wall->i1].x, sector->ceilAlt, vtx[wall->i1].z };
				count++;
			}

			if (wall->adjoin < 0)
			{
				lines[count * 2 + 0] = { vtx[wall->i0].x, sector->floorAlt, vtx[wall->i0].z };
				lines[count * 2 + 1] = { vtx[wall->i0].x, sector->ceilAlt, vtx[wall->i0].z };
				count++;

				if (overlay || hover || color != 0xffffffff)
				{
					lines[count * 2 + 0] = { vtx[wall->i1].x, sector->floorAlt, vtx[wall->i1].z };
					lines[count * 2 + 1] = { vtx[wall->i1].x, sector->ceilAlt,  vtx[wall->i1].z };
					count++;
				}
			}
			else
			{
				// lower
				f32 nextFloorAlt = s_levelData->sectors[wall->adjoin].floorAlt;
				if (nextFloorAlt < sector->floorAlt)
				{
					lines[count * 2 + 0] = { vtx[wall->i0].x, sector->floorAlt, vtx[wall->i0].z };
					lines[count * 2 + 1] = { vtx[wall->i0].x, nextFloorAlt, vtx[wall->i0].z };
					count++;
				}

				// upper
				f32 nextCeilAlt = s_levelData->sectors[wall->adjoin].ceilAlt;
				if (nextCeilAlt > sector->ceilAlt)
				{
					lines[count * 2 + 0] = { vtx[wall->i0].x, sector->ceilAlt, vtx[wall->i0].z };
					lines[count * 2 + 1] = { vtx[wall->i0].x, nextCeilAlt, vtx[wall->i0].z };
					count++;
				}
			}
			LineDraw3d::addLines(count, width, lines, colors);
		}
	}

	void drawLineToTarget3d(const Vec3f* p0, s32 sectorId, s32 wallId, f32 width, u32 color)
	{
		const EditorSector* sector = s_levelData->sectors.data() + sectorId;
		const Vec2f* vtx = sector->vertices.data();
		Vec3f target = { 0 };
		target.y = (sector->floorAlt + sector->ceilAlt) * 0.5f;

		if (wallId >= 0)
		{
			const EditorWall* wall = sector->walls.data() + wallId;
			target.x = (vtx[wall->i0].x + vtx[wall->i1].x) * 0.5f;
			target.z = (vtx[wall->i0].z + vtx[wall->i1].z) * 0.5f;
		}
		else
		{
			const u32 vtxCount = (u32)sector->vertices.size();
			for (u32 v = 0; v < vtxCount; v++)
			{
				target.x += vtx[v].x;
				target.z += vtx[v].z;
			}
			const f32 scale = 1.0f / f32(vtxCount);
			target.x *= scale;
			target.z *= scale;
		}
		Vec3f line[] = { *p0, target };
		LineDraw3d::addLine(width, line, &color);
	}

	// If the selected wall or sector has an INF script, draw lines towards
	// 1. Any objects it effects (targets and clients).
	// 2. Any slaves (sector).
	void drawTargetsAndClientLines3d(s32 sectorId, f32 width)
	{
		const EditorSector* sector = s_levelData->sectors.data() + sectorId;
		const EditorInfItem* item = sector->infItem;
		if (!item) { return; }

		Vec3f lineStart = { 0 };
		lineStart.y = (sector->floorAlt + sector->ceilAlt) * 0.5f;
		const Vec2f* vtx = sector->vertices.data();
		const u32 vtxCount = (u32)sector->vertices.size();
		for (u32 v = 0; v < vtxCount; v++)
		{
			lineStart.x += vtx[v].x;
			lineStart.z += vtx[v].z;
		}
		const f32 scale = 1.0f / f32(vtxCount);
		lineStart.x *= scale;
		lineStart.z *= scale;

		for (u32 c = 0; c < (u32)item->classData.size(); c++)
		{
			const EditorInfClassData* cdata = &item->classData[c];
			u32 stopCount = 0;
			if (cdata->iclass == INF_CLASS_TRIGGER)
			{
				stopCount = 1;
			}
			else if (cdata->iclass == INF_CLASS_ELEVATOR)
			{
				stopCount = (u32)cdata->stop.size();
			}

			for (u32 s = 0; s < stopCount; s++)
			{
				const u32 funcCount = (u32)cdata->stop[s].func.size();
				for (u32 f = 0; f < funcCount; f++)
				{
					const u32 funcId = cdata->stop[s].func[f].funcId;
					const u32 clientCount = (u32)cdata->stop[s].func[f].client.size();
					for (u32 cl = 0; cl < clientCount; cl++)
					{
						const EditorSector* clientSector = LevelEditorData::getSector(cdata->stop[s].func[f].client[cl].sectorName.c_str());
						const s32 clientWallId = cdata->stop[s].func[f].client[cl].wallId;
						if (!clientSector) { continue; }

						if (clientWallId >= 0)
						{
							drawLineToTarget3d(&lineStart, clientSector->id, clientWallId, 1.5f * width, 0xffff2020);
						}
						else
						{
							drawLineToTarget3d(&lineStart, clientSector->id, -1, 1.5f * width, 0xffff2020);
						}
					}

					// Draw to the adjoining lines.
					if (funcId == INF_MSG_ADJOIN)
					{
						drawLineToTarget3d(&lineStart, LevelEditorData::getSector(cdata->stop[s].func[f].arg[0].sValue.c_str())->id, cdata->stop[s].func[f].arg[1].iValue, 1.5f * width, 0xff2020ff);
						drawLineToTarget3d(&lineStart, LevelEditorData::getSector(cdata->stop[s].func[f].arg[2].sValue.c_str())->id, cdata->stop[s].func[f].arg[3].iValue, 1.5f * width, 0xff2020ff);
					}
				}
			}
		}
	}

	void drawTargetsAndClientLines3d(s32 sectorId, s32 wallId, f32 width)
	{
		const EditorSector* sector = s_levelData->sectors.data() + sectorId;
		const Vec2f* vtx = sector->vertices.data();
		const EditorWall* wall = sector->walls.data() + wallId;
		const EditorInfItem* item = wall->infItem;
				
		Vec3f lineStart;
		lineStart.x = (vtx[wall->i0].x + vtx[wall->i1].x) * 0.5f;
		lineStart.y = (sector->floorAlt + sector->ceilAlt) * 0.5f;
		lineStart.z = (vtx[wall->i0].z + vtx[wall->i1].z) * 0.5f;

		for (u32 c = 0; c < (u32)item->classData.size(); c++)
		{
			const EditorInfClassData* cdata = &item->classData[c];
			u32 stopCount = 0;
			if (cdata->iclass == INF_CLASS_TRIGGER)
			{
				stopCount = 1;
			}
			else if (cdata->iclass == INF_CLASS_ELEVATOR)
			{
				stopCount = (u32)cdata->stop.size();
			}

			for (u32 s = 0; s < stopCount; s++)
			{
				const u32 funcCount = (u32)cdata->stop[s].func.size();
				for (u32 f = 0; f < funcCount; f++)
				{
					const u32 clientCount = (u32)cdata->stop[s].func[f].client.size();
					for (u32 cl = 0; cl < clientCount; cl++)
					{
						const EditorSector* clientSector = LevelEditorData::getSector(cdata->stop[s].func[f].client[cl].sectorName.c_str());
						const s32 clientWallId = cdata->stop[s].func[f].client[cl].wallId;
						if (!clientSector) { continue; }

						if (clientWallId >= 0)
						{
							drawLineToTarget3d(&lineStart, clientSector->id, clientWallId, 1.5f * width, 0xffff2020);
						}
						else
						{
							drawLineToTarget3d(&lineStart, clientSector->id, -1, 1.5f * width, 0xffff2020);
						}
					}
				}
			}
		}
	}

	void drawTargetsAndClientSlaves3d(s32 sectorId, f32 width)
	{
		const EditorSector* sector = s_levelData->sectors.data() + sectorId;
		const EditorInfItem* item = sector->infItem;
		if (!item) { return; }

		Vec3f lineStart = { 0 };
		lineStart.y = (sector->floorAlt + sector->ceilAlt) * 0.5f;
		const Vec2f* vtx = sector->vertices.data();
		const u32 vtxCount = (u32)sector->vertices.size();
		for (u32 v = 0; v < vtxCount; v++)
		{
			lineStart.x += vtx[v].x;
			lineStart.z += vtx[v].z;
		}
		const f32 scale = 1.0f / f32(vtxCount);
		lineStart.x *= scale;
		lineStart.z *= scale;

		for (u32 c = 0; c < (u32)item->classData.size(); c++)
		{
			const EditorInfClassData* cdata = &item->classData[c];
			for (u32 s = 0; s < (u32)cdata->slaves.size(); s++)
			{
				const EditorSector* slaveSector = LevelEditorData::getSector(cdata->slaves[s].c_str());
				if (!slaveSector) { continue; }

				drawLineToTarget3d(&lineStart, slaveSector->id, -1, 1.25f * width, 0xff20ff20);
			}
		}
	}

	void drawLevel3d(u32 rtWidth, u32 rtHeight)
	{
		Vec3f upDir = { 0.0f, 1.0f, 0.0f };
		Vec3f lookDir = { sinf(s_camera.yaw) * cosf(s_camera.pitch), sinf(s_camera.pitch), cosf(s_camera.yaw) * cosf(s_camera.pitch) };

		s_camera.viewMtx = TFE_Math::computeViewMatrix(&lookDir, &upDir);
		s_camera.projMtx = TFE_Math::computeProjMatrix(c_editorCameraFov, f32(rtWidth) / f32(rtHeight), 0.01f, 5000.0f);
		s_camera.invProj = TFE_Math::computeInvProjMatrix(s_camera.projMtx);

		// Get the current sector (top-down only)
		const s32 layer = s_layerIndex + s_layerMin;
		const Vec2f topDownPos = { s_camera.pos.x, s_camera.pos.z };
		s32 overSector = LevelEditorData::findSector(layer, &topDownPos);
		if (overSector >= 0 && s_levelData->sectors[overSector].floorAlt < s_camera.pos.y)
		{
			overSector = -1;
		}

		if (s_levelData && s_showLowerLayers)
		{
			u32 sectorCount = (u32)s_levelData->sectors.size();
			EditorSector* sector = s_levelData->sectors.data();

			for (u32 i = 0; i < sectorCount; i++, sector++)
			{
				if (sector->layer >= layer) { continue; }
				drawSector3d(sector, &sector->triangles, false, false, true);
			}
			TrianglesColor3d::draw(&s_camera.pos, &s_camera.viewMtx, &s_camera.projMtx, true, s_showGridInSector ? s_gridHeight : c_gridInvisibleHeight);
			TFE_RenderBackend::clearRenderTargetDepth(s_view3d, 1.0f);
		}
		
		f32 pixelSize = 1.0f / (f32)rtHeight;
		if (!s_levelData)
		{
			Grid3d::draw(s_gridSize, s_gridHeight, s_subGridSize, s_gridOpacity, pixelSize, &s_camera.pos, &s_camera.viewMtx, &s_camera.projMtx);
		}

		if (!s_levelData) { return; }
		u32 sectorCount = (u32)s_levelData->sectors.size();
		EditorSector* sector = s_levelData->sectors.data();
		// Draw the sector faces.
		u32 color[] = { 0xff1a0f0d, 0xff1a0f0d, 0xff1a0f0d, 0xff1a0f0d };
		for (u32 i = 0; i < sectorCount; i++, sector++)
		{
			if (sector->layer != layer) { continue; }
			drawSector3d(sector, &sector->triangles);
		}
		TrianglesColor3d::draw(&s_camera.pos, &s_camera.viewMtx, &s_camera.projMtx, true, s_showGridInSector ? s_gridHeight : c_gridInvisibleHeight);

		const f32 width = 3.0f / f32(rtHeight);
		// Walls
		if (s_showSectorColors)
		{
			const u32 sectorClr[] = { 0xffffffff, 0xffff80ff, 0xffffff80 };
			s_infWalls.clear();
			for (u32 i = 0; i < INF_COUNT; i++)
			{
				sector = s_levelData->sectors.data();
				for (u32 s = 0; s < sectorCount; s++, sector++)
				{
					if (sector->layer != layer || sector->infType != i) { continue; }
					drawSector3d_Lines(sector, width, sectorClr[i]);
				}
			}
			drawInfWalls3d(width, 0xff2020ff);
		}
		else
		{
			sector = s_levelData->sectors.data();
			for (u32 i = 0; i < sectorCount; i++, sector++)
			{
				if (sector->layer != layer) { continue; }
				drawSector3d_Lines(sector, width, 0xffffffff);
			}
		}

		// Draw the 3d cursor in "Draw Mode"
		if (s_editMode == LEDIT_DRAW)
		{
			const f32 distFromCam = TFE_Math::distance(&s_cursor3d, &s_camera.pos);
			const f32 size = distFromCam * 16.0f / f32(rtHeight);

			drawBox(&s_cursor3d, size, 3.0f / f32(rtHeight), 0x80ff8020);
		}

		LineDraw3d::draw(&s_camera.pos, &s_camera.viewMtx, &s_camera.projMtx);

		// Draw selected or hovered elements.
		if (s_editMode == LEDIT_SECTOR && (s_hoveredSector >= 0 || s_selectedSector >= 0))
		{
			// Draw solid sector.
			if (s_selectedSector >= 0)
			{
				sector = s_levelData->sectors.data() + s_selectedSector;
				drawSector3d(sector, &sector->triangles, true, false);
			}
			if (s_hoveredSector >= 0)
			{
				sector = s_levelData->sectors.data() + s_hoveredSector;
				drawSector3d(sector, &sector->triangles, true, true);
			}
			TrianglesColor3d::draw(&s_camera.pos, &s_camera.viewMtx, &s_camera.projMtx, true, s_showGridInSector ? s_gridHeight : c_gridInvisibleHeight);

			// Draw outline (no depth buffer).
			if (s_selectedSector >= 0)
			{
				sector = s_levelData->sectors.data() + s_selectedSector;
				drawSector3d_Lines(sector, width * 1.25f, 0xffffA040);
			}
			if (s_hoveredSector >= 0)
			{
				sector = s_levelData->sectors.data() + s_hoveredSector;
				drawSector3d_Lines(sector, width * 1.25f, 0xffffC080);
			}
			LineDraw3d::draw(&s_camera.pos, &s_camera.viewMtx, &s_camera.projMtx, false);
		}
		if (s_editMode == LEDIT_WALL && (s_hoveredWall >= 0 || s_selectedWall >= 0))
		{
			// Draw solid wall.
			if (s_selectedWall >= 0)
			{
				u32 color[] = { 0x40ff4020, 0x40ff4020 };
				sector = s_levelData->sectors.data() + s_selectedWallSector;
				drawWallColor(sector, sector->vertices.data(), sector->walls.data() + s_selectedWall, color, true, s_selectWallPart);
			}
			if (s_hoveredWall >= 0)
			{
				u32 color[] = { 0x40ff8020, 0x40ff8020 };
				sector = s_levelData->sectors.data() + s_hoveredWallSector;
				drawWallColor(sector, sector->vertices.data(), sector->walls.data() + s_hoveredWall, color, true, s_hoveredWallPart);
			}
			TrianglesColor3d::draw(&s_camera.pos, &s_camera.viewMtx, &s_camera.projMtx, true, s_showGridInSector ? s_gridHeight : c_gridInvisibleHeight);

			// Draw the selected sector before drawing the walls.
			if (s_selectedWall >= 0 || s_hoveredWall >= 0)
			{
				highlightWall3d(true, width*1.25f);
				highlightWall3d(false, width*1.25f);
			}
			LineDraw3d::draw(&s_camera.pos, &s_camera.viewMtx, &s_camera.projMtx, false);
		}
		if (s_editMode == LEDIT_VERTEX && (s_hoveredVertex >= 0 || s_selectedVertex >= 0))
		{
			// Draw the selected sector before drawing the walls.
			if (s_selectedVertex >= 0 || s_hoveredVertex >= 0)
			{
				if (s_selectedVertex >= 0)
				{					
					sector = s_levelData->sectors.data() + s_selectedVertexSector;
					const Vec2f* vtx = sector->vertices.data() + s_selectedVertex;
					const Vec3f pos = { vtx->x, s_selectVertexPart == HIT_PART_FLOOR ? sector->floorAlt : sector->ceilAlt , vtx->z };

					const f32 distFromCam = TFE_Math::distance(&pos, &s_camera.pos);
					const f32 size = distFromCam * 16.0f / f32(rtHeight);

					// Add a screen aligned quad.
					drawScreenQuad(&pos, size, 0xA0ff4020);
					drawScreenQuadOutline(&pos, size, width, 0xffffA040);
				}
				if (s_hoveredVertex >= 0)
				{
					sector = s_levelData->sectors.data() + s_hoveredVertexSector;
					const Vec2f* vtx = sector->vertices.data() + s_hoveredVertex;
					const Vec3f pos = { vtx->x, s_hoveredVertexPart == HIT_PART_FLOOR ? sector->floorAlt : sector->ceilAlt , vtx->z };

					const f32 distFromCam = TFE_Math::distance(&pos, &s_camera.pos);
					const f32 size = distFromCam * 16.0f / f32(rtHeight);

					drawScreenQuad(&pos, size, 0xA0ff8020);
					drawScreenQuadOutline(&pos, size, width, 0xffffC080);
				}
			}
			TrianglesColor3d::draw(&s_camera.pos, &s_camera.viewMtx, &s_camera.projMtx, false, s_showGridInSector ? s_gridHeight : c_gridInvisibleHeight);
			LineDraw3d::draw(&s_camera.pos, &s_camera.viewMtx, &s_camera.projMtx, false);
		}

		if (s_selectedSector >= 0 && s_levelData->sectors[s_selectedSector].infType != INF_NONE)
		{
			// If the selected wall or sector has an INF script, draw lines towards
			drawTargetsAndClientLines3d(s_selectedSector, width);
			drawTargetsAndClientSlaves3d(s_selectedSector, width);
		}
		else if (s_selectedWall >= 0 && s_levelData->sectors[s_selectedWallSector].walls[s_selectedWall].infType != INF_NONE)
		{
			drawTargetsAndClientLines3d(s_selectedWallSector, s_selectedWall, width);
		}
		LineDraw3d::draw(&s_camera.pos, &s_camera.viewMtx, &s_camera.projMtx, false);

		// Objects
		u32 alphaFg = s_editMode == LEDIT_ENTITY ? 0xff000000 : 0xA0000000;
		u32 alphaBg = s_editMode == LEDIT_ENTITY ? 0xA0000000 : 0x60000000;
		sector = s_levelData->sectors.data();
		for (u32 i = 0; i < sectorCount; i++, sector++)
		{
			if (sector->layer != layer) { continue; }

			// For now just draw a quad + image.
			const u32 objCount = (u32)sector->objects.size();
			const EditorLevelObject* obj = sector->objects.data();
			for (u32 o = 0; o < objCount; o++, obj++)
			{
				f32 bwidth = 3.0f / f32(rtHeight);
				u32 bcolor = 0x00ffd7a4 | alphaBg;
				const bool selected = s_selectedEntity == o && s_selectedEntitySector == i;
				if ((s_hoveredEntity == o && s_hoveredEntitySector == i) || selected)
				{
					bwidth *= 1.25f;
					bcolor = 0x00ffe7b4 | alphaFg;
				}

				if (obj->oclass == CLASS_3D)
				{
					TFE_EditorRender::drawModel3d(sector, obj->displayModel, &obj->pos, &obj->rotMtx, s_palette->colors, alphaFg);
					TFE_EditorRender::drawModel3d_Bounds(obj->displayModel, &obj->pos, &obj->rotMtx, bwidth, bcolor);
				}
				else
				{
					// Draw the bounding box.
					drawBounds(&obj->worldCen, &obj->worldExt, bwidth, bcolor);

					if (obj->display)
					{
						const Vec3f r = s_camera.viewMtx.m0;
						const Vec3f u = { 0.0f, 1.0f, 0.0f };
						const Vec3f vtx[] =
						{
							{obj->worldCen.x - obj->worldExt.x * r.x, obj->worldCen.y - obj->worldExt.x * r.y - obj->worldExt.y * u.y, obj->worldCen.z - obj->worldExt.z * r.z },
							{obj->worldCen.x + obj->worldExt.x * r.x, obj->worldCen.y + obj->worldExt.x * r.y - obj->worldExt.y * u.y, obj->worldCen.z + obj->worldExt.z * r.z },
							{obj->worldCen.x + obj->worldExt.x * r.x, obj->worldCen.y + obj->worldExt.x * r.y + obj->worldExt.y * u.y, obj->worldCen.z + obj->worldExt.z * r.z },

							{obj->worldCen.x - obj->worldExt.x * r.x, obj->worldCen.y - obj->worldExt.x * r.y - obj->worldExt.y * u.y, obj->worldCen.z - obj->worldExt.z * r.z },
							{obj->worldCen.x + obj->worldExt.x * r.x, obj->worldCen.y + obj->worldExt.x * r.y + obj->worldExt.y * u.y, obj->worldCen.z + obj->worldExt.z * r.z },
							{obj->worldCen.x - obj->worldExt.x * r.x, obj->worldCen.y - obj->worldExt.x * r.y + obj->worldExt.y * u.y, obj->worldCen.z - obj->worldExt.z * r.z },
						};

						const Vec2f uv[6] =
						{
							{0.0f, 0.0f},
							{1.0f, 0.0f},
							{1.0f, 1.0f},

							{0.0f, 0.0f},
							{1.0f, 1.0f},
							{0.0f, 1.0f},
						};
						u32 baseRGB = selected ? 0x00ffa0a0 : 0x00ffffff;
						u32 colorFg[2] = { baseRGB | alphaFg, baseRGB | alphaFg };
						TrianglesColor3d::addTexturedTriangles(2, vtx, uv, uv, colorFg, obj->display->texture, TrianglesColor3d::TRANS_BLEND_CLAMP);

						// Draw a direction.
						const f32 angle = obj->orientation.y * PI / 180.0f;
						const Vec3f dir = { sinf(angle), 0.0f, cosf(angle) };

						// Make an arrow.
						Vec3f dP  = { dir.x * obj->worldExt.x, obj->worldExt.y, dir.z * obj->worldExt.z };
						Vec3f cen = { obj->worldCen.x + dP.x, obj->worldCen.y + dP.y, obj->worldCen.z + dP.z };
						Vec3f tan = { -dir.z * obj->worldExt.x * 0.5f, 0.0f, dir.x * obj->worldExt.z * 0.5f };
						// Make sure the direction arrow is visible.
						cen.y = std::min(cen.y, sector->floorAlt - 0.1f);

						Vec3f arrowVtx[] =
						{
							cen, {cen.x + tan.x - dP.x * 0.25f, cen.y, cen.z + tan.z - dP.z * 0.25f},
							cen, {cen.x - tan.x - dP.x * 0.25f, cen.y, cen.z - tan.z - dP.z * 0.25f},
						};

						const bool highlight = (s_hoveredEntity == o && s_hoveredEntitySector == i) || selected;
						const u32 alphaArrow = s_editMode == LEDIT_ENTITY ? (highlight ? 0xA0000000 : 0x60000000) : 0x30000000;
						u32 baseRGBArrow = 0x00ffff00;
						u32 baseClrArrow = baseRGBArrow | alphaArrow;
						u32 color[] = { baseClrArrow, baseClrArrow, baseClrArrow, baseClrArrow };
						LineDraw3d::addLines(2, bwidth, arrowVtx, color);
					}
				}
			}
		}
		TrianglesColor3d::draw(&s_camera.pos, &s_camera.viewMtx, &s_camera.projMtx, true, s_showGridInSector ? s_gridHeight : c_gridInvisibleHeight);
		LineDraw3d::draw(&s_camera.pos, &s_camera.viewMtx, &s_camera.projMtx, true, false);
		
		if (overSector >= 0 && s_gridAutoAdjust)
		{
			s_gridHeight = s_levelData->sectors[overSector].floorAlt;
		}
		if (overSector < 0 || s_showGridInSector)
		{
			Grid3d::draw(s_gridSize, s_gridHeight, s_subGridSize, s_gridOpacity, pixelSize, &s_camera.pos, &s_camera.viewMtx, &s_camera.projMtx);
		}
	}

	void saveDosBoxConfig()
	{
		char srcConfigPath[TFE_MAX_PATH];
		char dstConfigPath[TFE_MAX_PATH];
		TFE_Paths::appendPath(TFE_PathType::PATH_EMULATOR, "Dosbox.conf", srcConfigPath);
		TFE_Paths::appendPath(TFE_PathType::PATH_EMULATOR, "Dosbox_Temp.conf", dstConfigPath);

		FileUtil::copyFile(srcConfigPath, dstConfigPath);
	}

	void restoreDosBoxConfig()
	{
		char srcConfigPath[TFE_MAX_PATH];
		char dstConfigPath[TFE_MAX_PATH];
		TFE_Paths::appendPath(TFE_PathType::PATH_EMULATOR, "Dosbox_Temp.conf", srcConfigPath);
		TFE_Paths::appendPath(TFE_PathType::PATH_EMULATOR, "Dosbox.conf", dstConfigPath);

		FileUtil::copyFile(srcConfigPath, dstConfigPath);
		FileUtil::deleteFile(srcConfigPath);
	}

	bool writeDosBoxConfig(const char* levelName)
	{
		// Read the existing config.
		char srcConfigPath[TFE_MAX_PATH];
		TFE_Paths::appendPath(TFE_PathType::PATH_PROGRAM, "DosBox/DefaultDosBoxConfig.txt", srcConfigPath);

		FileStream srcConfig;
		if (!srcConfig.open(srcConfigPath, FileStream::MODE_READ))
		{
			return false;
		}

		const size_t len = srcConfig.getSize();
		std::vector<char> dosBoxConfig(len + 512);
		srcConfig.readBuffer(dosBoxConfig.data(), (u32)len);
		srcConfig.close();

		char* newConfig = dosBoxConfig.data();
		char commandLine[512];
		sprintf(commandLine, "dark.exe -uEDITOR.GOB -l%s -c\r\nexit\r\n", levelName);
		strcat(newConfig, commandLine);

		char dstConfigPath[TFE_MAX_PATH];
		TFE_Paths::appendPath(TFE_PathType::PATH_EMULATOR, "Dosbox.conf", dstConfigPath);

		FileStream config;
		if (!config.open(dstConfigPath, FileStream::MODE_WRITE))
		{
			return false;
		}
		config.writeBuffer(newConfig, (u32)strlen(newConfig));
		config.close();

		return true;
	}
	
	void play(bool playFromDos)
	{
		bool res = LevelEditorData::generateLevelData();
		res &= LevelEditorData::generateInfAsset();
		res &= LevelEditorData::generateObjects();
		if (res)
		{
			Vec2f pos2d = { s_camera.pos.x, s_camera.pos.z };
			s32 sectorId = LevelEditorData::findSector(s_layerIndex + s_layerMin, &pos2d);
			LevelObjectData* levelObj = TFE_LevelObjects::getLevelObjectData();

			if (playFromDos)
			{
				// Temporarily move the start point.
				s32 startIndex = -1;
				Vec3f originalStartPos;
				f32 originalAngle;
				LevelObject* obj = levelObj->objects.data();
				for (u32 i = 0; i < levelObj->objectCount; i++, obj++)
				{
					if (obj->oclass == CLASS_SPIRIT && obj->logics.size() && obj->logics[0].type == LOGIC_PLAYER)
					{
						startIndex = s32(i);
						originalStartPos = obj->pos;
						originalAngle = obj->orientation.y;
						obj->pos = s_camera.pos;
						obj->orientation.y = 180.0f + 360.0f * s_camera.yaw / (2.0f * PI);
					}
				}

				// Create GOB
				char gobPath[TFE_MAX_PATH];
				TFE_Paths::appendPath(PATH_SOURCE_DATA, "editor.gob", gobPath);
				Archive::deleteCustomArchive(s_outGob);
				s_outGob = Archive::createCustomArchive(ARCHIVE_GOB, gobPath);

				char basePath[TFE_MAX_PATH];
				TFE_Paths::appendPath(PATH_PROGRAM_DATA, s_levelData->name.c_str(), basePath);

				char levName[TFE_MAX_PATH];
				char objName[TFE_MAX_PATH];
				char infName[TFE_MAX_PATH];
				sprintf(levName, "%s.LEV", s_levelData->name.c_str());
				sprintf(objName, "%s.O",   s_levelData->name.c_str());
				sprintf(infName, "%s.INF", s_levelData->name.c_str());

				char levPath[TFE_MAX_PATH];
				char objPath[TFE_MAX_PATH];
				char infPath[TFE_MAX_PATH];
				TFE_Paths::appendPath(PATH_PROGRAM_DATA, levName, levPath);
				TFE_Paths::appendPath(PATH_PROGRAM_DATA, objName, objPath);
				TFE_Paths::appendPath(PATH_PROGRAM_DATA, infName, infPath);

				TFE_LevelAsset::save(s_levelData->name.c_str(), levPath);
				//TFE_InfAsset::save(infName, infPath);
				TFE_LevelObjects::save(s_levelData->name.c_str(), objPath);

				s_outGob->addFile(levName, levPath);
				s_outGob->addFile(objName, objPath);
				//s_outGob->addFile(infName, infPath);

				// Run
				saveDosBoxConfig();
				writeDosBoxConfig(s_levelData->name.c_str());

				char dosBoxPath[TFE_MAX_PATH];
				TFE_Paths::appendPath(TFE_PathType::PATH_EMULATOR, "Dosbox.exe", dosBoxPath);
				TFE_System::osShellExecute(dosBoxPath, TFE_Paths::getPath(TFE_PathType::PATH_EMULATOR), nullptr, true);

				restoreDosBoxConfig();
				if (startIndex >= 0)
				{
					LevelObject* obj = levelObj->objects.data() + startIndex;
					obj->pos = originalStartPos;
					obj->orientation.y = originalAngle;
				}
			}
			else
			{
				u32 w = 320, h = 200;
				if (TFE_GameLoop::startLevelFromExisting(&s_camera.pos, -s_camera.yaw + PI, sectorId, s_palette, levelObj, s_renderer, w, h))
				{
					s_runLevel = true;
					TFE_Input::enableRelativeMode(true);
					s_renderer->enableScreenClear(false);
				}
			}
		}
	}

	void draw(bool* isActive)
	{
		if (s_runLevel)
		{
			return;
		}

		// Draw the tool bar.
		toolbarBegin();
		{
			if (drawToolbarButton(s_editCtrlToolbarData, 0, false))
			{
				play(false);
			}
			ImGui::SameLine(0.0f, 32.0f);

			for (u32 i = 1; i < 6; i++)
			{
				if (drawToolbarButton(s_editCtrlToolbarData, i, i == s_editMode))
				{
					s_editMode = i;
					s_enableInfEditor = false;
					s_drawStarted = false;
					s_moveVertex = false;

					clearNewSector();
				}
				ImGui::SameLine();
			}
			ImGui::SameLine(0.0f, 32.0f);

			for (u32 i = 0; i < 6; i++)
			{
				if (drawToolbarButton(s_booleanToolbarData, i, i == s_boolMode))
				{
					s_boolMode = i;
				}
				ImGui::SameLine();
			}

			// TODO: Toolbar for rendering mode:
			// Wireframe, Shaded, Textured, Textured & Shaded

			ImGui::SameLine(0.0f, 32.0f);
			ImGui::PushItemWidth(64.0f);
			if (ImGui::Combo("Grid Size", &s_gridIndex, c_gridSizes, IM_ARRAYSIZE(c_gridSizes)))
			{
				s_gridSize = c_gridSizeMap[s_gridIndex];
			}
			ImGui::PopItemWidth();
			// Get the "Grid Size" combo position to align the message panel later.
			const ImVec2 itemPos = ImGui::GetItemRectMin();
						
			ImGui::SameLine(0.0f, 32.0f);
			ImGui::PushItemWidth(64.0f);
			ImGui::Combo("Layer", &s_layerIndex, s_layerStr, IM_ARRAYSIZE(s_layerStr));
			ImGui::PopItemWidth();

			// Message Panel
			messagePanel(itemPos);
		}
		toolbarEnd();
		
		// Draw the info bars.
		s_infoHeight = 486;

		infoToolBegin(s_infoHeight);
		{
			if (s_hoveredVertex >= 0 || s_selectedVertex >= 0)
			{
				infoPanelVertex();
			}
			else if (s_hoveredSector >= 0 || s_selectedSector >= 0)
			{
				infoPanelSector();
			}
			else if (s_hoveredWall >= 0 || s_selectedWall >= 0)
			{
				infoPanelWall();
			}
			else if (s_hoveredEntity >= 0 || s_selectedEntity >= 0)
			{
				infoPanelEntity();
			}
			else
			{
				infoPanelMap();
			}
		}
		infoToolEnd();
		// Browser
		browserBegin(s_infoHeight);
		{
			if (s_editMode == LEDIT_ENTITY)
			{
				browseEntities();
			}
			else
			{
				browseTextures();
			}
		}
		browserEnd();

		// Draw the grid.
		bool recreateRendertarget = !s_view3d;
		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);
		const u32 curWidth  = displayInfo.width - 480 - 16;
		const u32 curHeight = displayInfo.height - 68 - 18;
		if (s_view3d)
		{
			u32 width, height;
			TFE_RenderBackend::getRenderTargetDim(s_view3d, &width, &height);
												
			if (curWidth != width || curHeight != height)
			{
				TFE_RenderBackend::freeRenderTarget(s_view3d);
				s_view3d = nullptr;
				recreateRendertarget = true;
			}
		}
		if (recreateRendertarget)
		{
			s_view3d = TFE_RenderBackend::createRenderTarget(curWidth, curHeight, true);
		}
		TFE_RenderBackend::bindRenderTarget(s_view3d);
		{
			const f32 clearColor[] = { 0.05f, 0.06f, 0.1f, 1.0f };
			TFE_RenderBackend::clearRenderTarget(s_view3d, clearColor, 1.0f);
			TFE_RenderState::setColorMask(CMASK_RGB);

			// Calculate the sub-grid size.
			if (s_gridSize > 1.0f)
			{
				s_subGridSize = floorf(log2f(1.0f / s_zoomVisual));
				s_subGridSize = powf(2.0f, s_subGridSize);

				f32 scaledGrid = s_gridSize / s_subGridSize;
				if (floorf(scaledGrid * 100.0f)*0.01f != scaledGrid)
				{
					s_subGridSize = s_gridSize * 4.0f;
				}
			}
			else
			{
				s_subGridSize = std::min(floorf(log10f(0.1f / s_zoomVisual)), 2.0f);
				s_subGridSize = powf(10.0f, s_subGridSize);
			}
			s_subGridSize = std::max(0.0f, s_subGridSize);

			u32 rtWidth, rtHeight;
			TFE_RenderBackend::getRenderTargetDim(s_view3d, &rtWidth, &rtHeight);
			if (s_editView == EDIT_VIEW_2D)      { drawLevel2d(rtWidth, rtHeight); }
			else if (s_editView == EDIT_VIEW_3D) { drawLevel3d(rtWidth, rtHeight); }
		}
		TFE_RenderBackend::unbindRenderTarget();
		TFE_RenderState::setColorMask(CMASK_ALL);

		// Draw the edit window
		levelEditWinBegin();
		{
			const TextureGpu* texture = TFE_RenderBackend::getRenderTargetTexture(s_view3d);
			ImGui::ImageButton(TFE_RenderBackend::getGpuPtr(texture), { (f32)curWidth, (f32)curHeight }, { 0.0f, 0.0f }, { 1.0f, 1.0f }, 0, { 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f });
			const ImVec2 itemPos = ImGui::GetItemRectMin();
			s_editWinMapCorner = { itemPos.x, itemPos.y };

			s_hoveredSector = -1;
			s_hoveredWall = -1;
			if ((ImGui::IsWindowHovered() || ImGui::IsWindowFocused()) && (!HelpWindow::isOpen() || HelpWindow::isMinimized()))
			{
				editWinControls((s32)curWidth, (s32)curHeight);
			}

			// Draw the position of the currently hovered vertex.
			if (s_hoveredVertex >= 0 && s_hoveredVertex < s_levelData->sectors[s_hoveredVertexSector].vertices.size() && s_editView == EDIT_VIEW_2D)
			{
				const EditorSector* sector = s_levelData->sectors.data() + s_hoveredVertexSector;
				const Vec2f* vtx = sector->vertices.data() + s_hoveredVertex;
				const f32 rcpZoom = 1.0f / s_zoomVisual;
				s32 x = s32(( vtx->x - s_offset.x) * rcpZoom + s_editWinMapCorner.x ) - 56;
				s32 y = s32((-vtx->z - s_offset.z) * rcpZoom + s_editWinMapCorner.z ) - 40;
				x = std::min(s32(s_editWinMapCorner.x + s_editWinSize.x) - 128, std::max((s32)s_editWinMapCorner.x, x));
				y = std::min(s32(s_editWinMapCorner.z + s_editWinSize.z) - 32, std::max((s32)s_editWinMapCorner.z, y));

				const ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
					| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoInputs;

				ImVec4 windowBg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
				windowBg.w *= 0.75f;
				ImGui::PushStyleColor(ImGuiCol_ChildBg, windowBg);
				ImGui::SetNextWindowPos({ f32(x), f32(y) });
				ImGui::BeginChild("Coordinates", ImVec2(128, 32), true, window_flags);
				{
					ImGui::TextColored({ 1.0f, 1.0f, 1.0f, 0.75f }, "%0.2f, %0.2f", vtx->x, vtx->z);
				}
				ImGui::EndChild();
				ImGui::PopStyleColor();
			}
		}
		levelEditWinEnd();

		// Show the error popup if needed.
		showErrorPopup();

		// Load and other actions.
		if (s_loadLevel)
		{
			loadLevel();
			s_loadLevel = false;
		}
	}
		
	void menu()
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New", "Ctrl+N", (bool*)NULL))
			{
				// GOB (creates a new Gob if it doesn't exist)
				// Name
				// Slot (SECBASE, TALAY, etc.)
				// Resources (TEXTURE.GOB, SPRITES.GOB, SOUNDS.GOB, DARK.GOB, ...)
			}
			if (ImGui::MenuItem("Copy Level", NULL, (bool*)NULL))
			{
				// Source GOB
				// Select Level
				// Destination GOB
				// New Map from copy (same as New Level)
			}
			if (ImGui::MenuItem("Open", "Ctrl+O", (bool*)NULL))
			{
				// Open GOB
				// Open Level
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Close", NULL, (bool*)NULL))
			{
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Save", "Ctrl+S", (bool*)NULL))
			{
			}
			ImGui::Separator();
			// Path to Gob : filename
			if (ImGui::BeginMenu("Recent"))
			{
				for (u32 i = 0; i < s_recentCount; i++)
				{
					char recent[TFE_MAX_PATH];
					sprintf(recent, "%s: \"%s\" (%s)", s_recentLevels[i].gobName, s_recentLevels[i].levelName, s_recentLevels[i].levelFilename);
					if (ImGui::MenuItem(recent, NULL, (bool*)NULL))
					{
						strcpy(s_gobFile, s_recentLevels[i].gobPath);
						strcpy(s_levelFile, s_recentLevels[i].levelFilename);
						strcpy(s_levelName, s_recentLevels[i].levelName);
						s_loadLevel = true;
					}
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Undo", "Ctrl+Z", (bool*)NULL))
			{
			}
			if (ImGui::MenuItem("Redo", "Ctrl+Y", (bool*)NULL))
			{
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Cut", "Ctrl+X", (bool*)NULL))
			{
			}
			if (ImGui::MenuItem("Copy", "Ctrl+C", (bool*)NULL))
			{
			}
			if (ImGui::MenuItem("Paste", "Ctrl+V", (bool*)NULL))
			{
			}
			if (ImGui::MenuItem("Duplicate", "Ctrl+D", (bool*)NULL))
			{
			}
			if (ImGui::MenuItem("Delete", "Del", (bool*)NULL))
			{
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Clear Selection", "Backspace", (bool*)NULL))
			{
				s_selectedSector = -1;
				s_selectedVertex = -1;
				s_selectedWall = -1;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View"))
		{
			if (ImGui::MenuItem("2D", "Ctrl+1", s_editView == EDIT_VIEW_2D))
			{
				if (s_editView == EDIT_VIEW_3D)
				{
					const Vec2f scale = { s_editWinSize.x*0.5f, s_editWinSize.z*0.5f };
					s_offset.x =  s_camera.pos.x - scale.x*s_zoomVisual;
					s_offset.z = -s_camera.pos.z - scale.z*s_zoomVisual;
				}
				s_editView = EDIT_VIEW_2D;
				s_drawStarted = false;
			}
			if (ImGui::MenuItem("3D (Editor)", "Ctrl+2", s_editView == EDIT_VIEW_3D))
			{
				if (s_editView == EDIT_VIEW_2D)
				{
					const Vec2f scale = { s_editWinSize.x*0.5f, s_editWinSize.z*0.5f };
					s_camera.pos.x =  s_offset.x + scale.x*s_zoomVisual;
					s_camera.pos.z = -s_offset.z - scale.z*s_zoomVisual;
				}
				s_editView = EDIT_VIEW_3D;
				s_drawStarted = false;
			}
			if (ImGui::MenuItem("3D (Game)", "Ctrl+3", s_editView == EDIT_VIEW_3D_GAME))
			{
				s_editView = EDIT_VIEW_3D_GAME;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Show Lower Layers", "Ctrl+L", s_showLowerLayers))
			{
				s_showLowerLayers = !s_showLowerLayers;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Wireframe", "Ctrl+F1", s_sectorDrawMode == SDM_WIREFRAME))
			{
				s_sectorDrawMode = SDM_WIREFRAME;
			}
			if (ImGui::MenuItem("Lighting", "Ctrl+F2", s_sectorDrawMode == SDM_LIGHTING))
			{
				s_sectorDrawMode = SDM_LIGHTING;
			}
			if (ImGui::MenuItem("Textured (Floor)", "Ctrl+F3", s_sectorDrawMode == SDM_TEXTURED_FLOOR))
			{
				s_sectorDrawMode = SDM_TEXTURED_FLOOR;
			}
			if (ImGui::MenuItem("Textured (Ceiling)", "Ctrl+F4", s_sectorDrawMode == SDM_TEXTURED_CEIL))
			{
				s_sectorDrawMode = SDM_TEXTURED_CEIL;
			}
			if (ImGui::MenuItem("Fullbright", "Ctrl+F5", s_fullbright))
			{
				s_fullbright = !s_fullbright;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Display Sector Colors", "", s_showSectorColors))
			{
				s_showSectorColors = !s_showSectorColors;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Run"))
		{
			if (ImGui::MenuItem("Run [The Force Engine]", "Ctrl+R", (bool*)NULL))
			{
				play(false);
			}
			if (ImGui::MenuItem("Run [Dark Forces]", "Ctrl+T", (bool*)NULL))
			{
				play(true);
			}
			ImGui::EndMenu();
		}
	}

	void toolbarBegin()
	{
		bool toolbarActive = true;

		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		ImGui::SetWindowPos("MainToolbar", { 0.0f, 19.0f });
		ImGui::SetWindowSize("MainToolbar", { (f32)displayInfo.width - 480.0f, 48.0f });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus;

		ImGui::Begin("MainToolbar", &toolbarActive, window_flags);
	}

	void toolbarEnd()
	{
		ImGui::End();
	}
				
	void levelEditWinBegin()
	{
		bool gridActive = true;

		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		s_editWinSize = { (f32)displayInfo.width - 480.0f, (f32)displayInfo.height - 68.0f };

		ImGui::SetWindowPos("LevelEditWin", { s_editWinPos.x, s_editWinPos.z });
		ImGui::SetWindowSize("LevelEditWin", { s_editWinSize.x, s_editWinSize.z });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus;

		ImGui::Begin("LevelEditWin", &gridActive, window_flags);
	}

	void levelEditWinEnd()
	{
		ImGui::End();
	}

	void messagePanel(ImVec2 pos)
	{
		bool msgPanel = true;
		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground;

		ImGui::SetNextWindowPos({ pos.x, pos.y + 24.0f });
		ImGui::BeginChild("MsgPanel", { 256.0f, 20.0f }, false, window_flags);

		s32 mx, my;
		TFE_Input::getMousePos(&mx, &my);
		if (mx >= s_editWinPos.x && mx < s_editWinPos.x + s_editWinSize.x && my >= s_editWinPos.z && my < s_editWinPos.z + s_editWinSize.z)
		{
			// We want to zoom into the mouse position.
			s32 relX = s32(mx - s_editWinMapCorner.x);
			s32 relY = s32(my - s_editWinMapCorner.z);
			// Old position in world units.
			Vec2f worldPos;
			worldPos.x = s_offset.x + f32(relX) * s_zoomVisual;
			worldPos.z = s_offset.z + f32(relY) * s_zoomVisual;
			if (s_editView == EDIT_VIEW_2D)
			{
				ImGui::TextColored({ 0.5f, 0.5f, 0.5f, 0.75f }, "Pos %0.2f, %0.2f   Sub-grid %0.2f", worldPos.x, -worldPos.z, s_gridSize / s_subGridSize);
			}
			else
			{
				ImGui::TextColored({ 0.5f, 0.5f, 0.5f, 0.75f }, "Dir %0.3f, %0.3f, %0.3f   Sub-grid %0.2f", s_rayDir.x, s_rayDir.y, s_rayDir.z, s_gridSize / s_subGridSize);
			}
		}

		ImGui::EndChild();
	}
			   
	void infoToolBegin(s32 height)
	{
		bool infoTool = true;

		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		ImGui::SetWindowPos("Info Panel", { (f32)displayInfo.width - 480.0f, 19.0f });
		ImGui::SetWindowSize("Info Panel", { 480.0f, f32(height) });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;

		ImGui::Begin("Info Panel", &infoTool, window_flags); ImGui::SameLine();
	}
		
	void infoPanelMap()
	{
		if (!s_levelData) { return; }

		ImGui::Text("Level Name:"); ImGui::SameLine(); ImGui::InputText("", s_levelName, 64);
		ImGui::Text("Gob File: %s", s_gobFile);
		ImGui::Text("Level File: %s", s_levelFile);
		ImGui::Text("Sector Count: %u", s_levelData->sectors.size());
		ImGui::Text("Layer Range: %d, %d", s_levelData->layerMin, s_levelData->layerMax);
		ImGui::Separator();
		ImGui::LabelText("##GridLabel", "Grid Height");
		ImGui::SetNextItemWidth(196.0f);
		ImGui::InputFloat("##GridHeight", &s_gridHeight, 0.0f, 0.0f, "%0.2f", ImGuiInputTextFlags_CharsDecimal);
		ImGui::Checkbox("Grid Auto Adjust", &s_gridAutoAdjust);
		ImGui::Checkbox("Show Grid When Camera Is Inside a Sector", &s_showGridInSector);
	}

	void infoPanelVertex()
	{
		if (!s_levelData || (s_selectedVertex < 0 && s_hoveredVertex < 0)) { return; }
		EditorSector* sector = s_levelData->sectors.data() + ( (s_selectedVertex >= 0) ? s_selectedVertexSector : s_hoveredVertexSector );
		s32 index = s_selectedVertex >= 0 ? s_selectedVertex : s_hoveredVertex;
		if (index < 0 || !sector) { return; }

		Vec2f* vtx = sector->vertices.data() + index;

		ImGui::Text("Vertex %d of Sector %d", index, sector->id);
		char s_vtxPosX_str[64];
		char s_vtxPosZ_str[64];
		sprintf(s_vtxPosX_str, "%0.2f", vtx->x);
		sprintf(s_vtxPosZ_str, "%0.2f", vtx->z);

		ImGui::NewLine();
		ImGui::PushItemWidth(64.0f);
		ImGui::LabelText("##PositionLabel", "Position");
		ImGui::PopItemWidth();

		ImGui::SameLine();
		ImGui::PushItemWidth(196.0f);
		ImGui::InputFloat2("##VertexPosition", &vtx->x, "%0.2f", ImGuiInputTextFlags_CharsDecimal);
		ImGui::PopItemWidth();
	}
	
	void infoLabel(const char* labelId, const char* labelText, u32 width)
	{
		ImGui::PushItemWidth(f32(width));
		ImGui::LabelText(labelId, labelText);
		ImGui::PopItemWidth();
		ImGui::SameLine();
	}

	void infoIntInput(const char* labelId, u32 width, s32* value)
	{
		ImGui::PushItemWidth(f32(width));
		ImGui::InputInt(labelId, value);
		ImGui::PopItemWidth();
	}

	void infoUIntInput(const char* labelId, u32 width, u32* value)
	{
		ImGui::PushItemWidth(f32(width));
		ImGui::InputUInt(labelId, value);
		ImGui::PopItemWidth();
	}

	void infoFloatInput(const char* labelId, u32 width, f32* value)
	{
		ImGui::PushItemWidth(f32(width));
		ImGui::InputFloat(labelId, value, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
	}

	void infoPanelWall()
	{
		EditorSector* sector;
		EditorWall* wall;
		
		s32 wallId;
		if (s_selectedWall >= 0) { sector = s_levelData->sectors.data() + s_selectedWallSector; wallId = s_selectedWall; }
		else if (s_hoveredWall >= 0) { sector = s_levelData->sectors.data() + s_hoveredWallSector; wallId = s_hoveredWall; }
		else { return; }
		wall = sector->walls.data() + wallId;

		if (s_enableInfEditor)
		{
			infoPanelInfWall(sector, wallId);
			return;
		}

		Vec2f* vertices = sector->vertices.data();
		f32 len = TFE_Math::distance(&vertices[wall->i0], &vertices[wall->i1]);

		ImGui::Text("Wall ID: %d  Sector ID: %d  Length: %2.2f", wallId, sector->id, len);
		ImGui::Text("Vertices: [%d](%2.2f, %2.2f), [%d](%2.2f, %2.2f)", wall->i0, vertices[wall->i0].x, vertices[wall->i0].z, wall->i1, vertices[wall->i1].x, vertices[wall->i1].z);
		ImGui::Separator();

		// Adjoin data (should be carefully and rarely edited directly).
		ImGui::LabelText("##SectorAdjoin", "Adjoin"); ImGui::SameLine(64.0f);
		infoIntInput("##SectorAdjoinInput", 96, &wall->adjoin); ImGui::SameLine(180.0f);
		ImGui::LabelText("##SectorWalk", "Walk"); ImGui::SameLine(224.0f);
		infoIntInput("##SectorWalkInput", 96, &wall->walk); ImGui::SameLine(335.0f);
		ImGui::LabelText("##SectorMirror", "Mirror"); ImGui::SameLine(384);
		infoIntInput("##SectorMirrorInput", 96, &wall->mirror);

		s32 light = wall->light;
		ImGui::LabelText("##SectorLight", "Light Adjustment"); ImGui::SameLine(138.0f);
		infoIntInput("##SectorLightInput", 96, &light);
		wall->light = (s16)light;

		ImGui::SameLine();
		if (ImGui::Button("Edit INF Script##Wall"))
		{
			if (isValidName(sector->name))
			{
				// Enable the editor.
				s_enableInfEditor = true;
			}
			else
			{
				// Popup a message box.
				popupErrorMessage("INF script functionality relies on sector names for identification. Please give the sector a unique name (in this level) before proceeding.");
			}
		}

		ImGui::Separator();

		// Flags
		infoLabel("##Flags1Label", "Flags1", 48);
		infoUIntInput("##Flags1", 128, &wall->flags[0]);
		ImGui::SameLine();

		infoLabel("##Flags3Label", "Flags3", 48);
		infoUIntInput("##Flags3", 128, &wall->flags[2]);

		const f32 column[] = { 0.0f, 160.0f, 320.0f };

		ImGui::CheckboxFlags("Mask Wall##WallFlag", &wall->flags[0], WF1_ADJ_MID_TEX); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Illum Sign##WallFlag", &wall->flags[0], WF1_ILLUM_SIGN); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Horz Flip Texture##SectorFlag", &wall->flags[0], WF1_FLIP_HORIZ);

		ImGui::CheckboxFlags("Change WallLight##WallFlag", &wall->flags[0], WF1_CHANGE_WALL_LIGHT); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Tex Anchored##WallFlag", &wall->flags[0], WF1_TEX_ANCHORED); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Wall Morphs##SectorFlag", &wall->flags[0], WF1_WALL_MORPHS);

		ImGui::CheckboxFlags("Scroll Top Tex##WallFlag", &wall->flags[0], WF1_SCROLL_TOP_TEX); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Scroll Mid Tex##WallFlag", &wall->flags[0], WF1_SCROLL_MID_TEX); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Scroll Bottom Tex##SectorFlag", &wall->flags[0], WF1_SCROLL_BOT_TEX);

		ImGui::CheckboxFlags("Scroll Sign##WallFlag", &wall->flags[0], WF1_SCROLL_SIGN_TEX); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Hide On Map##WallFlag", &wall->flags[0], WF1_HIDE_ON_MAP); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Show On Map##SectorFlag", &wall->flags[0], WF1_SHOW_NORMAL_ON_MAP);

		ImGui::CheckboxFlags("Sign Anchored##WallFlag", &wall->flags[0], WF1_SIGN_ANCHORED); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Damage Wall##WallFlag", &wall->flags[0], WF1_DAMAGE_WALL); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Show As Ledge##SectorFlag", &wall->flags[0], WF1_SHOW_AS_LEDGE_ON_MAP);

		ImGui::CheckboxFlags("Show As Door##WallFlag", &wall->flags[0], WF1_SHOW_AS_DOOR_ON_MAP); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Always Walk##WallFlag", &wall->flags[2], WF3_ALWAYS_WALK); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Solid Wall##SectorFlag", &wall->flags[2], WF3_SOLID_WALL);

		ImGui::CheckboxFlags("Player Walk Only##WallFlag", &wall->flags[2], WF3_PLAYER_WALK_ONLY); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Cannot Shoot Thru##WallFlag", &wall->flags[2], WF3_CANNOT_FIRE_THROUGH);

		ImGui::Separator();

		EditorTexture* midTex = wall->mid.tex;
		EditorTexture* topTex = wall->top.tex;
		EditorTexture* botTex = wall->bot.tex;
		EditorTexture* sgnTex = wall->sign.tex;

		const f32 texCol = 150.0f;
		// Labels
		ImGui::Text("Mid Texture"); ImGui::SameLine(texCol);
		ImGui::Text("Sign Texture");

		// Textures - Mid / Sign
		const f32 midScale = midTex ? 1.0f / (f32)std::max(midTex->width, midTex->height) : 0.0f;
		const f32 sgnScale = sgnTex ? 1.0f / (f32)std::max(sgnTex->width, sgnTex->height) : 0.0f;
		const f32 aspectMid[] = { midTex ? f32(midTex->width) * midScale : 1.0f, midTex ? f32(midTex->height) * midScale : 1.0f };
		const f32 aspectSgn[] = { sgnTex ? f32(sgnTex->width) * sgnScale : 1.0f, sgnTex ? f32(sgnTex->height) * sgnScale : 1.0f };

		ImGui::ImageButton(midTex ? TFE_RenderBackend::getGpuPtr(midTex->texture) : nullptr, { 128.0f * aspectMid[0], 128.0f * aspectMid[1] });
		ImGui::SameLine(texCol);
		ImGui::ImageButton(sgnTex ? TFE_RenderBackend::getGpuPtr(sgnTex->texture) : nullptr, { 128.0f * aspectSgn[0], 128.0f * aspectSgn[1] });
		const ImVec2 imageLeft0 = ImGui::GetItemRectMin();
		const ImVec2 imageRight0 = ImGui::GetItemRectMax();

		// Names
		ImGui::Text("%s %dx%d", midTex ? midTex->name : "NONE", midTex ? midTex->width : 0, midTex ? midTex->height : 0); ImGui::SameLine(texCol);
		ImGui::Text("%s %dx%d", sgnTex ? sgnTex->name : "NONE", sgnTex ? sgnTex->width : 0, sgnTex ? sgnTex->height : 0);

		ImVec2 imageLeft1, imageRight1;
		if (wall->adjoin >= 0)
		{
			ImGui::NewLine();

			// Textures - Top / Bottom
			// Labels
			ImGui::Text("Top Texture"); ImGui::SameLine(texCol); ImGui::Text("Bottom Texture");

			const f32 topScale = topTex ? 1.0f / (f32)std::max(topTex->width, topTex->height) : 0.0f;
			const f32 botScale = botTex ? 1.0f / (f32)std::max(botTex->width, botTex->height) : 0.0f;
			const f32 aspectTop[] = { topTex ? f32(topTex->width) * topScale : 1.0f, topTex ? f32(topTex->height) * topScale : 1.0f };
			const f32 aspectBot[] = { botTex ? f32(botTex->width) * botScale : 1.0f, botTex ? f32(botTex->height) * botScale : 1.0f };

			ImGui::ImageButton(topTex ? TFE_RenderBackend::getGpuPtr(topTex->texture) : nullptr, { 128.0f * aspectTop[0], 128.0f * aspectTop[1] });
			ImGui::SameLine(texCol);
			ImGui::ImageButton(botTex ? TFE_RenderBackend::getGpuPtr(botTex->texture) : nullptr, { 128.0f * aspectBot[0], 128.0f * aspectBot[1] });
			imageLeft1 = ImGui::GetItemRectMin();
			imageRight1 = ImGui::GetItemRectMax();

			// Names
			ImGui::Text("%s %dx%d", topTex ? topTex->name : "NONE", topTex ? topTex->width : 0, topTex ? topTex->height : 0); ImGui::SameLine(texCol);
			ImGui::Text("%s %dx%d", botTex ? botTex->name : "NONE", botTex ? botTex->width : 0, botTex ? botTex->height : 0);
		}

		// Texture Offsets
		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		// Set 0
		ImVec2 offsetLeft = { imageLeft0.x + texCol + 8.0f, imageLeft0.y + 8.0f };
		ImVec2 offsetSize = { displayInfo.width - (imageLeft0.x + texCol + 16.0f), 128.0f };
		ImGui::SetNextWindowPos(offsetLeft);
		// A child window is used here in order to place the controls in the desired position - to the right of the image buttons.
		ImGui::BeginChild("##TextureOffsets0Wall", offsetSize);
		{
			ImGui::Text("Mid Offset");
			ImGui::PushItemWidth(128.0f);
			ImGui::InputFloat2("##MidOffsetInput", &wall->mid.offsetX, "%.2f");
			ImGui::PopItemWidth();

			ImGui::NewLine();

			ImGui::Text("Sign Offset");
			ImGui::PushItemWidth(128.0f);
			ImGui::InputFloat2("##SignOffsetInput", &wall->sign.offsetX, "%.2f");
			ImGui::PopItemWidth();
		}
		ImGui::EndChild();

		// Set 1
		if (wall->adjoin >= 0)
		{
			offsetLeft = { imageLeft1.x + texCol + 8.0f, imageLeft1.y + 8.0f };
			offsetSize = { displayInfo.width - (imageLeft1.x + texCol + 16.0f), 128.0f };
			ImGui::SetNextWindowPos(offsetLeft);
			// A child window is used here in order to place the controls in the desired position - to the right of the image buttons.
			ImGui::BeginChild("##TextureOffsets1Wall", offsetSize);
			{
				ImGui::Text("Top Offset");
				ImGui::PushItemWidth(128.0f);
				ImGui::InputFloat2("##TopOffsetInput", &wall->top.offsetX, "%.2f");
				ImGui::PopItemWidth();

				ImGui::NewLine();

				ImGui::Text("Bottom Offset");
				ImGui::PushItemWidth(128.0f);
				ImGui::InputFloat2("##BotOffsetInput", &wall->bot.offsetX, "%.2f");
				ImGui::PopItemWidth();
			}
			ImGui::EndChild();
		}
	}

	bool isValidName(const char* name)
	{
		if (strlen(name) > 0 && name[0] > 32 && name[0] < 127) { return true; }
		return false;
	}

	void addFuncText(EditorInfClassData* cdata, u32 stopIndex, u32 funcIndex, char** itemList, s32& itemCount, bool inclTarget)
	{
		const EditorInfFunction* func = &cdata->stop[stopIndex].func[funcIndex];
		const u32 funcId = func->funcId;
		const u32 clientCount = (u32)func->client.size();
		const u32 argCount = (u32)func->arg.size();

		const char* funcName = TFE_InfAsset::getFuncName(funcId);
		if (funcId < INF_MSG_LIGHTS)
		{
			char client[256];
			EditorSector* sector = LevelEditorData::getSector(func->client[0].sectorName.c_str());
			if (!sector) { return; }

			u32 sectorId = sector->id;
			s32 wallId = func->client[0].wallId;
			if (inclTarget)
			{
				if (wallId < 0) { sprintf(client, "%s", sector->name); }
				else { sprintf(client, "%s(%d)", sector->name, wallId); }
			}

			char funcText[256];
			if (argCount)
			{
				sprintf(funcText, "%s(", funcName);
				for (u32 a = 0; a < argCount; a++)
				{
					char argText[64];
					if (a < argCount - 1)
					{
						sprintf(argText, "%d, ", func->arg[a].iValue);
					}
					else
					{
						sprintf(argText, "%d", func->arg[a].iValue);
					}
					strcat(funcText, argText);
				}
				strcat(funcText, ")");
			}
			else
			{
				sprintf(funcText, "%s", funcName);
			}

			if (inclTarget) { sprintf(itemList[itemCount], "      Func: %s  Target: %s", funcText, client); }
			else { sprintf(itemList[itemCount], "      Func: %s", funcText); }

			itemCount++;
		}
		else
		{
			if (funcId == INF_MSG_LIGHTS)
			{
				sprintf(itemList[itemCount], "      Func: lights");
				itemCount++;
			}
			else if (funcId == INF_MSG_ADJOIN)
			{
				sprintf(itemList[itemCount], "      Func: adjoin(%s, %d, %s, %d)",
					func->arg[0].sValue.c_str(), func->arg[1].iValue,
					func->arg[2].sValue.c_str(), func->arg[3].iValue);
				itemCount++;
			}
			else if (funcId == INF_MSG_PAGE)
			{
				// This is supposed to be a string - Sounds are TODO.
				sprintf(itemList[itemCount], "      Func: page(TODO)");
				itemCount++;
			}
			else if (funcId == INF_MSG_TEXT)
			{
				sprintf(itemList[itemCount], "      Func: text(%d)", func->arg[0].iValue);
				itemCount++;
			}
			else if (funcId == INF_MSG_TEXTURE)
			{
				sprintf(itemList[itemCount], "      Func: texture(%d, %s)", func->arg[0].iValue, func->arg[1].sValue.c_str());
				itemCount++;
			}
		}
	}

	char* getEventMaskList(u32 mask, char* buffer, bool wall)
	{
		buffer[0] = 0;
		if (!mask) { return buffer; }

		if (mask & (INF_EVENT_ANY))
		{
			strcat(buffer, "(");
			if (mask & INF_EVENT_CROSS_LINE_FRONT) { strcat(buffer, "cross front, "); }
			if (mask & INF_EVENT_CROSS_LINE_BACK)  { strcat(buffer, "cross back, "); }
			if (mask & INF_EVENT_ENTER_SECTOR)     { strcat(buffer, "enter, "); }
			if (mask & INF_EVENT_LEAVE_SECTOR)     { strcat(buffer, "leave, "); }
			if (mask & INF_EVENT_NUDGE_FRONT)      { strcat(buffer, wall ? "nudge front, " : "nudge inside, "); }
			if (mask & INF_EVENT_NUDGE_BACK)       { strcat(buffer, wall ? "nudge back, " : "nudge outside, "); }
			if (mask & INF_EVENT_EXPLOSION)        { strcat(buffer, "explosion, "); }
			if (mask & INF_EVENT_SHOOT_LINE)       { strcat(buffer, "shoot, "); }
			if (mask & INF_EVENT_LAND)             { strcat(buffer, "land, "); }
			buffer[strlen(buffer) - 2] = 0;
			strcat(buffer, ")");
		}

		return buffer;
	}

	char* getEntityMaskList(u32 mask, char* buffer)
	{
		buffer[0] = 0;
		if (!mask) { return buffer; }

		strcat(buffer, "(");
		if (mask & INF_ENTITY_ENEMY)  { strcat(buffer, "enemy, "); }
		if (mask & INF_ENTITY_WEAPON) { strcat(buffer, "weapon, "); }
		if (mask & INF_ENTITY_PLAYER) { strcat(buffer, "player, "); }
		size_t len = strlen(buffer);
		if (len > 2) { buffer[len - 2] = 0; }
		strcat(buffer, ")");

		return buffer;
	}

	char* getKeyList(u32 keys, char* buffer)
	{
		buffer[0] = 0;
		if (!keys) { return buffer; }

		strcat(buffer, "(");
		if (keys & KEY_RED) { strcat(buffer, "red, "); }
		if (keys & KEY_YELLOW) { strcat(buffer, "yellow, "); }
		if (keys & KEY_BLUE) { strcat(buffer, "blue, "); }
		buffer[strlen(buffer) - 2] = 0;
		strcat(buffer, ")");

		return buffer;
	}

	bool isElevatorFloorCeil(u32 sclass)
	{
		return sclass == ELEVATOR_BASIC || sclass == ELEVATOR_INV || sclass == ELEVATOR_MOVE_FLOOR || sclass == ELEVATOR_MOVE_CEILING || sclass == ELEVATOR_MOVE_FC || sclass == ELEVATOR_MOVE_OFFSET || sclass == ELEVATOR_BASIC_AUTO;
	}

	bool isDoor(u32 sclass)
	{
		return sclass == ELEVATOR_DOOR || sclass == ELEVATOR_DOOR_MID || sclass == ELEVATOR_DOOR_INV;
	}
		
	void infEditor(EditorInfItem* item, bool wall)
	{
		InfEditState* infEditState = LevelEditorData::getInfEditState();

		// There will be context sensitive controls here, such as adding classes.

		// Get the current list of items.
		s32 itemCount = 0;
		strcpy(infEditState->itemList[itemCount], "Seq"); itemCount++;
		// Go through the data here...
		if (item)
		{
			const u32 classCount = (u32)item->classData.size();
			for (u32 c = 0; c < classCount; c++)
			{
				EditorInfClassData* cdata = &item->classData[c];
				const u32 iclass = cdata->iclass;
				const u32 sclass = cdata->isubclass;

				sprintf(infEditState->itemList[itemCount], "  Class: %s %s",
					TFE_InfAsset::getClassName(iclass), TFE_InfAsset::getSubclassName(iclass, sclass)); itemCount++;

				if (iclass == INF_CLASS_ELEVATOR)
				{
					if (!isDoor(sclass))
					{
						const f32 sgn = isElevatorFloorCeil(sclass) ? -1.0f : 1.0f;
						for (u32 s = 0; s < (u32)cdata->stop.size(); s++)
						{
							const u32 stop0Type = cdata->stop[s].stopValue0Type;
							const u32 stop1Type = cdata->stop[s].stopValue1Type;
							const u32 funcCount = (u32)cdata->stop[s].func.size();

							char value0[256];
							char value1[256];
							if (stop0Type == INF_STOP0_SECTORNAME)
							{
								sprintf(value0, "\"%s\"", cdata->stop[s].value0.sValue.c_str());
							}
							else if (stop0Type == INF_STOP0_ABSOLUTE)
							{
								// Show the values in the same coordinate space as the level.
								sprintf(value0, "%2.2f", cdata->stop[s].value0.fValue * sgn);
							}
							else if (stop0Type == INF_STOP0_RELATIVE)
							{
								sprintf(value0, "@%2.2f", cdata->stop[s].value0.fValue * sgn);
							}

							if (stop1Type == INF_STOP1_TIME)
							{
								sprintf(value1, "%2.2f", cdata->stop[s].time);
							}
							else if (stop1Type == INF_STOP1_HOLD)
							{
								sprintf(value1, "hold");
							}
							else if (stop1Type == INF_STOP1_TERMINATE)
							{
								sprintf(value1, "terminate");
							}
							else if (stop1Type == INF_STOP1_COMPLETE)
							{
								sprintf(value1, "complete");
							}

							sprintf(infEditState->itemList[itemCount], "    Stop %d: %s %s", s, value0, value1); itemCount++;

							for (u32 f = 0; f < funcCount; f++)
							{
								addFuncText(cdata, s, f, infEditState->itemList.data(), itemCount, true);
							}
						}
					}
				}
				else if (iclass == INF_CLASS_TRIGGER)
				{
					// Functions
					const u32 funcCount = (u32)cdata->stop[0].func.size();
					for (u32 f = 0; f < funcCount; f++)
					{
						addFuncText(cdata, 0, f, infEditState->itemList.data(), itemCount, false);
					}
					// Clients.
					const EditorInfFunction* func = &cdata->stop[0].func[0];
					const u32 clientCount = (u32)func->client.size();
					if (clientCount)
					{
						sprintf(infEditState->itemList[itemCount], "    Clients %u", clientCount); itemCount++;
						for (u32 c = 0; c < clientCount; c++)
						{
							const s32 wallId = func->client[c].wallId;
							if (wallId < 0)
							{
								sprintf(infEditState->itemList[itemCount], "      %s", func->client[c].sectorName.c_str()); itemCount++;
							}
							else
							{
								sprintf(infEditState->itemList[itemCount], "      %s(%d)", func->client[c].sectorName.c_str(), wallId); itemCount++;
							}
						}
					}
				}
				else
				{
					sprintf(infEditState->itemList[itemCount], "    Target: %s", cdata->var.target.c_str()); itemCount++;
				}

				// Slaves
				if (iclass == INF_CLASS_ELEVATOR)
				{
					const u32 slaveCount = (u32)cdata->slaves.size();
					sprintf(infEditState->itemList[itemCount], "    Slaves %u", slaveCount); itemCount++;
					for (u32 s = 0; s < slaveCount; s++)
					{
						sprintf(infEditState->itemList[itemCount], "      %s", cdata->slaves[s].c_str()); itemCount++;
					}
				}

				// Variables.
				u32 defaultEventMask = 0;
				if (iclass == INF_CLASS_ELEVATOR && (sclass == ELEVATOR_DOOR || sclass == ELEVATOR_DOOR_INV || sclass == ELEVATOR_DOOR_MID))
				{
					defaultEventMask = INF_EVENT_NUDGE_BACK;
				}
				else if (iclass == INF_CLASS_ELEVATOR && (sclass == ELEVATOR_BASIC || sclass == ELEVATOR_INV || sclass == ELEVATOR_BASIC_AUTO))
				{
					defaultEventMask = 52;
				}
				else if (iclass == INF_CLASS_ELEVATOR && (sclass == ELEVATOR_MORPH_MOVE1 || sclass == ELEVATOR_MORPH_MOVE2 || sclass == ELEVATOR_MORPH_SPIN1 || sclass == ELEVATOR_MORPH_SPIN2))
				{
					defaultEventMask = 60;
				}
				else if (iclass == INF_CLASS_TRIGGER)
				{
					defaultEventMask = INF_EVENT_CROSS_LINE_FRONT | INF_EVENT_CROSS_LINE_BACK | INF_EVENT_ENTER_SECTOR | INF_EVENT_LEAVE_SECTOR | INF_EVENT_NUDGE_FRONT | INF_EVENT_NUDGE_BACK | INF_EVENT_LAND;
				}

				char buffer[128];
				sprintf(infEditState->itemList[itemCount], "    Variables"); itemCount++;
				if (!cdata->var.master)
				{
					sprintf(infEditState->itemList[itemCount], "      master: off");
					itemCount++;
				}
				if (cdata->var.event_mask != defaultEventMask)
				{
					sprintf(infEditState->itemList[itemCount], "      event_mask: %u %s", cdata->var.event_mask, getEventMaskList(cdata->var.event_mask, buffer, wall));
					itemCount++;
				}
				if (cdata->var.event)
				{
					sprintf(infEditState->itemList[itemCount], "      event: %u", cdata->var.event);
					itemCount++;
				}
				if (cdata->var.entity_mask != INF_ENTITY_PLAYER)
				{
					sprintf(infEditState->itemList[itemCount], "      entity_mask: %u %s", cdata->var.entity_mask, getEntityMaskList(cdata->var.entity_mask, buffer));
					itemCount++;
				}
				if (cdata->var.speed != 30.0f && iclass == INF_CLASS_ELEVATOR)
				{
					sprintf(infEditState->itemList[itemCount], "      speed: %2.2f", cdata->var.speed);
					itemCount++;
				}
				if (cdata->var.start != 0 && iclass == INF_CLASS_ELEVATOR)
				{
					sprintf(infEditState->itemList[itemCount], "      start: %d", cdata->var.start);
					itemCount++;
				}
				if (sclass == ELEVATOR_MORPH_SPIN1 || sclass == ELEVATOR_MORPH_SPIN2 || sclass == ELEVATOR_ROTATE_WALL)
				{
					sprintf(infEditState->itemList[itemCount], "      center: %2.2f, %2.2f", cdata->var.center.x, cdata->var.center.z);
					itemCount++;
				}
				if (sclass == ELEVATOR_MORPH_MOVE1 || sclass == ELEVATOR_MORPH_MOVE2 || sclass == ELEVATOR_MOVE_WALL || sclass == ELEVATOR_SCROLL_WALL)
				{
					sprintf(infEditState->itemList[itemCount], "      angle: %2.2f", cdata->var.angle);
					itemCount++;
				}
				if (cdata->var.key != 0)
				{
					sprintf(infEditState->itemList[itemCount], "      key: %u %s", cdata->var.key, getKeyList(cdata->var.key, buffer));
					itemCount++;
				}
				u32 flagsDefault = 0;
				if (iclass == INF_CLASS_ELEVATOR && (sclass == ELEVATOR_SCROLL_FLOOR || sclass == ELEVATOR_MORPH_MOVE2 || sclass == ELEVATOR_MORPH_SPIN2))
				{
					flagsDefault = INF_MOVE_FLOOR | INF_MOVE_SECALT;
				}
				if (cdata->var.flags != flagsDefault)
				{
					sprintf(infEditState->itemList[itemCount], "      flags: %u", cdata->var.flags);
					itemCount++;
				}
				if (cdata->var.target != "")
				{
					sprintf(infEditState->itemList[itemCount], "      target: %s", cdata->var.target.c_str());
					itemCount++;
				}
				// Skip sound for now.
				// Skip "object_mask" for now.
			}
		}
		strcpy(infEditState->itemList[itemCount], "Seqend"); itemCount++;

		ImGui::SetNextItemWidth(440.0f);
		ImGui::ListBox("##InfDescrSector", &infEditState->editIndex, infEditState->itemList.data(), itemCount, 20);
	}

	void infoPanelInfSector(EditorSector* sector)
	{
		if (!isValidName(sector->name))
		{
			s_enableInfEditor = false;
			return;
		}
		InfEditState* infEditState = LevelEditorData::getInfEditState();
		if (infEditState->sector != sector || infEditState->wallId >= 0)
		{
			infEditState->sector = sector;
			infEditState->wallId = -1;
			infEditState->item = sector->infItem;
			infEditState->editIndex = 0;

			infEditState->itemMem.resize(1024 * 64);
			infEditState->itemList.resize(1024);
			char* itemMem = infEditState->itemMem.data();
			for (u32 i = 0; i < 1024; i++, itemMem += 64)
			{
				infEditState->itemList[i] = itemMem;
			}
		}

		// Sector Name (optional, used by INF)
		infoLabel("##NameLabel", "Name", 32);
		ImGui::PushItemWidth(240.0f);
		ImGui::InputText("##NameSector", sector->name, 32);
		ImGui::PopItemWidth();
		ImGui::SameLine();

		if (ImGui::Button("Return to Properties##Sector"))
		{
			s_enableInfEditor = false;
		}
		ImGui::Separator();

		infEditor(infEditState->item, false);
	}

	void infoPanelInfWall(EditorSector* sector, u32 wallId)
	{
		if (!isValidName(sector->name))
		{
			s_enableInfEditor = false;
			return;
		}
		InfEditState* infEditState = LevelEditorData::getInfEditState();
		if (infEditState->sector != sector || infEditState->wallId != wallId)
		{
			infEditState->sector = sector;
			infEditState->wallId = wallId;
			infEditState->item = sector->walls[wallId].infItem;
			infEditState->editIndex = 0;

			infEditState->itemMem.resize(1024 * 64);
			infEditState->itemList.resize(1024);
			char* itemMem = infEditState->itemMem.data();
			for (u32 i = 0; i < 1024; i++, itemMem += 64)
			{
				infEditState->itemList[i] = itemMem;
			}
		}

		ImGui::Text("Wall ID: %d  Sector: %s", wallId, sector->name);
		if (ImGui::Button("Return to Properties##Wall"))
		{
			s_enableInfEditor = false;
		}
		ImGui::Separator();

		infEditor(infEditState->item, true);
	}
		
	void infoPanelSector()
	{
		EditorSector* sector;
		if (s_selectedSector >= 0) { sector = s_levelData->sectors.data() + s_selectedSector; }
		else if (s_hoveredSector >= 0) { sector = s_levelData->sectors.data() + s_hoveredSector; }
		else { return; }

		if (s_enableInfEditor)
		{
			infoPanelInfSector(sector);
			return;
		}

		ImGui::Text("Sector ID: %d      Wall Count: %u", sector->id, (u32)sector->walls.size());
		ImGui::Separator();

		// Sector Name (optional, used by INF)
		infoLabel("##NameLabel", "Name", 32);
		ImGui::PushItemWidth(240.0f);
		ImGui::InputText("##NameSector", sector->name, 32);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("Edit INF Script##Sector"))
		{
			if (isValidName(sector->name))
			{
				// Enable the editor.
				s_enableInfEditor = true;
			}
			else
			{
				// Popup a message box.
				popupErrorMessage("INF script functionality relies on sector names for identification. Please give the sector a unique name (in this level) before proceeding.");
			}
		}

		// Layer and Ambient
		s32 layer = sector->layer;
		infoLabel("##LayerLabel", "Layer", 32);
		infoIntInput("##LayerSector", 96, &layer);
		if (layer != sector->layer)
		{
			sector->layer = layer;
			// Adjust layer range.
			s_levelData->layerMin = std::min(s_levelData->layerMin, (s8)layer);
			s_levelData->layerMax = std::max(s_levelData->layerMax, (s8)layer);
			// Change the current layer so we can still see the sector.
			s_layerIndex = layer - s_layerMin;
		}
		
		ImGui::SameLine(0.0f, 16.0f);

		s32 ambient = (s32)sector->ambient;
		infoLabel("##AmbientLabel", "Ambient", 48);
		infoIntInput("##AmbientSector", 96, &ambient);
		sector->ambient = std::max(0, std::min(31, ambient));

		// Heights
		infoLabel("##HeightLabel", "Heights", 64);

		infoLabel("##FloorHeightLabel", "Floor", 32);
		infoFloatInput("##FloorHeight", 64, &sector->floorAlt);
		ImGui::SameLine();

		infoLabel("##SecondHeightLabel", "Second", 44);
		infoFloatInput("##SecondHeight", 64, &sector->secAlt);
		ImGui::SameLine();

		infoLabel("##CeilHeightLabel", "Ceiling", 48);
		infoFloatInput("##CeilHeight", 64, &sector->ceilAlt);

		ImGui::Separator();

		// Flags
		infoLabel("##Flags1Label", "Flags1", 48);
		infoUIntInput("##Flags1", 128, &sector->flags[0]);
		ImGui::SameLine();

		infoLabel("##Flags2Label", "Flags2", 48);
		infoUIntInput("##Flags2", 128, &sector->flags[1]);

		infoLabel("##Flags3Label", "Flags3", 48);
		infoUIntInput("##Flags3", 128, &sector->flags[2]);

		const f32 column[] = { 0.0f, 160.0f, 320.0f };

		ImGui::CheckboxFlags("Exterior##SectorFlag", &sector->flags[0], SEC_FLAGS1_EXTERIOR); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Pit##SectorFlag", &sector->flags[0], SEC_FLAGS1_PIT); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("No Walls##SectorFlag", &sector->flags[0], SEC_FLAGS1_NOWALL_DRAW);

		ImGui::CheckboxFlags("Exterior Ceil Adj##SectorFlag", &sector->flags[0], SEC_FLAGS1_EXT_ADJ); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Exterior Floor Adj##SectorFlag", &sector->flags[0], SEC_FLAGS1_EXT_FLOOR_ADJ);  ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Secret##SectorFlag", &sector->flags[0], SEC_FLAGS1_SECRET);

		ImGui::CheckboxFlags("Door##SectorFlag", &sector->flags[0], SEC_FLAGS1_DOOR); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Ice Floor##SectorFlag", &sector->flags[0], SEC_FLAGS1_ICE_FLOOR); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Snow Floor##SectorFlag", &sector->flags[0], SEC_FLAGS1_SNOW_FLOOR);
		
		ImGui::CheckboxFlags("Crushing##SectorFlag", &sector->flags[0], SEC_FLAGS1_CRUSHING); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Low Damage##SectorFlag", &sector->flags[0], SEC_FLAGS1_LOW_DAMAGE); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("High Damage##SectorFlag", &sector->flags[0], SEC_FLAGS1_HIGH_DAMAGE);

		ImGui::CheckboxFlags("No Smart Obj##SectorFlag", &sector->flags[0], SEC_FLAGS1_NO_SMART_OBJ); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Smart Obj##SectorFlag", &sector->flags[0], SEC_FLAGS1_SMART_OBJ); ImGui::SameLine(column[2]);
		ImGui::CheckboxFlags("Safe Sector##SectorFlag", &sector->flags[0], SEC_FLAGS1_SAFESECTOR);

		ImGui::CheckboxFlags("Mag Seal##SectorFlag", &sector->flags[0], SEC_FLAGS1_MAG_SEAL); ImGui::SameLine(column[1]);
		ImGui::CheckboxFlags("Exploding Wall##SectorFlag", &sector->flags[0], SEC_FLAGS1_EXP_WALL);

		ImGui::Separator();

		// Textures
		EditorTexture* floorTex = sector->floorTexture.tex;
		EditorTexture* ceilTex = sector->ceilTexture.tex;

		void* floorPtr = floorTex ? TFE_RenderBackend::getGpuPtr(floorTex->texture) : nullptr;
		void* ceilPtr  = ceilTex ? TFE_RenderBackend::getGpuPtr(ceilTex->texture) : nullptr;

		const f32 texCol = 150.0f;
		// Labels
		ImGui::Text("Floor Texture"); ImGui::SameLine(texCol);
		ImGui::Text("Ceiling Texture");
		
		// Textures
		const f32 floorScale = floorTex ? 1.0f / (f32)std::max(floorTex->width, floorTex->height) : 1.0f;
		const f32 ceilScale  = ceilTex ? 1.0f / (f32)std::max(ceilTex->width, ceilTex->height) : 1.0f;
		const f32 aspectFloor[] = { floorTex ? f32(floorTex->width) * floorScale : 1.0f, floorTex ? f32(floorTex->height) * floorScale : 1.0f };
		const f32 aspectCeil[] = { ceilTex ? f32(ceilTex->width) * ceilScale : 1.0f, ceilTex ? f32(ceilTex->height) * ceilScale : 1.0f };

		ImGui::ImageButton(floorPtr, { 128.0f * aspectFloor[0], 128.0f * aspectFloor[1] });
		ImGui::SameLine(texCol);
		ImGui::ImageButton(ceilPtr, { 128.0f * aspectCeil[0], 128.0f * aspectCeil[1] });
		const ImVec2 imageLeft  = ImGui::GetItemRectMin();
		const ImVec2 imageRight = ImGui::GetItemRectMax();

		// Names
		if (floorTex)
		{
			ImGui::Text("%s %dx%d", floorTex->name, floorTex->width, floorTex->height); ImGui::SameLine(texCol);
		}
		if (ceilTex)
		{
			ImGui::Text("%s %dx%d", ceilTex->name, ceilTex->width, ceilTex->height); ImGui::SameLine();
		}

		// Texture Offsets
		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		ImVec2 offsetLeft = { imageLeft.x + texCol + 8.0f, imageLeft.y + 8.0f };
		ImVec2 offsetSize = { displayInfo.width - (imageLeft.x + texCol + 16.0f), 128.0f };
		ImGui::SetNextWindowPos(offsetLeft);
		// A child window is used here in order to place the controls in the desired position - to the right of the image buttons.
		ImGui::BeginChild("##TextureOffsetsSector", offsetSize);
		{
			ImGui::Text("Floor Offset");
			ImGui::PushItemWidth(128.0f);
			ImGui::InputFloat2("##FloorOffsetInput", &sector->floorTexture.offsetX, "%.2f");
			ImGui::PopItemWidth();

			ImGui::NewLine();

			ImGui::Text("Ceil Offset");
			ImGui::PushItemWidth(128.0f);
			ImGui::InputFloat2("##CeilOffsetInput", &sector->ceilTexture.offsetX, "%.2f");
			ImGui::PopItemWidth();
		}
		ImGui::EndChild();
	}

	void infoPanelEntity()
	{
		s32 entityId = s_selectedEntity >= 0 ? s_selectedEntity : s_hoveredEntity;
		if (entityId < 0 || !s_levelData) { return; }

		EditorSector* sector = s_levelData->sectors.data() + (s_selectedEntity >= 0 ? s_selectedEntitySector : s_hoveredEntitySector);
		EditorLevelObject* obj = sector->objects.data() + entityId;

		// difficulty: -3, -2, -1, 0, 1, 2, 3 <+ 3> = 0, 1, 2, 3, 4, 5, 6
		const s32 diffToFlagMap[] = { DIFF_EASY | DIFF_MEDIUM | DIFF_HARD, DIFF_EASY | DIFF_MEDIUM, DIFF_EASY, DIFF_EASY | DIFF_MEDIUM | DIFF_HARD, DIFF_EASY | DIFF_MEDIUM | DIFF_HARD, DIFF_MEDIUM | DIFF_HARD, DIFF_HARD };
		u32 diffFlags = diffToFlagMap[obj->difficulty + 3];

		// Entity name = resource.
		ImGui::LabelText("##Entity", "Entity: %s", obj->dataFile.c_str());
		// Class.
		ImGui::LabelText("##EntityClass", "Class: %s", c_objectClassName[obj->oclass]);
		// Difficulty.
		ImGui::LabelText("##EntityDiff", "Difficulty:  Easy");  ImGui::SameLine(136.0f);
		ImGui::CheckboxFlags("##EntityDiffEasy", &diffFlags, DIFF_EASY); ImGui::SameLine(168.0f);
		ImGui::LabelText("##EntityDiffM", "Medium");  ImGui::SameLine(220.0f);
		ImGui::CheckboxFlags("##EntityDiffMed", &diffFlags, DIFF_MEDIUM); ImGui::SameLine(252.0f);
		ImGui::LabelText("##EntityDiffH", "Hard");  ImGui::SameLine(290.0f);
		ImGui::CheckboxFlags("##EntityDiffHard", &diffFlags, DIFF_HARD);
		// Position.
		ImGui::LabelText("##EntityClass", "Position:"); ImGui::SameLine(96.0f);
		ImGui::InputFloat3("##EntityPos", obj->pos.m, "%0.2f");
		// Orientation.
		ImGui::PushItemWidth(64.0f);
		ImGui::LabelText("##EntityAngleLabel", "Angle"); ImGui::SameLine(64.0f);
		ImGui::InputFloat("##EntityAngle", &obj->orientation.y, 0.0f, 0.0f, "%0.2f"); ImGui::SameLine(150.0f);
		ImGui::LabelText("##EntityPitchLabel", "Pitch"); ImGui::SameLine(204.0f);
		ImGui::InputFloat("##EntityPitch", &obj->orientation.x, 0.0f, 0.0f, "%0.2f"); ImGui::SameLine(290.0f);
		ImGui::LabelText("##EntityRollLabel", "Roll"); ImGui::SameLine(344.0f);
		ImGui::InputFloat("##EntityRoll", &obj->orientation.z, 0.0f, 0.0f, "%0.2f");
		ImGui::PopItemWidth();
		ImGui::Separator();
		ImGui::Text("Logics");
		// TODO: Visual controls for orientation (on the map).
		// TODO: Logic and Variable editing.
		const u32 logicCount = (u32)obj->logics.size();
		Logic* logic = obj->logics.data();
		for (u32 i = 0; i < logicCount; i++, logic++)
		{
			ImGui::Text("  %s", c_logicName[logic->type]);
			if (logic->type == LOGIC_UPDATE)
			{
				char updateFlags[256]="";
				if (logic->flags & 8)
				{
					strcat(updateFlags, "X-Axis");
					if (logic->flags > 8) { strcat(updateFlags, "|"); }
				}
				if (logic->flags & 16)
				{
					strcat(updateFlags, "Y-Axis");
					if (logic->flags > 16) { strcat(updateFlags, "|"); }
				}
				if (logic->flags & 32)
				{
					strcat(updateFlags, "Z-Axis");
				}

				ImGui::LabelText("##UpdateFlags", "    FLAGS: %u (%s)", logic->flags, updateFlags);
				if (logic->flags & 8)
				{
					ImGui::LabelText("##UpdatePitch", "    D_PITCH:");
					ImGui::SameLine(96.0f);
					ImGui::SetNextItemWidth(96.0f);
					ImGui::InputFloat("##UpdatePitchInput", &logic->rotation.x, 0.0f, 0.0f, "%.2f");
				}
				if (logic->flags & 16)
				{
					ImGui::LabelText("##UpdateYaw", "    D_YAW:");
					ImGui::SameLine(96.0f);
					ImGui::SetNextItemWidth(96.0f);
					ImGui::InputFloat("##UpdateYawInput", &logic->rotation.y, 0.0f, 0.0f, "%.2f");
				}
				if (logic->flags & 32)
				{
					ImGui::LabelText("##UpdateRoll", "    D_ROLL:");
					ImGui::SameLine(96.0f);
					ImGui::SetNextItemWidth(96.0f);
					ImGui::InputFloat("##UpdateRollInput", &logic->rotation.z, 0.0f, 0.0f, "%.2f");
				}
			}
			else if (logic->type == LOGIC_KEY)
			{
				ImGui::Text("    VUE: %s %s", logic->vue->name, TFE_VueAsset::getTransformName(logic->vue, logic->vueId));
				ImGui::Text("    VUE_APPEND: %s %s", logic->vueAppend->name, TFE_VueAsset::getTransformName(logic->vueAppend, logic->vueAppendId));
				ImGui::Text("    PAUSE: %s", obj->comFlags & LCF_PAUSE ? "true" : "false");
				ImGui::Text("    FRAME_RATE: %2.2f", logic->frameRate);
			}
		}
		const u32 genCount = (u32)obj->generators.size();
		EnemyGenerator* gen = obj->generators.data();
		for (u32 i = 0; i < genCount; i++, gen++)
		{
			ImGui::Text("  Generator %s", c_logicName[gen->type]);
		}
		ImGui::Separator();
		ImGui::Text("Common Variables");

		ImGui::LabelText("##EntityEYELabel", "  EYE"); ImGui::SameLine(48.0f);
		ImGui::CheckboxFlags("##EntityEYE", &obj->comFlags, LCF_EYE);  ImGui::SameLine(108.0f);

		ImGui::LabelText("##EntityBOSSLabel", "  BOSS"); ImGui::SameLine(156.0f);
		ImGui::CheckboxFlags("##EntityBOSS", &obj->comFlags, LCF_BOSS);

		if (obj->radius >= 0.0f)
		{
			ImGui::LabelText("##EntityRadius", "  RADIUS:"); ImGui::SameLine(96.0f);
			ImGui::SetNextItemWidth(96.0f);
			ImGui::InputFloat("##EntityRadiusInput", &obj->radius, 0.0f, 0.0f, "%.2f");
		}

		ImGui::LabelText("##EntityHeight", "  HEIGHT:"); ImGui::SameLine(96.0f);
		ImGui::SetNextItemWidth(96.0f);
		ImGui::InputFloat("##EntityHeightInput", &obj->height, 0.0f, 0.0f, "%.2f");
		
		// Correct the bounding box center if needed.
		obj->worldCen = obj->pos;
		obj->worldCen.y = obj->pos.y - obj->worldExt.y;
		if (obj->oclass != CLASS_SPIRIT && obj->oclass != CLASS_SAFE && obj->oclass != CLASS_SOUND && obj->display)
		{
			obj->worldCen.y += obj->display->scale.z * fabsf(obj->display->rect[1]) * c_spriteTexelToWorldScale;
		}

		// Fixup difficulty
		if (diffFlags == (DIFF_EASY | DIFF_MEDIUM | DIFF_HARD))
		{
			obj->difficulty = 0;
		}
		else if (diffFlags == (DIFF_EASY | DIFF_MEDIUM))
		{
			obj->difficulty = -2;
		}
		else if (diffFlags == (DIFF_MEDIUM | DIFF_HARD))
		{
			obj->difficulty = 2;
		}
		else if (diffFlags == DIFF_EASY)
		{
			obj->difficulty = -1;
		}
		else if (diffFlags == DIFF_MEDIUM)
		{
			// This is not actually valid, so pick medium + easy.
			obj->difficulty = -2;
		}
		else if (diffFlags == DIFF_HARD)
		{
			obj->difficulty = 3;
		}
	}

	void infoToolEnd()
	{
		ImGui::End();
	}

	void browserBegin(s32 offset)
	{
		bool browser = true;

		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		ImGui::SetWindowPos("Browser", { (f32)displayInfo.width - 480.0f, 19.0f + f32(offset)});
		ImGui::SetWindowSize("Browser", { 480.0f, (f32)displayInfo.height - f32(offset+20) });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;

		ImGui::Begin("Browser", &browser, window_flags);
	}
		
	void browseTextures()
	{
		if (!s_levelData) { return; }

		u32 count = (u32)s_levelData->textures.size();
		u32 rows = count / 6;
		u32 leftOver = count - rows * 6;
		f32 y = 0.0f;
		for (u32 i = 0; i < rows; i++)
		{
			for (u32 t = 0; t < 6; t++)
			{
				EditorTexture* texture = &s_levelData->textures[i * 6 + t];
				void* ptr = TFE_RenderBackend::getGpuPtr(texture->texture);
				u32 w = 64, h = 64;
				if (texture->width > texture->height)
				{
					w = 64;
					h = 64 * texture->height / texture->width;
				}
				else if (texture->width < texture->height)
				{
					h = 64;
					w = 64 * texture->width / texture->height;
				}

				if (ImGui::ImageButton(ptr, ImVec2(f32(w), f32(h)), ImVec2(0.0f,0.0f), ImVec2(1.0f, 1.0f), (64 - s32(w))/2 + 2))
				{
					// TODO: select textures.
					// TODO: add hover functionality (tool tip - but show full texture + file name + size)
				}
				ImGui::SameLine();
			}
			ImGui::NewLine();
		}
	}

	void browseEntities()
	{
	}

	void browserEnd()
	{
		ImGui::End();
	}
		
	void loadLevel()
	{
		TFE_LevelList::unload();
		s_selectedSector = -1;

		// First setup the GOB - for now assume DARK.GOB
		if (strcasecmp("DARK.GOB", s_gobFile) == 0)
		{
			TFE_AssetSystem::clearCustomArchive();
		}
		else
		{
			char gobPath[TFE_MAX_PATH];
			TFE_Paths::appendPath(TFE_PathType::PATH_SOURCE_DATA, s_gobFile, gobPath);

			char extension[TFE_MAX_PATH];
			FileUtil::getFileExtension(s_gobFile, extension);

			ArchiveType type = ARCHIVE_COUNT;
			if (strcasecmp(extension, "gob") == 0) { type = ARCHIVE_GOB; }
			else if (strcasecmp(extension, "lfd") == 0) { type = ARCHIVE_LFD; }
			else { assert(0); }

			Archive* archive = Archive::getArchive(type, s_gobFile, gobPath);
			TFE_AssetSystem::setCustomArchive(archive);
		}
		
		TFE_LevelList::load();
		if (TFE_LevelAsset::load(s_levelFile))
		{
			const LevelData* levelData = TFE_LevelAsset::getLevelData();
			
			char infFile[64];
			strcpy(infFile, s_levelFile);
			const size_t len = strlen(s_levelFile);
			infFile[len - 3] = 'I';
			infFile[len - 2] = 'N';
			infFile[len - 1] = 'F';
			TFE_InfAsset::load(infFile);
			const InfData* levelInf = TFE_InfAsset::getInfData();

			// Load Objects.
			char objFile[64];
			strcpy(objFile, s_levelFile);
			objFile[len - 3] = 'O';
			objFile[len - 2] = 0;
			objFile[len - 1] = 0;
			TFE_LevelObjects::load(objFile);
			const LevelObjectData* levelObj = TFE_LevelObjects::getLevelObjectData();

			// Get the palette.
			char palFile[64];
			strcpy(palFile, s_levelFile);
			palFile[len - 3] = 'P';
			palFile[len - 2] = 'A';
			palFile[len - 1] = 'L';
			s_palette = TFE_Palette::get256(palFile);
			s_renderer->setPalette(s_palette);

			s_levelData = nullptr;
			if (LevelEditorData::convertLevelDataToEditor(levelData, s_palette, levelInf, levelObj))
			{
				s_levelData = LevelEditorData::getEditorLevelData();
			}
		}
	}

	void drawSectorPolygon(const EditorSector* sector, bool hover, u32 colorOverride)
	{
		if (!sector || sector->triangles.count == 0) { return; }
		
		const SectorTriangles* poly = &sector->triangles;
		const u32 triCount = poly->count;
		const Vec2f* vtx = poly->vtx.data();
		
		// Draw the sector polygon which has already been triangulated.
		u32 color = hover ? 0x40ff8020 : 0x40ff4020;
		if (colorOverride > 0) { color = colorOverride; }

		for (u32 p = 0; p < triCount; p++, vtx += 3)
		{
			TriColoredDraw2d::addTriangles(1, vtx, &color);
		}
	}

	void drawTexturedSectorPolygon(const EditorSector* sector, u32 color, bool floorTex)
	{
		if (!sector || sector->triangles.count == 0) { return; }

		const SectorTriangles* poly = &sector->triangles;
		const EditorTexture* texture = floorTex ? sector->floorTexture.tex : sector->ceilTexture.tex;
		const u32 triCount = poly->count;
		const Vec2f* vtx = poly->vtx.data();

		const f32* texOffsets = floorTex ? &sector->floorTexture.offsetX : &sector->ceilTexture.offsetX;
		const f32 scaleX = texture ? -8.0f / f32(texture->width)  : 1.0f;
		const f32 scaleZ = texture ? -8.0f / f32(texture->height) : 1.0f;

		// Draw the sector polygon which has already been triangulated.
		for (u32 p = 0; p < triCount; p++, vtx += 3)
		{
			Vec2f uv[3] = { { (vtx[0].x - texOffsets[0]) * scaleX, (vtx[0].z - texOffsets[1]) * scaleZ },
							{ (vtx[1].x - texOffsets[0]) * scaleX, (vtx[1].z - texOffsets[1]) * scaleZ },
							{ (vtx[2].x - texOffsets[0]) * scaleX, (vtx[2].z - texOffsets[1]) * scaleZ } };
			TriTexturedDraw2d::addTriangles(1, vtx, uv, &color, texture ? texture->texture : nullptr);
		}
	}

	void popupErrorMessage(const char* errorMessage)
	{
		strcpy(s_errorMessage, errorMessage);
		s_showError = true;
	}

	void showErrorPopup()
	{
		if (!s_showError) { return; }

		bool keepOpen = true;
		ImGui::OpenPopup("Error##Popup");

		ImGui::SetNextWindowSize({ 384, 128 });
		if (ImGui::BeginPopupModal("Error##Popup", &keepOpen))
		{
			TFE_Markdown::draw(s_errorMessage);
			ImGui::EndPopup();
		}

		if (!keepOpen) { s_showError = false; }
	}

	/////////////////////////////////////////////////////////////////////
	// 2D Viewport Editing
	/////////////////////////////////////////////////////////////////////
	void editSector2d(Vec2f worldPos, s32 sectorId)
	{
		if (s_editMode != LEDIT_SECTOR || !s_levelData)
		{
			return;
		}

		// Select sector.
		if (TFE_Input::mousePressed(MBUTTON_LEFT))
		{
			s_selectedSector = sectorId;
		}
		else if (TFE_Input::keyPressed(KEY_DELETE) && s_selectedSector >= 0)
		{
			deleteSector(s_levelData->sectors.data() + s_selectedSector);
		}
		else
		{
			s_hoveredSector = sectorId;
		}
	}

	void editWalls2d(Vec2f worldPos)
	{
		if (s_editMode != LEDIT_WALL || !s_levelData)
		{
			s_selectedWall = -1;
			s_selectedWallSector = -1;
			return;
		}

		// Select Wall.
		if (TFE_Input::mousePressed(MBUTTON_LEFT))
		{
			s_selectedWallSector = LevelEditorData::findSector(s_layerIndex + s_layerMin, &worldPos);
			s_selectedWall = LevelEditorData::findClosestWall(&s_selectedWallSector, s_layerIndex + s_layerMin, &worldPos, s_zoomVisual * 32.0f);
			if (!s_moveWall && s_selectedWall >= 0)
			{
				s_moveWall = true;
				// get the plane...
				s_drawPlaneNrm = { 0.0f, 1.0f, 0.0f };
				s_drawPlaneOrg = s_cursor3d;

				EditorWall* wall = &s_levelData->sectors[s_selectedWallSector].walls[s_selectedWall];
				s_drawBaseVtx[0] = s_levelData->sectors[s_selectedWallSector].vertices[wall->i0];
				s_drawBaseVtx[1] = s_levelData->sectors[s_selectedWallSector].vertices[wall->i1];
			}
		}
		else if (!s_moveWall)
		{
			s_hoveredWallSector = LevelEditorData::findSector(s_layerIndex + s_layerMin, &worldPos);
			s_hoveredWall = LevelEditorData::findClosestWall(&s_hoveredWallSector, s_layerIndex + s_layerMin, &worldPos, s_zoomVisual * 32.0f);
		}
	}

	void editVertices2d(Vec2f worldPos)
	{
		// Find the closest vertex (if in vertex mode)...
		// Note this should be optimized.
		s_hoveredVertex = -1;
		if (s_editMode != LEDIT_VERTEX || !s_levelData)
		{
			return;
		}

		const s32 sectorCount = (s32)s_levelData->sectors.size();
		const EditorSector* sector = s_levelData->sectors.data();
		const f32 maxValidDistSq = s_zoomVisual * s_zoomVisual * 256.0f;
		f32 minDistSq = maxValidDistSq;

		s32 curVtx = -1, curVtxSector = -1;
		for (s32 s = 0; s < sectorCount; s++, sector++)
		{
			if (sector->layer != s_layerIndex + s_layerMin) { continue; }

			const Vec2f* vtx = sector->vertices.data();
			const s32 vtxCount = (s32)sector->vertices.size();
			for (s32 v = 0; v < vtxCount; v++, vtx++)
			{
				const Vec2f diff = { vtx->x - worldPos.x, vtx->z - worldPos.z };
				const f32 distSq = diff.x*diff.x + diff.z*diff.z;
				if (distSq < minDistSq && distSq < maxValidDistSq)
				{
					minDistSq = distSq;
					curVtx = v;
					curVtxSector = s;
				}
			}
		}

		if (curVtx >= 0)
		{
			if (TFE_Input::mousePressed(MBUTTON_LEFT))
			{
				s_selectedVertexSector = curVtxSector;
				s_selectedVertex = curVtx;
				s_moveVertex = true;
				// get the plane...
				s_drawPlaneNrm = { 0.0f, 1.0f, 0.0f };
				s_drawPlaneOrg = s_cursor3d;
			}
			else if (!s_moveVertex)
			{
				s_hoveredVertexSector = curVtxSector;
				s_hoveredVertex = curVtx;
			}
		}
		else if (TFE_Input::mousePressed(MBUTTON_LEFT))
		{
			s_selectedVertexSector = -1;
			s_selectedVertex = -1;
		}
	}

	void editEntities2d(Vec2f worldPos)
	{
		s_hoveredEntity = -1;
		if (s_editMode != LEDIT_ENTITY || !s_levelData)
		{
			return;
		}

		s_hoveredEntitySector = LevelEditorData::findSector(s_layerIndex + s_layerMin, &worldPos);
		if (s_hoveredEntitySector >= 0)
		{
			const EditorSector* sector = s_levelData->sectors.data() + s_hoveredEntitySector;
			const u32 count = (u32)sector->objects.size();
			const EditorLevelObject* obj = sector->objects.data();
			for (u32 i = 0; i < count; i++, obj++)
			{
				const f32 width = obj->display ? (f32)obj->display->width : 1.0f;
				const f32 height = obj->display ? (f32)obj->display->height : 1.0f;
				// Half width
				f32 w;
				if (obj->oclass == CLASS_SPIRIT || obj->oclass == CLASS_SAFE || obj->oclass == CLASS_SOUND) { w = 1.0f; }
				else if (obj->oclass == CLASS_3D && obj->displayModel)
				{
					if (worldPos.x >= obj->displayModel->localAabb[0].x + obj->pos.x && worldPos.x < obj->displayModel->localAabb[1].x + obj->pos.x &&
						worldPos.z >= obj->displayModel->localAabb[0].z + obj->pos.z && worldPos.z < obj->displayModel->localAabb[1].z + obj->pos.z)
					{
						s_hoveredEntity = i;
						break;
					}
				}
				else { w = obj->display ? (f32)obj->display->width * obj->display->scale.x / 16.0f : 1.0f; }

				if (obj->oclass != CLASS_3D)
				{
					const f32 x0 = obj->worldCen.x - obj->worldExt.x, x1 = obj->worldCen.x + obj->worldExt.x;
					const f32 z0 = obj->worldCen.z - obj->worldExt.z, z1 = obj->worldCen.z + obj->worldExt.z;
					if (worldPos.x >= x0 && worldPos.x < x1 && worldPos.z >= z0 && worldPos.z < z1)
					{
						s_hoveredEntity = i;
						break;
					}
				}
			}
		}
		if (TFE_Input::mousePressed(MBUTTON_LEFT))
		{
			s_selectedEntity = s_hoveredEntity;
			s_selectedEntitySector = s_hoveredEntitySector;
		}
	}

	bool pointInAABB2d(const Vec3f* p, const Vec3f* aabb)
	{
		if (p->x < aabb[0].x || p->x > aabb[1].x || p->z < aabb[0].z || p->z > aabb[1].z)
		{
			return false;
		}
		return true;
	}

	bool pointInAABB2d(const Vec2f* p, const Vec3f* aabb)
	{
		if (p->x < aabb[0].x || p->x > aabb[1].x || p->z < aabb[0].z || p->z > aabb[1].z)
		{
			return false;
		}
		return true;
	}

	bool aabbOverlap2d(const Vec3f* aabb0, const Vec3f* aabb1)
	{
		return aabb0[0].z < aabb1[1].z && aabb0[1].z > aabb1[0].z && aabb0[0].x < aabb1[1].x && aabb0[1].x > aabb1[0].x;
	}

	bool wallsOverlapVertices(const EditorWall* w0, const EditorWall* w1, const Vec2f* vtx0, const Vec2f* vtx1)
	{
		const Vec2f& v0 = vtx0[w0->i0];
		const Vec2f& v1 = vtx0[w0->i1];

		const Vec2f& v2 = vtx1[w1->i0];
		const Vec2f& v3 = vtx1[w1->i1];

		// The walls should be going in opposite directions.
		if (fabsf(v0.x - v3.x) < 0.005f && fabsf(v1.x - v2.x) < 0.005f &&
			fabsf(v0.z - v3.z) < 0.005f && fabsf(v1.z - v2.z) < 0.005f)
		{
			return true;
		}
		return false;
	}

	bool editInsertVertex2d(Vec2f worldPos, s32 sectorId, s32 wallId)
	{
		EditorSector* sector = s_levelData->sectors.data() + sectorId;
		// If the wall has an adjoin, than that wall will also need to be split and the adjoins fixed up.
		if (sector->walls[wallId].adjoin >= 0)
		{
			// TODO
			return false;
		}
		else
		{
			EditorWall* wall = &sector->walls[wallId];
			Vec2f* vtx = sector->vertices.data();
			Vec2f pOffset = { worldPos.x - vtx[wall->i0].x, worldPos.z - vtx[wall->i0].z };
			Vec2f wallDir = { vtx[wall->i1].x - vtx[wall->i0].x, vtx[wall->i1].z - vtx[wall->i0].z };
			wallDir = TFE_Math::normalize(&wallDir);

			// Projection of the cursor position onto the wall which gives the parametric coordinate along the line.
			f32 proj = pOffset.x*wallDir.x + pOffset.z*wallDir.z;

			// Given the parametric coordinate, compute the new vertex.
			Vec2f newVtx = { vtx[wall->i0].x + proj * wallDir.x, vtx[wall->i0].z + proj * wallDir.z };

			// Then the vertex must be inserted in the list and walls adjusted.
			s32 prevWallCount = (s32)sector->walls.size();
			s32 prevVtxCount = (s32)sector->vertices.size();
			sector->walls.resize(sector->walls.size() + 1);
			sector->vertices.push_back(newVtx);

			// baseWall doesn't change.
			EditorWall* baseWall = &sector->walls[wallId];
			// Move all walls beyond baseWall "up" one.
			for (s32 w = prevWallCount; w > wallId + 1; w--)
			{
				sector->walls[w] = sector->walls[w - 1];
			}
			sector->walls[wallId + 1] = sector->walls[wallId];
			sector->walls[wallId].i1 = prevVtxCount;
			sector->walls[wallId + 1].i0 = prevVtxCount;

			sector->needsUpdate = true;
		}
		return true;
	}

	bool editInsertVertex2d(Vec2f worldPos)
	{
		s32 sectorId = -1;
		s32 wallId = LevelEditorData::findClosestWall(&sectorId, s_layerIndex + s_layerMin, &worldPos, s_zoomVisual * 32.0f);
		if (wallId < 0 || sectorId < 0) { return false; }

		return editInsertVertex2d(worldPos, sectorId, wallId);
	}

	void deleteSector(EditorSector* sector)
	{
		// Delete sectors.
		u32 sectorCount = (u32)s_levelData->sectors.size();
		for (u32 s = sector->id; s < sectorCount - 1; s++)
		{
			s_levelData->sectors[s] = s_levelData->sectors[s + 1];
			s_levelData->sectors[s].id = s;
		}
		s_levelData->sectors.resize(s_levelData->sectors.size() - 1);
		sectorCount--;

		// Adjust adjoin Ids.
		EditorSector* fixSec = s_levelData->sectors.data();
		for (u32 s = 0; s < sectorCount; s++, fixSec++)
		{
			const u32 wallCount = (u32)fixSec->walls.size();
			EditorWall* wall = fixSec->walls.data();

			for (u32 w = 0; w < wallCount; w++, wall++)
			{
				if (wall->adjoin == s_selectedSector)
				{
					wall->adjoin = -1;
					wall->walk = -1;
					fixSec->needsUpdate = true;
				}
				else if (wall->adjoin > s_selectedSector)
				{
					wall->adjoin--;
					wall->walk = wall->adjoin;
					fixSec->needsUpdate = true;
				}
			}
		}

		// Clear selection.
		s_selectedWall = -1;
		s_selectedSector = -1;
		LevelEditorData::updateSectors();
	}

	bool isPointOnWallOrVertex(const Vec2f* point, const EditorSector* sector, s32* wallId)
	{
		const u32 count = (u32)sector->walls.size();
		const EditorWall* wall = sector->walls.data();
		const Vec2f* vtx = sector->vertices.data();
		const f32 eps = 0.01f;
		const f32 epsSq = eps * eps;
		for (u32 w = 0; w < count; w++, wall++)
		{
			// first check vertex 0.
			if (TFE_Math::distanceSq(point, &vtx[wall->i0]) < epsSq)
			{
				*wallId = w;
				return true;
			}

			// then check the line segment.
			Vec2f closest;
			Geometry::closestPointOnLineSegment(vtx[wall->i0], vtx[wall->i1], *point, &closest);
			if (TFE_Math::distanceSq(point, &closest) < epsSq)
			{
				*wallId = w;
				return true;
			}
		}

		return false;
	}

	s32 splitSide(const Vec2f& v0, const Vec2f& normal, const Vec2f& point)
	{
		const Vec2f offset = { point.x - v0.x, point.z - v0.z };
		return (offset.x*normal.x + offset.z*normal.z) > -0.005f ? 0 : 1;
	}

	u16 addVertex(const Vec2f& vtx, std::vector<Vec2f>& list)
	{
		// Just loop through the vertices for now.
		const u32 count = (u32)list.size();
		const Vec2f* vtxList = list.data();
		for (u32 v = 0; v < count; v++)
		{
			f32 dx = fabsf(vtx.x - vtxList[v].x);
			f32 dz = fabsf(vtx.z - vtxList[v].z);
			if (dx < 0.01f && dz < 0.01f)
			{
				return u16(v);
			}
		}

		u16 index = u16(count);
		list.push_back(vtx);
		return index;
	}

	void splitSector(EditorSector* sector, Vec2f v0, Vec2f v1, u32 insideVertexCount, const Vec2f* insideVtx)
	{
		EditorSector sec0 = *sector;
		EditorSector sec1 = *sector;
		sec0.vertices.clear();
		sec0.walls.clear();
		sec0.objects.clear();

		sec1.vertices.clear();
		sec1.walls.clear();
		sec1.objects.clear();
				
		const u32 wallCount = (u32)sector->walls.size();
		const Vec2f* vtx = sector->vertices.data();

		s32 splitWallId0 = -1;
		s32 splitWallId1 = -1;

		// Find out which wall that v0 is attached to.
		// Then find out if the splitting line is aligned with the wall or going the opposite direction.
		EditorWall* wall = sector->walls.data();
		/*
		for (u32 w = 0; w < wallCount; w++, wall++)
		{
			Vec2f it;
			Geometry::closestPointOnLineSegment(vtx[wall->i0], vtx[wall->i1], v0, &it);
			if (TFE_Math::distanceSq(&v0, &it) <= 0.01f * 0.01f)
			{
				// If it is going the opposite direction, then flip the splitting line (and interior points).
				//f32 alignment = (v1.x - v0.x)*(vtx[wall->i1].x - it.x) + (v1.z - v0.z)*(vtx[wall->i1].z - it.z);
				f32 alignment = (v1.x - it.x)*(vtx[wall->i1].z - it.z) - (vtx[wall->i1].x - it.x)*(v1.z - it.z);
				if (alignment < 0.0f)
				{
					std::swap(v0.x, v1.x);
					std::swap(v0.z, v1.z);
				}
				break;
			}
		}
		*/

		// Normal = {-dz, dx}
		Vec2f normal = { v0.z - v1.z, v1.x - v0.x };
		normal = TFE_Math::normalize(&normal);

		if (insideVertexCount == 0)
		{
			// The most basic version...
			// Determine which walls are on one side or the other.
			// Determine which walls need to be split.
			wall = sector->walls.data();
			for (u32 w = 0; w < wallCount; w++, wall++)
			{
				const Vec2f& w0 = vtx[wall->i0];
				const Vec2f& w1 = vtx[wall->i1];

				const s32 side0 = splitSide(v0, normal, w0);
				const s32 side1 = splitSide(v0, normal, w1);
								
				if (side0 == 0 && side1 == 0)
				{
					// Add to sector 0.
					EditorWall newWall = sector->walls[w];
					newWall.i0 = addVertex(w0, sec0.vertices);
					newWall.i1 = addVertex(w1, sec0.vertices);

					if (newWall.adjoin >= 0)
					{
						s_levelData->sectors[newWall.adjoin].walls[sector->walls[w].mirror].mirror = (s32)sec0.walls.size();
					}
					sec0.walls.push_back(newWall);
				}
				else if (side0 == 1 && side1 == 1)
				{
					// Add to sector 1.
					EditorWall newWall = sector->walls[w];
					newWall.i0 = addVertex(w0, sec1.vertices);
					newWall.i1 = addVertex(w1, sec1.vertices);

					if (newWall.adjoin >= 0)
					{
						s_levelData->sectors[newWall.adjoin].walls[sector->walls[w].mirror].mirror = (s32)sec1.walls.size();
						s_levelData->sectors[newWall.adjoin].walls[sector->walls[w].mirror].adjoin = (s32)s_levelData->sectors.size();
					}
					sec1.walls.push_back(newWall);
				}
				else
				{
					// Find the intersection, this point gets added to both sides.
					f32 s, t;
					Geometry::lineSegmentIntersect(&w0, &w1, &v0, &v1, &s, &t);

					// Find the intersection, this point gets added to both sides.
					s = std::max(0.0f, std::min(1.0f, s));
					const Vec2f it = { w0.x + s*(w1.x - w0.x), w0.z + s*(w1.z - w0.z) };

					s32 splitVtxIdx;
					f32 dx = fabsf(v0.x - it.x);
					f32 dz = fabsf(v0.z - it.z);
					if (dx < 0.01f && dz < 0.01f)
					{
						splitVtxIdx = 0;
					}
					else
					{
						splitVtxIdx = 1;
					}

					// Add the first vertex to the correct side.
					EditorWall newWall0 = sector->walls[w];
					EditorWall newWall1 = sector->walls[w];
					if (side0 == 0)
					{
						newWall0.i0 = addVertex(w0, sec0.vertices);
						newWall0.i1 = addVertex(it, sec0.vertices);

						newWall1.i0 = addVertex(it, sec1.vertices);
						newWall1.i1 = addVertex(w1, sec1.vertices);

						if (sector->walls[w].adjoin >= 0)
						{
							// we need to split the mirror as well and then match up the adjoins.
							EditorSector* adjoin = &s_levelData->sectors[sector->walls[w].adjoin];
							s32 mirror = sector->walls[w].mirror;

							// Split the mirror edge: i0 -> it -> i1
							// mirror: i0 -> it; mirror + 1: it -> i1
							s32 adjoinWallCount = (s32)adjoin->walls.size();
							adjoin->walls.resize(adjoin->walls.size() + 1);
							for (s32 wA = adjoinWallCount; wA > mirror; wA--)
							{
								adjoin->walls[wA] = adjoin->walls[wA - 1];
							}
							adjoin->walls[mirror].i1 = addVertex(it, adjoin->vertices);
							adjoin->walls[mirror + 1].i0 = adjoin->walls[mirror].i1;

							// Setup mirrors.
							// Which side is which?
							if (splitSide(v0, normal, adjoin->vertices[adjoin->walls[mirror].i0]) == 0)
							{
								adjoin->walls[mirror].mirror = (s32)sec0.walls.size();
								adjoin->walls[mirror].adjoin = (s32)sector->id;
								adjoin->walls[mirror].walk = (s32)sector->id;
								newWall0.mirror = mirror;

								adjoin->walls[mirror + 1].mirror = (s32)sec1.walls.size();
								adjoin->walls[mirror + 1].adjoin = (s32)s_levelData->sectors.size();
								adjoin->walls[mirror + 1].walk = (s32)s_levelData->sectors.size();
								newWall1.mirror = mirror + 1;
							}
							else
							{
								adjoin->walls[mirror].mirror = (s32)sec1.walls.size();
								adjoin->walls[mirror].adjoin = (s32)s_levelData->sectors.size();
								adjoin->walls[mirror].walk = (s32)s_levelData->sectors.size();
								newWall1.mirror = mirror;

								adjoin->walls[mirror + 1].mirror = (s32)sec0.walls.size();
								adjoin->walls[mirror + 1].adjoin = (s32)sector->id;
								adjoin->walls[mirror + 1].walk = (s32)sector->id;
								newWall0.mirror = mirror + 1;
							}
						}

						sec0.walls.push_back(newWall0);
						sec1.walls.push_back(newWall1);

						// Add the wall from this intersection to the previous point.
						newWall0.i0 = newWall0.i1;
						newWall0.i1 = addVertex(splitVtxIdx == 0 ? v1 : v0, sec0.vertices);
						newWall0.adjoin = (s32)s_levelData->sectors.size();
						newWall0.walk = newWall0.adjoin;
						newWall0.mirror = -1; // Will be filled out at the end.
						splitWallId0 = (s32)sec0.walls.size();
						sec0.walls.push_back(newWall0);
					}
					else
					{
						newWall0.i0 = addVertex(it, sec0.vertices);
						newWall0.i1 = addVertex(w1, sec0.vertices);

						newWall1.i0 = addVertex(w0, sec1.vertices);
						newWall1.i1 = addVertex(it, sec1.vertices);

						if (sector->walls[w].adjoin >= 0)
						{
							// we need to split the mirror as well and then match up the adjoins.
							EditorSector* adjoin = &s_levelData->sectors[sector->walls[w].adjoin];
							s32 mirror = sector->walls[w].mirror;

							// Split the mirror edge: i0 -> it -> i1
							// mirror: i0 -> it; mirror + 1: it -> i1
							s32 adjoinWallCount = (s32)adjoin->walls.size();
							adjoin->walls.resize(adjoin->walls.size() + 1);
							for (s32 wA = adjoinWallCount; wA > mirror; wA--)
							{
								adjoin->walls[wA] = adjoin->walls[wA - 1];
							}
							adjoin->walls[mirror].i1 = addVertex(it, adjoin->vertices);
							adjoin->walls[mirror + 1].i0 = adjoin->walls[mirror].i1;

							// Setup mirrors.
							// Which side is which?
							if (splitSide(v0, normal, adjoin->vertices[adjoin->walls[mirror].i0]) == 0)
							{
								adjoin->walls[mirror].mirror = (s32)sec0.walls.size();
								adjoin->walls[mirror].adjoin = (s32)sector->id;
								adjoin->walls[mirror].walk = (s32)sector->id;
								newWall0.mirror = mirror;

								adjoin->walls[mirror + 1].mirror = (s32)sec1.walls.size();
								adjoin->walls[mirror + 1].adjoin = (s32)s_levelData->sectors.size();
								adjoin->walls[mirror + 1].walk = (s32)s_levelData->sectors.size();
								newWall1.mirror = mirror + 1;
							}
							else
							{
								adjoin->walls[mirror].mirror = (s32)sec1.walls.size();
								adjoin->walls[mirror].adjoin = (s32)s_levelData->sectors.size();
								adjoin->walls[mirror].walk = (s32)s_levelData->sectors.size();
								newWall1.mirror = mirror;

								adjoin->walls[mirror + 1].mirror = (s32)sec0.walls.size();
								adjoin->walls[mirror + 1].adjoin = (s32)sector->id;
								adjoin->walls[mirror + 1].walk = (s32)sector->id;
								newWall0.mirror = mirror + 1;
							}
						}

						sec0.walls.push_back(newWall0);
						sec1.walls.push_back(newWall1);

						// Add the wall from this intersection to the previous point.
						newWall1.i0 = newWall1.i1;
						newWall1.i1 = addVertex(splitVtxIdx == 0 ? v1 : v0, sec1.vertices);
						newWall1.adjoin = sector->id;
						newWall1.walk = newWall1.adjoin;
						newWall1.mirror = -1;	// Will be filled out at the end.
						splitWallId1 = (s32)sec1.walls.size();
						sec1.walls.push_back(newWall1);
					}
				}
			}
		}
		else
		{
		}

		if (splitWallId0 >= 0) { sec0.walls[splitWallId0].mirror = splitWallId1; }
		if (splitWallId1 >= 0) { sec1.walls[splitWallId1].mirror = splitWallId0; }

		*sector = sec0;
		sector->needsUpdate = true;

		LevelEditorData::addNewSectorFullCopy(sec1);
		LevelEditorData::updateSectors();
	}

	void insertSector2d()
	{
		// First determine the signed area and reverse the vertex/wall order if necessary.
		// This way sectors can be drawn either clockwise or anticlockwise and achieve the same result.
		const s32 srcVertexCount = (s32)s_newSector.vertices.size();
		const f32 signedArea = TFE_Polygon::signedArea((u32)srcVertexCount, s_newSector.vertices.data());
		if (signedArea < 0.0f)
		{
			std::vector<Vec2f> srcVtx = s_newSector.vertices;
			for (s32 v = 0; v < srcVertexCount; v++)
			{
				s_newSector.vertices[v] = srcVtx[srcVertexCount - v - 1];
			}
		}

		// For now use the first texture to get default textures...
		s32 count = (s32)s_levelData->sectors.size();
		EditorSector* sector = s_levelData->sectors.data();
		LevelEditorData::addNewSector(s_newSector, sector->floorTexture.tex, sector->ceilTexture.tex, sector->walls[0].mid.tex);

		sector = &s_levelData->sectors.back();
		const u32 srcWallCount = (u32)sector->walls.size();
		const s32 srcLayer = sector->layer;
		// Source shape bounds are expanded slightly to robustly handle cases where only purely horizontal or vertical lines are overlapping.
		const Vec3f srcAabb[2] =
		{
			{ sector->aabb[0].x - 0.1f, sector->aabb[0].y, sector->aabb[0].z - 0.1f },
			{ sector->aabb[1].x + 0.1f, sector->aabb[1].y, sector->aabb[1].z + 0.1f },
		};

		// First determine if all vertices are inside of a single sector.
		const Vec2f* vtx = s_newSector.vertices.data();
		EditorSector* neighbor = s_levelData->sectors.data();
		for (s32 s = 0; s < count; s++, neighbor++)
		{
			//  Make sure the sector is on the correct layer.
			if (srcLayer != neighbor->layer) { continue; }

			// See if the AABBs overlap.
			if (!aabbOverlap2d(srcAabb, neighbor->aabb)) { continue; }

			u32 vSrcInside = 0;
			u32 attachedCount = 0;
			u32 insideCount = 0;
			s32 wallAttach[1024];
			s32 inside[1024];
			for (s32 vSrc = 0; vSrc < srcVertexCount; vSrc++)
			{
				// See if the point is inside the sector AABB.
				if (!pointInAABB2d(&vtx[vSrc], neighbor->aabb)) { break; }

				// Test if the point is inside the sector polygon.
				s32 wallId;
				if (isPointOnWallOrVertex(&vtx[vSrc], neighbor, &wallId))
				{
					wallAttach[attachedCount++] = vSrc;
				}
				else if (!Geometry::pointInSector(&vtx[vSrc], (u32)neighbor->vertices.size(), neighbor->vertices.data(), (u32)neighbor->walls.size(), (u8*)neighbor->walls.data(), sizeof(EditorWall)))
				{
					break;
				}
				else
				{
					inside[insideCount++] = vSrc;
				}
				vSrcInside++;
			}

			// Is the new sector or shape completely inside of this sector?
			if (vSrcInside == srcVertexCount)
			{
				// TODO: We should verify that no edges of the new sector intersect edges of the neighbor sector.
				// TODO: Check if any edges overlap edges with adjoins so they can be connected directly (i.e. making a staircase)
				if (attachedCount > 1)
				{
					// If the new shape is touching the parent sector in at least two places, then what we really want to do is split that sector.
					// If there are two attachment points only - then we just spit along that edge.
					// If there are at least two attachment points and some interior points, then the split edges will be between the two attachment points that
					// surround the interior points and go through those interior points.
					// If degenerate cases, just pick a single valid case and go with it (the mapper can always use undo if we get this wrong and then better
					// specify the split.

					// Split the sector in two along the straight edge.
					if (attachedCount == 2 && insideCount == 0)
					{
						// Copy the vertices.
						Vec2f v0 = sector->vertices[0];
						Vec2f v1 = sector->vertices[1];

						// Delete the new sector.
						s_levelData->sectors.resize(s_levelData->sectors.size() - 1);

						// Split the neighbor sector.
						splitSector(neighbor, v0, v1);
					}
					// Split the sector in two with multiple interior adjoins.
					else if (attachedCount == 2 && insideCount > 0)
					{
						// Copy the vertices.
						Vec2f v0 = sector->vertices[wallAttach[0]];
						Vec2f v1 = sector->vertices[wallAttach[1]];

						Vec2f insideVtx[128];
						u32 insideVertexCount = 0;
						for (u32 v = 0; v < insideCount; v++)
						{
							if (inside[v] > wallAttach[0] && inside[v] < wallAttach[1])
							{
								insideVtx[insideVertexCount++] = sector->vertices[inside[v]];
							}
						}

						// Delete the new sector.
						s_levelData->sectors.resize(s_levelData->sectors.size() - 1);

						// Split the neighbor sector.
						splitSector(neighbor, v0, v1, insideVertexCount, insideVtx);
					}
					// Split the sector in two along a single edge, but we have to figure out what edge that is...
					else if (attachedCount > 2 && insideCount == 0)
					{
					}
					// Finally, the most complex case - split the sector in two with multiple interior adjoins, but we have to figure out which attachments/interior edges to use...
					else if (attachedCount > 2 && insideCount > 0)
					{
					}
					// If we get here then this is not cleanly resolvable, just return.
					else
					{
						// Delete the new sector if it is invalid. This may happen when trying to split a sector but not getting the configuration right.
						if (sector->vertices.size() < 3)
						{
							s_levelData->sectors.resize(s_levelData->sectors.size() - 1);
						}
						return;
					}
				}
				else
				{
					// Delete the new sector if it is invalid. This may happen when trying to split a sector but not getting the configuration right.
					if (sector->vertices.size() < 3)
					{
						s_levelData->sectors.resize(s_levelData->sectors.size() - 1);
						return;
					}

					// The wall the vertex is sitting on should be split at that vertex. Otherwise there are graphical artifacts.
					if (attachedCount == 1)
					{
						// TODO: Split the wall, see above.
						// Note that if the attachment is already a vertex, such a split is unnecessary, so check that first.
					}

					// For the first pass, keep it simple and assume it is a self contained interior sector.
					// 1. Add new edges to the neighbor sector going in the *opposite* direction. This is a hole in the "neighbor" sector polygon.
					// 2. Link up adjoins between the hole and the new sector.
					std::vector<Vec2f>& srcVtx = s_newSector.vertices;
					const u32 baseVertexIndex = (u32)neighbor->vertices.size();
					const u32 baseWallIndex = (u32)neighbor->walls.size();
					for (s32 v = 0; v < srcVertexCount; v++)
					{
						const s32 vRev = srcVertexCount - v - 1;
						neighbor->vertices.push_back(srcVtx[vRev]);

						EditorWall newWall = sector->walls[vRev];
						newWall.i0 = baseVertexIndex + ((v > 0) ? (v - 1) : (srcVertexCount - 1));
						newWall.i1 = baseVertexIndex + v;
						newWall.adjoin = sector->id;
						newWall.walk = sector->id;
						newWall.mirror = vRev;

						sector->walls[v].adjoin = s;
						sector->walls[v].walk = s;
						sector->walls[v].mirror = baseWallIndex + srcVertexCount - v - 1;

						neighbor->walls.push_back(newWall);
					}
					// Copy outer sector attributes to the sub-sector.
					sector->ambient = neighbor->ambient;
					sector->flags[0] = neighbor->flags[0];
					sector->flags[1] = neighbor->flags[1];
					sector->flags[2] = neighbor->flags[2];
					sector->floorAlt = neighbor->floorAlt;
					sector->ceilAlt = neighbor->ceilAlt;
					sector->floorTexture = neighbor->floorTexture;
					sector->ceilTexture = neighbor->ceilTexture;
					// The outer sector needs to be updated since new walls were added.
					neighbor->needsUpdate = true;
				}
				
				// We don't need to check for wall overlaps since this is either a sub-sector or splitting a sector.
				LevelEditorData::updateSectors();
				return;
			}
		}

		// Then determine if any walls overlap.
		for (u32 wSrc = 0; wSrc < srcWallCount; wSrc++)
		{
			const Vec2f* srcVtx = sector->vertices.data();
			EditorWall* srcWall = &sector->walls[wSrc];

			EditorSector* overlapSector = nullptr;
			s32 overlapWall = -1;
			// Once we have the new sector, see if there are any overlapping lines from other sectors such that:
			// 1. The sectors are on the same layer.
			// 2. The sectors do NOT overlap (? - how to determine this robustly)
			// 3. A line overlaps with a line in the other sector.
			EditorSector* neighbor = s_levelData->sectors.data();
			for (s32 s = 0; s < count && !overlapSector; s++, neighbor++)
			{
				//  Make sure the sector is on the correct layer.
				if (srcLayer != neighbor->layer) { continue; }

				// See if the AABBs overlap.
				if (!aabbOverlap2d(srcAabb, neighbor->aabb)) { continue; }

				// Check the walls for overlap.
				const u32 dstWallCount = (u32)neighbor->walls.size();
				const Vec2f* dstVtx = neighbor->vertices.data();
				for (u32 wDst = 0; wDst < dstWallCount; wDst++)
				{
					EditorWall* dstWall = &neighbor->walls[wDst];

					// Next determine if the walls overlap vertices (the easy test).
					if (wallsOverlapVertices(srcWall, dstWall, srcVtx, dstVtx))
					{
						overlapSector = neighbor;
						overlapWall = wDst;
						break;
					}

					// If the vertices don't overlap, do the segments overlap? Should we split the dstWall?
					const Vec2f* w0 = &dstVtx[dstWall->i0];
					const Vec2f* w1 = &dstVtx[dstWall->i1];

					const Vec2f* v0 = &srcVtx[srcWall->i0];
					const Vec2f* v1 = &srcVtx[srcWall->i1];

					Vec2f p0, p1;
					Geometry::closestPointOnLineSegment(*w0, *w1, *v0, &p0);
					Geometry::closestPointOnLineSegment(*w0, *w1, *v1, &p1);

					f32 d0 = TFE_Math::distanceSq(v0, &p0);
					f32 d1 = TFE_Math::distanceSq(v1, &p1);
					if (fabsf(d0) < 0.02f && fabsf(d1) < 0.02f)
					{
						if (editInsertVertex2d(p1, s, wDst) && editInsertVertex2d(p0, s, wDst+1))
						{
							overlapSector = neighbor;
							overlapWall = wDst + 1;
							break;
						}
					}
				}
			}

			if (overlapSector)
			{
				srcWall->adjoin = overlapSector->id;
				srcWall->walk = srcWall->adjoin;
				srcWall->mirror = overlapWall;

				EditorWall* dstWall = &overlapSector->walls[overlapWall];
				dstWall->adjoin = count;
				dstWall->walk = dstWall->adjoin;
				dstWall->mirror = wSrc;

				// Snap the vertices together just in case.
				sector->vertices[srcWall->i0] = overlapSector->vertices[dstWall->i1];
				sector->vertices[srcWall->i1] = overlapSector->vertices[dstWall->i0];

				sector->needsUpdate = true;
				overlapSector->needsUpdate = true;
			}
		}

		LevelEditorData::updateSectors();
	}

	void clearNewSector()
	{
		s_newSector.vertices.clear();
		s_newSector.walls.clear();
		s_newSector.triangles.count = 0;
		s_newSector.triangles.vtx.clear();
	}

	void editDrawSectors2d(Vec2f worldPos)
	{
		if (s_editMode != LEDIT_DRAW || !s_levelData)
		{
			return;
		}
		if (s_newSector.vertices.size() < 1)
		{
			s_newSector.vertices.resize(1);
		}

		s_cursorSnapped = { s_cursor3d.x, s_gridHeight, s_cursor3d.z };
		if (!snapToGeometry(&s_cursorSnapped, true))
		{
			snapToGrid(&s_cursorSnapped, true);
		}

		s_newSector.vertices.back() = { s_cursorSnapped.x, s_cursorSnapped.z };

		// If left mouse is pressed, add a new vertex.
		if (TFE_Input::mousePressed(MBUTTON_LEFT))
		{
			if (s_newSector.vertices.size() == 1)
			{
				s_newSector.floorAlt = s_gridHeight;
				s_newSector.ceilAlt = s_gridHeight - 16.0f;
				s_newSector.layer = s_layerIndex + s_layerMin;
				s_newSector.ambient = 30;
			}

			s_newSector.vertices.push_back({ s_cursorSnapped.x, s_cursorSnapped.z });

			// Build walls.
			size_t count = s_newSector.vertices.size();
			s_newSector.walls.resize(count);
			for (size_t w = 0; w < count; w++)
			{
				s_newSector.walls[w].adjoin = -1;
				s_newSector.walls[w].mirror = -1;
				s_newSector.walls[w].walk = -1;
				s_newSector.walls[w].i0 = u16(w);
				s_newSector.walls[w].i1 = u16((w + 1) % count);
			}
		}
		else if (TFE_Input::keyPressed(KEY_ESCAPE))
		{
			// Clear the new sector.
			clearNewSector();
			return;
		}
		else if (TFE_Input::mousePressed(MBUTTON_RIGHT) || TFE_Input::keyPressed(KEY_RETURN))
		{
			if (s_newSector.walls.size() >= 3)
			{
				s_newSector.vertices.resize(s_newSector.vertices.size() - 1);

				s_newSector.walls.resize(s_newSector.walls.size() - 1);
				EditorWall& lastWall = s_newSector.walls[s_newSector.walls.size() - 1];
				lastWall.i1 = 0;

				insertSector2d();
			}
			
			// Clear the new sector.
			clearNewSector();
			return;
		}

		// Triangulate
		static Polygon contour;
		contour.vtxCount = (s32)s_newSector.vertices.size();
		memcpy(contour.vtx, s_newSector.vertices.data(), sizeof(Vec2f) * contour.vtxCount);
		if (fabsf(contour.vtx[contour.vtxCount - 1].x - contour.vtx[contour.vtxCount - 2].x) < 0.01f && fabsf(contour.vtx[contour.vtxCount - 1].z - contour.vtx[contour.vtxCount - 2].z) < 0.01f)
		{
			contour.vtxCount--;
		}
		if (contour.vtxCount < 3)
		{
			return;
		}

		// Setup triangles
		u32 polyCount;
		const Triangle* tri = TFE_Polygon::decomposeComplexPolygon(1, &contour, &polyCount);
		s_newSector.triangles.count = polyCount;
		s_newSector.triangles.vtx.resize(polyCount * 3);

		u32 tIdx = 0;
		for (u32 p = 0; p < polyCount; p++, tri++)
		{
			s_newSector.triangles.vtx[p * 3 + 0] = tri->vtx[0];
			s_newSector.triangles.vtx[p * 3 + 1] = tri->vtx[1];
			s_newSector.triangles.vtx[p * 3 + 2] = tri->vtx[2];
		}
	}
}
