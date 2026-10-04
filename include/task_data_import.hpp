//================================================================================================
/// @file task_data_import.hpp
///
/// @brief Builds a DDOP from a device description in an ISOXML TASKDATA.XML
/// @author Sujan Dumaru
///
/// @copyright 2026 The Open-Agriculture developers
//================================================================================================
#ifndef TASK_DATA_IMPORT_HPP
#define TASK_DATA_IMPORT_HPP

#include "isobus/isobus/isobus_device_descriptor_object_pool.hpp"

#include <cstddef>
#include <string>
#include <vector>

/// The DVC elements of a TASKDATA.XML as "A  B" labels, in file order. Empty if it does not parse or has no inline DVC.
std::vector<std::string> list_task_data_devices(const std::string &taskData);

/// Adds the objects of the deviceIndex-th DVC to an empty pool whose TC level is already set.
bool import_task_data_device(const std::string &taskData, std::size_t deviceIndex, isobus::DeviceDescriptorObjectPool &pool);

#endif // TASK_DATA_IMPORT_HPP
