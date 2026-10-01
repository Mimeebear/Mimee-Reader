#include <Epub/ParsedText.h>
#include <Epub/blocks/TextBlock.h>
#include <Epub/hyphenation/Hyphenator.h>
#include <Epub/hyphenation/thai/ThaiWordBreaker.h>
#include <GfxRenderer.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace {

struct Line {
  std::vector<std::string> words;
  std::vector<int16_t> xpos;
};

// Fixture metrics (stub renderer): every glyph is 8 px wide and a space is 4 px.
std::vector<Line> layout(const std::vector<const char*>& words, const bool hyphenation, const uint16_t width) {
  GfxRenderer renderer;
  BlockStyle style;
  style.alignment = CssTextAlign::Justify;
  style.textIndentDefined = true;
  ParsedText text(false, hyphenation, false, style);
  for (const char* word : words) text.addWord(word, EpdFontFamily::REGULAR);
  std::vector<Line> lines;
  text.layoutAndExtractLines(renderer, 0, width, [&](std::unique_ptr<TextBlock> block, auto) {
    auto& line = lines.emplace_back();
    for (uint16_t i = 0; i < block->wordCount(); ++i) {
      line.words.emplace_back(block->wordText(i));
      line.xpos.push_back(block->wordXpos(i));
    }
  });
  return lines;
}

std::vector<std::vector<std::string>> wordsOf(const std::vector<Line>& lines) {
  std::vector<std::vector<std::string>> result;
  result.reserve(lines.size());
  for (const auto& line : lines) result.push_back(line.words);
  return result;
}

std::vector<std::string> thaiSegments(const std::string_view text) {
  std::vector<std::string> segments;
  size_t start = 0;
  while (start < text.size()) {
    const size_t end = ThaiWordBreaker::nextBoundary(text, start);
    if (end <= start || end > text.size()) return {};
    segments.emplace_back(text.substr(start, end - start));
    start = end;
  }
  return segments;
}

std::vector<std::string> thaiSegmentsInMixedText(const std::string& text) {
  std::vector<std::string> result;
  const auto* const start = reinterpret_cast<const unsigned char*>(text.data());
  const auto* ptr = start;
  const auto* const end = start + text.size();
  while (ptr < end) {
    const auto* runStart = ptr;
    uint32_t cp = utf8NextCodepoint(&ptr);
    if (!ThaiWordBreaker::isThaiCodepoint(cp)) continue;
    while (ptr < end) {
      const auto* nextStart = ptr;
      cp = utf8NextCodepoint(&ptr);
      if (!ThaiWordBreaker::isThaiCodepoint(cp)) {
        ptr = nextStart;
        break;
      }
    }
    if (ptr > runStart) {
      const std::string_view run(reinterpret_cast<const char*>(runStart), static_cast<size_t>(ptr - runStart));
      const auto segments = thaiSegments(run);
      result.insert(result.end(), segments.begin(), segments.end());
    }
  }
  return result;
}

}  // namespace

TEST(ThaiTrieSegmentation, MatchesLibthaiReferenceWords) {
  // Reference boundaries captured from libthai 0.1.29 th_brk_wc_find_breaks.
  EXPECT_EQ(thaiSegments("ภาษาไทย"), (std::vector<std::string>{"ภาษา", "ไทย"}));
  EXPECT_EQ(thaiSegments("การอ่านหนังสือ"), (std::vector<std::string>{"การ", "อ่าน", "หนังสือ"}));
  EXPECT_EQ(thaiSegments("ทดสอบภาษาไทยบนเครื่องอ่าน"),
            (std::vector<std::string>{"ทดสอบ", "ภาษา", "ไทย", "บน", "เครื่อง", "อ่าน"}));
  EXPECT_EQ(thaiSegments("ประเทศไทยมีภาษาไทย"),
            (std::vector<std::string>{"ประเทศ", "ไทย", "มี", "ภาษา", "ไทย"}));
  EXPECT_EQ(thaiSegments("สวัสดีครับ"), (std::vector<std::string>{"สวัสดี", "ครับ"}));
  EXPECT_EQ(thaiSegments("คอมพิวเตอร์ทำงาน"), (std::vector<std::string>{"คอมพิวเตอร์", "ทำงาน"}));
  EXPECT_EQ(thaiSegments("อินเทอร์เน็ตความเร็วสูง"),
            (std::vector<std::string>{"อินเทอร์เน็ต", "ความ", "เร็ว", "สูง"}));
  EXPECT_EQ(thaiSegments("กรุงเทพมหานคร"), (std::vector<std::string>{"กรุงเทพ", "มหา", "นคร"}));
  EXPECT_EQ(thaiSegments("คำศัพท์ภาษาไทย"), (std::vector<std::string>{"คำ", "ศัพท์", "ภาษา", "ไทย"}));
}

TEST(ThaiTrieSegmentation, LeavesNonThaiRunsUntouched) {
  EXPECT_EQ(thaiSegmentsInMixedText("ปี 2026 มีภาษาไทย EPUB"),
            (std::vector<std::string>{"ปี", "มี", "ภาษา", "ไทย"}));
  EXPECT_EQ(thaiSegmentsInMixedText("อ่าน EPUB บน X4"), (std::vector<std::string>{"อ่าน", "บน"}));
  EXPECT_EQ(thaiSegmentsInMixedText("ประเทศไทยใช้ e-ink reader"),
            (std::vector<std::string>{"ประเทศ", "ไทย", "ใช้"}));
  EXPECT_EQ(thaiSegmentsInMixedText("เวอร์ชัน 1.6.5 พร้อมแล้ว"),
            (std::vector<std::string>{"เวอร์ชัน", "พร้อม", "แล้ว"}));
  EXPECT_FALSE(ThaiWordBreaker::containsThai("English EPUB 2026"));
}

