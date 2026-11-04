//================================================================================================
/// @file ddop_validator.cpp
///
/// @brief Implements the ISO 11783-10 conformance checks that "Check for Errors" runs on a DDOP
/// @author Sujan Dumaru
///
/// @copyright 2026 The Open-Agriculture developers
//================================================================================================
#include "ddop_validator.hpp"

#include "isobus/isobus/isobus_data_dictionary.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>

using Object = isobus::task_controller_object::Object;
using ObjectTypes = isobus::task_controller_object::ObjectTypes;
using DeviceObject = isobus::task_controller_object::DeviceObject;
using DeviceElementObject = isobus::task_controller_object::DeviceElementObject;
using DeviceProcessDataObject = isobus::task_controller_object::DeviceProcessDataObject;
using DevicePropertyObject = isobus::task_controller_object::DevicePropertyObject;
using DeviceValuePresentationObject = isobus::task_controller_object::DeviceValuePresentationObject;
using Severity = DDOPValidationFinding::Severity;

constexpr std::uint16_t NULL_OBJECT_ID = 0xFFFF;

constexpr std::uint8_t FIRST_VERSION_4_ONLY_FEATURE_LEVEL = 4;

// A process data message carries the element number in 12 bits
constexpr std::uint16_t MAX_ADDRESSABLE_ELEMENT_NUMBER = 4095;

// The length of a designator, serial number or software version is stored in a single byte
constexpr std::size_t MAX_LENGTH_PREFIXED_STRING_LENGTH = 255;

constexpr std::uint8_t RESERVED_PROPERTIES_BITS = 0xF8;
constexpr std::uint8_t RESERVED_TRIGGER_METHOD_BITS = 0xE0;
constexpr std::uint8_t MAX_NUMBER_OF_DECIMALS = 7;
constexpr float MIN_SCALE = 0.000000001f;
constexpr float MAX_SCALE = 100000000.0f;
constexpr std::size_t MAX_LABELLED_DESIGNATOR_LENGTH = 32;

// The version 4 designator's 128 byte ceiling is 32 four-byte characters, not a longer allowance
constexpr std::size_t MAX_DESIGNATOR_CHARACTERS = 32;

constexpr std::uint16_t REQUEST_DEFAULT_PROCESS_DATA_DDI = 0xDFFF;
constexpr std::uint8_t ALL_TRIGGER_METHODS = 0x1F;

template<typename T>
static std::vector<std::shared_ptr<T>> objects_of_type(isobus::DeviceDescriptorObjectPool &pool)
{
	std::vector<std::shared_ptr<T>> retVal;

	for (std::uint16_t i = 0; i < pool.size(); i++)
	{
		auto object = std::dynamic_pointer_cast<T>(pool.get_object_by_index(i));

		if (nullptr != object)
		{
			retVal.push_back(object);
		}
	}
	return retVal;
}

static std::string join_object_ids(const std::vector<std::uint16_t> &objectIDs)
{
	std::string retVal;

	for (const auto &objectID : objectIDs)
	{
		retVal += (retVal.empty() ? "" : ", ") + std::to_string(objectID);
	}
	return retVal;
}

static bool is_utf8_continuation_byte(char character)
{
	return 0x80 == (0xC0 & static_cast<std::uint8_t>(character));
}

static std::size_t utf8_length(const std::string &value)
{
	return static_cast<std::size_t>(std::count_if(value.begin(), value.end(), [](char character) { return !is_utf8_continuation_byte(character); }));
}

static std::string object_label(const std::shared_ptr<Object> &object)
{
	std::string retVal = object->get_table_id() + " " + std::to_string(object->get_object_id());
	std::string designator = object->get_designator();

	if (designator.size() > MAX_LABELLED_DESIGNATOR_LENGTH)
	{
		designator.resize(MAX_LABELLED_DESIGNATOR_LENGTH);

		// A UTF-8 sequence cut in half renders as a replacement glyph
		while (!designator.empty() && is_utf8_continuation_byte(designator.back()))
		{
			designator.pop_back();
		}
		designator += "...";
	}

	if (!designator.empty())
	{
		retVal += " (\"" + designator + "\")";
	}
	return retVal;
}

