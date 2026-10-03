#include "MimeeBookshelfTheme.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalStorage.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "components/icons/blocks.h"
#include "components/icons/folder.h"
#include "components/icons/library.h"
#include "components/icons/settings2.h"
#include "components/icons/transfer.h"
#include "fontIds.h"
#include "util/BookProgress.h"

namespace {
constexpr int kIconSize = 32;
constexpr int kHighlightDiameter = 40;

const uint8_t* iconBitmap(UIIcon icon) {
  switch (icon) {
    case UIIcon::Folder:
      return FolderIcon;
    case UIIcon::Library:
      return LibraryIcon;
    case UIIcon::Transfer:
      return TransferIcon;
    case UIIcon::Settings:
      return Settings2Icon;
    case UIIcon::Blocks:
      return BlocksIcon;
    default:
      return nullptr;
  }
}
}  // namespace

int MimeeBookshelfTheme::homeCoverThumbHeight(const GfxRenderer& renderer) const {
  (void)renderer;
  // Thumbs are at least as wide as the slot (same rule as Lyra3Covers / Neko); the draw crops the overflow.
  return std::max(MimeeBookshelfLayout::COVER_H, MimeeBookshelfLayout::COVER_W * 5 / 3 + 2);
}

void MimeeBookshelfTheme::drawRecentBookCover(GfxRenderer& renderer, Rect rect,
                                              const std::vector<RecentBook>& recentBooks, const int selectorIndex,
                                              bool& coverRendered, bool& coverBufferStored, bool& bufferRestored,
                                              std::function<bool()> storeCoverBuffer) const {
  (void)bufferRestored;
  using namespace MimeeBookshelfLayout;
  const Geometry g = compute(rect.width, rect.y);
  const int bookCount = std::min(static_cast<int>(recentBooks.size()), MAX_BOOKS);
  const int thumbH = homeCoverThumbHeight(renderer);

  // Draws a book's thumbnail. fit=false fills the slot (crops overflow); fit=true shrinks to fit, left-aligned.
  const auto drawThumb = [&](const RecentBook& book, const Rect& slot, const bool fit, Rect& drawn) -> bool {
    drawn = slot;
    if (book.coverBmpPath.empty()) return false;
    const std::string thumbPath = UITheme::getCoverThumbPath(book.coverBmpPath, thumbH);
    HalFile file;
    if (!Storage.openFileForRead("HOME", thumbPath, file)) return false;
    bool ok = false;
    Bitmap bitmap(file);
    if (bitmap.parseHeaders() == BmpReaderError::Ok) {
      if (!fit) {
        ok = drawCoverThumbFill(renderer, bitmap, slot);
      } else {
        const float scale = std::min(1.0f, std::min(static_cast<float>(slot.width) / bitmap.getWidth(),
                                                    static_cast<float>(slot.height) / bitmap.getHeight()));
        const int dw = std::max(1, static_cast<int>(bitmap.getWidth() * scale));
        const int dh = std::max(1, static_cast<int>(bitmap.getHeight() * scale));
        const int dx = slot.x;
        const int dy = slot.y + (slot.height - dh) / 2;
        const auto clip = renderer.getClipRect();
        const int left = std::max(slot.x, clip[0]);
        const int top = std::max(slot.y, clip[1]);
        renderer.setClipRect(left, top, std::max(0, std::min(slot.x + slot.width, clip[0] + clip[2]) - left),
                             std::max(0, std::min(slot.y + slot.height, clip[1] + clip[3]) - top));
        ok = renderer.drawBitmap(bitmap, dx, dy, dw, dh);
        renderer.setClipRect(clip[0], clip[1], clip[2], clip[3]);
        drawn = Rect{dx, dy, dw, dh};
      }
    }
    file.close();
    return ok;
  };

  // Covers without a thumbnail show their title inside the empty rectangle.
  const auto drawTitleInside = [&](const std::string& title, const Rect& r, const int maxLines) {
    const auto lines = renderer.wrappedText(SMALL_FONT_ID, title.c_str(), r.width - 12, maxLines);
    const int lh = renderer.getLineHeight(SMALL_FONT_ID);
    int y = r.y + 10;
    for (const auto& line : lines) {
      renderer.drawText(SMALL_FONT_ID, r.x + 6, y, line.c_str(), true);
      y += lh;
    }
  };

  // Static layer (title + six covers): drawn once, cached by HomeActivity.
  if (!coverRendered) {
    renderer.drawText(UI_12_FONT_ID, g.cover[0].x, rect.y + 8, "Bookshelf", true);
    for (int i = 0; i < bookCount; ++i) {
      Rect drawn{};
      const bool hasCover = drawThumb(recentBooks[i], g.cover[i], false, drawn);
      renderer.drawRect(g.cover[i].x, g.cover[i].y, g.cover[i].width, g.cover[i].height, true);
      if (!hasCover) drawTitleInside(recentBooks[i].title, g.cover[i], 8);
    }
    coverBufferStored = storeCoverBuffer();
    coverRendered = coverBufferStored;
  }

  // Dynamic layer: selection ring around the chosen cover.
  if (selectorIndex >= 0 && selectorIndex < bookCount) {
    const Rect& b = g.cover[selectorIndex];
    renderer.drawRoundedRect(b.x - 5, b.y - 5, b.width + 10, b.height + 10, 3, 6, true);
  }

  // Dynamic layer: third row = selected book (the newest one while the icon bar has focus).
  if (bookCount > 0) {
    const RecentBook& shown = recentBooks[(selectorIndex >= 0 && selectorIndex < bookCount) ? selectorIndex : 0];
    Rect drawn{};
    const bool hasCover = drawThumb(shown, g.detailCover, true, drawn);
    renderer.drawRect(drawn.x, drawn.y, drawn.width, drawn.height, true);
    if (!hasCover) drawTitleInside(shown.title, drawn, 4);

    const int textX = drawn.x + drawn.width + 16;
    const int maxW = g.detail.x + g.detail.width - textX;
    const auto titleLines = renderer.wrappedText(UI_12_FONT_ID, shown.title.c_str(), maxW, 3);
    const auto authorLines = renderer.wrappedText(SMALL_FONT_ID, shown.author.c_str(), maxW, 1);
    const int progress = loadBookProgress(shown.path);  // -1 when unavailable
    const int lh1 = renderer.getLineHeight(UI_12_FONT_ID);
    const int lh2 = renderer.getLineHeight(SMALL_FONT_ID);
    constexpr int barH = 10;
    int block = static_cast<int>(titleLines.size()) * lh1;
    if (!authorLines.empty()) block += 4 + lh2;
    if (progress >= 0) block += 12 + barH;
    int y = std::max(g.detail.y, g.detail.y + (g.detail.height - block) / 2);
    for (const auto& line : titleLines) {
      renderer.drawText(UI_12_FONT_ID, textX, y, line.c_str(), true);
      y += lh1;
    }
    if (!authorLines.empty()) {
      renderer.drawText(SMALL_FONT_ID, textX, y + 4, authorLines[0].c_str(), true);
      y += 4 + lh2;
    }
    if (progress >= 0) {
      const int p = std::min(100, progress);
      const int barY = y + 12;
      const int barW = std::max(40, maxW - 64);
      renderer.drawRect(textX, barY, barW, barH, true);
      const int fill = (barW - 4) * p / 100;
      if (fill > 0) renderer.fillRect(textX + 2, barY + 2, fill, barH - 4, true);
      char text[8];
      snprintf(text, sizeof(text), "%d%%", p);
      renderer.drawText(SMALL_FONT_ID, textX + barW + 8, barY + barH / 2 - lh2 / 2, text, true);
    }
  }
}

