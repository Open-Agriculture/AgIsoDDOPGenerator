//================================================================================================
/// @file ddop_validator_test.cpp
///
/// @brief Checks every rule validate_ddop() applies, against pools built to break exactly one rule
/// @author Sujan Dumaru
///
/// @copyright 2026 The Open-Agriculture developers
//================================================================================================

#include "ddop_validator.hpp"
#include "isobus/utility/iop_file_interface.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <string>
#include <vector>

using namespace isobus::task_controller_object;

// Not assert(): the repository builds Release by default, which would define NDEBUG and delete both
// the check and the pool it is being run on
#define CHECK(condition) check_condition((condition), #condition, __LINE__)

static void check_condition(bool result, const char *text, int line)
{
	if (!result)
	{
		std::fprintf(stderr, "FAIL line %d: %s\n", line, text);
		std::exit(1);
	}
}

static std::size_t count(const std::vector<DDOPValidationFinding> &findings, DDOPValidationFinding::Severity severity)
{
	return std::count_if(findings.begin(), findings.end(), [severity](const DDOPValidationFinding &finding) {
		return severity == finding.severity;
	});
}

// The conformant pool every rule check breaks one thing in.
static std::unique_ptr<isobus::DeviceDescriptorObjectPool> make_clean_pool(std::uint8_t version)
{
	std::unique_ptr<isobus::DeviceDescriptorObjectPool> pool(new isobus::DeviceDescriptorObjectPool(version));
	std::array<std::uint8_t, 7> localization = { 'e', 'n', 0x50, 0x00, 0x00, 0x00, 0xFF };

	CHECK(pool->add_device("Sprayer", "1.0", "SN1", "STRUCT1", localization, {}, 0xA000000000000000));
	CHECK(pool->add_device_element("Root", 0, 0, DeviceElementObject::Type::Device, 1));
	CHECK(pool->add_device_value_presentation("mm", 0, 1.0f, 0, 2));
	CHECK(pool->add_device_process_data("Width", 70, 2, 0x01, 0x01, 3));

	// Marking a member of the default set obliges the pool to carry the object a TC requests that set with
	CHECK(pool->add_device_process_data("Request Default Process Data", 0xDFFF, 0xFFFF, 0x00, 0x1F, 4));
	auto root = std::static_pointer_cast<DeviceElementObject>(pool->get_object_by_id(1));
	root->add_reference_to_child_object(3);
	root->add_reference_to_child_object(4);
	return pool;
}

// How many findings mention the given text, which is how each rule is identified.
static std::size_t times_fired(const std::vector<DDOPValidationFinding> &findings, const std::string &text)
{
	return std::count_if(findings.begin(), findings.end(), [&text](const DDOPValidationFinding &finding) {
		return std::string::npos != finding.message.find(text);
	});
}

static bool fires(const std::vector<DDOPValidationFinding> &findings, const std::string &text)
{
	return 0 != times_fired(findings, text);
}

// Naming every rule is what makes this coverage rather than a count that any twenty findings satisfy.
static void check_every_rule_fires(const std::vector<DDOPValidationFinding> &findings, std::initializer_list<const char *> rules)
{
	for (const auto &rule : rules)
	{
		if (!fires(findings, rule))
		{
			printf("  MISSING RULE: %s\n", rule);
			CHECK(false);
		}
	}
}

// A pool that breaks nothing must produce no findings at all, or every real DDOP becomes noise.
static void check_clean_pool_is_silent()
{
	CHECK(validate_ddop(*make_clean_pool(4)).empty());
}

