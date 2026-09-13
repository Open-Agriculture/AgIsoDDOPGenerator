//================================================================================================
/// @file example_pool_test.cpp
///
/// @brief Checks that a DDOP shipped with this repository survives a load/save cycle
/// @author Sujan Dumaru
///
/// @copyright 2026 The Open-Agriculture developers
//================================================================================================
#include "isobus/isobus/isobus_device_descriptor_object_pool.hpp"

#include <cstdint>
#include <cstdio>
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
		std::fprintf(stderr, "usage: %s <path to .iop> [task controller version: 3 or 4 (default)]\n", argv[0]);
		return 1;
	}

	std::ifstream inputFile(argv[1], std::ios::binary);

	if (!inputFile)
	{
		std::fprintf(stderr, "FAIL: cannot open %s\n", argv[1]);
		return 1;
	}

	const std::vector<std::uint8_t> fileBytes((std::istreambuf_iterator<char>(inputFile)),
	                                          std::istreambuf_iterator<char>());
	std::vector<std::uint8_t> loadedBytes = fileBytes;

	isobus::DeviceDescriptorObjectPool pool(("4" == version) ? 4 : 3);

	if (!pool.deserialize_binary_object_pool(loadedBytes, isobus::NAME(0)))
	{
		std::fprintf(stderr, "FAIL: %s did not deserialize\n", argv[1]);
		return 1;
	}

	std::vector<std::uint8_t> savedBytes;

	if (!pool.generate_binary_object_pool(savedBytes))
	{
		std::fprintf(stderr, "FAIL: %s loaded but cannot be saved again\n", argv[1]);
		return 1;
	}

	if (savedBytes != fileBytes)
	{
		std::fprintf(stderr,
		             "FAIL: saving %s produced different bytes (%zu in, %zu out)\n",
		             argv[1],
		             fileBytes.size(),
		             savedBytes.size());
		return 1;
	}

	std::printf("PASS: %u objects, %zu bytes, byte-identical round trip\n", pool.size(), savedBytes.size());
	return 0;
}
