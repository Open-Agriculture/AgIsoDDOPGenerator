//================================================================================================
/// @file task_data_import_test.cpp
///
/// @brief Checks that the devices in a TASKDATA.XML import into DDOPs that survive an ISOXML and a binary round trip
/// @author Sujan Dumaru
///
/// @copyright 2026 The Open-Agriculture developers
//================================================================================================
#include "task_data_import.hpp"
#include "isobus/isobus/can_constants.hpp"
#include "isobus/isobus/isobus_device_descriptor_object_pool.hpp"
#include "tinyxml2.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using isobus::task_controller_object::DeviceElementObject;
using isobus::task_controller_object::DeviceObject;
using isobus::task_controller_object::DeviceProcessDataObject;
using isobus::task_controller_object::DevicePropertyObject;
using isobus::task_controller_object::DeviceValuePresentationObject;

static const std::string A_UMLAUT = "\xC3\xA4";
static const std::uint16_t NO_PRESENTATION = isobus::NULL_OBJECT_ID;

static int failureCount = 0;

static void fail(const std::string &description)
{
	std::fprintf(stderr, "FAIL: %s\n", description.c_str());
	failureCount++;
}

static void check(bool condition, const std::string &description)
{
	if (!condition)
	{
		fail(description);
	}
}

template<typename T>
static std::shared_ptr<T> get_object(isobus::DeviceDescriptorObjectPool &pool, std::uint16_t objectID)
{
	return std::dynamic_pointer_cast<T>(pool.get_object_by_id(objectID));
}

static bool import_device(const std::string &taskData, std::size_t deviceIndex, isobus::DeviceDescriptorObjectPool &pool)
{
	const bool imported = import_task_data_device(taskData, deviceIndex, pool);

	check(imported, "DVC number " + std::to_string(deviceIndex + 1) + " imports");
	return imported;
}

static void check_device(isobus::DeviceDescriptorObjectPool &pool,
                         const std::string &label,
                         const std::string &designator,
                         const std::string &softwareVersion,
                         const std::string &serialNumber,
                         std::uint64_t name,
                         const std::string &structureLabel,
                         const std::array<std::uint8_t, 7> &localizationLabel)
{
	const auto device = get_object<DeviceObject>(pool, 0);

	if (nullptr == device)
	{
		fail(label + " has no device object");
		return;
	}
	check(designator == device->get_designator(), label + " designator");
	check(softwareVersion == device->get_software_version(), label + " software version");
	check(serialNumber == device->get_serial_number(), label + " serial number");
	check(name == device->get_iso_name(), label + " NAME");
	check(structureLabel == device->get_structure_label(), label + " structure label");
	check(localizationLabel == device->get_localization_label(), label + " localization label");
}

static void check_element(isobus::DeviceDescriptorObjectPool &pool,
                          std::uint16_t objectID,
                          DeviceElementObject::Type type,
                          std::uint16_t elementNumber,
                          std::uint16_t parentID,
                          const std::string &designator,
                          const std::vector<std::uint16_t> &children)
{
	const std::string label = "DET " + std::to_string(objectID);
	const auto element = get_object<DeviceElementObject>(pool, objectID);

	if (nullptr == element)
	{
		fail(label + " is missing or not a DET");
		return;
	}

	std::vector<std::uint16_t> importedChildren;
	for (std::uint16_t i = 0; i < element->get_number_child_objects(); i++)
	{
		importedChildren.push_back(element->get_child_object_id(i));
	}

	check(type == element->get_type(), label + " type");
	check(elementNumber == element->get_element_number(), label + " element number");
	check(parentID == element->get_parent_object(), label + " parent");
	check(designator == element->get_designator(), label + " designator");
	check(children == importedChildren, label + " keeps its DORs in file order");
}

