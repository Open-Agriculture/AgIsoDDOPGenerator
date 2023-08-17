//================================================================================================
/// @file gui.cpp
///
/// @brief The main file for running the application's GUI
/// @author Adrian Del Grosso
///
/// @copyright 2023 Adrian Del Grosso and the Open-Agriculture developers
//================================================================================================
#include "gui.hpp"
#include "L2DFileDialog.hpp"
#include "SDL.h"
#include "SDL_opengl.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"
#include "isobus/utility/iop_file_interface.hpp"
#include "logsink.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>

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
	bool shouldExit = false;
	while (false == shouldExit)
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
				shouldExit = true;
			}
			if ((SDL_WINDOWEVENT == lEvent.type) &&
			    (SDL_WINDOWEVENT_CLOSE == lEvent.window.event) &&
			    (lEvent.window.windowID == SDL_GetWindowID(lpWindow)))
			{
				shouldExit = true;
			}
		}

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		// GUI Main Code:
		bool prevSaveAsModalState = saveAsModal;
		shouldExit = render_menu_bar();
		render_open_file_menu();

		if ((saveAsModal != prevSaveAsModalState) && saveAsModal)
		{
			ImGui::OpenPopup("##Save As Modal");
		}

		render_save();

		if ((nullptr != currentObjectPool) && currentPoolValid)
		{
			// A pool is being worked on
			ImGui::SetNextWindowSize({ lIO.DisplaySize.x, lIO.DisplaySize.y - 20 });
			ImGui::SetNextWindowPos({ 0, 18 });
			ImGui::Begin("DDOP", NULL, ImGuiWindowFlags_NoCollapse);

			// Tree child windows
			{
				ImGui::BeginChild("ChildL", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, ImGui::GetContentRegionAvail().y), false);
				ImGui::SeparatorText("Object Tree");
				render_object_tree();
				ImGui::EndChild();

				ImGui::SameLine();

				ImGui::BeginChild("ChildR", ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y), false);
				if (0xFFFF != selectedObjectID)
				{
					ImGui::SeparatorText("Edit Selected Object");
					auto selectedObject = currentObjectPool->get_object_by_id(selectedObjectID);
					if (nullptr != selectedObject)
					{
						ImGui::Text("Object Type: ");
						ImGui::SameLine();
						ImGui::Text((get_object_type_string(selectedObject->get_object_type()) + " (" + selectedObject->get_table_id() + ") ").c_str());
						render_current_selected_object_settings(selectedObject);
					}
				}
				ImGui::EndChild();
			}

			ImGui::End();
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

			if (!currentPoolValid)
			{
				ImGui::BeginDisabled();
			}
			if (true == ImGui::MenuItem("Save as", "Save current DDOP to a file"))
			{
				FileDialog::file_dialog_open = false;
				saveAsModal = true;
				memset(filePathBuffer, 0, IM_ARRAYSIZE(filePathBuffer));
			}
			if (true == ImGui::MenuItem("Close", "Closes the active file"))
			{
				currentObjectPool.reset();
				currentPoolValid = false;
			}
			else if (!currentPoolValid)
			{
				ImGui::EndDisabled();
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

		if (!selectedFileToRead.empty() &&
		      (selectedFileToRead.substr(selectedFileToRead.length() - 4) == ".iop") ||
		    (selectedFileToRead.substr(selectedFileToRead.length() - 4) == ".IOP"))
		{
			loadedIopData = isobus::IOPFileInterface::read_iop_file(selectedFileToRead);

			if (!loadedIopData.empty())
			{
				selectedObjectID = 0xFFFF;
				logger.logHistory.clear();
				currentObjectPool.reset();
				currentObjectPool = std::make_unique<isobus::DeviceDescriptorObjectPool>();

				if (0 == FileDialog::versions_current_idx)
				{
					currentObjectPool->set_task_controller_compatibility_level(3);
				}
				else
				{
					currentObjectPool->set_task_controller_compatibility_level(4);
				}

				if (true == currentObjectPool->deserialize_binary_object_pool(loadedIopData, isobus::NAME(0)))
				{
					// Valid pool?
					currentPoolValid = true;
				}
				else
				{
					currentObjectPool.reset();

					ImGui::OpenPopup("Error Loading DDOP");
				}
			}
		}
		else
		{
			// No valid pool selected
		}
	}

	if (ImGui::BeginPopupModal("Error Loading DDOP", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("There were errors loading the DDOP. Make sure you selected the correct TC version.");
		ImGui::Separator();

		for (auto &logString : logger.logHistory)
		{
			ImGui::Text(logString.logText.c_str());
		}

		ImGui::SetItemDefaultFocus();
		if (ImGui::Button("OK", ImVec2(120, 0)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void DDOPGeneratorGUI::parseElementChildrenOfElement(std::uint16_t aObjectID)
{
	// Search device elements for all elements that refer to aObjectID their parent
	for (std::uint32_t k = 0; k < currentObjectPool->size(); k++)
	{
		auto currentObject = currentObjectPool->get_object_by_index(k);

		if ((isobus::task_controller_object::ObjectTypes::DeviceElement == currentObject->get_object_type()) &&
		    (nullptr != std::dynamic_pointer_cast<isobus::task_controller_object::DeviceElementObject>(currentObject)) &&
		    (std::dynamic_pointer_cast<isobus::task_controller_object::DeviceElementObject>(currentObject)->get_parent_object() == aObjectID))
		{
			auto currentElement = std::dynamic_pointer_cast<isobus::task_controller_object::DeviceElementObject>(currentObject);
			std::string elementType = get_element_type_string(currentElement->get_type());

			ImGuiTreeNodeFlags rootElementFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
			if (selectedObjectID == currentElement->get_object_id())
			{
				rootElementFlags |= ImGuiTreeNodeFlags_Selected;
			}

			ImGui::Indent();
			bool isElementOpen = ImGui::TreeNodeEx((currentElement->get_designator() + " (" + currentElement->get_table_id() + " " + std::to_string(currentElement->get_object_id()) + ")").c_str(), rootElementFlags);
			ImGui::Unindent();

			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
			{
				selectedObjectID = currentElement->get_object_id();
				on_selected_object_changed(currentElement);
			}

			if (isElementOpen)
			{
				ImGui::Text(("Element Number: " + std::to_string(currentElement->get_element_number())).c_str());
				ImGui::Text(("Type: " + elementType).c_str());

				parseChildren(currentElement);
				ImGui::TreePop();

				ImGui::Indent();
				parseElementChildrenOfElement(currentElement->get_object_id());
				ImGui::Unindent();
			}
		}
	}
}

void DDOPGeneratorGUI::parseChildren(std::shared_ptr<isobus::task_controller_object::DeviceElementObject> element)
{
	for (std::uint32_t c = 0; c < element->get_number_child_objects(); c++)
	{
		ImGuiTreeNodeFlags childFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
		auto currentChild = currentObjectPool->get_object_by_id(element->get_child_object_id(c));

		if (selectedObjectID == currentChild->get_object_id())
		{
			childFlags |= ImGuiTreeNodeFlags_Selected;
		}

		bool isChildOpen = false;

		if (currentChild->get_object_type() != isobus::task_controller_object::ObjectTypes::DeviceElement)
		{
			ImGui::Indent();
			isChildOpen = ImGui::TreeNodeEx((currentChild->get_designator() + " (" + currentChild->get_table_id() + " " + std::to_string(currentChild->get_object_id()) + ")").c_str(), childFlags);
			ImGui::Unindent();

			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
			{
				selectedObjectID = currentChild->get_object_id();
				on_selected_object_changed(currentChild);
			}
		}

		if (isChildOpen)
		{
			if (isobus::task_controller_object::ObjectTypes::DeviceProcessData == currentChild->get_object_type())
			{
				auto currentDPD = std::dynamic_pointer_cast<isobus::task_controller_object::DeviceProcessDataObject>(currentChild);

				ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "DDI: %u", currentDPD->get_ddi());

				bool areAnyTriggers = false;
				ImGui::Text("Triggers:");
				ImGui::Indent();
				for (std::uint8_t t = 0; t < 5; t++)
				{
					if (0 != ((1 << t) & currentDPD->get_trigger_methods_bitfield()))
					{
						switch (static_cast<isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods>(1 << t))
						{
							case isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::DistanceInterval:
							{
								ImGui::BulletText("Distance Interval");
								areAnyTriggers = true;
							}
							break;

							case isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::OnChange:
							{
								ImGui::BulletText("On Change");
								areAnyTriggers = true;
							}
							break;

							case isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::ThresholdLimits:
							{
								ImGui::BulletText("Threshold Limits");
								areAnyTriggers = true;
							}
							break;

							case isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::TimeInterval:
							{
								ImGui::BulletText("Time Interval");
								areAnyTriggers = true;
							}
							break;

							case isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::Total:
							{
								ImGui::BulletText("Total");
								areAnyTriggers = true;
							}
							break;

							default:
							{
								ImGui::BulletText("Proprietary or Reserved");
								areAnyTriggers = true;
							}
							break;
						}

						if (!areAnyTriggers)
						{
							ImGui::BulletText("None");
						}
					}
				}
				ImGui::Unindent();

				bool areAnyProperties = false;
				ImGui::Text("Properties:");
				ImGui::Indent();
				for (std::uint8_t t = 0; t < 3; t++)
				{
					if (0 != ((1 << t) & currentDPD->get_properties_bitfield()))
					{
						switch (static_cast<isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit>(1 << t))
						{
							case isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::Settable:
							{
								ImGui::BulletText("Settable");
								areAnyProperties = true;
							}
							break;

							case isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::MemberOfDefaultSet:
							{
								ImGui::BulletText("Member of Default Set");
								areAnyProperties = true;
							}
							break;

							case isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::ControlSource:
							{
								ImGui::BulletText("Control Source");
								areAnyProperties = true;
							}
							break;

							default:
							{
								ImGui::BulletText("Proprietary or Reserved");
								areAnyProperties = true;
							}
							break;
						}

						if (!areAnyProperties)
						{
							ImGui::BulletText("None");
						}
					}
				}
				ImGui::Unindent();

				// Try and get the presentation
				if (0xFFFF != currentDPD->get_device_value_presentation_object_id())
				{
					auto lDVP = std::dynamic_pointer_cast<isobus::task_controller_object::DeviceValuePresentationObject>(currentObjectPool->get_object_by_id(currentDPD->get_device_value_presentation_object_id()));

					if ((nullptr != lDVP) &&
					    (ImGui::TreeNode(("Presentation: " + lDVP->get_designator() + " (" + lDVP->get_table_id() + " " + std::to_string(lDVP->get_object_id()) + ")").c_str())))

					{
						ImGui::Text("Number of Decimals: %u", lDVP->get_number_of_decimals());
						ImGui::Text("Offset: %d", lDVP->get_offset());
						ImGui::Text("Scale: %f", lDVP->get_scale());
						ImGui::TreePop();
					}
				}
			}
			else if (isobus::task_controller_object::ObjectTypes::DeviceProperty == currentChild->get_object_type())
			{
				auto lpDPT = std::dynamic_pointer_cast<isobus::task_controller_object::DevicePropertyObject>(currentChild);

				ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "DDI: %u", lpDPT->get_ddi());

				ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Value: %d", lpDPT->get_value());

				// Try and get the presentation
				if (0xFFFF != lpDPT->get_device_value_presentation_object_id())
				{
					auto lDVP = std::dynamic_pointer_cast<isobus::task_controller_object::DeviceValuePresentationObject>(currentObjectPool->get_object_by_id(lpDPT->get_device_value_presentation_object_id()));

					if ((nullptr != lDVP) &&
					    (ImGui::TreeNode(("Presentation: " + lDVP->get_designator() + " (" + lDVP->get_table_id() + " " + std::to_string(lDVP->get_object_id()) + ")").c_str())))

					{
						ImGui::Text("Number of Decimals: %u", lDVP->get_number_of_decimals());
						ImGui::Text("Offset: %d", lDVP->get_offset());
						ImGui::Text("Scale: %f", lDVP->get_scale());
						ImGui::TreePop();
					}
				}
			}
			ImGui::TreePop();
		}
	}
}

void DDOPGeneratorGUI::render_object_tree()
{
	for (std::uint32_t j = 0; j < currentObjectPool->size(); j++)
	{
		auto lpObject = currentObjectPool->get_object_by_index(j);

		if (nullptr != lpObject)
		{
			if (isobus::task_controller_object::ObjectTypes::Device == lpObject->get_object_type())
			{
				ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;

				if (selectedObjectID == lpObject->get_object_id())
				{
					base_flags |= ImGuiTreeNodeFlags_Selected;
				}

				bool isOpen = ImGui::TreeNodeEx((lpObject->get_designator() + "(" + lpObject->get_table_id() + " " + std::to_string(lpObject->get_object_id()) + ")").c_str(), base_flags);

				if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
				{
					selectedObjectID = lpObject->get_object_id();
					on_selected_object_changed(lpObject);
				}

				if (isOpen)
				{
					ImGui::Text(("Serial Number: " + std::dynamic_pointer_cast<isobus::task_controller_object::DeviceObject>(lpObject)->get_serial_number()).c_str());

					// Search for all elements with the device object as their parent and render recursively
					// Search device elements for all elements that refer to aElementNumbers their parent
					for (std::uint32_t k = 0; k < currentObjectPool->size(); k++)
					{
						auto currentObject = currentObjectPool->get_object_by_index(k);

						if ((isobus::task_controller_object::ObjectTypes::DeviceElement == currentObject->get_object_type()) &&
						    (nullptr != std::dynamic_pointer_cast<isobus::task_controller_object::DeviceElementObject>(currentObject)) &&
						    (std::dynamic_pointer_cast<isobus::task_controller_object::DeviceElementObject>(currentObject)->get_parent_object() == lpObject->get_object_id()))
						{
							parseElementChildrenOfElement(lpObject->get_object_id());
							parseChildren(std::dynamic_pointer_cast<isobus::task_controller_object::DeviceElementObject>(currentObject));
						}
					}

					ImGui::TreePop();
					break;
				}
				break;
			}
		}
	}
}

void DDOPGeneratorGUI::render_device_settings(std::shared_ptr<isobus::task_controller_object::DeviceObject> object)
{
	ImGui::InputText("Designator", designatorBuffer, IM_ARRAYSIZE(designatorBuffer));

	auto designator = std::string(designatorBuffer);
	if (designator != object->get_designator())
	{
		object->set_designator(designator);
	}

	ImGui::InputText("Software Version", softwareVersionBuffer, IM_ARRAYSIZE(softwareVersionBuffer));

	auto version = std::string(softwareVersionBuffer);
	if (version != object->get_software_version())
	{
		object->set_software_version(version);
	}

	ImGui::InputText("Serial Number", serialNumberBuffer, IM_ARRAYSIZE(serialNumberBuffer));

	auto serial = std::string(serialNumberBuffer);
	if (serial != object->get_serial_number())
	{
		object->set_serial_number(serial);
	}

	ImGui::InputText("Structure Label", structureLabelBuffer, IM_ARRAYSIZE(structureLabelBuffer));

	auto structureLabel = std::string(structureLabelBuffer);
	if (structureLabel != object->get_structure_label())
	{
		object->set_structure_label(structureLabel);
	}

	ImGui::InputText("Extended Structure Label", extendedStructureLabelBuffer, IM_ARRAYSIZE(extendedStructureLabelBuffer));

	auto extendedStructureLabel = std::string(extendedStructureLabelBuffer);
	if (extendedStructureLabel != object->get_structure_label())
	{
		std::vector<std::uint8_t> convertedLabel(extendedStructureLabel.begin(), extendedStructureLabel.end());
		object->set_extended_structure_label(convertedLabel);
	}

	ImGui::InputText("ISO NAME (hex)", hexIsoNameBuffer, IM_ARRAYSIZE(hexIsoNameBuffer));

	char **nameEnd = nullptr;
	std::string tempNAME(hexIsoNameBuffer);
	auto integerISONAME = strtoull(tempNAME.c_str(), nameEnd, 16);

	if (integerISONAME != object->get_iso_name())
	{
		object->set_iso_name(integerISONAME);
	}

	ImGui::SeparatorText("Localization Label");

	{
		const char *strings[] = { "Comma", "Decimal", "Reserved", "N/A" };
		if (ImGui::BeginListBox("Time Format", { 110, 100 }))
		{
			for (int i = 0; i < IM_ARRAYSIZE(strings); i++)
			{
				const bool is_selected = (static_cast<std::uint8_t>(timeFormat) == i);
				if (ImGui::Selectable(strings[i], is_selected))
				{
					timeFormat = static_cast<isobus::LanguageCommandInterface::TimeFormats>(i);
				}

				if (is_selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndListBox();
		}
	}

	{
		const char *strings[] = { "ddmmyyyy", "ddyyyymm", "mmyyyydd", "mmddyyyy", "yyyymmdd", "yyyyddmm" };
		ImGui::SameLine();
		if (ImGui::BeginListBox("Date Format", { 110, 100 }))
		{
			for (int i = 0; i < IM_ARRAYSIZE(strings); i++)
			{
				const bool is_selected = (static_cast<std::uint8_t>(dateFormat) == i);
				if (ImGui::Selectable(strings[i], is_selected))
				{
					dateFormat = static_cast<isobus::LanguageCommandInterface::DateFormats>(i);
				}

				if (is_selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndListBox();
		}
	}

	{
		const char *strings[] = { "Metric", "Imperial/US", "Reserved", "N/A" };
		ImGui::SameLine();
		if (ImGui::BeginListBox("Distance Units", { 110, 100 }))
		{
			for (int i = 0; i < IM_ARRAYSIZE(strings); i++)
			{
				const bool is_selected = (static_cast<std::uint8_t>(distanceUnitSystem) == i);
				if (ImGui::Selectable(strings[i], is_selected))
				{
					distanceUnitSystem = static_cast<isobus::LanguageCommandInterface::DistanceUnits>(i);
				}

				if (is_selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndListBox();
		}
	}

	{
		const char *strings[] = { "Metric", "Imperial/US", "Reserved", "N/A" };
		if (ImGui::BeginListBox("Area Units", { 110, 100 }))
		{
			for (int i = 0; i < IM_ARRAYSIZE(strings); i++)
			{
				const bool is_selected = (static_cast<std::uint8_t>(areaUnitSystem) == i);
				if (ImGui::Selectable(strings[i], is_selected))
				{
					areaUnitSystem = static_cast<isobus::LanguageCommandInterface::AreaUnits>(i);
				}

				if (is_selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndListBox();
		}
	}

	{
		const char *strings[] = { "Metric", "Imperial", "US", "N/A" };
		ImGui::SameLine();
		if (ImGui::BeginListBox("Volume Units", { 110, 100 }))
		{
			for (int i = 0; i < IM_ARRAYSIZE(strings); i++)
			{
				const bool is_selected = (static_cast<std::uint8_t>(volumeUnitSystem) == i);
				if (ImGui::Selectable(strings[i], is_selected))
				{
					volumeUnitSystem = static_cast<isobus::LanguageCommandInterface::VolumeUnits>(i);
				}

				if (is_selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndListBox();
		}
	}

	{
		const char *strings[] = { "Metric", "Imperial", "US", "N/A" };
		ImGui::SameLine();
		if (ImGui::BeginListBox("Mass Units", { 110, 100 }))
		{
			for (int i = 0; i < IM_ARRAYSIZE(strings); i++)
			{
				const bool is_selected = (static_cast<std::uint8_t>(massUnitSystem) == i);
				if (ImGui::Selectable(strings[i], is_selected))
				{
					massUnitSystem = static_cast<isobus::LanguageCommandInterface::MassUnits>(i);
				}

				if (is_selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndListBox();
		}
	}

	{
		const char *strings[] = { "Metric", "Imperial/US", "Reserved", "N/A" };
		if (ImGui::BeginListBox("Force Units", { 110, 100 }))
		{
			for (int i = 0; i < IM_ARRAYSIZE(strings); i++)
			{
				const bool is_selected = (static_cast<std::uint8_t>(forceUnitSystem) == i);
				if (ImGui::Selectable(strings[i], is_selected))
				{
					forceUnitSystem = static_cast<isobus::LanguageCommandInterface::ForceUnits>(i);
				}

				if (is_selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndListBox();
		}
	}

	{
		const char *strings[] = { "Metric", "Imperial/US", "Reserved", "N/A" };
		ImGui::SameLine();
		if (ImGui::BeginListBox("Temperature Units", { 110, 100 }))
		{
			for (int i = 0; i < IM_ARRAYSIZE(strings); i++)
			{
				const bool is_selected = (static_cast<std::uint8_t>(temperatureUnitSystem) == i);
				if (ImGui::Selectable(strings[i], is_selected))
				{
					temperatureUnitSystem = static_cast<isobus::LanguageCommandInterface::TemperatureUnits>(i);
				}

				if (is_selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndListBox();
		}
	}

	{
		const char *strings[] = { "Metric", "Imperial", "US", "N/A" };
		if (ImGui::BeginListBox("Generic Units", { 110, 100 }))
		{
			for (int i = 0; i < IM_ARRAYSIZE(strings); i++)
			{
				const bool is_selected = (static_cast<std::uint8_t>(genericUnitSystem) == i);
				if (ImGui::Selectable(strings[i], is_selected))
				{
					genericUnitSystem = static_cast<isobus::LanguageCommandInterface::UnitSystem>(i);
				}

				if (is_selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndListBox();
		}
	}

	{
		const char *strings[] = { "Metric", "Imperial/US", "Reserved", "N/A" };
		ImGui::SameLine();
		if (ImGui::BeginListBox("Pressure Units", { 110, 100 }))
		{
			for (int i = 0; i < IM_ARRAYSIZE(strings); i++)
			{
				const bool is_selected = (static_cast<std::uint8_t>(pressureUnitSystem) == i);
				if (ImGui::Selectable(strings[i], is_selected))
				{
					pressureUnitSystem = static_cast<isobus::LanguageCommandInterface::PressureUnits>(i);
				}

				if (is_selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndListBox();
		}
	}

	auto localizationData = generate_localization_label();
	auto currentLocalization = object->get_localization_label();

	if (localizationData != currentLocalization)
	{
		object->set_localization_label(localizationData);
	}
}

void DDOPGeneratorGUI::render_device_element_settings(std::shared_ptr<isobus::task_controller_object::DeviceElementObject> object)
{
	ImGui::InputText("Designator", designatorBuffer, IM_ARRAYSIZE(designatorBuffer));

	auto designator = std::string(designatorBuffer);
	if (designator != object->get_designator())
	{
		object->set_designator(designator);
	}

	ImGui::InputInt("Element Number", &elementNumberBuffer);

	if (elementNumberBuffer > 4095)
	{
		// 12 bits is the max element
		elementNumberBuffer = 4095;
	}

	if (object->get_element_number() != elementNumberBuffer)
	{
		object->set_element_number(elementNumberBuffer);
	}

	ImGui::InputInt("Parent Object ID", &parentObjectBuffer);

	if (object->get_parent_object() > 0xFFFF)
	{
		parentObjectBuffer = 0xFFFF;
	}

	if (parentObjectBuffer != object->get_parent_object())
	{
		object->set_parent_object(parentObjectBuffer);
	}

	auto parent = currentObjectPool->get_object_by_id(parentObjectBuffer);
	if (nullptr != parent)
	{
		std::string designator = "Parent's designator is \"" + parent->get_designator() + "\"";
		ImGui::Text(designator.c_str());
	}
}

void DDOPGeneratorGUI::render_device_process_data_settings(std::shared_ptr<isobus::task_controller_object::DeviceProcessDataObject> object)
{
	ImGui::InputText("Designator", designatorBuffer, IM_ARRAYSIZE(designatorBuffer));

	auto designator = std::string(designatorBuffer);
	if (designator != object->get_designator())
	{
		object->set_designator(designator);
	}
}

void DDOPGeneratorGUI::render_device_property_settings(std::shared_ptr<isobus::task_controller_object::DevicePropertyObject> object)
{
	ImGui::InputText("Designator", designatorBuffer, IM_ARRAYSIZE(designatorBuffer));

	auto designator = std::string(designatorBuffer);
	if (designator != object->get_designator())
	{
		object->set_designator(designator);
	}
}

void DDOPGeneratorGUI::render_device_presentation_settings(std::shared_ptr<isobus::task_controller_object::DeviceValuePresentationObject> object)
{
	ImGui::InputText("Designator", designatorBuffer, IM_ARRAYSIZE(designatorBuffer));

	auto designator = std::string(designatorBuffer);
	if (designator != object->get_designator())
	{
		object->set_designator(designator);
	}
}

void DDOPGeneratorGUI::render_current_selected_object_settings(std::shared_ptr<isobus::task_controller_object::Object> object)
{
	if (nullptr != object)
	{
		switch (object->get_object_type())
		{
			case isobus::task_controller_object::ObjectTypes::Device:
			{
				render_device_settings(std::dynamic_pointer_cast<isobus::task_controller_object::DeviceObject>(object));
			}
			break;

			case isobus::task_controller_object::ObjectTypes::DeviceElement:
			{
				render_device_element_settings(std::dynamic_pointer_cast<isobus::task_controller_object::DeviceElementObject>(object));
			}
			break;

			case isobus::task_controller_object::ObjectTypes::DeviceProcessData:
			{
				render_device_process_data_settings(std::dynamic_pointer_cast<isobus::task_controller_object::DeviceProcessDataObject>(object));
			}
			break;

			case isobus::task_controller_object::ObjectTypes::DeviceProperty:
			{
				render_device_property_settings(std::dynamic_pointer_cast<isobus::task_controller_object::DevicePropertyObject>(object));
			}
			break;

			case isobus::task_controller_object::ObjectTypes::DeviceValuePresentation:
			{
				render_device_presentation_settings(std::dynamic_pointer_cast<isobus::task_controller_object::DeviceValuePresentationObject>(object));
			}
			break;
		}
	}
}

void DDOPGeneratorGUI::on_selected_object_changed(std::shared_ptr<isobus::task_controller_object::Object> newObject)
{
	memset(designatorBuffer, 0, sizeof(designatorBuffer));
	memcpy(designatorBuffer, newObject->get_designator().c_str(), newObject->get_designator().length() <= 128 ? newObject->get_designator().length() : 128);

	switch (newObject->get_object_type())
	{
		case isobus::task_controller_object::ObjectTypes::Device:
		{
			auto object = std::dynamic_pointer_cast<isobus::task_controller_object::DeviceObject>(newObject);
			memset(softwareVersionBuffer, 0, sizeof(softwareVersionBuffer));
			memset(serialNumberBuffer, 0, sizeof(serialNumberBuffer));
			memset(structureLabelBuffer, 0, sizeof(structureLabelBuffer));
			memset(hexIsoNameBuffer, 0, sizeof(hexIsoNameBuffer));
			memset(extendedStructureLabelBuffer, 0, sizeof(extendedStructureLabelBuffer));

			memcpy(softwareVersionBuffer, object->get_software_version().c_str(), object->get_software_version().length() <= 128 ? object->get_software_version().length() : 128);
			memcpy(serialNumberBuffer, object->get_serial_number().c_str(), object->get_serial_number().length() <= 128 ? object->get_serial_number().length() : 128);
			memcpy(structureLabelBuffer, object->get_structure_label().c_str(), object->get_structure_label().length() <= 7 ? object->get_structure_label().length() : 7);
			memcpy(extendedStructureLabelBuffer, object->get_extended_structure_label().data(), object->get_extended_structure_label().size() <= 128 ? object->get_extended_structure_label().size() : 128);

			std::ostringstream hexStream;
			hexStream << std::hex << object->get_iso_name();
			std::string hexNAME = hexStream.str();

			for (std::size_t i = 0; (i < sizeof(hexIsoNameBuffer) && i < hexNAME.length()); i++)
			{
				hexIsoNameBuffer[i] = hexNAME[i];
			}

			auto localization = object->get_localization_label();
			languageCode.clear();
			languageCode.push_back(localization.at(0));
			languageCode.push_back(localization.at(1));
			timeFormat = static_cast<isobus::LanguageCommandInterface::TimeFormats>((localization.at(2) >> 4) & 0x03);
			decimalSymbol = static_cast<isobus::LanguageCommandInterface::DecimalSymbols>((localization.at(2) >> 6) & 0x03);
			dateFormat = static_cast<isobus::LanguageCommandInterface::DateFormats>(localization.at(3));
			massUnitSystem = static_cast<isobus::LanguageCommandInterface::MassUnits>(localization.at(4) & 0x03);
			volumeUnitSystem = static_cast<isobus::LanguageCommandInterface::VolumeUnits>((localization.at(4) >> 2) & 0x03);
			areaUnitSystem = static_cast<isobus::LanguageCommandInterface::AreaUnits>((localization.at(4) >> 4) & 0x03);
			distanceUnitSystem = static_cast<isobus::LanguageCommandInterface::DistanceUnits>((localization.at(4) >> 6) & 0x03);
			genericUnitSystem = static_cast<isobus::LanguageCommandInterface::UnitSystem>(localization.at(5) & 0x03);
			forceUnitSystem = static_cast<isobus::LanguageCommandInterface::ForceUnits>((localization.at(5) >> 2) & 0x03);
			pressureUnitSystem = static_cast<isobus::LanguageCommandInterface::PressureUnits>((localization.at(5) >> 4) & 0x03);
			temperatureUnitSystem = static_cast<isobus::LanguageCommandInterface::TemperatureUnits>((localization.at(5) >> 6) & 0x03);
		}
		break;

		case isobus::task_controller_object::ObjectTypes::DeviceElement:
		{
			auto object = std::dynamic_pointer_cast<isobus::task_controller_object::DeviceElementObject>(newObject);
			elementNumberBuffer = object->get_element_number();
			parentObjectBuffer = object->get_parent_object();
		}
		break;
	}
}

std::string DDOPGeneratorGUI::get_element_type_string(isobus::task_controller_object::DeviceElementObject::Type type)
{
	std::string elementType = "Proprietary";

	switch (type)
	{
		case isobus::task_controller_object::DeviceElementObject::Type::Device:
		{
			elementType = "Device";
		}
		break;

		case isobus::task_controller_object::DeviceElementObject::Type::Bin:
		{
			elementType = "Bin";
		}
		break;

		case isobus::task_controller_object::DeviceElementObject::Type::Connector:
		{
			elementType = "Connector";
		}
		break;

		case isobus::task_controller_object::DeviceElementObject::Type::Function:
		{
			elementType = "Function";
		}
		break;

		case isobus::task_controller_object::DeviceElementObject::Type::NavigationReference:
		{
			elementType = "Navigation Reference";
		}
		break;

		case isobus::task_controller_object::DeviceElementObject::Type::Section:
		{
			elementType = "Section";
		}
		break;

		case isobus::task_controller_object::DeviceElementObject::Type::Unit:
		{
			elementType = "Unit";
		}
		break;

		default:
			break;
	}
	return elementType;
}

std::string DDOPGeneratorGUI::get_object_type_string(isobus::task_controller_object::ObjectTypes type)
{
	std::string retVal = "Unknown";

	switch (type)
	{
		case isobus::task_controller_object::ObjectTypes::Device:
		{
			retVal = "Device Object";
		}
		break;

		case isobus::task_controller_object::ObjectTypes::DeviceElement:
		{
			retVal = "Device Element Object";
		}
		break;

		case isobus::task_controller_object::ObjectTypes::DeviceProcessData:
		{
			retVal = "Device Process Data Object";
		}
		break;

		case isobus::task_controller_object::ObjectTypes::DeviceProperty:
		{
			retVal = "Device Property Object";
		}
		break;

		case isobus::task_controller_object::ObjectTypes::DeviceValuePresentation:
		{
			retVal = "Device Value Presentation Object";
		}
		break;

		default:
			break;
	}
	return retVal;
}

void DDOPGeneratorGUI::render_save()
{
	bool shouldShowSaveFailed = false;
	bool shouldShowSaveSucceeded = false;
	if (ImGui::BeginPopupModal("##Save As Modal", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Enter file name");
		ImGui::Separator();

		ImGui::InputText("File Name", filePathBuffer, IM_ARRAYSIZE(filePathBuffer));

		ImGui::SetItemDefaultFocus();
		if (ImGui::Button("Save", ImVec2(120, 0)))
		{
			ImGui::CloseCurrentPopup();

			if ((nullptr != currentObjectPool) && currentPoolValid)
			{
				std::vector<std::uint8_t> binaryDDOP;
				logger.logHistory.clear();
				auto serializationSuccess = currentObjectPool->generate_binary_object_pool(binaryDDOP);

				if (serializationSuccess)
				{
					auto fileName = std::string(filePathBuffer);

					if (fileName.empty())
					{
						fileName = "device_descriptor_object_pool.iop";
					}
					std::ofstream outFile(fileName, std::ios_base::trunc | std::ios_base::binary);

					if (outFile)
					{
						outFile.write(reinterpret_cast<char *>(binaryDDOP.data()), binaryDDOP.size());
						shouldShowSaveSucceeded = true;
					}
					else
					{
						shouldShowSaveFailed = true;
					}
				}
				else
				{
					shouldShowSaveFailed = true;
				}
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (shouldShowSaveFailed)
	{
		ImGui::OpenPopup("Save Failed");
	}
	else if (shouldShowSaveSucceeded)
	{
		ImGui::OpenPopup("Save Success");
	}

	if (ImGui::BeginPopupModal("Save Success", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("File was saved.");

		if (ImGui::Button("OK", ImVec2(120, 0)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal("Save Failed", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("File Saving Failed");
		ImGui::Separator();
		for (auto &logString : logger.logHistory)
		{
			ImGui::Text(logString.logText.c_str());
		}

		if (ImGui::Button("OK", ImVec2(120, 0)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

const std::array<std::uint8_t, 7> DDOPGeneratorGUI::generate_localization_label()
{
	std::array<std::uint8_t, 7> retVal = { 0 };

	if (languageCode.size() >= 2)
	{
		retVal[0] = languageCode[0];
		retVal[1] = languageCode[1];
	}
	else
	{
		retVal[0] = ' ';
		retVal[1] = ' ';
	}
	retVal[2] = ((static_cast<std::uint8_t>(timeFormat) << 4) |
	             (static_cast<std::uint8_t>(decimalSymbol) << 6));
	retVal[3] = static_cast<std::uint8_t>(dateFormat);
	retVal[4] = (static_cast<std::uint8_t>(massUnitSystem) |
	             (static_cast<std::uint8_t>(volumeUnitSystem) << 2) |
	             (static_cast<std::uint8_t>(areaUnitSystem) << 4) |
	             (static_cast<std::uint8_t>(distanceUnitSystem) << 6));
	retVal[5] = (static_cast<std::uint8_t>(genericUnitSystem) |
	             (static_cast<std::uint8_t>(forceUnitSystem) << 2) |
	             (static_cast<std::uint8_t>(pressureUnitSystem) << 4) |
	             (static_cast<std::uint8_t>(temperatureUnitSystem) << 6));
	retVal[6] = 0xFF;
	return retVal;
}
