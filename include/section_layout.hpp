//================================================================================================
/// @file section_layout.hpp
///
/// @brief Defines the boom and section layout drawn across the implement's lateral (Y) axis
/// @author Sujan Dumaru
///
/// @copyright 2026 The Open-Agriculture developers
//================================================================================================

#pragma once

#include "isobus/isobus/isobus_device_descriptor_object_pool.hpp"

#include <cstdint>

/// @brief Renders a DDOP's booms and sections along the lateral (Y) axis, to scale when fully dimensioned.
/// @param[in] pool The object pool whose section layout is drawn
/// @param[in,out] selectedObjectID The selected object, updated when a section is clicked
/// @returns true when a section click changed the selected object
bool render_section_layout(isobus::DeviceDescriptorObjectPool &pool, std::uint16_t &selectedObjectID);
