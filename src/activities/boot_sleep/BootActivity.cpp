#include "BootActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "fontIds.h"
#include "images/BootLogo.h"

void BootActivity::onEnter() {
  Activity::onEnter();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const int logoY = (pageHeight - BootLogoHeight) / 2;
  const int textY = logoY + BootLogoHeight + 70;

  renderer.clearScreen();
  renderer.drawImage(BootLogo, (pageWidth - BootLogoWidth) / 2, logoY, BootLogoWidth, BootLogoHeight);
  renderer.drawCenteredText(UI_10_FONT_ID, textY, tr(STR_CROSSPOINT), true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(SMALL_FONT_ID, textY + 25, tr(STR_BOOTING));
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight - 30, CROSSPOINT_VERSION);
  renderer.displayBuffer();
}
