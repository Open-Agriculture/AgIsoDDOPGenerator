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
#include "isobus/isobus/isobus_language_command_interface.hpp"

#include <memory>
#include <string>
#include <vector>

class DDOPGeneratorGUI
{
public:
	DDOPGeneratorGUI() = default;

	void start();

private:
	static constexpr std::size_t FILE_PATH_BUFFER_MAX_LENGTH = 1024;

	bool render_menu_bar();
	void render_open_file_menu();
	void parseElementChildrenOfElement(std::uint16_t objectID);
	void parseChildren(std::shared_ptr<isobus::task_controller_object::DeviceElementObject> element);
	void render_object_tree();
	void render_device_settings(std::shared_ptr<isobus::task_controller_object::DeviceObject> object);
	void render_device_element_settings(std::shared_ptr<isobus::task_controller_object::DeviceElementObject> object);
	void render_device_process_data_settings(std::shared_ptr<isobus::task_controller_object::DeviceProcessDataObject> object);
	void render_device_property_settings(std::shared_ptr<isobus::task_controller_object::DevicePropertyObject> object);
	void render_device_presentation_settings(std::shared_ptr<isobus::task_controller_object::DeviceValuePresentationObject> object);
	void render_object_components(std::shared_ptr<isobus::task_controller_object::Object> object);
	void render_current_selected_object_settings(std::shared_ptr<isobus::task_controller_object::Object> object);
	void render_device_element_components(std::shared_ptr<isobus::task_controller_object::DeviceElementObject> object);
	void render_device_process_data_components(std::shared_ptr<isobus::task_controller_object::DeviceProcessDataObject> object);
	void render_device_property_components(std::shared_ptr<isobus::task_controller_object::DevicePropertyObject> object);
	void render_device_presentation_components(std::shared_ptr<isobus::task_controller_object::DeviceValuePresentationObject> object);
	void render_save();
	void render_all_objects();
	void on_selected_object_changed(std::shared_ptr<isobus::task_controller_object::Object> newObject);
	static std::string get_element_type_string(isobus::task_controller_object::DeviceElementObject::Type type);
	static std::string get_object_type_string(isobus::task_controller_object::ObjectTypes type);
	const std::array<std::uint8_t, 7> generate_localization_label();
	std::uint16_t get_first_unused_id() const;

	std::string languageCode;
	isobus::LanguageCommandInterface::DecimalSymbols decimalSymbol = isobus::LanguageCommandInterface::DecimalSymbols::Point;
	isobus::LanguageCommandInterface::TimeFormats timeFormat = isobus::LanguageCommandInterface::TimeFormats::TwelveHourAmPm;
	isobus::LanguageCommandInterface::DateFormats dateFormat = isobus::LanguageCommandInterface::DateFormats::mmddyyyy;
	isobus::LanguageCommandInterface::DistanceUnits distanceUnitSystem = isobus::LanguageCommandInterface::DistanceUnits::Metric;
	isobus::LanguageCommandInterface::AreaUnits areaUnitSystem = isobus::LanguageCommandInterface::AreaUnits::Metric;
	isobus::LanguageCommandInterface::VolumeUnits volumeUnitSystem = isobus::LanguageCommandInterface::VolumeUnits::Metric;
	isobus::LanguageCommandInterface::MassUnits massUnitSystem = isobus::LanguageCommandInterface::MassUnits::Metric;
	isobus::LanguageCommandInterface::TemperatureUnits temperatureUnitSystem = isobus::LanguageCommandInterface::TemperatureUnits::Metric;
	isobus::LanguageCommandInterface::PressureUnits pressureUnitSystem = isobus::LanguageCommandInterface::PressureUnits::Metric;
	isobus::LanguageCommandInterface::ForceUnits forceUnitSystem = isobus::LanguageCommandInterface::ForceUnits::Metric;
	isobus::LanguageCommandInterface::UnitSystem genericUnitSystem = isobus::LanguageCommandInterface::UnitSystem::Metric;

	std::unique_ptr<isobus::DeviceDescriptorObjectPool> currentObjectPool;
	std::vector<std::uint8_t> loadedIopData;
	char filePathBuffer[FILE_PATH_BUFFER_MAX_LENGTH] = { 0 };
	char designatorBuffer[129] = { 0 };
	char softwareVersionBuffer[129] = { 0 };
	char serialNumberBuffer[129] = { 0 };
	char structureLabelBuffer[8] = { 0 };
	char extendedStructureLabelBuffer[129] = { 0 };
	char hexIsoNameBuffer[17] = { 0 };
	char languageCodeBuffer[3] = { 0 };
	std::string lastFileName;
	int elementNumberBuffer = 0;
	int parentObjectBuffer = 0;
	int ddiBuffer = 0;
	int objectIDBuffer = 0;
	int presentationObjectBuffer = 0;
	int valueBuffer = 0;
	int numberDecimalsBuffer = 0;
	int offsetBuffer = 0;
	int versionIndex = 0;
	int addChildComboIndex = 0;
	float scaleBuffer = 0.0f;
	std::uint16_t selectedObjectID = 0xFFFF;
	std::array<bool, 8> propertiesBitfieldBuffer = { false };
	std::array<bool, 8> triggerBitfieldBuffer = { false };
	bool openFileDialogue = false;
	bool saveModal = false;
	bool saveAsModal = false;
	bool currentPoolValid = false;
};

#endif // GUI_HPP
