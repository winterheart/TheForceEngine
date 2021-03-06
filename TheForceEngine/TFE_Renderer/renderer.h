#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Software Rendering Library
// The software rendering library handles rendering using the CPU,
// where the GPU is only used for blitting the results (and maybe
// post processing).
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Asset/textureAsset.h>

enum RendererType
{
	TFE_RENDERER_SOFTWARE_CPU = 0,
	// TODO: Add TFE_RENDERER_SOFTWARE_GPU_OPENGL, TFE_RENDERER_GPU_OPENGL, ...
	TFE_RENDERER_COUNT
};

struct Palette256;
struct ColorMap;
struct Texture;
struct TextureFrame;
struct Font;
struct Frame;
struct Sprite;
struct Model;

// Interface.
class TFE_Renderer
{
public:
	virtual bool init() = 0;
	virtual void destroy() = 0;

	virtual void begin() = 0;
	virtual void applyColorEffect() = 0;
	virtual void end() = 0;

	virtual bool changeResolution(u32 width, u32 height, bool widescreen, bool asyncVirtualDisplay, bool gpuColorConversion) = 0;
	virtual void getResolution(u32* width, u32* height) = 0;
	virtual void enableScreenClear(bool enable) = 0;
	virtual void clearScreen() = 0;

	// Temporary.
	virtual u8*  getDisplay() = 0;

	// Draw Commands
	virtual void setPalette(const Palette256* pal) = 0;
	virtual void setColorMap(const ColorMap* cmp) = 0;
	virtual void setPaletteColor(u8 index, u32 color) = 0;
	virtual Palette256 getPalette() = 0;
	virtual const ColorMap* getColorMap() = 0;
	virtual void enablePalEffects(bool grayScale, bool green) = 0;
	virtual void drawMapLines(s32 layers, f32 xOffset, f32 zOffset, f32 scale) = 0;
	virtual void drawPalette() = 0;
	virtual void drawTexture(Texture* texture, s32 x, s32 y) = 0;
	virtual void drawTextureHorizontal(Texture* texture, s32 x, s32 y) = 0;
	virtual void drawFont(Font* font) = 0;
	virtual void drawFrame(Frame* frame, s32 x, s32 y) = 0;
	virtual void drawSprite(Sprite* sprite, s32 x, s32 y, u32 anim, u8 angle) = 0;

	virtual void blitImage(const TextureFrame* texture, s32 x0, s32 y0, s32 scaleX, s32 scaleY, u8 lightLevel = 31, TextureLayout layout = TEX_LAYOUT_VERT) = 0;
	virtual void print(const char* text, const Font* font, s32 x0, s32 y0, s32 scaleX, s32 scaleY, u8 overrideColor = 0, bool fixedWidth = false) = 0;
	virtual s32  getStringPixelLength(const char* str, const Font* font) = 0;
	
	virtual void drawLine(const Vec2f* v0, const Vec2f* v1, u8 color) = 0;
	virtual void drawColoredColumn(s32 x0, s32 y0, s32 y1, u8 color) = 0;
	virtual void drawTexturedColumn(s32 x0, s32 y0, s32 y1, f32 v0, f32 v1, s8 lightLevel, const u8* image, s32 texHeight) = 0;
	virtual void drawMaskTexturedColumn(s32 x0, s32 y0, s32 y1, f32 v0, f32 v1, s8 lightLevel, const u8* image, s32 texHeight) = 0;
	virtual void drawClampedTexturedColumn(s32 x0, s32 y0, s32 y1, f32 v0, f32 v1, s8 lightLevel, const u8* image, s32 texHeight, s32 offset, bool skipTexels) = 0;
	virtual void drawSkyColumn(s32 x0, s32 y0, s32 y1, s32 halfScrHeight, s32 offsetY, const u8* image, s32 texHeight) = 0;
	virtual void drawSpriteColumn(s32 x0, s32 y0, s32 y1, f32 v0, f32 v1, s8 lightLevel, const u8* image, s32 texHeight) = 0;
	virtual void drawSpan(s32 x0, s32 x1, s32 y, f32 z, f32 r0, f32 r1, f32 xOffset, f32 zOffset, s8 lightLevel, s32 texWidth, s32 texHeight, const u8* image) = 0;
	virtual void drawSpanClip(s32 x0, s32 x1, s32 y, f32 z, f32 r0, f32 r1, f32 xOffset, f32 zOffset, s8 lightLevel, s32 texWidth, s32 texHeight, const u8* image) = 0;
	
	virtual void drawColoredQuad(s32 x, s32 y, s32 w, s32 h, u8 color) = 0;
	virtual void drawHLine(s32 x0, s32 x1, s32 y, u8 color, u8 lightLevel) = 0;
	virtual void drawHLineGouraud(s32 x0, s32 x1, s32 l0, s32 l1, s32 y, u8 color) = 0;
	virtual void drawHLineTextured(s32 x0, s32 x1, s32 y, s32 u0, s32 v0, s32 u1, s32 v1, s32 texWidth, s32 texHeight, const u8* image, u8 lightLevel) = 0;
	virtual void drawHLineTexturedPC(s32 x0, s32 x1, s32 y, s32 u0, s32 v0, s32 u1, s32 v1, s32 z0, s32 z1, s32 texWidth, s32 texHeight, const u8* image, u8 lightLevel) = 0;
	virtual void drawHLineTexturedGouraud(s32 x0, s32 x1, s32 y, s32 u0, s32 v0, s32 u1, s32 v1, s32 l0, s32 l1, s32 texWidth, s32 texHeight, const u8* image) = 0;
	virtual void drawHLineTexturedGouraudPC(s32 x0, s32 x1, s32 y, s32 u0, s32 v0, s32 u1, s32 v1, s32 z0, s32 z1, s32 l0, s32 l1, s32 texWidth, s32 texHeight, const u8* image) = 0;
		
	virtual void setHLineClip(s32 x0, s32 x1, const s16* upperYMask, const s16* lowerYMask) = 0;

	// Basic pure horizontal and vertical lines
	virtual void drawHorizontalLine(s32 x0, s32 x1, s32 y, u8 color) = 0;
	virtual void drawVerticalLine(s32 y0, s32 y1, s32 x, u8 color) = 0;

	// Debug
	virtual void clearMapMarkers() = 0;
	virtual void addMapMarker(s32 layer, f32 x, f32 z, u8 color) = 0;
	virtual u32 getMapMarkerCount() = 0;

	static TFE_Renderer* create(RendererType type);
	static void destroy(TFE_Renderer* renderer);
};