// The stack stops at the first broken reference. The whole point of the validator is that it does not.
static void check_every_error_is_reported()
{
	isobus::DeviceDescriptorObjectPool pool(4);
	std::array<std::uint8_t, 7> localization = { 'e', 'n', 0x50, 0x00, 0x00, 0x00, 0xFF };

	CHECK(pool.add_device("Sprayer", "1.0", "SN1", "STRUCT1", localization, {}, 0xA000000000000000));
	CHECK(pool.add_device_element("Root", 0, 0, DeviceElementObject::Type::Device, 1));
	CHECK(pool.add_device_element("No parent", 1, 0, DeviceElementObject::Type::Function, 2));
	CHECK(pool.add_device_element("Ghost parent", 2, 900, DeviceElementObject::Type::Function, 3));
	CHECK(pool.add_device_element("Parent is process data", 3, 10, DeviceElementObject::Type::Function, 4));
	CHECK(pool.add_device_process_data("Ghost presentation", 70, 900, 0x00, 0x01, 10));
	CHECK(pool.add_device_process_data("Presentation is an element", 71, 1, 0x00, 0x01, 11));
	std::static_pointer_cast<DeviceElementObject>(pool.get_object_by_id(2))->set_parent_object(0xFFFF);

	auto root = std::static_pointer_cast<DeviceElementObject>(pool.get_object_by_id(1));
	root->add_reference_to_child_object(99);
	root->add_reference_to_child_object(2);

	std::vector<std::uint8_t> binaryPool;
	auto findings = validate_ddop(pool);

	CHECK(!pool.generate_binary_object_pool(binaryPool));

	// The stack stops at whichever of these it reaches first, so each one has to be named here to stay covered
	check_every_rule_fires(findings, { "has no parent object", "as its parent, but no object in the pool has that ID", "as its parent. Only the device object or another device element", "references child object 99, but no object in the pool has that ID", "as a child. A device element may only have", "as its value presentation, but no object in the pool has that ID", "as its value presentation. Only a device value presentation object" });
	CHECK(7 == count(findings, DDOPValidationFinding::Severity::Error));
}

// A parent cycle serializes cleanly, so it can only ever be a warning.
static void check_parent_cycle_is_a_warning()
{
	isobus::DeviceDescriptorObjectPool pool(4);
	std::array<std::uint8_t, 7> localization = { 'e', 'n', 0x50, 0x00, 0x00, 0x00, 0xFF };

	CHECK(pool.add_device("Sprayer", "1.0", "SN1", "STRUCT1", localization, {}, 0xA000000000000000));
	CHECK(pool.add_device_element("Root", 0, 0, DeviceElementObject::Type::Device, 1));
	CHECK(pool.add_device_element("A", 1, 3, DeviceElementObject::Type::Function, 2));
	CHECK(pool.add_device_element("B", 2, 2, DeviceElementObject::Type::Function, 3));

	std::vector<std::uint8_t> binaryPool;
	auto findings = validate_ddop(pool);

	CHECK(pool.generate_binary_object_pool(binaryPool));
	CHECK(0 == count(findings, DDOPValidationFinding::Severity::Error));
	CHECK(2 == times_fired(findings, "cycle of parent references"));
}

// A TC reads the default set by requesting DDI 0xDFFF, so marking members without it is unaskable.
// All 9 corpus pools that mark a member carry that object on element 0 offering every trigger method.
static void check_default_set_needs_its_request_object()
{
	auto missing = make_clean_pool(4);
	CHECK(std::static_pointer_cast<DeviceElementObject>(missing->get_object_by_id(1))->remove_reference_to_child_object(4));
	CHECK(missing->remove_object_by_id(4));
	CHECK(fires(validate_ddop(*missing), "no object for DDI 57343"));

	auto wrongTriggers = make_clean_pool(4);
	std::static_pointer_cast<DeviceProcessDataObject>(wrongTriggers->get_object_by_id(4))->set_trigger_methods_bitfield(0x01);
	CHECK(fires(validate_ddop(*wrongTriggers), "has to offer every trigger method"));

	auto wrongElement = make_clean_pool(4);
	CHECK(std::static_pointer_cast<DeviceElementObject>(wrongElement->get_object_by_id(1))->remove_reference_to_child_object(4));
	CHECK(wrongElement->add_device_element("Bin", 7, 1, DeviceElementObject::Type::Bin, 5));
	std::static_pointer_cast<DeviceElementObject>(wrongElement->get_object_by_id(5))->add_reference_to_child_object(4);
	CHECK(fires(validate_ddop(*wrongElement), "hangs off element number 7"));
}