static void check_device_element_references(isobus::DeviceDescriptorObjectPool &pool, std::vector<DDOPValidationFinding> &findings)
{
	for (const auto &element : objects_of_type<DeviceElementObject>(pool))
	{
		if (NULL_OBJECT_ID == element->get_parent_object())
		{
			findings.push_back({ Severity::Error, object_label(element) + " has no parent object. Every device element must name the device object or another device element as its parent." });
		}
		else
		{
			auto parent = pool.get_object_by_id(element->get_parent_object());

			if (nullptr == parent)
			{
				findings.push_back({ Severity::Error, object_label(element) + " names object " + std::to_string(element->get_parent_object()) + " as its parent, but no object in the pool has that ID." });
			}
			else if ((ObjectTypes::Device != parent->get_object_type()) &&
			         (ObjectTypes::DeviceElement != parent->get_object_type()))
			{
				findings.push_back({ Severity::Error, object_label(element) + " names " + object_label(parent) + " as its parent. Only the device object or another device element may be a parent." });
			}
		}

		for (std::uint16_t child = 0; child < element->get_number_child_objects(); child++)
		{
			auto childObject = pool.get_object_by_id(element->get_child_object_id(child));

			if (nullptr == childObject)
			{
				findings.push_back({ Severity::Error, object_label(element) + " references child object " + std::to_string(element->get_child_object_id(child)) + ", but no object in the pool has that ID." });
			}
			else if ((ObjectTypes::DeviceProcessData != childObject->get_object_type()) &&
			         (ObjectTypes::DeviceProperty != childObject->get_object_type()))
			{
				findings.push_back({ Severity::Error, object_label(element) + " has " + object_label(childObject) + " as a child. A device element may only have device process data and device property children." });
			}
		}
	}
}

static void check_value_presentation_reference(isobus::DeviceDescriptorObjectPool &pool, const std::shared_ptr<Object> &object, std::uint16_t presentationObjectID, std::vector<DDOPValidationFinding> &findings)
{
	if (NULL_OBJECT_ID == presentationObjectID)
	{
		return;
	}
	auto presentation = pool.get_object_by_id(presentationObjectID);

	if (nullptr == presentation)
	{
		findings.push_back({ Severity::Error, object_label(object) + " names object " + std::to_string(presentationObjectID) + " as its value presentation, but no object in the pool has that ID." });
	}
	else if (ObjectTypes::DeviceValuePresentation != presentation->get_object_type())
	{
		findings.push_back({ Severity::Error, object_label(object) + " names " + object_label(presentation) + " as its value presentation. Only a device value presentation object may be one." });
	}
}

static void check_presentation_references(isobus::DeviceDescriptorObjectPool &pool, std::vector<DDOPValidationFinding> &findings)
{
	for (const auto &processData : objects_of_type<DeviceProcessDataObject>(pool))
	{
		check_value_presentation_reference(pool, processData, processData->get_device_value_presentation_object_id(), findings);
	}

	for (const auto &property : objects_of_type<DevicePropertyObject>(pool))
	{
		check_value_presentation_reference(pool, property, property->get_device_value_presentation_object_id(), findings);
	}
}