TEST(ThaiTrieSegmentation, UnknownTextFallsBackAtThaiClusters) {
  const std::string unknown = "\xE0\xB8\x81\xE0\xB8\xB4\xE0\xB9\x88\xE0\xB8\x82";
  EXPECT_EQ(thaiSegments(unknown), (std::vector<std::string>{"\xE0\xB8\x81\xE0\xB8\xB4\xE0\xB9\x88", "\xE0\xB8\x82"}));

  const std::string leadingVowel = "\xE0\xB9\x80\xE0\xB8\x81\xE0\xB8\xB4\xE0\xB9\x88\xE0\xB8\x82";
  EXPECT_EQ(thaiSegments(leadingVowel), (std::vector<std::string>{"\xE0\xB9\x80\xE0\xB8\x81\xE0\xB8\xB4\xE0\xB9\x88", "\xE0\xB8\x82"}));
}

TEST(ThaiTrieSegmentation, LineLayoutUsesTrieWhenHyphenationIsDisabled) {
  Hyphenator::setPreferredLanguage("th");
  const auto lines = layout({"ภาษาไทย"}, false, 40);
  const std::vector<std::vector<std::string>> expected{{"ภาษา-"}, {"ไทย"}};
  EXPECT_EQ(wordsOf(lines), expected);
}

TEST(ThaiTrieSegmentation, LineLayoutUsesTrieWhenHyphenationIsEnabled) {
  Hyphenator::setPreferredLanguage("th");
  const auto lines = layout({"ภาษาไทย"}, true, 40);
  const std::vector<std::vector<std::string>> expected{{"ภาษา-"}, {"ไทย"}};
  EXPECT_EQ(wordsOf(lines), expected);
}

TEST(KoreanLineBreaking, HyphenationOffWrapsAtSpacesOnly) {
  Hyphenator::setPreferredLanguage("ko");
  // 가나 다라마바 needs 52 px; with hyphenation off the Hangul word moves down whole.
  const auto lines = layout({"가나", "다라마바", "사"}, false, 50);
  const std::vector<std::vector<std::string>> expected{{"가나"}, {"다라마바", "사"}};
  EXPECT_EQ(wordsOf(lines), expected);
}

TEST(KoreanLineBreaking, HyphenationOnSplitsBetweenSyllablesWithoutHyphen) {
  Hyphenator::setPreferredLanguage("ko");
  // 가나 + space leaves 30 px: the widest syllable prefix that fits is 다라마 (24 px).
  const auto lines = layout({"가나", "다라마바사아", "자"}, true, 50);
  const std::vector<std::vector<std::string>> expected{{"가나", "다라마"}, {"바사아", "자"}};
  ASSERT_EQ(wordsOf(lines), expected);
  // 16 + 4 + 24 = 44 px leaves 6 px, all of it on the one word space.
  EXPECT_EQ(lines[0].xpos, (std::vector<int16_t>{0, 26}));
}

TEST(KoreanLineBreaking, HyphenationOnKeepsTrailingPunctuationWithSyllable) {
  Hyphenator::setPreferredLanguage("ko");
  // 가나다 + space leaves 36 px. 라마바사 (32 px) would fit, but 사. stays together, so 라마바 is used.
  const auto lines = layout({"가나다", "라마바사.", "자"}, true, 64);
  const std::vector<std::vector<std::string>> expected{{"가나다", "라마바"}, {"사.", "자"}};
  EXPECT_EQ(wordsOf(lines), expected);
}

TEST(KoreanLineBreaking, HyphenationOnSplitsShortWords) {
  Hyphenator::setPreferredLanguage("ko");
  // 가나다 + space leaves 22 px, so the three-syllable word splits after 라마 (16 px).
  const auto lines = layout({"가나다", "라마바", "자"}, true, 50);
  const std::vector<std::vector<std::string>> expected{{"가나다", "라마"}, {"바", "자"}};
  EXPECT_EQ(wordsOf(lines), expected);
}

TEST(KoreanLineBreaking, HyphenationOnSplitsWhereHangulMeetsOtherScriptsOrBrackets) {
  Hyphenator::setPreferredLanguage("ko");
  // 가 + space leaves 52 px. 소신(ab) (48 px) fits; the split may fall before "(" or after ")",
  // never inside the brackets.
  const auto lines = layout({"가", "소신(ab)이"}, true, 64);
  const std::vector<std::vector<std::string>> expected{{"가", "소신(ab)"}, {"이"}};
  EXPECT_EQ(wordsOf(lines), expected);
}

TEST(KoreanLineBreaking, HyphenationOnSplitsBetweenDigitAndHangul) {
  Hyphenator::setPreferredLanguage("ko");
  // 가나다 + space leaves 22 px: 12 (16 px) fits, 12월 (24 px) does not.
  const auto lines = layout({"가나다", "12월부터", "자"}, true, 50);
  const std::vector<std::vector<std::string>> expected{{"가나다", "12"}, {"월부터", "자"}};
  EXPECT_EQ(wordsOf(lines), expected);
}
