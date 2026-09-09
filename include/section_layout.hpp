//================================================================================================
/// @file section_layout.hpp
///
/// @brief Defines the boom and section layout drawn across the implement's lateral (Y) axis
/// @author Sujan Dumaru
///
/// @copyright 2026 The Open-Agriculture developers
//================================================================================================
#ifndef SECTION_LAYOUT_HPP
#define SECTION_LAYOUT_HPP

#include "isobus/isobus/isobus_device_descriptor_object_pool.hpp"

#include <cstdint>

/// @brief Renders the booms and sections described by a DDOP across the lateral (Y) axis.
/// Draws to scale when the pool stores static offsets and positive widths; otherwise draws an
/// evenly spaced schematic.
/// @param[in] pool The object pool whose section layout is drawn
/// @param[in,out] selectedObjectID The selected object, updated when a section is clicked
/// @returns true when a section click changed the selected object
bool render_section_layout(isobus::DeviceDescriptorObjectPool &pool, std::uint16_t &selectedObjectID);

#endif // SECTION_LAYOUT_HPP
