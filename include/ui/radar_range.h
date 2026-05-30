#pragma once

#include <cstddef>
#include <cstdint>

namespace ui::radar {

/**
 * Range presets (label on ring 3 = ¾ of outer radius).
 *
 * Recommended for ADS-B on a 1.28″ display:
 *   5 km  — pattern / very local (airfield vicinity)
 *  10 km  — default; neighborhood spotting
 *  20 km  — wider metro picture
 *
 * Outer radius (for aircraft math) is ring-3 distance ÷ 0.75.
 */
struct RangePreset {
  const char* ring3_label;
  float outer_km;
};

constexpr RangePreset kRangePresets[] = {
    {"5km", 6.7f},
    {"10km", 13.3f},
    {"20km", 26.7f},
};

constexpr size_t kRangePresetCount =
    sizeof(kRangePresets) / sizeof(kRangePresets[0]);

/** Load saved range from flash (or default). Call once after boot. */
void rangeInit();
/** Cycle preset and save to flash. */
void rangeNext();
const RangePreset& rangeCurrent();
uint8_t rangeIndex();

}  // namespace ui::radar
