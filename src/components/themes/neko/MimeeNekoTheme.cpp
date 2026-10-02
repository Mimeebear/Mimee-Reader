#include "MimeeNekoTheme.h"

#include <GfxRenderer.h>
#include <HalStorage.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "NekoArt.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "components/icons/blocks.h"
#include "components/icons/folder.h"
#include "components/icons/library.h"
#include "components/icons/settings2.h"
#include "components/icons/transfer.h"
#include "fontIds.h"

static_assert(NekoArt::NekoCatW == 164 && NekoArt::NekoCatH == 143, "cat size must match MimeeNekoLayout");

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

// 1bpp MSB-first art with a mask layer: mask bit set = opaque, ink bit set = black.
void drawMasked(const GfxRenderer& renderer, const int x, const int y, const int w, const int h, const uint8_t* ink,
                const uint8_t* mask) {
  const int stride = (w + 7) / 8;
  for (int py = 0; py < h; ++py) {
    for (int px = 0; px < w; ++px) {
      const int idx = py * stride + (px >> 3);
      const uint8_t bit = static_cast<uint8_t>(0x80 >> (px & 7));
      if (mask[idx] & bit) renderer.drawPixel(x + px, y + py, (ink[idx] & bit) != 0);
    }
  }
}
}  // namespace

void MimeeNekoTheme::drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                                         const int selectorIndex, bool& coverRendered, bool& coverBufferStored,
                                         bool& bufferRestored, std::function<bool()> storeCoverBuffer) const {
  (void)bufferRestored;
  using namespace MimeeNekoLayout;
  const Geometry g = compute(rect.width, rect.y);
  const int bookCount = std::min(static_cast<int>(recentBooks.size()), MAX_BOOKS);

  // Static layer (frame, cat, card, clovers, covers): drawn once, cached by HomeActivity.
  if (!coverRendered) {
    renderer.drawRoundedRect(g.frame.x, g.frame.y, g.frame.width, g.frame.height, 2, 16, true);
    drawMasked(renderer, g.cat.x, g.cat.y, NekoArt::NekoCatW, NekoArt::NekoCatH, NekoArt::NekoCatInk,
               NekoArt::NekoCatMask);

    renderer.drawRoundedRect(g.card.x, g.card.y, g.card.width, g.card.height, 2, 14, true);
    drawMasked(renderer, g.card.x + 6, g.card.y + 6, NekoArt::NekoCloverW, NekoArt::NekoCloverH,
               NekoArt::NekoCloverInk, NekoArt::NekoCloverMask);
    drawMasked(renderer, g.card.x + g.card.width - NekoArt::NekoCloverW - 4,
               g.card.y + g.card.height - NekoArt::NekoCloverH - 6, NekoArt::NekoCloverW, NekoArt::NekoCloverH,
               NekoArt::NekoCloverInk, NekoArt::NekoCloverMask);

    for (int i = 0; i < bookCount; ++i) {
      const Rect& b = g.book[i];
      bool hasCover = false;
      const std::string& coverPath = recentBooks[i].coverBmpPath;
      if (!coverPath.empty()) {
        const std::string thumbPath = UITheme::getCoverThumbPath(coverPath, homeCoverThumbHeight(renderer));
        HalFile file;
        if (Storage.openFileForRead("HOME", thumbPath, file)) {
          Bitmap bitmap(file);
          if (bitmap.parseHeaders() == BmpReaderError::Ok) {
            drawCoverThumbFill(renderer, bitmap, b);
            hasCover = true;
          }
          file.close();
        }
      }
      renderer.drawRect(b.x, b.y, b.width, b.height, true);
      if (!hasCover) {
        // No cover: show the title inside the empty rectangle.
        const auto lines = renderer.wrappedText(SMALL_FONT_ID, recentBooks[i].title.c_str(), b.width - 12, 5);
        const int lh = renderer.getLineHeight(SMALL_FONT_ID);
        int y = b.y + 10;
        for (const auto& line : lines) {
          renderer.drawText(SMALL_FONT_ID, b.x + 6, y, line.c_str(), true);
          y += lh;
        }
      }
    }
    coverBufferStored = storeCoverBuffer();
    coverRendered = coverBufferStored;
  }

  // Dynamic layer: selection ring and the card text (drawn over the restored cache).
  if (selectorIndex >= 0 && selectorIndex < bookCount) {
    const Rect& b = g.book[selectorIndex];
    renderer.drawRoundedRect(b.x - 6, b.y - 6, b.width + 12, b.height + 12, 3, 6, true);
  }
  if (bookCount > 0) {
    // With the icon bar selected the card keeps showing the newest book (the one Back resumes).
    const RecentBook& shown = recentBooks[(selectorIndex >= 0 && selectorIndex < bookCount) ? selectorIndex : 0];
    const int textX = g.card.x + 40;
    const int maxW = g.card.width - 80;
    const auto titleLines = renderer.wrappedText(UI_12_FONT_ID, shown.title.c_str(), maxW, 2);
    const auto authorLines = renderer.wrappedText(SMALL_FONT_ID, shown.author.c_str(), maxW, 1);
    const int lh1 = renderer.getLineHeight(UI_12_FONT_ID);
    const int lh2 = renderer.getLineHeight(SMALL_FONT_ID);
    const int block = static_cast<int>(titleLines.size()) * lh1 + (authorLines.empty() ? 0 : 4 + lh2);
    int y = g.card.y + (g.card.height - block) / 2;
    for (const auto& line : titleLines) {
      renderer.drawText(UI_12_FONT_ID, textX, y, line.c_str(), true);
      y += lh1;
    }
    if (!authorLines.empty()) {
      renderer.drawText(SMALL_FONT_ID, textX, y + 4, authorLines[0].c_str(), true);
    }
  }
}

void MimeeNekoTheme::drawButtonMenu(GfxRenderer& renderer, Rect rect, int buttonCount, int selectedIndex,
                                    const std::function<std::string(int index)>& buttonLabel,
                                    const std::function<UIIcon(int index)>& rowIcon) const {
  (void)buttonLabel;
  using namespace MimeeNekoLayout;
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