static void check_process_data(isobus::DeviceDescriptorObjectPool &pool,
                               std::uint16_t objectID,
                               std::uint16_t ddi,
                               std::uint8_t properties,
                               std::uint8_t triggers,
                               const std::string &designator,
                               std::uint16_t presentationID)
{
	const std::string label = "DPD " + std::to_string(objectID);
	const auto processData = get_object<DeviceProcessDataObject>(pool, objectID);

	if (nullptr == processData)
	{
		fail(label + " is missing or not a DPD");
		return;
	}
	check(ddi == processData->get_ddi(), label + " DDI");
	check(properties == processData->get_properties_bitfield(), label + " properties");
	check(triggers == processData->get_trigger_methods_bitfield(), label + " trigger methods");
	check(designator == processData->get_designator(), label + " designator");
	check(presentationID == processData->get_device_value_presentation_object_id(), label + " DVP reference");
}

static void check_property(isobus::DeviceDescriptorObjectPool &pool,
                           std::uint16_t objectID,
                           std::uint16_t ddi,
                           std::int32_t value,
                           const std::string &designator,
                           std::uint16_t presentationID)
{
	const std::string label = "DPT " + std::to_string(objectID);
	const auto property = get_object<DevicePropertyObject>(pool, objectID);

	if (nullptr == property)
	{
		fail(label + " is missing or not a DPT");
		return;
	}
	check(ddi == property->get_ddi(), label + " DDI");
	check(value == property->get_value(), label + " value");
	check(designator == property->get_designator(), label + " designator");
	check(presentationID == property->get_device_value_presentation_object_id(), label + " DVP reference");
}

static void check_presentation(isobus::DeviceDescriptorObjectPool &pool,
                               std::uint16_t objectID,
                               std::int32_t offset,
                               float scale,
                               std::uint8_t decimals,
                               const std::string &unit)
{
	const std::string label = "DVP " + std::to_string(objectID);
	const auto presentation = get_object<DeviceValuePresentationObject>(pool, objectID);

	if (nullptr == presentation)
	{
		fail(label + " is missing or not a DVP");
		return;
	}
	check(offset == presentation->get_offset(), label + " offset");
	check(std::fabs(presentation->get_scale() - scale) <= (std::fabs(scale) * 1e-5f), label + " scale");
	check(decimals == presentation->get_number_of_decimals(), label + " decimals");
	check(unit == presentation->get_designator(), label + " unit");
}

static void check_device_list(const std::string &taskData)
{
	const std::vector<std::string> labels = list_task_data_devices(taskData);

	check(4 == labels.size(), "list_task_data_devices finds 4 DVCs past the skipped CTR, TSK, PFD and VPN elements");
	if (4 == labels.size())
	{
		check("DVC-1  Tractor ECU" == labels.at(0), "the first DVC is listed as \"A  B\"");
		check("DVC-2  Sprayer & Boom" == labels.at(1), "the entity in the second DVC designator is decoded in the list");
		check("DVC-3  Sprayer Plus" == labels.at(2), "the third DVC is listed after a skipped PFD");
		check(R"(DVC-4  Quote "Q" <tag> & 'a')" == labels.at(3), "the fourth DVC is listed after a skipped VPN");
	}
}

static void check_first_device(const std::string &taskData)
{
	isobus::DeviceDescriptorObjectPool pool(4);

	if (import_device(taskData, 0, pool))
	{
		check(1 == pool.size(), "DVC-1 holds only its device");
		check_device(pool, "DVC-1", "Tractor ECU", "1.0", "1234", 0xA00084000C200002, std::string("\x01\0\0\0\0\0\0", 7), { 0x64, 0x65, 0, 0, 0, 0, 0xFF });
	}
}

