// main.cpp - application entry point

// Implementation-specific headers
#include "cst/winapi/SingleInstanceGuard/SingleInstanceGuard.hpp"
#include "MainWindow.h"



// Application entry point
int WINAPI wWinMain(
	_In_ HINSTANCE     hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ PWSTR         pCmdLine,
	_In_ int           nCmdShow )
{
	cst::winapi::SingleInstanceGuard exists{ MainWindow::ClassName };
	if (exists) { return 0; }

	return MainWindow{}.loop();
}


