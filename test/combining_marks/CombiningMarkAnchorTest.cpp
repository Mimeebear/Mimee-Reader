#include <gtest/gtest.h>

#include "lib/EpdFont/EpdFontData.h"

// ============================================================================
// Anchor selection and placement for combining marks without GPOS tables.
//
// Hebrew niqqud whose identity depends on position must not use the default
// centre-and-raise heuristic: dagesh sits inside the letter body, the
// shin/sin dots sit over the letter's right/left arm, and holam hangs over
// the left corner (see PR #2541 review feedback).
// ============================================================================

using combiningMark::Anchor;
using combiningMark::anchorFor;
using combiningMark::anchorOver;
using combiningMark::anchorOverRotated90CW;
using combiningMark::raiseAboveBase;

TEST(ThaiMarkStack, ClassifiesUpperMarksAndToneMarks) {
  for (const uint32_t cp : {0x0E31, 0x0E34, 0x0E35, 0x0E36, 0x0E37, 0x0E38, 0x0E39, 0x0E3A, 0x0E47, 0x0E48,
                            0x0E49, 0x0E4A, 0x0E4B, 0x0E4C, 0x0E4D, 0x0E4E}) {
    EXPECT_TRUE(thaiMark::isMark(cp));
  }
  EXPECT_FALSE(thaiMark::isMark(0x0E33));  // Sara Am retains its normal advance.
  EXPECT_TRUE(thaiMark::isUpperStackMark(0x0E36));
  EXPECT_TRUE(thaiMark::isToneOrThanthakhat(0x0E48));
  EXPECT_TRUE(thaiMark::isToneOrThanthakhat(0x0E4C));
  EXPECT_FALSE(thaiMark::isToneOrThanthakhat(0x0E47));
}

TEST(ThaiMarkStack, RaisesOnlyWhenAboveMarkBoxesWouldOverlap) {
  EXPECT_EQ(thaiMark::raiseAbovePrevious(0, 26, 6), 0);
  EXPECT_EQ(thaiMark::raiseAbovePrevious(27, 26, 6), 8);
  EXPECT_EQ(thaiMark::raiseAbovePrevious(30, 26, 6), 11);
}

TEST(ThaiMarkStack, CoversReportedThaiSequencesUsingSarabunMetrics) {
  // top/height values are from the generated Sarabun 14 regular EpdGlyph records.
  thaiMark::StackState stack;
  EXPECT_EQ(stack.raiseFor(0x0E48, 26, 6, 0x0E33, 26), 7);  // คว่ำ: mai ek before Sara Am.

  stack.reset();
  stack.observe(0x0E36, 26, 0);
  EXPECT_EQ(stack.raiseFor(0x0E49, 28, 8, 0x0E33, 30), 11);  // Keep the higher Sara Am obstacle.

  stack.reset();
  stack.observe(0x0E31, 27, 0);
  EXPECT_EQ(stack.raiseFor(0x0E49, 28, 8, 0, 0), 8);  // ครั้ง: mai han-akat + mai tho.

  stack.reset();
  stack.observe(0x0E36, 27, 0);
  EXPECT_EQ(stack.raiseFor(0x0E48, 26, 6, 0, 0), 8);  // หนึ่ง: sara ue + mai ek.

  stack.reset();
  EXPECT_EQ(stack.raiseFor(0x0E48, 26, 6, 0, 0), 0);  // ก่อน: no upper vowel.

  stack.reset();
  EXPECT_EQ(stack.raiseFor(0x0E49, 28, 8, 0x0E33, 26), 7);  // น้ำ: mai tho before Sara Am.

  stack.reset();
  stack.observe(0x0E39, -2, 8);  // ผู้: sara u is below the baseline, not an upper-stack anchor.
  EXPECT_EQ(stack.raiseFor(0x0E49, 28, 8, 0, 0), 0);

  stack.reset();
  stack.observe(0x0E35, 28, 0);
  EXPECT_EQ(stack.raiseFor(0x0E48, 26, 6, 0, 0), 9);  // ปี่: sara ii + mai ek.
}

