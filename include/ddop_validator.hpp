//================================================================================================
/// @file ddop_validator.hpp
///
/// @brief Defines the ISO 11783-10 conformance checks that "Check for Errors" runs on a DDOP
/// @author Sujan Dumaru
///
/// @copyright 2026 The Open-Agriculture developers
//================================================================================================
#ifndef DDOP_VALIDATOR_HPP
#define DDOP_VALIDATOR_HPP

#include "isobus/isobus/isobus_device_descriptor_object_pool.hpp"

#include <cstdint>
#include <string>
#include <vector>

constexpr std::uint16_t PROPRIETARY_DDI_RANGE_START = 57344;
constexpr std::uint16_t PROPRIETARY_DDI_RANGE_END = 65534; // 65535 is reserved

/// @brief One problem found in a device descriptor object pool
struct DDOPValidationFinding
{
	/// @brief How badly the problem breaks the pool
	enum class Severity
	{
		Error, ///< The pool cannot be serialized at all
		Warning ///< The pool serializes, but a TC may reject it or read it wrongly
	};

	Severity severity; ///< How badly the problem breaks the pool
	std::string message; ///< What is wrong, naming the objects involved
};

/// @brief Checks a pool against the ISO 11783-10 rules a task controller applies to a DDOP
/// @details The stack's serialization stops at the first broken reference, so its failure conditions
/// are re-derived here to report all of them in one pass.
/// @param[in,out] pool The object pool to check. It is serialized, but never modified.
/// @returns Every finding, errors first
std::vector<DDOPValidationFinding> validate_ddop(isobus::DeviceDescriptorObjectPool &pool);

#endif // DDOP_VALIDATOR_HPP