static void check_second_device(const std::string &taskData)
{
	isobus::DeviceDescriptorObjectPool pool(4);

	if (!import_device(taskData, 1, pool))
	{
		return;
	}

	const auto device = get_object<DeviceObject>(pool, 0);
	const auto rootElement = get_object<DeviceElementObject>(pool, 5001);
	const auto section = get_object<DeviceElementObject>(pool, 5003);
	const auto workState = get_object<DeviceProcessDataObject>(pool, 5006);
	const auto offsetX = get_object<DevicePropertyObject>(pool, 5005);
	const auto hours = get_object<DeviceValuePresentationObject>(pool, 6001);
	const auto hectares = get_object<DeviceValuePresentationObject>(pool, 6002);

	if ((nullptr == device) || (nullptr == rootElement) || (nullptr == section) || (nullptr == workState) ||
	    (nullptr == offsetX) || (nullptr == hours) || (nullptr == hectares))
	{
		fail("DVC-2: an imported object is missing or has the wrong type");
		return;
	}

	check(9 == pool.size(), "the pool holds the device and its 8 objects");
	check(0xA00086000CE01234 == device->get_iso_name(), "D is read as a big-endian NAME");
	check("Sprayer & Boom" == device->get_designator(), "the designator is entity-decoded");
	check(device->get_serial_number().empty(), "a DVC without E has an empty serial number");
	check("7654321" == device->get_structure_label(), "F is read in reversed byte order");
	check((0x65 == device->get_localization_label().at(0)) && (0x6E == device->get_localization_label().at(1)) &&
	        (0xFF == device->get_localization_label().at(6)),
	      "G is read in reversed byte order");

	check(DeviceElementObject::Type::Device == rootElement->get_type(), "the root DET is a Device element");
	check(5001 == section->get_parent_object(), "the section DET has the root DET as its parent");
	check(DeviceElementObject::Type::Section == section->get_type(), "the section DET has type 4");
	check(section->get_designator().empty(), "a DET without D has an empty designator");
	check((2 == section->get_number_child_objects()) && (5005 == section->get_child_object_id(0)) &&
	        (5006 == section->get_child_object_id(1)),
	      "the section DET keeps its DOR children in order");

	check(0x008D == workState->get_ddi(), "a DPD DDI is read as hex");
	check(isobus::NULL_OBJECT_ID == workState->get_device_value_presentation_object_id(), "a DPD without F has no DVP");
	check(-1200 == offsetX->get_value(), "a DPT value can be negative");
	check(isobus::NULL_OBJECT_ID == offsetX->get_device_value_presentation_object_id(), "a DPT without E has no DVP");
	check(std::fabs(hours->get_scale() - 0.0000012345) < 1e-9, "the hours scale keeps its precision");
	check(std::fabs(hectares->get_scale() - 0.000099999997474) < 1e-9, "the hectares scale keeps its precision");
	check("ha   " == hectares->get_designator(), "a space-padded unit is kept as is");

	std::vector<std::uint8_t> binaryPool;
	check(pool.generate_binary_object_pool(binaryPool), "the imported pool serializes");
}

static void check_third_device(const std::string &taskData)
{
	isobus::DeviceDescriptorObjectPool pool(4);

	if (!import_device(taskData, 2, pool))
	{
		return;
	}

	std::string sixteenUmlauts;
	for (std::size_t i = 0; i < 16; i++)
	{
		sixteenUmlauts += A_UMLAUT;
	}

	check(23 == pool.size(), "DVC-3 holds the device and its 22 objects, all listed out of order");
	check_device(pool, "DVC-3", "Sprayer Plus", "3.1", "SN-0003", 0xA00086000CE05678, "GFEDCBA", { 0x72, 0x73, 0, 0, 0, 0, 0xFF });

	check_element(pool, 7001, DeviceElementObject::Type::Device, 0, 0, "Sprayer Device", { 7104, 7101, 7114, 7102 });
	check_element(pool, 7002, DeviceElementObject::Type::Function, 1, 7001, "Function", {});
	check_element(pool, 7003, DeviceElementObject::Type::Bin, 2, 7002, "Tank " + A_UMLAUT, { 7113, 7111 });
	check_element(pool, 7004, DeviceElementObject::Type::Section, 3, 7002, "Sec" + A_UMLAUT + "tion", { 7101, 7106 });
	check_element(pool, 7005, DeviceElementObject::Type::Unit, 4, 7004, "Unit Hi", { 7103, 7105 });
	check_element(pool, 7006, DeviceElementObject::Type::Connector, 5, 7001, sixteenUmlauts, { 7112 });
	check_element(pool, 7007, DeviceElementObject::Type::NavigationReference, 6, 7001, "Nav", { 7115 });
	check(32 == sixteenUmlauts.size(), "the connector designator is 32 bytes");

	check_process_data(pool, 7101, 0x0074, 1, 8, "Total Area", 7201);
	check_process_data(pool, 7102, 0x0077, 3, 9, "Total Time", 7202);
	check_process_data(pool, 7103, 0x008D, 0, 0, "", NO_PRESENTATION);
	check_process_data(pool, 7104, 0x8001, 5, 31, "Proprietary Rate", NO_PRESENTATION);
	check_process_data(pool, 7105, 0x00AF, 1, 2, "Distance Driven", 7203);
	check_process_data(pool, 7106, 0x9ABC, 2, 4, "ABCDEFGHIJKLMNOPQRSTUVWXYZ012345", NO_PRESENTATION);
	const auto thirtyTwoBytes = get_object<DeviceProcessDataObject>(pool, 7106);
	check((nullptr != thirtyTwoBytes) && (32 == thirtyTwoBytes->get_designator().size()), "the DPD designator is 32 bytes");

	check_property(pool, 7111, 0x0086, 0, "Zero Offset", NO_PRESENTATION);
	check_property(pool, 7112, 0x0087, -1200, "Negative Offset", 7202);
	check_property(pool, 7113, 0x0088, INT32_MAX, "Int32 Max", NO_PRESENTATION);
	check_property(pool, 7114, 0x0086, INT32_MIN, "Int32 Min", NO_PRESENTATION);
	check_property(pool, 7115, 0x0089, 65536, "Large With DVP", 7204);

	check_presentation(pool, 7201, 0, 1.0f, 0, "m");
	check_presentation(pool, 7202, -100, 0.001f, 3, "mm");
	check_presentation(pool, 7203, 2000000000, 1e-7f, 7, "");
	check_presentation(pool, 7204, INT32_MIN, 100.0f, 0, "%");
}