// A TC addresses the device itself as element 0. Every pool in the corpus numbers its device element 0.
static void check_device_element_is_number_zero()
{
	auto pool = make_clean_pool(4);
	std::static_pointer_cast<DeviceElementObject>(pool->get_object_by_id(1))->set_element_number(1);
	CHECK(fires(validate_ddop(*pool), "has the type Device but element number 1"));
}

// The version 4 limit is 32 UTF-8 characters. The stack's 128 bytes is what 32 four-byte characters take.
static void check_version_4_designator_counts_characters()
{
	auto tooMany = make_clean_pool(4);
	tooMany->get_object_by_id(1)->set_designator(std::string(33, 'a'));
	CHECK(fires(validate_ddop(*tooMany), "33 UTF-8 characters"));

	auto atTheLimit = make_clean_pool(4);
	atTheLimit->get_object_by_id(1)->set_designator(std::string(32, 'a'));
	CHECK(validate_ddop(*atTheLimit).empty());

	// 32 two-byte characters is 64 bytes, which the old byte-only rule would have let through and this one must too
	std::string accented;
	for (int i = 0; i < 32; i++)
	{
		accented += "\xC3\xA9";
	}
	auto multiByte = make_clean_pool(4);
	multiByte->get_object_by_id(1)->set_designator(accented);
	CHECK(validate_ddop(*multiByte).empty());
}

// A scale of NaN or infinity passes a "scale <= 0" test, so it needs a test of its own.
static void check_non_finite_scale_is_reported()
{
	auto pool = make_clean_pool(4);
	std::static_pointer_cast<DeviceValuePresentationObject>(pool->get_object_by_id(2))->set_scale(std::numeric_limits<float>::quiet_NaN());
	CHECK(fires(validate_ddop(*pool), "scale that is not a finite number"));

	auto infinite = make_clean_pool(4);
	std::static_pointer_cast<DeviceValuePresentationObject>(infinite->get_object_by_id(2))->set_scale(std::numeric_limits<float>::infinity());
	CHECK(fires(validate_ddop(*infinite), "scale that is not a finite number"));
}

// The stack builds every device object with ID 0, so a device object ID does not survive being read back.
// Counting objects cannot see that; comparing the bytes can.
static void check_read_back_sees_a_dropped_field()
{
	auto pool = make_clean_pool(4);
	pool->get_object_by_id(0)->set_object_id(5);
	std::static_pointer_cast<DeviceElementObject>(pool->get_object_by_id(1))->set_parent_object(5);

	std::vector<std::uint8_t> binaryPool;
	CHECK(pool->generate_binary_object_pool(binaryPool));
	CHECK(fires(validate_ddop(*pool), "cannot be serialized again after being read back"));

	// A version 3 designator over 32 bytes is truncated on the way back in, which leaves the count untouched
	auto truncated = make_clean_pool(3);
	truncated->get_object_by_id(1)->set_designator(std::string(40, 'a'));

	std::vector<std::uint8_t> otherPool;
	CHECK(truncated->generate_binary_object_pool(otherPool));
	CHECK(fires(validate_ddop(*truncated), "does not reproduce the same bytes"));
}

