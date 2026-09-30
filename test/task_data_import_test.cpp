//================================================================================================
/// @file task_data_import_test.cpp
///
/// @brief Checks that a hand-written TASKDATA.XML device description imports into a DDOP
/// @author Sujan Dumaru
///
/// @copyright 2026 The Open-Agriculture developers
//================================================================================================
#include "task_data_import.hpp"
#include "isobus/isobus/can_constants.hpp"
#include "isobus/isobus/isobus_device_descriptor_object_pool.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

static const std::string TASK_DATA = R"(<?xml version="1.0" encoding="UTF-8"?>
<ISO11783_TaskData VersionMajor="4" VersionMinor="2" DataTransferOrigin="2">
<DVC A="DVC-1" B="Tractor ECU" C="1.0" D="A00084000C200002" E="1234" F="00000000000001" G="FF000000006564"/>
<DVC A="DVC-2" B="Sprayer &amp; Boom" C="2.3.4" D="A00086000CE01234" F="31323334353637" G="FF000000006E65">
	<DET A="DET-1" B="5001" C="1" D="Sprayer" E="0" F="0">
		<DOR A="5002"/>
		<DOR A="5004"/>
	</DET>
	<DET A="DET-2" B="5003" C="4" E="1" F="5001">
		<DOR A="5005"/>
		<DOR A="5006"/>
	</DET>
	<DPD A="5002" B="0074" C="1" D="8" E="Total Area" F="6002"/>
	<DPD A="5004" B="0077" C="1" D="8" E="Total Time" F="6001"/>
	<DPD A="5006" B="008D" C="1" D="31" E="Work State"/>
	<DPT A="5005" B="0086" C="-1200" D="Offset X"/>
	<DVP A="6001" B="0" C="0.0000012345" D="1" E="h"/>
	<DVP A="6002" B="0" C="0.000099999997474" D="2" E="ha   "/>
</DVC>
</ISO11783_TaskData>
)";

static int failureCount = 0;

static void check(bool condition, const char *description)
{
	if (!condition)
	{
		std::fprintf(stderr, "FAIL: %s\n", description);
		failureCount++;
	}
}

template<typename T>
static std::shared_ptr<T> get_object(isobus::DeviceDescriptorObjectPool &pool, std::uint16_t objectID)
{
	return std::dynamic_pointer_cast<T>(pool.get_object_by_id(objectID));
}

int main()
{
	const std::vector<std::string> labels = list_task_data_devices(TASK_DATA);
	check((2 == labels.size()) && ("DVC-1  Tractor ECU" == labels.at(0)) && ("DVC-2  Sprayer & Boom" == labels.at(1)),
	      "list_task_data_devices returns both DVCs as \"A  B\"");

	isobus::DeviceDescriptorObjectPool pool(4);
	if (!import_task_data_device(TASK_DATA, 1, pool))
	{
		std::fprintf(stderr, "FAIL: the second DVC did not import\n");
		return 1;
	}

	const auto device = get_object<isobus::task_controller_object::DeviceObject>(pool, 0);
	const auto rootElement = get_object<isobus::task_controller_object::DeviceElementObject>(pool, 5001);
	const auto section = get_object<isobus::task_controller_object::DeviceElementObject>(pool, 5003);
	const auto workState = get_object<isobus::task_controller_object::DeviceProcessDataObject>(pool, 5006);
	const auto offsetX = get_object<isobus::task_controller_object::DevicePropertyObject>(pool, 5005);
	const auto hours = get_object<isobus::task_controller_object::DeviceValuePresentationObject>(pool, 6001);
	const auto hectares = get_object<isobus::task_controller_object::DeviceValuePresentationObject>(pool, 6002);

	if ((nullptr == device) || (nullptr == rootElement) || (nullptr == section) || (nullptr == workState) ||
	    (nullptr == offsetX) || (nullptr == hours) || (nullptr == hectares))
	{
		std::fprintf(stderr, "FAIL: an imported object is missing or has the wrong type\n");
		return 1;
	}

	check(9 == pool.size(), "the pool holds the device and its 8 objects");
	check(0xA00086000CE01234 == device->get_iso_name(), "D is read as a big-endian NAME");
	check("Sprayer & Boom" == device->get_designator(), "the designator is entity-decoded");
	check(device->get_serial_number().empty(), "a DVC without E has an empty serial number");
	check("7654321" == device->get_structure_label(), "F is read in reversed byte order");
	check((0x65 == device->get_localization_label().at(0)) && (0x6E == device->get_localization_label().at(1)) &&
	        (0xFF == device->get_localization_label().at(6)),
	      "G is read in reversed byte order");

	check(isobus::task_controller_object::DeviceElementObject::Type::Device == rootElement->get_type(), "the root DET is a Device element");
	check(5001 == section->get_parent_object(), "the section DET has the root DET as its parent");
	check(isobus::task_controller_object::DeviceElementObject::Type::Section == section->get_type(), "the section DET has type 4");
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

	const std::vector<std::pair<std::string, std::string>> rejects = {
		{ R"(B="5003" C="4")", R"(B="5003" C="9")" }, { R"(B="5003" C="4")", R"(B="5003" C="-1")" }, { R"(B="5003" C="4")", R"(B="5003" C="4x")" }, { R"(F="31323334353637")", R"(F="3132333435363738")" }, { R"(<DPD A="5004")", R"(<DPD A="5002")" }
	};
	for (const auto &reject : rejects)
	{
		std::string badTaskData = TASK_DATA;
		badTaskData.replace(badTaskData.find(reject.first), reject.first.size(), reject.second);
		isobus::DeviceDescriptorObjectPool badPool(4);
		check(!import_task_data_device(badTaskData, 1, badPool), (reject.first + " -> " + reject.second + " is rejected").c_str());
	}

	for (const std::string &notTaskData : { std::string(), std::string("DVC\x01\x02"), std::string("<Other/>") })
	{
		isobus::DeviceDescriptorObjectPool emptyPool(4);
		check(list_task_data_devices(notTaskData).empty(), "input that is not a TASKDATA.XML lists no devices");
		check(!import_task_data_device(notTaskData, 0, emptyPool), "input that is not a TASKDATA.XML does not import");
	}

	if (0 == failureCount)
	{
		std::printf("PASS: TASKDATA.XML device import\n");
	}
	return (0 == failureCount) ? 0 : 1;
}
