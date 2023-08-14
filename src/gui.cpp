//================================================================================================
/// @file gui.cpp
///
/// @brief The main file for running the application's GUI
/// @author Adrian Del Grosso
///
/// @copyright 2023 Adrian Del Grosso and the Open-Agriculture developers
//================================================================================================
#include "SDL.h"
#include "SDL_opengl.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include "gui.hpp"
#include "L2DFileDialog.hpp"
#include "isobus/utility/iop_file_interface.hpp"
#include "logsink.hpp"

#include <cstdio>

void DDOPGeneratorGUI::start()
{
	isobus::CANStackLogger::set_can_stack_logger_sink(&logger);

	// Setup SDL
	if (0 != SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER))
	{
		printf("Error: %s\n", SDL_GetError());
		return;
	}

	// Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
	// GL ES 2.0 + GLSL 100
	const char *glsl_version = "#version 100";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
	// GL 3.2 Core + GLSL 150
	const char *glsl_version = "#version 150";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
	// GL 3.0 + GLSL 130
	const char *glsl_version = "#version 130";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

	// Create window with graphics context
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_WindowFlags lWindowFlags = static_cast<SDL_WindowFlags>(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	SDL_Window *lpWindow = SDL_CreateWindow("AgIsoStack DDOP Generator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, lWindowFlags);
	SDL_GLContext lpGLContext = SDL_GL_CreateContext(lpWindow);
	SDL_GL_MakeCurrent(lpWindow, lpGLContext);
	SDL_GL_SetSwapInterval(1); // Enable vsync

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &lIO = ImGui::GetIO();
	(void)lIO;
	lIO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	ImGuiStyle &lStyle = ImGui::GetStyle();

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForOpenGL(lpWindow, lpGLContext);
	ImGui_ImplOpenGL3_Init(glsl_version);

	// Load Fonts
	// - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
	// - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
	// - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
	// - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
	// - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
	// - Read 'docs/FONTS.md' for more instructions and details.
	// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
	//io.Fonts->AddFontDefault();
	//io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
	//ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
	//IM_ASSERT(font != NULL);

	// Our state
	ImVec4 lClearColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	// Main loop
	bool lShouldExit = false;
	while (false == lShouldExit)
	{
		// Poll and handle events (inputs, window resize, etc.)
		// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
		// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
		// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
		// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
		SDL_Event lEvent;
		while (0 != SDL_PollEvent(&lEvent))
		{
			ImGui_ImplSDL2_ProcessEvent(&lEvent);
			if (SDL_QUIT == lEvent.type)
			{
				lShouldExit = true;
			}
			if ((SDL_WINDOWEVENT == lEvent.type) &&
			    (SDL_WINDOWEVENT_CLOSE == lEvent.window.event) &&
			    (lEvent.window.windowID == SDL_GetWindowID(lpWindow)))
			{
				lShouldExit = true;
			}
		}

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		// GUI Main Code:
		render_menu_bar();
		render_open_file_menu();

		if (nullptr != currentObjectPool)
		{
			// A pool is being worked on
		}

		ImGui::ShowDemoWindow(); // For testing

		// Rendering
		ImGui::Render();
		glViewport(0, 0, static_cast<int>(lIO.DisplaySize.x), static_cast<int>(lIO.DisplaySize.y));
		glClearColor(lClearColor.x * lClearColor.w, lClearColor.y * lClearColor.w, lClearColor.z * lClearColor.w, lClearColor.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		SDL_GL_SwapWindow(lpWindow);
	}

	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_GL_DeleteContext(lpGLContext);
	SDL_DestroyWindow(lpWindow);
	SDL_Quit();
}

bool DDOPGeneratorGUI::render_menu_bar()
{
	bool retVal = false;

	if (true == ImGui::BeginMainMenuBar())
	{
		if (true == ImGui::BeginMenu("File"))
		{
			if (true == ImGui::MenuItem("Quit", "Exit gracefully"))
			{
				retVal = true;
			}
			if (true == ImGui::MenuItem("Open", "Load a DDOP from a file"))
			{
				FileDialog::file_dialog_open = true;
				openFileDialogue = true;			
			}
			if (true == ImGui::MenuItem("Save as", "Save current DDOP to a file"))
			{

			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
	return retVal;
}

void DDOPGeneratorGUI::render_open_file_menu()
{
	if (FileDialog::file_dialog_open)
	{
		FileDialog::ShowFileDialog(&FileDialog::file_dialog_open, filePathBuffer, FILE_PATH_BUFFER_MAX_LENGTH, FileDialog::FileDialogType::OpenFile);
	}
	else if (openFileDialogue)
	{
		std::string selectedFileToRead(filePathBuffer);
		memset(filePathBuffer, 0, FILE_PATH_BUFFER_MAX_LENGTH);
		openFileDialogue = false;

		if ((selectedFileToRead.substr(selectedFileToRead.length() - 4) == ".iop") ||
			  (selectedFileToRead.substr(selectedFileToRead.length() - 4) == ".IOP"))
		{
			loadedIopData = isobus::IOPFileInterface::read_iop_file(selectedFileToRead);

			if (!loadedIopData.empty())
			{
				logger.logHistory.clear();
				currentObjectPool.reset();
				currentObjectPool = std::make_unique<isobus::DeviceDescriptorObjectPool>();
				
				if (true == currentObjectPool->deserialize_binary_object_pool(loadedIopData, isobus::NAME(0)))
				{

				}
				else
				{

				}
			}
		}
		else
		{

		}
	}
}
