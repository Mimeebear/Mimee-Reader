#pragma once

#include "components/themes/lyra/LyraTheme.h"

// Geometry shared by drawing (MimeeNekoTheme.cpp) and touch hit-testing (HomeActivity).
// Designed on a 480x800 portrait screen; horizontal sizes follow the page width.
namespace MimeeNekoLayout {
constexpr int TILE_HEIGHT = 600;  // tile starts at homeTopPadding, covers cat + card + book frame
constexpr int MENU_OFFSET = 44;
constexpr int FRAME_H = 449;   // gap from tile bottom to the icon bar
constexpr int COLS = 3;
constexpr int ROWS = 2;
constexpr int MAX_BOOKS = COLS * ROWS;
constexpr int BOOK_H = 200;
constexpr int BAR_H = 48;

struct Geometry {
  Rect cat;
  Rect card;
  Rect frame;
  Rect bar;
  Rect strip;  // one-line title of the selected book, between frame and icon bar
  Rect cell[MAX_BOOKS];  // whole grid cell (touch target)
  Rect book[MAX_BOOKS];  // cover rectangle inside the cell
};

inline Geometry compute(const int pageWidth, const int tileY) {
  Geometry g;
  const int sideMargin = 16;
  g.frame = Rect{sideMargin, tileY + 145, pageWidth - 2 * sideMargin, FRAME_H};
  g.cat = Rect{22, tileY + 4, 164, 143};
  g.card = Rect{200, tileY + 14, pageWidth - 200 - 32, 110};
  g.bar = Rect{g.frame.x, tileY + TILE_HEIGHT + MENU_OFFSET, g.frame.width, BAR_H};
  g.strip = Rect{g.frame.x, g.frame.y + g.frame.height, g.frame.width, g.bar.y - (g.frame.y + g.frame.height)};
  const int pad = 12;
  const int colW = (g.frame.width - 2 * pad) / COLS;
  const int rowH = (g.frame.height - 2 * pad) / ROWS;
  const int bookW = colW - 12;
  for (int r = 0; r < ROWS; ++r) {
    for (int c = 0; c < COLS; ++c) {
      const int i = r * COLS + c;
      g.cell[i] = Rect{g.frame.x + pad + c * colW, g.frame.y + pad + r * rowH, colW, rowH};
      g.book[i] = Rect{g.cell[i].x + (colW - bookW) / 2, g.cell[i].y + (rowH - BOOK_H) / 2, bookW, BOOK_H};
    }
  }
  return g;
}
}  // namespace MimeeNekoLayout

namespace MimeeNekoMetrics {
constexpr ThemeMetrics values = [] {
  ThemeMetrics v = LyraMetrics::values;
  v.homeTopPadding = 56;
  v.homeCoverHeight = MimeeNekoLayout::BOOK_H;
  v.homeCoverTileHeight = MimeeNekoLayout::TILE_HEIGHT;
  v.homeRecentBooksCount = MimeeNekoLayout::MAX_BOOKS;
  v.homeContinueReadingInMenu = false;
  v.homeMenuTopOffset = MimeeNekoLayout::MENU_OFFSET;
  return v;
}();
}  // namespace MimeeNekoMetrics

class MimeeNekoTheme : public LyraTheme {
 public:
  void drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                           const int selectorIndex, bool& coverRendered, bool& coverBufferStored,
                           bool& bufferRestored, std::function<bool()> storeCoverBuffer) const override;
  void drawButtonMenu(GfxRenderer& renderer, Rect rect, int buttonCount, int selectedIndex,
                      const std::function<std::string(int index)>& buttonLabel,
                      const std::function<UIIcon(int index)>& rowIcon) const override;
  int homeCoverThumbHeight(const GfxRenderer& renderer) const override;
  bool hasNekoGridHome() const override { return true; }
};