void MimeeBookshelfTheme::drawButtonMenu(GfxRenderer& renderer, Rect rect, int buttonCount, int selectedIndex,
                                         const std::function<std::string(int index)>& buttonLabel,
                                         const std::function<UIIcon(int index)>& rowIcon) const {
  (void)buttonLabel;
  using namespace MimeeBookshelfLayout;
  if (buttonCount <= 0 || rowIcon == nullptr) return;
  const Geometry g = compute(rect.width, rect.y - TILE_HEIGHT - MENU_OFFSET);
  const Rect& bar = g.bar;
  renderer.drawRoundedRect(bar.x, bar.y, bar.width, bar.height, 2, bar.height / 2, true);
  for (int i = 0; i < buttonCount; ++i) {
    const int cx = bar.x + (2 * i + 1) * bar.width / (2 * buttonCount);
    const int cy = bar.y + bar.height / 2;
    if (i == selectedIndex) {
      renderer.fillRoundedRect(cx - kHighlightDiameter / 2, cy - kHighlightDiameter / 2, kHighlightDiameter,
                               kHighlightDiameter, kHighlightDiameter / 2, Color::LightGray);
    }
    const uint8_t* bitmap = iconBitmap(rowIcon(i));
    if (bitmap != nullptr) renderer.drawIcon(bitmap, cx - kIconSize / 2, cy - kIconSize / 2, kIconSize);
  }
}