static void check_device_object(isobus::DeviceDescriptorObjectPool &pool, std::vector<DDOPValidationFinding> &findings)
{
	auto devices = objects_of_type<DeviceObject>(pool);

	if (devices.empty())
	{
		findings.push_back({ Severity::Warning, "The pool has no device object. ISO 11783-10 requires exactly one, and a TC will reject a pool without it." });
		return;
	}

	if (1 < devices.size())
	{
		findings.push_back({ Severity::Warning, "The pool has " + std::to_string(devices.size()) + " device objects. ISO 11783-10 requires exactly one." });
	}
	const auto &device = devices.back();

	if (device->get_structure_label().empty())
	{
		findings.push_back({ Severity::Warning, "The device object's structure label is empty, so it is sent as seven spaces. A TC keys its cached copy of the pool on this label, and every pool that leaves it empty presents the same one, so the TC cannot tell one version of the pool from another." });
	}

	if (0 == device->get_iso_name())
	{
		findings.push_back({ Severity::Warning, "The device object's ISO NAME is 0, which is unlikely to be the client's real NAME. ISO 11783-10 requires it to be the NAME the client claims its address with, and the ISOBUS stack this app is built on rejects a pool whose NAME differs from the uploading client's." });
	}

	const std::string structureLabel = device->get_structure_label().substr(0, DeviceObject::MAX_STRUCTURE_AND_LOCALIZATION_LABEL_LENGTH);

	for (std::size_t i = 0; i < structureLabel.size(); i++)
	{
		if (0xFF == static_cast<std::uint8_t>(structureLabel[i]))
		{
			findings.push_back({ Severity::Warning, "Byte " + std::to_string(i + 1) + " of the device object's structure label is 0xFF. ISO 11783-10 allows 0 to 254 in each byte, because a TC answers with 0xFF bytes when it has no pool stored under the requested label." });
		}
	}

	if (0xFF != device->get_localization_label()[6])
	{
		findings.push_back({ Severity::Warning, "Byte 7 of the device object's localization label is reserved and must be 0xFF." });
	}

	if (!device->get_extended_structure_label().empty() &&
	    (pool.get_task_controller_compatibility_level() < FIRST_VERSION_4_ONLY_FEATURE_LEVEL))
	{
		findings.push_back({ Severity::Warning, "The device object carries an extended structure label, which only a version 4 TC reads. This pool is set to version " + std::to_string(pool.get_task_controller_compatibility_level()) + "." });
	}

	if (device->get_extended_structure_label().size() > DeviceObject::MAX_EXTENDED_STRUCTURE_LABEL_LENGTH)
	{
		findings.push_back({ Severity::Warning, "The device object's extended structure label is " + std::to_string(device->get_extended_structure_label().size()) + " bytes, above the ISO 11783-10 limit of " + std::to_string(DeviceObject::MAX_EXTENDED_STRUCTURE_LABEL_LENGTH) + "." });
	}

	if (device->get_structure_label().size() > DeviceObject::MAX_STRUCTURE_AND_LOCALIZATION_LABEL_LENGTH)
	{
		findings.push_back({ Severity::Warning, "The device object's structure label is " + std::to_string(device->get_structure_label().size()) + " bytes, above the ISO 11783-10 limit of " + std::to_string(DeviceObject::MAX_STRUCTURE_AND_LOCALIZATION_LABEL_LENGTH) + ". The stack will truncate it." });
	}
}

static void check_string_length(const std::string &description, const std::string &value, std::uint8_t taskControllerVersion, std::vector<DDOPValidationFinding> &findings)
{
	const bool isVersion4 = (taskControllerVersion >= FIRST_VERSION_4_ONLY_FEATURE_LEVEL);
	const std::size_t byteLimit = isVersion4 ? Object::MAX_DESIGNATOR_LENGTH : Object::MAX_DESIGNATOR_LEGACY_LENGTH;

	if (value.size() > MAX_LENGTH_PREFIXED_STRING_LENGTH)
	{
		findings.push_back({ Severity::Warning, description + " is " + std::to_string(value.size()) + " bytes. Its length is stored in a single byte, so a TC will read the wrong length and every object after it in the pool becomes unreadable." });
	}
	else if (value.size() > byteLimit)
	{
		findings.push_back({ Severity::Warning, description + " is " + std::to_string(value.size()) + " bytes, above the " + std::to_string(byteLimit) + " byte limit for a version " + std::to_string(taskControllerVersion) + " pool." });
	}
	else if (isVersion4 && (utf8_length(value) > MAX_DESIGNATOR_CHARACTERS))
	{
		findings.push_back({ Severity::Warning, description + " is " + std::to_string(utf8_length(value)) + " UTF-8 characters, above the ISO 11783-10 limit of " + std::to_string(MAX_DESIGNATOR_CHARACTERS) + " for a version 4 pool. The 128 byte limit is what 32 four-byte characters take up, not a longer allowance." });
	}
}

static void check_string_lengths(isobus::DeviceDescriptorObjectPool &pool, std::vector<DDOPValidationFinding> &findings)
{
	const std::uint8_t version = pool.get_task_controller_compatibility_level();

	for (const auto &object : objects_of_type<Object>(pool))
	{
		check_string_length("The designator of " + object_label(object), object->get_designator(), version, findings);
	}

	for (const auto &device : objects_of_type<DeviceObject>(pool))
	{
		check_string_length("The device object's serial number", device->get_serial_number(), version, findings);
		check_string_length("The device object's software version", device->get_software_version(), version, findings);
	}
}

static void check_element_numbers(isobus::DeviceDescriptorObjectPool &pool, std::vector<DDOPValidationFinding> &findings)
{
	std::map<std::uint16_t, std::vector<std::uint16_t>> objectIDsByElementNumber;

	for (const auto &element : objects_of_type<DeviceElementObject>(pool))
	{
		objectIDsByElementNumber[element->get_element_number()].push_back(element->get_object_id());

		if (element->get_element_number() > MAX_ADDRESSABLE_ELEMENT_NUMBER)
		{
			findings.push_back({ Severity::Warning, object_label(element) + " uses element number " + std::to_string(element->get_element_number()) + ". A process data message carries the element number in 12 bits, so a TC cannot address anything above " + std::to_string(MAX_ADDRESSABLE_ELEMENT_NUMBER) + "." });
		}
	}

	for (const auto &elementNumberAndObjectIDs : objectIDsByElementNumber)
	{
		if (1 < elementNumberAndObjectIDs.second.size())
		{
			findings.push_back({ Severity::Warning, "Element number " + std::to_string(elementNumberAndObjectIDs.first) + " is used by device elements " + join_object_ids(elementNumberAndObjectIDs.second) + ". A TC addresses process data by element number, so each device element needs its own." });
		}
	}
}

