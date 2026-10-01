//================================================================================================
/// @file task_data_import.cpp
///
/// @brief Builds a DDOP from a device description in an ISOXML TASKDATA.XML
/// @author Sujan Dumaru
///
/// @copyright 2026 The Open-Agriculture developers
//================================================================================================

#include "task_data_import.hpp"
#include "isobus/isobus/can_constants.hpp"
#include "isobus/isobus/can_stack_logger.hpp"
#include "tinyxml2.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <locale>
#include <memory>
#include <sstream>

constexpr std::int64_t MAX_ELEMENT_NUMBER = 4095;
constexpr std::int64_t MAX_TRIGGER_METHODS = 31;
constexpr std::int64_t MAX_DECIMALS = 7;
constexpr std::size_t MAX_SOFTWARE_VERSION_LENGTH = std::numeric_limits<std::uint8_t>::max(); // The serialized length is one byte
constexpr std::uint8_t LONG_DESIGNATOR_TC_VERSION = 4;
constexpr std::size_t MAX_DESIGNATOR_CHARACTERS = 32;

static bool log_invalid_attribute(const tinyxml2::XMLElement &element, const char *attribute)
{
	const char *id = element.Attribute("A");
	const char *value = element.Attribute(attribute);
	LOG_ERROR("[DDOP]: TASKDATA %s \"%s\" has a missing or invalid %s attribute (value \"%.40s\").", element.Name(), (nullptr != id) ? id : "", attribute, (nullptr != value) ? value : "");
	return false;
}

static std::string read_text(const tinyxml2::XMLElement &element, const char *attribute)
{
	const char *text = element.Attribute(attribute);
	return (nullptr != text) ? text : "";
}

// The stack silently truncates an over-long designator. From TC version 4 the limit is 32 characters, up to 128 UTF-8 bytes.
static bool read_designator(const tinyxml2::XMLElement &element, const char *attribute, const isobus::DeviceDescriptorObjectPool &pool, std::string &text)
{
	text = read_text(element, attribute);
	const bool longDesignators = LONG_DESIGNATOR_TC_VERSION <= pool.get_task_controller_compatibility_level();
	const std::size_t maxBytes = longDesignators ? isobus::task_controller_object::Object::MAX_DESIGNATOR_LENGTH : isobus::task_controller_object::Object::MAX_DESIGNATOR_LEGACY_LENGTH;
	const std::size_t characters = static_cast<std::size_t>(std::count_if(text.begin(), text.end(), [](char byte) { return 0x80 != (static_cast<unsigned char>(byte) & 0xC0); }));

	return ((text.size() <= maxBytes) && (!longDesignators || (characters <= MAX_DESIGNATOR_CHARACTERS))) || log_invalid_attribute(element, attribute);
}

static bool read_hex(const tinyxml2::XMLElement &element, const char *attribute, std::size_t digits, std::uint64_t &value)
{
	const char *text = element.Attribute(attribute);
	const bool valid = (nullptr != text) && (digits == std::strlen(text)) &&
	  std::all_of(text, text + digits, [](char digit) { return 0 != std::isxdigit(static_cast<unsigned char>(digit)); });

	if (valid)
	{
		value = std::strtoull(text, nullptr, 16);
	}
	return valid || log_invalid_attribute(element, attribute);
}

// ISOXML writes a label last byte first, so the number's lowest byte is label byte 0.
static std::array<std::uint8_t, 7> to_label(std::uint64_t value)
{
	std::array<std::uint8_t, 7> label = {};

	for (std::size_t i = 0; i < label.size(); i++)
	{
		label[i] = static_cast<std::uint8_t>(value >> (8 * i));
	}
	return label;
}

// The classic locale keeps a decimal comma locale from misreading a scale such as 0.001.
template<typename T>
static bool parse_number(const char *text, T &value)
{
	std::istringstream stream((nullptr != text) ? text : "");
	stream.imbue(std::locale::classic());
	stream >> value;
	return (!stream.fail()) && stream.eof();
}

template<typename T>
static bool read_integer(const tinyxml2::XMLElement &element,
                         const char *attribute,
                         T &value,
                         std::int64_t minimum = std::numeric_limits<T>::min(),
                         std::int64_t maximum = std::numeric_limits<T>::max())
{
	std::int64_t number = 0;
	const bool valid = parse_number(element.Attribute(attribute), number) && (minimum <= number) && (number <= maximum);

	if (valid)
	{
		value = static_cast<T>(number);
	}
	return valid || log_invalid_attribute(element, attribute);
}

