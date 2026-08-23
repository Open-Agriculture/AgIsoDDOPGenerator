//================================================================================================
/// @file section_layout.cpp
///
/// @brief Draws the boom and section layout across the implement's lateral (Y) axis
/// @author Sujan Dumaru
///
/// @copyright 2026 The Open-Agriculture developers
//================================================================================================
#include "section_layout.hpp"

#include "imgui.h"
#include "isobus/isobus/isobus_device_descriptor_object_pool_helpers.hpp"
#include "isobus/isobus/isobus_standard_data_description_indices.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

/// @brief One section, flattened out of the boom/sub-boom hierarchy and ready to draw
struct FlatSection
{
	std::string designator;
	std::int32_t xOffset_mm = 0;
	std::int32_t yOffset_mm = 0;
	std::int32_t width_mm = 0;
	std::uint16_t objectID = 0xFFFF;
	std::uint16_t elementNumber = 0;
	bool hasXOffset = false;
	bool hasYOffset = false;
	bool hasWidth = false;
	bool hasInvalidWidth = false;
	bool usesMaximumWidth = false;
};

using ElementsByNumber = std::map<std::uint16_t, std::shared_ptr<isobus::task_controller_object::DeviceElementObject>>;
using Span = std::pair<float, float>; ///< The left and right edge of one section along the Y axis

constexpr float CANVAS_HEIGHT = 124.0f;
constexpr float CANVAS_PADDING = 12.0f;
constexpr float BAR_TOP = 40.0f;
constexpr float BAR_HEIGHT = 42.0f;
constexpr float RULER_TOP = 92.0f;
constexpr float MINIMUM_BAR_WIDTH = 2.0f;
constexpr float MINIMUM_TICK_SPACING = 4.0f;
constexpr float MINIMUM_LABEL_SPACING = 56.0f;
constexpr float MINIMUM_LABEL_STEP_MM = 100.0f;
constexpr int MAXIMUM_LABEL_STEPS = 60;
constexpr float KILOMETRE_MM = 1000000.0f;
constexpr ImU32 SELECTED_BORDER_COLOR = IM_COL32(96, 205, 255, 255);
constexpr ImU32 OVERLAP_FILL_COLOR = IM_COL32(210, 62, 62, 132);
constexpr ImU32 OVERLAP_BORDER_COLOR = IM_COL32(245, 92, 82, 255);
constexpr ImU32 GAP_FILL_COLOR = IM_COL32(224, 166, 48, 84);
constexpr ImU32 GAP_BORDER_COLOR = IM_COL32(245, 194, 72, 255);

/// @brief Builds a lookup of every device element in the pool keyed by its element number.
/// The DDOP helper reports geometry by element number but the pool can only be searched by
/// object ID or index, so without this every section lookup would rescan the whole pool.
/// @param[in] pool The object pool to index
/// @returns Every device element in the pool, keyed by element number
static ElementsByNumber map_elements_by_number(isobus::DeviceDescriptorObjectPool &pool)
{
	ElementsByNumber retVal;

	for (std::uint16_t i = 0; i < pool.size(); i++)
	{
		auto object = pool.get_object_by_index(i);

		if ((nullptr != object) &&
		    (isobus::task_controller_object::ObjectTypes::DeviceElement == object->get_object_type()))
		{
			auto element = std::static_pointer_cast<isobus::task_controller_object::DeviceElementObject>(object);
			retVal.emplace(element->get_element_number(), element);
		}
	}
	return retVal;
}

/// @brief Reads a static device property value off an element by DDI.
/// @param[in] pool The object pool the element belongs to
/// @param[in] element The element whose properties to search
/// @param[in] ddi The DDI to look for
/// @param[out] value The property's value, untouched if the element has no such property
/// @returns true if the element has a device property with that DDI
static bool get_property_value(isobus::DeviceDescriptorObjectPool &pool,
                               const std::shared_ptr<isobus::task_controller_object::DeviceElementObject> &element,
                               isobus::DataDescriptionIndex ddi,
                               std::int32_t &value)
{
	if (nullptr == element)
	{
		return false;
	}

	for (std::uint16_t i = 0; i < element->get_number_child_objects(); i++)
	{
		auto child = pool.get_object_by_id(element->get_child_object_id(i));

		if ((nullptr != child) &&
		    (isobus::task_controller_object::ObjectTypes::DeviceProperty == child->get_object_type()))
		{
			auto property = std::static_pointer_cast<isobus::task_controller_object::DevicePropertyObject>(child);

			if (static_cast<std::uint16_t>(ddi) == property->get_ddi())
			{
				value = property->get_value();
				return true;
			}
		}
	}
	return false;
}