TEST(AnchorFor, PositionSensitiveNiqqud) {
  EXPECT_EQ(anchorFor(0x05BC), Anchor::CenterNative);  // dagesh/mapiq
  EXPECT_EQ(anchorFor(0x05BA), Anchor::CenterNative);  // holam haser for vav
  EXPECT_EQ(anchorFor(0x05C1), Anchor::RightNative);   // shin dot
  EXPECT_EQ(anchorFor(0x05C2), Anchor::LeftNative);    // sin dot
  EXPECT_EQ(anchorFor(0x05B9), Anchor::LeftNative);    // holam
}

TEST(AnchorFor, EverythingElseKeepsCentreRaisedDefault) {
  EXPECT_EQ(anchorFor(0x05B0), Anchor::CenterRaised);  // Hebrew sheva
  EXPECT_EQ(anchorFor(0x05B7), Anchor::CenterRaised);  // Hebrew patach
  EXPECT_EQ(anchorFor(0x064E), Anchor::CenterRaised);  // Arabic fatha
  EXPECT_EQ(anchorFor(0x0651), Anchor::CenterRaised);  // Arabic shadda
  EXPECT_EQ(anchorFor(0x0301), Anchor::CenterRaised);  // combining acute
}

// Base glyph: cursor 100, left 1, width 12.  Mark: left 2, width 4.
TEST(AnchorOver, HorizontalPlacementPerAnchor) {
  // Centered: base bitmap spans [101, 113), centre 107; mark starts at 105.
  EXPECT_EQ(anchorOver(Anchor::CenterRaised, 100, 1, 12, 2, 4), 105 - 2);
  EXPECT_EQ(anchorOver(Anchor::CenterNative, 100, 1, 12, 2, 4), 105 - 2);
  // Left-aligned: mark bitmap starts at the base bitmap's left edge (101).
  EXPECT_EQ(anchorOver(Anchor::LeftNative, 100, 1, 12, 2, 4), 101 - 2);
  // Right-aligned: mark bitmap ends at the base bitmap's right edge (113).
  EXPECT_EQ(anchorOver(Anchor::RightNative, 100, 1, 12, 2, 4), 109 - 2);
}

// The rotated coordinate system inverts every left/width term, so the mark's
// offset from the base cursor must be the exact mirror of the unrotated one.
TEST(AnchorOverRotated90CW, MirrorsUnrotatedOffsets) {
  for (const Anchor anchor : {Anchor::CenterRaised, Anchor::CenterNative, Anchor::LeftNative, Anchor::RightNative}) {
    const int offset = anchorOver(anchor, 100, 1, 12, 2, 4) - 100;
    EXPECT_EQ(anchorOverRotated90CW(anchor, 100, 1, 12, 2, 4), 100 - offset);
  }
}

TEST(RaiseAboveBase, NativeAnchorsKeepFontDesignedHeight) {
  // A dagesh-like dot designed inside the letter body (top 8 of a base whose
  // top is 12) must NOT be hoisted above the letter.
  EXPECT_EQ(raiseAboveBase(Anchor::CenterNative, 8, 3, 12), 0);
  // Shin/sin dots overlapping the letter's top must not be pushed clear.
  EXPECT_EQ(raiseAboveBase(Anchor::RightNative, 13, 3, 12), 0);
  EXPECT_EQ(raiseAboveBase(Anchor::LeftNative, 13, 3, 12), 0);
}

TEST(RaiseAboveBase, CentreRaisedBehaviourUnchanged) {
  // Above-baseline mark colliding with the base: raised to restore a 1px gap.
  // gap = markTop - markHeight - baseTop = 10 - 3 - 12 = -5  ->  raise 6.
  EXPECT_EQ(raiseAboveBase(Anchor::CenterRaised, 10, 3, 12), 6);
  // Already clear of the base: no raise.
  EXPECT_EQ(raiseAboveBase(Anchor::CenterRaised, 16, 3, 12), 0);
  // Below-baseline mark (kasra, cedilla): stays at font-native position.
  EXPECT_EQ(raiseAboveBase(Anchor::CenterRaised, 2, 4, 12), 0);
}
