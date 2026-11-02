//================================================================================================
/// @file example_pool_test.cpp
///
/// @brief Checks that a DDOP shipped with this repository survives a load/save cycle
/// @author Sujan Dumaru
///
/// @copyright 2026 The Open-Agriculture developers
//================================================================================================

#include "isobus/isobus/isobus_device_descriptor_object_pool.hpp"
#include "task_data_import.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

int main(int argc, char **argv)
{
	const std::string version = (3 == argc) ? argv[2] : "4";
	const bool validArgumentCount = (2 == argc) || (3 == argc);
	const bool validVersion = ("3" == version) || ("4" == version);

	if (!validArgumentCount || !validVersion)
	{
		std::fprintf(stderr, "usage: %s <path to .ddop> [task controller version: 3 or 4 (default)]\n", argv[0]);
		return EXIT_FAILURE;
	}

	std::ifstream inputFile(argv[1], std::ios::binary);

	if (!inputFile)
	{
		std::fprintf(stderr, "FAIL: cannot open %s\n", argv[1]);
		return EXIT_FAILURE;
	}

	const std::vector<std::uint8_t> fileBytes((std::istreambuf_iterator<char>(inputFile)),
	                                          std::istreambuf_iterator<char>());
	std::vector<std::uint8_t> loadedBytes = fileBytes;

	isobus::DeviceDescriptorObjectPool pool(("4" == version) ? 4 : 3);

	if (!pool.deserialize_binary_object_pool(loadedBytes, isobus::NAME(0)))
	{
		std::fprintf(stderr, "FAIL: %s did not deserialize\n", argv[1]);
		return EXIT_FAILURE;
	}

	std::vector<std::uint8_t> savedBytes;

	if (!pool.generate_binary_object_pool(savedBytes))
	{
		std::fprintf(stderr, "FAIL: %s loaded but cannot be saved again\n", argv[1]);
		return EXIT_FAILURE;
	}

	if (savedBytes != fileBytes)
	{
		std::fprintf(stderr,
		             "FAIL: saving %s produced different bytes (%zu in, %zu out)\n",
		             argv[1],
		             fileBytes.size(),
		             savedBytes.size());
		return EXIT_FAILURE;
	}

	std::string exportedXml;
	isobus::DeviceDescriptorObjectPool importedPool(pool.get_task_controller_compatibility_level());
	std::vector<std::uint8_t> importedBytes;
	std::string reexportedXml;

	if (!pool.generate_task_data_iso_xml(exportedXml) ||
	    (1 != list_task_data_devices(exportedXml).size()) ||
	    !import_task_data_device(exportedXml, 0, importedPool) ||
	    !importedPool.generate_binary_object_pool(importedBytes) ||
	    !importedPool.generate_task_data_iso_xml(reexportedXml))
	{
		std::fprintf(stderr, "FAIL: %s did not survive an ISOXML export and import\n", argv[1]);
		return EXIT_FAILURE;
	}

	if (exportedXml != reexportedXml)
	{
		const auto offset = static_cast<std::size_t>(std::mismatch(exportedXml.begin(), exportedXml.end(), reexportedXml.begin(), reexportedXml.end()).first - exportedXml.begin());
		std::fprintf(stderr,
		             "FAIL: re-exporting the imported ISOXML of %s differs at byte %zu:\n%.60s\n%.60s\n",
		             argv[1],
		             offset,
		             exportedXml.c_str() + offset,
		             reexportedXml.c_str() + offset);
		return EXIT_FAILURE;
	}

	std::printf("PASS: %u objects, %zu bytes, byte-identical round trip, identical ISOXML round trip\n", pool.size(), savedBytes.size());
	return EXIT_SUCCESS;
}