static bool has_parent_cycle(isobus::DeviceDescriptorObjectPool &pool, std::shared_ptr<DeviceElementObject> element)
{
	std::set<std::uint16_t> visited;

	while (visited.insert(element->get_object_id()).second)
	{
		auto parent = std::dynamic_pointer_cast<DeviceElementObject>(pool.get_object_by_id(element->get_parent_object()));

		if (nullptr == parent)
		{
			return false;
		}
		element = parent;
	}
	return true;
}

// Several elements naming the device object as their parent is allowed, so only the Device-type element is checked
static void check_element_hierarchy(isobus::DeviceDescriptorObjectPool &pool, std::vector<DDOPValidationFinding> &findings)
{
	std::vector<std::uint16_t> deviceTypeElements;

	for (const auto &element : objects_of_type<DeviceElementObject>(pool))
	{
		if (DeviceElementObject::Type::Device == element->get_type())
		{
			auto parent = pool.get_object_by_id(element->get_parent_object());

			deviceTypeElements.push_back(element->get_object_id());

			if (0 != element->get_element_number())
			{
				findings.push_back({ Severity::Warning, object_label(element) + " has the type Device but element number " + std::to_string(element->get_element_number()) + ". A TC addresses the device itself as element 0, so it will not find this element there." });
			}

			// A missing parent is already an error, so only a parent that resolves to the wrong object is reported here
			if ((nullptr != parent) && (ObjectTypes::Device != parent->get_object_type()))
			{
				findings.push_back({ Severity::Warning, object_label(element) + " has the type Device, but its parent is " + object_label(parent) + ". ISO 11783-10 makes the device object the parent of the element that represents the whole device." });
			}
		}

		if (has_parent_cycle(pool, element))
		{
			findings.push_back({ Severity::Warning, object_label(element) + " sits in a cycle of parent references, so following its parents never reaches the device object. A TC cannot place it in the element tree, and walking the tree may not terminate." });
		}
	}

	if (deviceTypeElements.empty())
	{
		findings.push_back({ Severity::Warning, "No device element has the type Device. ISO 11783-10 requires exactly one, and it is the element a TC treats as the whole device." });
	}
	else if (1 < deviceTypeElements.size())
	{
		findings.push_back({ Severity::Warning, "Device elements " + join_object_ids(deviceTypeElements) + " all have the type Device. ISO 11783-10 requires exactly one." });
	}
}

static void check_property_and_process_data_overlap(isobus::DeviceDescriptorObjectPool &pool, std::vector<DDOPValidationFinding> &findings)
{
	for (const auto &element : objects_of_type<DeviceElementObject>(pool))
	{
		std::set<std::uint16_t> propertyDDIs;
		std::set<std::uint16_t> processDataDDIs;

		for (std::uint16_t child = 0; child < element->get_number_child_objects(); child++)
		{
			auto childObject = pool.get_object_by_id(element->get_child_object_id(child));
			auto property = std::dynamic_pointer_cast<DevicePropertyObject>(childObject);
			auto processData = std::dynamic_pointer_cast<DeviceProcessDataObject>(childObject);

			if (nullptr != property)
			{
				propertyDDIs.insert(property->get_ddi());
			}
			else if (nullptr != processData)
			{
				processDataDDIs.insert(processData->get_ddi());
			}
		}

		for (const auto &ddi : propertyDDIs)
		{
			if (0 != processDataDDIs.count(ddi))
			{
				findings.push_back({ Severity::Warning, object_label(element) + " carries DDI " + std::to_string(ddi) + " both as a device property and as device process data. ISO 11783-10 does not allow the same DDI on one device element through both." });
			}
		}
	}
}

