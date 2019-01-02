#pragma once
#include <Windows.h>
#include <string>
#include <vector>

#include "imgui\imgui.h"
#include "imgui\DX9\imgui_impl_dx9.h"

struct IDirect3DDevice9;

namespace DankmemzMenu
{
	void GUI_Init(HWND, IDirect3DDevice9*);

	void openMenu();

	void selectionWindow();
	void mainWindow();

	extern int selection;
	extern bool d3dinit;
	extern ImFont* cheat_font;
	extern ImFont* title_font;
	extern ImFont* tab_icons;
}

extern bool pressedKey[256];