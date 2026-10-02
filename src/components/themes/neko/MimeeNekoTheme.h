#pragma once

#include "components/themes/lyra/LyraTheme.h"

namespace MimeeNekoMetrics {
constexpr ThemeMetrics values = [] {
  ThemeMetrics v = LyraMetrics::values;
  return v;  // layout tuning comes in step 2
}();
}  // namespace MimeeNekoMetrics

class MimeeNekoTheme : public LyraTheme {};
