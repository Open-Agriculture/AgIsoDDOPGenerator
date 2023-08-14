//================================================================================================
/// @file main.cpp
///
/// @brief Implements main for the DDOP generator application
/// @author Adrian Del Grosso
///
/// @copyright 2023 Adrian Del Grosso and the Open-Agriculture developers
//================================================================================================
#include "gui.hpp"

// Hide console window on windows, todo: maybe there's a nicer way to do this
// from CMake that is cross-platform friendly
#ifdef WIN32
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif // WIN32

int main(int aArgCount, char *apArgValues[], char *[]) 
{
	DDOPGeneratorGUI GUI;

	GUI.start();

	return 0;
}