static bool import_object(const tinyxml2::XMLElement &element, isobus::DeviceDescriptorObjectPool &pool)
{
	const std::string name = element.Name();
	std::uint16_t objectID = 0;
	std::uint64_t ddi = 0;
	std::uint16_t presentationID = isobus::NULL_OBJECT_ID;
	bool success = true;

	if ("DET" == name)
	{
		std::uint8_t type = 0;
		std::uint16_t elementNumber = 0;
		std::uint16_t parentID = 0;
		std::string designator;

		success = read_integer(element, "B", objectID) &&
		  read_integer(element, "C", type, 1, 7) &&
		  read_integer(element, "E", elementNumber, 0, MAX_ELEMENT_NUMBER) &&
		  read_integer(element, "F", parentID) &&
		  read_designator(element, "D", pool, designator) &&
		  pool.add_device_element(designator, elementNumber, parentID, static_cast<isobus::task_controller_object::DeviceElementObject::Type>(type), objectID);

		for (auto reference = element.FirstChildElement("DOR"); success && (nullptr != reference); reference = reference->NextSiblingElement("DOR"))
		{
			std::uint16_t childID = 0;
			success = read_integer(*reference, "A", childID);

			if (success)
			{
				std::static_pointer_cast<isobus::task_controller_object::DeviceElementObject>(pool.get_object_by_id(objectID))->add_reference_to_child_object(childID);
			}
		}
	}
	else if ("DPD" == name)
	{
		std::uint8_t properties = 0;
		std::uint8_t triggers = 0;
		std::string designator;

		success = read_integer(element, "A", objectID) &&
		  read_hex(element, "B", 4, ddi) &&
		  read_integer(element, "C", properties) &&
		  read_integer(element, "D", triggers, 0, MAX_TRIGGER_METHODS) &&
		  ((nullptr == element.Attribute("F")) || read_integer(element, "F", presentationID)) &&
		  read_designator(element, "E", pool, designator) &&
		  pool.add_device_process_data(designator, static_cast<std::uint16_t>(ddi), presentationID, properties, triggers, objectID);
	}
	else if ("DPT" == name)
	{
		std::int32_t value = 0;
		std::string designator;

		success = read_integer(element, "A", objectID) &&
		  read_hex(element, "B", 4, ddi) &&
		  read_integer(element, "C", value) &&
		  ((nullptr == element.Attribute("E")) || read_integer(element, "E", presentationID)) &&
		  read_designator(element, "D", pool, designator) &&
		  pool.add_device_property(designator, value, static_cast<std::uint16_t>(ddi), presentationID, objectID);
	}
	else if ("DVP" == name)
	{
		std::int32_t offset = 0;
		float scale = 0.0f;
		std::uint8_t decimals = 0;
		std::string designator;

		success = read_integer(element, "A", objectID) &&
		  read_integer(element, "B", offset) &&
		  // Zero is allowed: the exporter rounds a scale below 1e-6 to 0.000000.
		  ((parse_number(element.Attribute("C"), scale) && (0.0f <= scale)) || log_invalid_attribute(element, "C")) &&
		  read_integer(element, "D", decimals, 0, MAX_DECIMALS) &&
		  read_designator(element, "E", pool, designator) &&
		  pool.add_device_value_presentation(designator, offset, scale, decimals, objectID);
	}
	return success;
}

static const tinyxml2::XMLElement *first_device(tinyxml2::XMLDocument &document, const std::string &taskData)
{
	const tinyxml2::XMLElement *device = nullptr;

	if (tinyxml2::XML_SUCCESS != document.Parse(taskData.data(), taskData.size()))
	{
		LOG_ERROR("[DDOP]: The file is not valid XML: %s", document.ErrorStr());
	}
	else if (nullptr == document.FirstChildElement("ISO11783_TaskData"))
	{
		LOG_ERROR("[DDOP]: The XML file has no ISO11783_TaskData root element.");
	}
	else
	{
		device = document.FirstChildElement("ISO11783_TaskData")->FirstChildElement("DVC");

		if (nullptr == device)
		{
			LOG_ERROR("[DDOP]: The TASKDATA.XML has no inline DVC element. Device descriptions in files referenced through XFR are not supported.");
		}
	}
	return device;
}

std::vector<std::string> list_task_data_devices(const std::string &taskData)
{
	tinyxml2::XMLDocument document;
	std::vector<std::string> labels;

	for (auto device = first_device(document, taskData); nullptr != device; device = device->NextSiblingElement("DVC"))
	{
		labels.push_back(read_text(*device, "A") + "  " + read_text(*device, "B"));
	}
	return labels;
}

bool import_task_data_device(const std::string &taskData, std::size_t deviceIndex, isobus::DeviceDescriptorObjectPool &pool)
{
	tinyxml2::XMLDocument document;
	const tinyxml2::XMLElement *device = first_device(document, taskData);

	for (std::size_t i = 0; (nullptr != device) && (i < deviceIndex); i++)
	{
		device = device->NextSiblingElement("DVC");

		if (nullptr == device)
		{
			LOG_ERROR("[DDOP]: The TASKDATA.XML has no DVC number %zu.", deviceIndex + 1);
		}
	}

	std::uint64_t isoName = 0;
	std::uint64_t structureLabel = 0;
	std::uint64_t localizationLabel = 0;
	std::string designator;
	std::string serialNumber;
	bool success = (nullptr != device) &&
	  ((read_text(*device, "C").size() <= MAX_SOFTWARE_VERSION_LENGTH) || log_invalid_attribute(*device, "C")) &&
	  read_hex(*device, "D", 16, isoName) &&
	  read_hex(*device, "F", 14, structureLabel) &&
	  read_hex(*device, "G", 14, localizationLabel) &&
	  read_designator(*device, "B", pool, designator) &&
	  read_designator(*device, "E", pool, serialNumber);

	if (success)
	{
		const std::array<std::uint8_t, 7> structureBytes = to_label(structureLabel);
		success = pool.add_device(designator, read_text(*device, "C"), serialNumber, std::string(structureBytes.begin(), structureBytes.end()), to_label(localizationLabel), {}, isoName);
	}

	for (auto child = success ? device->FirstChildElement() : nullptr; success && (nullptr != child); child = child->NextSiblingElement())
	{
		success = import_object(*child, pool);
	}
	return success;
}
