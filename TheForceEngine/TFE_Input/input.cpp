#include <TFE_Input/input.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_System/system.h>
#include <TFE_System/parser.h>
#include <memory.h>
#include <string.h>
#include <assert.h>

namespace TFE_Input
{
	#define BUFFERED_TEXT_LEN 64

	////////////////////////////////////////////////////////
	// Input State
	////////////////////////////////////////////////////////
	f32 s_axis[AXIS_COUNT] = { 0 };
	u8 s_buttonDown[CONTROLLER_BUTTON_COUNT] = { 0 };
	u8 s_buttonPressed[CONTROLLER_BUTTON_COUNT] = { 0 };

	u8 s_keyDown[KEY_COUNT] = { 0 };
	u8 s_keyPressed[KEY_COUNT] = { 0 };

	char s_bufferedText[BUFFERED_TEXT_LEN];
	u8 s_bufferedKey[KEY_COUNT];

	u8 s_mouseDown[MBUTTON_COUNT] = { 0 };
	u8 s_mousePressed[MBUTTON_COUNT] = { 0 };
	
	s32 s_mouseWheel[2] = { 0 };
	s32 s_mouseMove[2] = { 0 };
	s32 s_mousePos[2] = { 0 };

	bool s_relativeMode = false;

	static const char* const* s_controllerAxisNames;
	static const char* const* s_controllerButtonNames;
	static const char* const* s_mouseAxisNames;
	static const char* const* s_mouseButtonNames;
	static const char* const* s_keyboardNames;

	////////////////////////////////////////////////////////
	// Implementation
	////////////////////////////////////////////////////////
	void endFrame()
	{
		for (u32 i = 0; i < CONTROLLER_BUTTON_COUNT; i++)
		{
			s_buttonPressed[i] = 0;
		}
		for (u32 i = 0; i < MBUTTON_COUNT; i++)
		{
			s_mousePressed[i] = 0;
		}
		s_mouseWheel[0] = 0;
		s_mouseWheel[1] = 0;
		memset(s_keyPressed, 0, KEY_COUNT);
		memset(s_bufferedKey, 0, KEY_COUNT);
		memset(s_bufferedText, 0, BUFFERED_TEXT_LEN);
	}

	// Set, from the OS
	void setAxis(Axis axis, f32 value)
	{
		s_axis[axis] = value;
	}

	void setButtonDown(Button button)
	{
		if (!s_buttonDown[button])
		{
			s_buttonPressed[button] = 1;
		}
		s_buttonDown[button] = 1;
	}

	void setButtonUp(Button button)
	{
		s_buttonDown[button] = 0;
	}

	void setKeyDown(KeyboardCode key)
	{
		if (!s_keyDown[key])
		{
			s_keyPressed[key] = 1;
		}
		s_keyDown[key] = 1;
	}

	void setKeyUp(KeyboardCode key)
	{
		s_keyDown[key] = 0;
	}

	void setMouseButtonDown(MouseButton button)
	{
		if (!s_mouseDown[button])
		{
			s_mousePressed[button] = 1;
		}
		s_mouseDown[button] = 1;
	}

	void setMouseButtonUp(MouseButton button)
	{
		s_mouseDown[button] = 0;
	}

	void setMouseWheel(s32 dx, s32 dy)
	{
		s_mouseWheel[0] = dx;
		s_mouseWheel[1] = dy;
	}

	void setRelativeMousePos(s32 x, s32 y)
	{
		s_mouseMove[0] = x;
		s_mouseMove[1] = y;
	}

	void setMousePos(s32 x, s32 y)
	{
		s_mousePos[0] = x;
		s_mousePos[1] = y;
	}

	void enableRelativeMode(bool enable)
	{
		s_relativeMode = enable;
	}
	
	// Buffered Input
	void setBufferedInput(const char* text)
	{
		strcpy(s_bufferedText, text);
	}

	void setBufferedKey(KeyboardCode key)
	{
		s_bufferedKey[key] = 1;
	}

	// Get
	f32 getAxis(Axis axis)
	{
		return s_axis[axis];
	}

	void getMouseWheel(s32* dx, s32* dy)
	{
		assert(dx && dy);

		*dx = s_mouseWheel[0];
		*dy = s_mouseWheel[1];
	}

	void getMouseMove(s32* x, s32* y)
	{
		assert(x && y);

		*x = s_mouseMove[0];
		*y = s_mouseMove[1];
	}