/// @brief Converts one section reported by the DDOP helper into a drawable section.
/// @param[in] pool The object pool the section belongs to
/// @param[in] elements The pool's elements keyed by element number
/// @param[in] section The section reported by the helper
/// @returns The section, ready to draw
static FlatSection flatten_section(isobus::DeviceDescriptorObjectPool &pool,
                                   const ElementsByNumber &elements,
                                   const isobus::DeviceDescriptorObjectPoolHelper::Section &section)
{
	FlatSection retVal;

	retVal.elementNumber = section.elementNumber;
	retVal.hasXOffset = section.xOffset_mm.exists();
	retVal.hasYOffset = section.yOffset_mm.exists();
	retVal.xOffset_mm = section.xOffset_mm.get();
	retVal.yOffset_mm = section.yOffset_mm.get();

	auto match = elements.find(section.elementNumber);
	auto element = (elements.end() != match) ? match->second : nullptr;

	if (nullptr != element)
	{
		retVal.designator = element->get_designator();
		retVal.objectID = element->get_object_id();
	}

	if (section.width_mm.exists())
	{
		retVal.width_mm = section.width_mm.get();
	}
	else
	{
		// The helper only reads DDI 67, so implement sections that declare their width with the
		// static DDI 70 maximum would otherwise have no width at all. Drop this once AgIsoStack
		// #530 or #618 merges.
		std::int32_t width = 0;

		if (get_property_value(pool, element, isobus::DataDescriptionIndex::MaximumWorkingWidth, width))
		{
			retVal.width_mm = width;
			retVal.usesMaximumWidth = true;
		}
		else
		{
			return retVal;
		}
	}
	retVal.hasWidth = (0 < retVal.width_mm);
	retVal.hasInvalidWidth = !retVal.hasWidth;
	return retVal;
}

/// @brief Collects every section of a boom, including those held by its sub booms.
/// @param[in] pool The object pool the boom belongs to
/// @param[in] elements The pool's elements keyed by element number
/// @param[in] boom The boom reported by the helper
/// @returns Every section of the boom, ready to draw
static std::vector<FlatSection> flatten_boom_sections(isobus::DeviceDescriptorObjectPool &pool,
                                                      const ElementsByNumber &elements,
                                                      const isobus::DeviceDescriptorObjectPoolHelper::Boom &boom)
{
	std::vector<FlatSection> retVal;

	for (const auto &section : boom.sections)
	{
		retVal.push_back(flatten_section(pool, elements, section));
	}

	for (const auto &subBoom : boom.subBooms)
	{
		for (const auto &section : subBoom.sections)
		{
			retVal.push_back(flatten_section(pool, elements, section));
		}
	}
	return retVal;
}

/// @brief Shows one offset in metres, or says the DDOP does not carry it.
/// @param[in] label What the offset describes
/// @param[in] offset_mm The offset in mm
/// @param[in] exists Whether the DDOP actually stored the offset
static void render_offset_line(const char *label, std::int32_t offset_mm, bool exists)
{
	if (exists)
	{
		ImGui::Text("%s: %+.3f m", label, offset_mm / 1000.0f);
	}
	else
	{
		ImGui::Text("%s: not stored in this DDOP", label);
	}
}

/// @brief Shows the detail of one section as a tooltip.
/// @param[in] section The section being hovered
/// @param[in] toScale Whether the section is being drawn at its real offset and width
static void render_section_tooltip(const FlatSection &section, bool toScale)
{
	const char *widthSource = section.usesMaximumWidth ? "DDI 70 Maximum" : "DDI 67 Actual";

	ImGui::BeginTooltip();
	ImGui::Text("Element %u%s%s", section.elementNumber, section.designator.empty() ? "" : " - ", section.designator.c_str());
	ImGui::Separator();
	render_offset_line("Lateral offset", section.yOffset_mm, section.hasYOffset);
	render_offset_line("Fore/aft offset", section.xOffset_mm, section.hasXOffset);

	if (section.hasInvalidWidth)
	{
		ImGui::Text("Width (%s): invalid %.3f m; must be greater than zero", widthSource, section.width_mm / 1000.0f);
	}
	else if (section.hasWidth)
	{
		ImGui::Text("Width (%s): %.3f m", widthSource, section.width_mm / 1000.0f);
	}
	else
	{
		ImGui::TextUnformatted("Width: not stored in this DDOP");
	}

	if (!toScale)
	{
		ImGui::Separator();
		ImGui::TextUnformatted("Drawn evenly spaced because this boom is not fully dimensioned.");
	}
	ImGui::EndTooltip();
}

