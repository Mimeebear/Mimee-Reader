#pragma once

#include "components/themes/lyra/LyraTheme.h"

// Layout designed on a 480x800 portrait screen; horizontal positions follow the page width.
namespace MimeeBookshelfLayout {
constexpr int COLS = 3;
constexpr int ROWS = 2;
constexpr int MAX_BOOKS = COLS * ROWS;
constexpr int COVER_W = 141;
constexpr int COVER_H = 212;
constexpr int GAP = 10;
constexpr int GRID_TOP = 54;  // from the tile top (room for the "Bookshelf" title)
constexpr int DETAIL_COVER_W = 91;
constexpr int DETAIL_H = 136;
constexpr int TILE_HEIGHT = 634;  // title + 2 cover rows + detail row
constexpr int MENU_OFFSET = 10;   // gap from tile bottom to the icon bar
constexpr int BAR_H = 48;

struct Geometry {
  Rect cell[MAX_BOOKS];   // touch targets: contiguous, equal-width cells
  Rect cover[MAX_BOOKS];  // cover rectangles
  Rect detail;            // third row (selected book)
  Rect detailCover;       // slot for the small cover
  Rect bar;               // icon bar
};

inline Geometry compute(const int pageWidth, const int tileY) {
  Geometry g;
  const int step = COVER_W + GAP;
  const int left = (pageWidth - (COLS * COVER_W + (COLS - 1) * GAP)) / 2;
  for (int r = 0; r < ROWS; ++r) {
    const int rowY = tileY + GRID_TOP + r * (COVER_H + GAP);
    for (int c = 0; c < COLS; ++c) {
      const int i = r * COLS + c;
      g.cover[i] = Rect{left + c * step, rowY, COVER_W, COVER_H};
      g.cell[i] = Rect{left - GAP / 2 + c * step, rowY - GAP / 2, step, COVER_H + GAP};
    }
  }
  const int detailY = tileY + GRID_TOP + ROWS * (COVER_H + GAP);
  g.detail = Rect{left, detailY, pageWidth - 2 * left, DETAIL_H};
  g.detailCover = Rect{left, detailY, DETAIL_COVER_W, DETAIL_H};
  g.bar = Rect{left, tileY + TILE_HEIGHT + MENU_OFFSET, pageWidth - 2 * left, BAR_H};
  return g;
}
}  // namespace MimeeBookshelfLayout

namespace MimeeBookshelfMetrics {
constexpr ThemeMetrics values = [] {
  ThemeMetrics v = LyraMetrics::values;
  v.homeTopPadding = 56;
  v.homeCoverHeight = MimeeBookshelfLayout::COVER_H;
  v.homeCoverTileHeight = MimeeBookshelfLayout::TILE_HEIGHT;
  v.homeRecentBooksCount = MimeeBookshelfLayout::MAX_BOOKS;
  v.homeContinueReadingInMenu = false;
  v.homeMenuTopOffset = MimeeBookshelfLayout::MENU_OFFSET;
  return v;
}();
}  // namespace MimeeBookshelfMetrics

class MimeeBookshelfTheme : public LyraTheme {
 public:
  void drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                           const int selectorIndex, bool& coverRendered, bool& coverBufferStored,
                           bool& bufferRestored, std::function<bool()> storeCoverBuffer) const override;
  void drawButtonMenu(GfxRenderer& renderer, Rect rect, int buttonCount, int selectedIndex,
                      const std::function<std::string(int index)>& buttonLabel,
                      const std::function<UIIcon(int index)>& rowIcon) const override;
  int homeCoverThumbHeight(const GfxRenderer& renderer) const override;
  bool hasBookshelfHome() const override { return true; }
};
