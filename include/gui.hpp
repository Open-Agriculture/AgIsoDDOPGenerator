//================================================================================================
/// @file gui.hpp
///
/// @brief Defines the main class for running the application's GUI
/// @author Adrian Del Grosso
///
/// @copyright 2023 Adrian Del Grosso and the Open-Agriculture developers
//================================================================================================
#ifndef GUI_HPP
#define GUI_HPP

#include "isobus/isobus/isobus_device_descriptor_object_pool.hpp"

#include <string>
#include <memory>
#include <vector>

class DDOPGeneratorGUI 
{
   public:
    DDOPGeneratorGUI() = default;

    void start();

    bool render_menu_bar();
    void render_open_file_menu();

    private:
	  static constexpr std::size_t FILE_PATH_BUFFER_MAX_LENGTH = 1024;

	  std::unique_ptr<isobus::DeviceDescriptorObjectPool> currentObjectPool;
	  std::vector<std::uint8_t> loadedIopData;
	  char filePathBuffer[FILE_PATH_BUFFER_MAX_LENGTH] = { 0 };
	  std::string lastFileName;
	  bool openFileDialogue = false;
};

#endif // GUI_HPP