/// @brief Draws the sections of one boom onto an ImGui canvas.
/// @param[in] sections The sections to draw
/// @param[in] toScale Whether every section has a static offset and width
/// @param[in,out] selectedObjectID The selected object, updated when a section is clicked
/// @returns true when a section click changed the selected object
static bool draw_sections(const std::vector<FlatSection> &sections, bool toScale, std::uint16_t &selectedObjectID)
{
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const float canvasWidth = std::max(ImGui::GetContentRegionAvail().x, 120.0f);
	const float top = origin.y + BAR_TOP;
	ImDrawList *drawList = ImGui::GetWindowDrawList();

	// Every stock theme makes ImGuiCol_FrameBg translucent, which would let the object editor
	// behind this window read straight through the diagram.
	ImVec4 canvasColor = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
	canvasColor.w = 1.0f;
	drawList->AddRectFilled(origin, ImVec2(origin.x + canvasWidth, origin.y + CANVAS_HEIGHT), ImGui::GetColorU32(canvasColor));

	std::vector<Span> spans;
	float minimum_mm = 0.0f;
	float maximum_mm = toScale ? 0.0f : static_cast<float>(sections.size());
	spans.reserve(sections.size());

	for (std::size_t i = 0; i < sections.size(); i++)
	{
		if (toScale)
		{
			const float halfWidth_mm = sections[i].width_mm / 2.0f;
			spans.emplace_back(sections[i].yOffset_mm - halfWidth_mm, sections[i].yOffset_mm + halfWidth_mm);
			minimum_mm = std::min(minimum_mm, spans.back().first);
			maximum_mm = std::max(maximum_mm, spans.back().second);
		}
		else
		{
			spans.emplace_back(static_cast<float>(i), static_cast<float>(i) + 0.92f);
		}
	}

	const float scale = (canvasWidth - (2.0f * CANVAS_PADDING)) / std::max(maximum_mm - minimum_mm, 1.0f);
	const auto toX = [origin, minimum_mm, scale](float value_mm) {
		return origin.x + CANVAS_PADDING + ((value_mm - minimum_mm) * scale);
	};
	const auto barRectangle = [&spans, &toX, top](std::size_t index) {
		const float left = toX(spans[index].first);
		return std::make_pair(ImVec2(left, top),
		                      ImVec2(std::max(toX(spans[index].second), left + MINIMUM_BAR_WIDTH), top + BAR_HEIGHT));
	};
	std::size_t hoveredIndex = sections.size();

	for (std::size_t i = 0; i < sections.size(); i++)
	{
		const auto rectangle = barRectangle(i);
		const ImU32 alternatingFill = (0 == (i % 2)) ? IM_COL32(84, 138, 92, 255) : IM_COL32(62, 110, 70, 255);

		drawList->AddRectFilled(rectangle.first, rectangle.second, toScale ? alternatingFill : IM_COL32(96, 96, 104, 255));
		drawList->AddRect(rectangle.first, rectangle.second, ImGui::GetColorU32(ImGuiCol_Border));

		if (ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(rectangle.first, rectangle.second))
		{
			hoveredIndex = i;
		}

		char label[8] = { 0 };
		std::snprintf(label, sizeof(label), "%u", sections[i].elementNumber);
		const ImVec2 labelSize = ImGui::CalcTextSize(label);

		if (labelSize.x < ((rectangle.second.x - rectangle.first.x) - 4.0f))
		{
			drawList->AddText(ImVec2(((rectangle.first.x + rectangle.second.x) - labelSize.x) / 2.0f, top + ((BAR_HEIGHT - labelSize.y) / 2.0f)),
			                  ImGui::GetColorU32(ImGuiCol_Text),
			                  label);
		}
	}

	if (toScale)
	{
		std::vector<Span> sortedSpans = spans;
		std::sort(sortedSpans.begin(), sortedSpans.end());

		const auto drawDiagnosticSpan = [drawList, &toX, top](float left_mm, float right_mm, ImU32 fill, ImU32 border) {
			const float left = toX(left_mm);
			const float right = toX(right_mm);
			drawList->AddRectFilled(ImVec2(left, top), ImVec2(right, top + BAR_HEIGHT), fill);
			drawList->AddLine(ImVec2(left, top), ImVec2(left, top + BAR_HEIGHT), border, 2.0f);
			drawList->AddLine(ImVec2(right, top), ImVec2(right, top + BAR_HEIGHT), border, 2.0f);
		};

		float coveredRight_mm = sortedSpans.front().second;
		for (std::size_t i = 1; i < sortedSpans.size(); i++)
		{
			if (sortedSpans[i].first < coveredRight_mm)
			{
				const float overlapRight_mm = std::min(coveredRight_mm, sortedSpans[i].second);

				if (sortedSpans[i].first < overlapRight_mm)
				{
					drawDiagnosticSpan(sortedSpans[i].first, overlapRight_mm, OVERLAP_FILL_COLOR, OVERLAP_BORDER_COLOR);
				}
			}
			else if (coveredRight_mm < sortedSpans[i].first)
			{
				drawDiagnosticSpan(coveredRight_mm, sortedSpans[i].first, GAP_FILL_COLOR, GAP_BORDER_COLOR);
			}
			coveredRight_mm = std::max(coveredRight_mm, sortedSpans[i].second);
		}

		const float rulerY = origin.y + RULER_TOP;
		drawList->AddLine(ImVec2(origin.x + CANVAS_PADDING, rulerY),
		                  ImVec2((origin.x + canvasWidth) - CANVAS_PADDING, rulerY),
		                  ImGui::GetColorU32(ImGuiCol_TextDisabled));

		// A DDOP may describe anything from a hand boom to an implement kilometres wide, so a
		// fixed step leaves the small pools with a lone zero and smears the large ones. Grow
		// through 0.1, 0.2, 0.5, 1, 2, 5 m and so on until the labels are far enough to read.
		float labelStep_mm = MINIMUM_LABEL_STEP_MM;

		for (int i = 0; (i < MAXIMUM_LABEL_STEPS) && ((labelStep_mm * scale) < MINIMUM_LABEL_SPACING); i++)
		{
			labelStep_mm *= ((1 == (i % 3)) ? 2.5f : 2.0f);
		}

		const float tickStep_mm = labelStep_mm / 5.0f;
		const bool inKilometres = (KILOMETRE_MM <= labelStep_mm);
		const float labelDivisor = inKilometres ? KILOMETRE_MM : 1000.0f;
		const char *labelUnit = inKilometres ? "km" : "m";

		// A degenerate canvas leaves the scale at or below zero, which no step can satisfy.
		if (MINIMUM_TICK_SPACING < (tickStep_mm * scale))
		{
			const int lastTick = static_cast<int>(std::floor(maximum_mm / tickStep_mm));

			for (int tick = static_cast<int>(std::ceil(minimum_mm / tickStep_mm)); tick <= lastTick; tick++)
			{
				const bool isLabelled = (0 == (tick % 5));
				const float tickX = toX(tick * tickStep_mm);

				drawList->AddLine(ImVec2(tickX, rulerY),
				                  ImVec2(tickX, rulerY + (isLabelled ? 8.0f : 4.0f)),
				                  ImGui::GetColorU32(isLabelled ? ImGuiCol_Text : ImGuiCol_TextDisabled));

				if (isLabelled)
				{
					char label[16] = { 0 };
					std::snprintf(label, sizeof(label), "%g %s", (tick * tickStep_mm) / labelDivisor, labelUnit);
					const ImVec2 labelSize = ImGui::CalcTextSize(label);
					drawList->AddText(ImVec2(tickX - (labelSize.x / 2.0f), rulerY + 9.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled), label);
				}
			}
		}
	}

	bool selectionChanged = false;

	if (hoveredIndex < sections.size())
	{
		// Only the topmost section may claim the cursor. A second BeginTooltip in the same frame
		// reopens the same tooltip window, so overlapping sections would append into one another.
		render_section_tooltip(sections[hoveredIndex], toScale);

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && (0xFFFF != sections[hoveredIndex].objectID))
		{
			selectionChanged = (selectedObjectID != sections[hoveredIndex].objectID);
			selectedObjectID = sections[hoveredIndex].objectID;
		}
	}

	for (std::size_t i = 0; i < sections.size(); i++)
	{
		if ((0xFFFF != sections[i].objectID) && (selectedObjectID == sections[i].objectID))
		{
			const auto rectangle = barRectangle(i);
			drawList->AddRect(rectangle.first, rectangle.second, SELECTED_BORDER_COLOR, 0.0f, 0, 3.0f);
		}
	}

	ImGui::Dummy(ImVec2(canvasWidth, CANVAS_HEIGHT));
	return selectionChanged;
}