static void check_ddi(const std::shared_ptr<Object> &object, std::uint16_t ddi, std::vector<DDOPValidationFinding> &findings)
{
	if (ddi > PROPRIETARY_DDI_RANGE_END)
	{
		findings.push_back({ Severity::Warning, object_label(object) + " uses DDI " + std::to_string(ddi) + ", which ISO 11783-11 reserves." });
	}
	else if ((ddi < PROPRIETARY_DDI_RANGE_START) &&
	         (isobus::DataDictionary::get_entry(ddi).ddi != ddi))
	{
		findings.push_back({ Severity::Warning, object_label(object) + " uses DDI " + std::to_string(ddi) + ", which is not in this app's bundled copy of the ISO 11783-11 data dictionary. Either it is not a defined DDI, or the bundled copy predates it." });
	}
}

static void check_process_data(isobus::DeviceDescriptorObjectPool &pool, std::vector<DDOPValidationFinding> &findings)
{
	for (const auto &property : objects_of_type<DevicePropertyObject>(pool))
	{
		check_ddi(property, property->get_ddi(), findings);
	}

	for (const auto &processData : objects_of_type<DeviceProcessDataObject>(pool))
	{
		const std::uint8_t properties = processData->get_properties_bitfield();
		const std::uint8_t triggers = processData->get_trigger_methods_bitfield();
		const bool isControlSource = (0 != (properties & static_cast<std::uint8_t>(DeviceProcessDataObject::PropertiesBit::ControlSource)));

		check_ddi(processData, processData->get_ddi(), findings);

		if (0 != (properties & RESERVED_PROPERTIES_BITS))
		{
			findings.push_back({ Severity::Warning, object_label(processData) + " sets reserved bits in its properties bitfield. ISO 11783-10 defines only the first three." });
		}

		if (isControlSource && (pool.get_task_controller_compatibility_level() < FIRST_VERSION_4_ONLY_FEATURE_LEVEL))
		{
			findings.push_back({ Severity::Warning, object_label(processData) + " is marked as a control source, which only a version 4 TC understands. This pool is set to version " + std::to_string(pool.get_task_controller_compatibility_level()) + "." });
		}

		if (isControlSource && (0 != (properties & static_cast<std::uint8_t>(DeviceProcessDataObject::PropertiesBit::Settable))))
		{
			findings.push_back({ Severity::Warning, object_label(processData) + " is marked both settable and a control source. ISO 11783-10 makes those two mutually exclusive." });
		}

		if (0 != (triggers & RESERVED_TRIGGER_METHOD_BITS))
		{
			findings.push_back({ Severity::Warning, object_label(processData) + " sets reserved bits in its trigger methods bitfield. ISO 11783-10 defines only the first five." });
		}
	}
}

// A TC reads the default set by requesting DDI 0xDFFF from element 0
static void check_default_process_data(isobus::DeviceDescriptorObjectPool &pool, std::vector<DDOPValidationFinding> &findings)
{
	std::shared_ptr<Object> requestObject;
	bool markedAMember = false;

	for (const auto &processData : objects_of_type<DeviceProcessDataObject>(pool))
	{
		if (0 != (processData->get_properties_bitfield() & static_cast<std::uint8_t>(DeviceProcessDataObject::PropertiesBit::MemberOfDefaultSet)))
		{
			markedAMember = true;
		}

		if (REQUEST_DEFAULT_PROCESS_DATA_DDI == processData->get_ddi())
		{
			requestObject = processData;

			if (ALL_TRIGGER_METHODS != processData->get_trigger_methods_bitfield())
			{
				findings.push_back({ Severity::Warning, object_label(processData) + " is the request default process data object, which has to offer every trigger method. A TC may not accept the pool when it offers fewer." });
			}
		}
	}

	if (!markedAMember)
	{
		return;
	}

	if (nullptr == requestObject)
	{
		findings.push_back({ Severity::Warning, "Process data objects are marked as members of the default set, but the pool has no object for DDI 57343 (0xDFFF), which is what a TC requests to read that set. Nothing will ever ask for these values." });
		return;
	}

	for (const auto &element : objects_of_type<DeviceElementObject>(pool))
	{
		for (std::uint16_t child = 0; child < element->get_number_child_objects(); child++)
		{
			if (requestObject->get_object_id() == element->get_child_object_id(child))
			{
				if (0 != element->get_element_number())
				{
					findings.push_back({ Severity::Warning, object_label(requestObject) + " is the request default process data object, but it hangs off element number " + std::to_string(element->get_element_number()) + ". A TC asks element 0 for it." });
				}
				return;
			}
		}
	}
	findings.push_back({ Severity::Warning, object_label(requestObject) + " is the request default process data object, but it is not a child of any device element, so a TC cannot request it." });
}