static void check_fourth_device(const std::string &taskData)
{
	isobus::DeviceDescriptorObjectPool pool(4);

	if (!import_device(taskData, 3, pool))
	{
		return;
	}

	check(6 == pool.size(), "DVC-4 holds the device and its 5 objects");
	check_device(pool, "DVC-4", R"(Quote "Q" <tag> & 'a')", "<4>", "&", 0xA00086000CE09ABC, "GFEDCBA", { 0x72, 0x73, 0, 0, 0, 0, 0xFF });
	check_element(pool, 8001, DeviceElementObject::Type::Device, 0, 0, "Root <1>", {});
	check_element(pool, 8002, DeviceElementObject::Type::Section, 1, 8001, "a & b", { 8101, 8102 });
	check_process_data(pool, 8101, 0x0074, 1, 8, R"(Rate "x")", 8201);
	check_property(pool, 8102, 0x0086, 7, "It's <NCR> " + A_UMLAUT, 8201);
	check_presentation(pool, 8201, 0, 1.0f, 0, "<&>");
}

static void check_rejects(const std::string &taskData)
{
	const std::vector<std::pair<std::string, std::string>> rejects = {
		{ R"(B="5003" C="4")", R"(B="5003" C="9")" }, { R"(B="5003" C="4")", R"(B="5003" C="-1")" }, { R"(B="5003" C="4")", R"(B="5003" C="4x")" }, { R"(F="31323334353637")", R"(F="3132333435363738")" }, { R"(<DPD A="5004")", R"(<DPD A="5002")" }
	};
	for (const auto &reject : rejects)
	{
		const std::size_t position = taskData.find(reject.first);
		const std::string description = reject.first + " -> " + reject.second + " is rejected";

		if (std::string::npos == position)
		{
			fail(reject.first + " is not in the file, so its reject case edits nothing");
			continue;
		}

		std::string badTaskData = taskData;
		badTaskData.replace(position, reject.first.size(), reject.second);
		isobus::DeviceDescriptorObjectPool badPool(4);
		check(!import_task_data_device(badTaskData, 1, badPool), description);
	}

	for (const std::string &notTaskData : { std::string(), std::string("DVC\x01\x02"), std::string("<Other/>") })
	{
		isobus::DeviceDescriptorObjectPool emptyPool(4);
		check(list_task_data_devices(notTaskData).empty(), "input that is not a TASKDATA.XML lists no devices");
		check(!import_task_data_device(notTaskData, 0, emptyPool), "input that is not a TASKDATA.XML does not import");
	}
}