/// @brief Shows one of a boom's own offsets on its header line.
/// @param[in] label What the offset describes
/// @param[in] offset_mm The offset reported by the helper
static void render_boom_offset(const char *label, const isobus::DeviceDescriptorObjectPoolHelper::ObjectPoolValue &offset_mm)
{
	ImGui::SameLine();

	if (offset_mm.exists())
	{
		ImGui::Text("| %s %+.3f m", label, offset_mm.get() / 1000.0f);
	}
	else
	{
		ImGui::TextDisabled("| %s not stored", label);
	}
}

/// @brief Renders the heading and canvas for one boom.
/// @param[in] pool The object pool the boom belongs to
/// @param[in] elements The pool's elements keyed by element number
/// @param[in] boom The boom reported by the helper
/// @param[in,out] selectedObjectID The selected object, updated when a section is clicked
/// @returns true when a section click changed the selected object
static bool render_boom(isobus::DeviceDescriptorObjectPool &pool,
                        const ElementsByNumber &elements,
                        const isobus::DeviceDescriptorObjectPoolHelper::Boom &boom,
                        std::uint16_t &selectedObjectID)
{
	auto sections = flatten_boom_sections(pool, elements, boom);
	auto match = elements.find(boom.elementNumber);
	std::string designator = (elements.end() != match) ? match->second->get_designator() : std::string();

	ImGui::SeparatorText(designator.empty() ? "Boom" : designator.c_str());
	ImGui::Text("Element %u, %zu section%s", boom.elementNumber, sections.size(), (1 == sections.size()) ? "" : "s");
	render_boom_offset("Fore/aft", boom.xOffset_mm);
	render_boom_offset("Lateral", boom.yOffset_mm);

	if (sections.empty())
	{
		ImGui::TextDisabled("This function element has no section elements to draw.");
		return false;
	}

	const bool toScale = std::all_of(sections.cbegin(), sections.cend(), [](const FlatSection &section) {
		return section.hasYOffset && section.hasWidth;
	});
	const bool hasInvalidWidth = std::any_of(sections.cbegin(), sections.cend(), [](const FlatSection &section) {
		return section.hasInvalidWidth;
	});

	if (!toScale)
	{
		std::sort(sections.begin(), sections.end(), [](const FlatSection &left, const FlatSection &right) {
			return left.elementNumber < right.elementNumber;
		});
	}

	ImGui::TextUnformatted("Rear view - lateral (Y) axis only");

	if (hasInvalidWidth)
	{
		ImGui::TextColored(ImVec4(0.95f, 0.38f, 0.30f, 1.0f), "INVALID GEOMETRY - section widths must be greater than zero");
	}

	if (!toScale)
	{
		ImGui::TextUnformatted("SCHEMATIC - not to scale");
	}
	return draw_sections(sections, toScale, selectedObjectID);
}

bool render_section_layout(isobus::DeviceDescriptorObjectPool &pool, std::uint16_t &selectedObjectID)
{
	auto implement = isobus::DeviceDescriptorObjectPoolHelper::get_implement_geometry(pool);

	if (implement.booms.empty())
	{
		ImGui::TextWrapped("This DDOP has no device object, so it describes no geometry.");
		return false;
	}

	auto elements = map_elements_by_number(pool);
	bool selectionChanged = false;

	for (std::size_t i = 0; i < implement.booms.size(); i++)
	{
		ImGui::PushID(static_cast<int>(i));

		if (render_boom(pool, elements, implement.booms[i], selectedObjectID))
		{
			selectionChanged = true;
		}
		ImGui::PopID();
	}
	return selectionChanged;
}
