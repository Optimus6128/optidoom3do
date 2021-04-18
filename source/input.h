#ifndef INPUT_H
#define INPUT_H

#include "types.h"

enum {
	JOY_BUTTON_UP, JOY_BUTTON_DOWN, JOY_BUTTON_LEFT, JOY_BUTTON_RIGHT,
	JOY_BUTTON_A, JOY_BUTTON_B, JOY_BUTTON_C,
	JOY_BUTTON_LPAD, JOY_BUTTON_RPAD,
	JOY_BUTTON_SELECT, JOY_BUTTON_START, JOY_BUTTONS_NUM
};

enum {
	MOUSE_BUTTON_LEFT, MOUSE_BUTTON_MIDDLE, MOUSE_BUTTON_RIGHT, MOUSE_BUTTONS_NUM
};

typedef struct MousePosition
{
	int32 x, y;
}MousePosition;


void initInput(void);
void updateInput(void);

bool isAnyJoyButtonPressed(void);
bool isJoyButtonPressed(int joyButtonId);
bool isJoyButtonPressedOnce(int joyButtonId);
bool isMouseButtonPressed(int mouseButtonId);
bool isMouseButtonPressedOnce(int mouseButtonId);

MousePosition getMousePosition(void);
MousePosition getMousePositionDiff(void);

#endif