static void check_binary_round_trip(isobus::DeviceDescriptorObjectPool &pool, const std::string &label)
{
	std::vector<std::uint8_t> binaryPool;
	std::vector<std::uint8_t> loadedBytes;
	std::vector<std::uint8_t> savedBytes;
	isobus::DeviceDescriptorObjectPool loadedPool(pool.get_task_controller_compatibility_level());

	if (!pool.generate_binary_object_pool(binaryPool))
	{
		fail(label + ": the imported pool does not serialize");
		return;
	}

	loadedBytes = binaryPool;
	if (!loadedPool.deserialize_binary_object_pool(loadedBytes, isobus::NAME(0)) || !loadedPool.generate_binary_object_pool(savedBytes))
	{
		fail(label + ": the imported pool's binary does not deserialize and serialize again");
		return;
	}

	if (savedBytes != binaryPool)
	{
		const auto offset = static_cast<std::size_t>(std::mismatch(binaryPool.begin(), binaryPool.end(), savedBytes.begin(), savedBytes.end()).first - binaryPool.begin());
		fail(label + ": saving the deserialized binary differs at byte " + std::to_string(offset) + " (" + std::to_string(binaryPool.size()) + " in, " + std::to_string(savedBytes.size()) + " out)");
	}
}

static void check_xml_round_trip(isobus::DeviceDescriptorObjectPool &pool, const std::string &label)
{
	std::string exportedXml;
	std::string reexportedXml;
	isobus::DeviceDescriptorObjectPool importedPool(pool.get_task_controller_compatibility_level());

	if (!pool.generate_task_data_iso_xml(exportedXml))
	{
		fail(label + ": the imported pool does not export to ISOXML");
		return;
	}

	if (!import_task_data_device(exportedXml, 0, importedPool))
	{
		tinyxml2::XMLDocument document;
		const bool parses = tinyxml2::XML_SUCCESS == document.Parse(exportedXml.data(), exportedXml.size());
		fail(label + ": the exported ISOXML does not import again (" + (parses ? std::string("it parses but an attribute is invalid") : std::string("it is not valid XML: ") + document.ErrorStr()) + ")");
		return;
	}

	if (!importedPool.generate_task_data_iso_xml(reexportedXml))
	{
		fail(label + ": the re-imported pool does not export to ISOXML");
		return;
	}

	if (exportedXml != reexportedXml)
	{
		const auto offset = static_cast<std::size_t>(std::mismatch(exportedXml.begin(), exportedXml.end(), reexportedXml.begin(), reexportedXml.end()).first - exportedXml.begin());
		fail(label + ": re-exporting the imported ISOXML differs at byte " + std::to_string(offset) + ":\n" + exportedXml.substr(offset, 60) + "\n" + reexportedXml.substr(offset, 60));
	}
}

int main(int argc, char **argv)
{
	if (2 != argc)
	{
		std::fprintf(stderr, "usage: %s <path to TASKDATA.XML>\n", argv[0]);
		return EXIT_FAILURE;
	}

	std::ifstream inputFile(argv[1], std::ios::binary);

	if (!inputFile)
	{
		std::fprintf(stderr, "FAIL: cannot open %s\n", argv[1]);
		return EXIT_FAILURE;
	}

	const std::string taskData((std::istreambuf_iterator<char>(inputFile)), std::istreambuf_iterator<char>());

	check_device_list(taskData);
	check_first_device(taskData);
	check_second_device(taskData);
	check_third_device(taskData);
	check_fourth_device(taskData);
	check_rejects(taskData);

	const std::size_t deviceCount = list_task_data_devices(taskData).size();
	for (std::size_t i = 0; i < deviceCount; i++)
	{
		const std::string label = "DVC number " + std::to_string(i + 1) + " round trip";
		isobus::DeviceDescriptorObjectPool pool(4);

		if (import_device(taskData, i, pool))
		{
			check_binary_round_trip(pool, label);
			check_xml_round_trip(pool, label);
		}
	}

	if (0 == failureCount)
	{
		std::printf("PASS: TASKDATA.XML device import, %zu devices round tripped\n", deviceCount);
	}
	return (0 == failureCount) ? EXIT_SUCCESS : EXIT_FAILURE;
}