// Every warning rule fires at least once, so none of them is dead code.
static void print_rule_coverage()
{
	isobus::DeviceDescriptorObjectPool pool(3);
	std::array<std::uint8_t, 7> badLocalization = { 'e', 'n', 0x50, 0, 0, 0, 0x00 };

	pool.add_device("", "sw", "sn", "", badLocalization, {}, 0);
	auto device = std::static_pointer_cast<DeviceObject>(pool.get_object_by_id(0));
	device->set_structure_label("TOOLONGLABEL");
	device->set_extended_structure_label(std::vector<std::uint8_t>(40, 'x'));

	pool.add_device_element("Root", 5000, 0, DeviceElementObject::Type::Device, 1);
	pool.add_device_element("Second root", 5000, 0, DeviceElementObject::Type::Device, 2);
	pool.add_device_element("A", 1, 4, DeviceElementObject::Type::Function, 3);
	pool.add_device_element("B", 2, 3, DeviceElementObject::Type::Function, 4);
	pool.add_device_process_data("Ghost", 40000, 0xFFFF, 0x86, 0xE1, 10);
	pool.add_device_property("Prop", 1, 0xFFFF, 0xFFFF, 11);
	std::static_pointer_cast<DevicePropertyObject>(pool.get_object_by_id(11))->set_ddi(0xFFFF);
	pool.add_device_value_presentation("unit", 0, 0.0f, 99, 12);
	pool.add_device_value_presentation("w", 0, 1.0f, 0, 13);

	// Set after adding, because add_device_value_presentation truncates a long designator itself
	pool.get_object_by_id(13)->set_designator(std::string(40, 'w'));
	pool.get_object_by_id(1)->set_designator(std::string(300, 'z'));

	auto findings = validate_ddop(pool);

	printf("\n=== every rule, on one deliberately broken pool ===\n");
	for (const auto &finding : findings)
	{
		printf("  %s %s\n", (DDOPValidationFinding::Severity::Error == finding.severity) ? "ERROR  " : "WARNING", finding.message.c_str());
	}
	check_every_rule_fires(findings, { "sits in a cycle of parent references", "element number 5000", "is used by device elements", "structure label is 12 bytes", "extended structure label is 40 bytes", "only a version 4 TC reads", "localization label is reserved", "ISO NAME is 0", "byte limit for a version 3 pool", "stored in a single byte", "all have the type Device", "all name the device object as their parent", "has the type Device but element number", "bundled copy of the ISO 11783-11 data dictionary", "which ISO 11783-11 reserves", "reserved bits in its properties bitfield", "reserved bits in its trigger methods bitfield", "both settable and a control source", "control source, which only a version 4 TC", "scale of 0.000000", "decimals", "not a child of any device element" });
}

int main(int argc, char **argv)
{
	check_clean_pool_is_silent();
	check_every_error_is_reported();
	check_parent_cycle_is_a_warning();
	check_default_set_needs_its_request_object();
	check_device_element_is_number_zero();
	check_version_4_designator_counts_characters();
	check_non_finite_scale_is_reported();
	check_read_back_sees_a_dropped_field();
	print_rule_coverage();

	// Any file paths on the command line are loaded and dumped too, which is how the rules were checked
	// against a corpus of real vendor DDOPs that cannot be redistributed here
	for (int i = 1; i < argc; i++)
	{
		auto data = isobus::IOPFileInterface::read_iop_file(argv[i]);
		auto pool = std::unique_ptr<isobus::DeviceDescriptorObjectPool>(new isobus::DeviceDescriptorObjectPool());
		bool loaded = false;

		for (std::uint8_t version : { 3, 4 })
		{
			pool->set_task_controller_compatibility_level(version);
			loaded = pool->deserialize_binary_object_pool(data, isobus::NAME(0));

			if (loaded)
			{
				break;
			}
			pool.reset(new isobus::DeviceDescriptorObjectPool());
		}
		printf("\n=== %s ===\n", argv[i]);

		if (!loaded)
		{
			printf("  could not be loaded as a version 3 or version 4 pool\n");
			continue;
		}

		for (const auto &finding : validate_ddop(*pool))
		{
			printf("  %s %s\n", (DDOPValidationFinding::Severity::Error == finding.severity) ? "ERROR  " : "WARNING", finding.message.c_str());
		}
	}
	printf("\nAll assertions passed.\n");
	return 0;
}