	void getMousePos(s32* x, s32* y)
	{
		assert(x && y);

		*x = s_mousePos[0];
		*y = s_mousePos[1];
	}

	bool buttonDown(Button button)
	{
		return s_buttonDown[button] != 0;
	}

	bool buttonPressed(Button button)
	{
		return s_buttonPressed[button] != 0;
	}

	bool keyDown(KeyboardCode key)
	{
		return s_keyDown[key] != 0;
	}

	bool keyPressed(KeyboardCode key)
	{
		return s_keyPressed[key] != 0;
	}

	bool mouseDown(MouseButton button)
	{
		return s_mouseDown[button] != 0;
	}

	bool mousePressed(MouseButton button)
	{
		return s_mousePressed[button] != 0;
	}

	bool relativeModeEnabled()
	{
		return s_relativeMode;
	}

	// Buffered Input
	const char* getBufferedText()
	{
		return s_bufferedText;
	}

	bool bufferedKeyDown(KeyboardCode key)
	{
		return s_bufferedKey[key];
	}

	bool loadKeyNames(const char* path)
	{
		FileStream file;
		if (!file.open(path, FileStream::MODE_READ))
		{
			return false;
		}

		// Read the file into memory.
		const size_t len = file.getSize();
		char* contents = new char[len+1];
		if (!contents)
		{
			file.close();
			return false;
		}
		file.readBuffer(contents, (u32)len);
		file.close();

		TFE_Parser parser;
		parser.init(contents, len);
		parser.addCommentString(";");
		parser.addCommentString("#");
		parser.addCommentString("//");

		size_t bufferPos = 0;
		enum KeySection
		{
			Unknown = 0,
			ControllerAxis,
			ControllerButtons,
			MouseAxis,
			MouseButtons,
			Keyboard,
		};

		KeySection section = Unknown;
		char** curList = nullptr;
		while (bufferPos < len)
		{
			const char* line = parser.readLine(bufferPos);
			if (!line) { break; }

			TokenList tokens;
			parser.tokenizeLine(line, tokens);
			if (tokens.size() < 1) { continue; }
			const char* item = tokens[0].c_str();

			if (strcasecmp(item, "[ControllerAxis]") == 0)
			{
				section = ControllerAxis;
				s_controllerAxisNames = new const char*[AXIS_COUNT];
				curList = (char**)s_controllerAxisNames;
			}
			else if (strcasecmp(item, "[ControllerButtons]") == 0)
			{
				section = ControllerButtons;
				s_controllerButtonNames = new const char*[CONTROLLER_BUTTON_COUNT];
				curList = (char**)s_controllerButtonNames;
			}
			else if (strcasecmp(item, "[MouseAxis]") == 0)
			{
				section = MouseAxis;
				s_mouseAxisNames = new const char*[MOUSE_AXIS_COUNT];
				curList = (char**)s_mouseAxisNames;
			}
			else if (strcasecmp(item, "[MouseButtons]") == 0)
			{
				section = MouseButtons;
				s_mouseButtonNames = new const char*[MBUTTON_COUNT];
				curList = (char**)s_mouseButtonNames;
			}
			else if (strcasecmp(item, "[Keyboard]") == 0)
			{
				section = Keyboard;
				s_keyboardNames = new const char*[KEY_LAST + 2];
				curList = (char**)s_keyboardNames;
			}
			else if (section == Unknown)
			{
				TFE_System::logWrite(LOG_ERROR, "Input", "Cannot parse Key Name lists '%s'", path);
				delete[] contents;
				return false;
			}
			else
			{
				*curList = new char[strlen(item) + 1];
				strcpy(*curList, item);
				curList++;
			}
		};

		delete[] contents;
		return true;
	}

	const char* getControllerAxisName(Axis axis)
	{
		return s_controllerAxisNames ? s_controllerAxisNames[axis] : "";
	}

	const char* getControllButtonName(Button button)
	{
		return s_controllerButtonNames ? s_controllerButtonNames[button] : "";
	}

	const char* getMouseAxisName(MouseAxis axis)
	{
		return s_mouseAxisNames ? s_mouseAxisNames[axis] : "";
	}

	const char* getMouseButtonName(MouseButton button)
	{
		return s_mouseButtonNames ? s_mouseButtonNames[button] : "";
	}

	const char* getKeyboardName(KeyboardCode key)
	{
		if (key > KEY_LAST && s_controllerAxisNames)
		{
			return s_keyboardNames[KEY_LAST];
		}

		return s_keyboardNames ? s_keyboardNames[key] : "";
	}
}