static void check_value_presentations(isobus::DeviceDescriptorObjectPool &pool, std::vector<DDOPValidationFinding> &findings)
{
	for (const auto &presentation : objects_of_type<DeviceValuePresentationObject>(pool))
	{
		const float scale = presentation->get_scale();

		// Negated so that NaN, which fails every comparison, is reported too
		if (!((scale >= MIN_SCALE) && (scale <= MAX_SCALE)))
		{
			std::ostringstream scaleText;
			scaleText << scale;
			findings.push_back({ Severity::Warning, object_label(presentation) + " has a scale of " + scaleText.str() + ", outside the 0.000000001 to 100000000 range ISO 11783-10 allows. A TC presents a value as (raw value + offset) * scale." });
		}

		if (presentation->get_number_of_decimals() > MAX_NUMBER_OF_DECIMALS)
		{
			findings.push_back({ Severity::Warning, object_label(presentation) + " asks for " + std::to_string(presentation->get_number_of_decimals()) + " decimals. ISO 11783-10 allows at most " + std::to_string(MAX_NUMBER_OF_DECIMALS) + "." });
		}
	}
}

static void check_unreferenced_objects(isobus::DeviceDescriptorObjectPool &pool, std::vector<DDOPValidationFinding> &findings)
{
	std::set<std::uint16_t> referencedObjectIDs;

	for (const auto &element : objects_of_type<DeviceElementObject>(pool))
	{
		for (std::uint16_t child = 0; child < element->get_number_child_objects(); child++)
		{
			referencedObjectIDs.insert(element->get_child_object_id(child));
		}
	}

	for (const auto &object : objects_of_type<Object>(pool))
	{
		if ((0 == referencedObjectIDs.count(object->get_object_id())) &&
		    ((ObjectTypes::DeviceProcessData == object->get_object_type()) ||
		     (ObjectTypes::DeviceProperty == object->get_object_type())))
		{
			findings.push_back({ Severity::Warning, object_label(object) + " is not a child of any device element, so a TC can never address it." });
		}
	}
}

static void check_stack_read_back(isobus::DeviceDescriptorObjectPool &pool, bool foundAnError, std::vector<DDOPValidationFinding> &findings)
{
	std::vector<std::uint8_t> binaryPool;

	if (!pool.generate_binary_object_pool(binaryPool))
	{
		if (!foundAnError)
		{
			findings.push_back({ Severity::Error, "The pool could not be serialized, for a reason none of these checks covers. The log below is what the ISOBUS stack reported." });
		}
		return;
	}
	isobus::DeviceDescriptorObjectPool readBack(pool.get_task_controller_compatibility_level());

	if (!readBack.deserialize_binary_object_pool(binaryPool, isobus::NAME(0)))
	{
		findings.push_back({ Severity::Warning, "The serialized pool cannot be read back by the ISOBUS stack this app is built on. That is not proof a TC will reject it, but nothing here can say what a TC would make of it." });
		return;
	}
	std::vector<std::uint8_t> reserializedPool;

	// Comparing bytes also catches a field the reader drops or rewrites, such as the device object's ID
	if (!readBack.generate_binary_object_pool(reserializedPool))
	{
		findings.push_back({ Severity::Warning, "The pool serializes, but the result cannot be serialized again after being read back. A reader does not end up holding the pool you built." });
	}
	else if (reserializedPool != binaryPool)
	{
		findings.push_back({ Severity::Warning, "Reading the serialized pool back and writing it out again does not reproduce the same bytes, so at least one field does not survive the trip. A reader holds something other than the pool you built." });
	}
}

std::vector<DDOPValidationFinding> validate_ddop(isobus::DeviceDescriptorObjectPool &pool)
{
	std::vector<DDOPValidationFinding> findings;

	check_device_element_references(pool, findings);
	check_presentation_references(pool, findings);

	const bool foundAnError = !findings.empty();

	check_device_object(pool, findings);
	check_string_lengths(pool, findings);
	check_element_numbers(pool, findings);
	check_element_hierarchy(pool, findings);
	check_property_and_process_data_overlap(pool, findings);
	check_process_data(pool, findings);
	check_default_process_data(pool, findings);
	check_value_presentations(pool, findings);
	check_unreferenced_objects(pool, findings);
	check_stack_read_back(pool, foundAnError, findings);

	std::stable_partition(findings.begin(), findings.end(), [](const DDOPValidationFinding &finding) {
		return Severity::Error == finding.severity;
	});
	return findings;
}
