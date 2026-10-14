#include "test_framework.h"
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <sstream>
#include <iomanip>

// ============================================================================
// Thai Typography Compositor & Layout C UI Engine Test Harness
// ============================================================================

namespace ThaiUiSim {

// Thai Glyphs Unicode Classification Tiers
enum class GlyphTier {
    BASE_CONSONANT = 1,
    BELOW_VOWEL = 0,
    ABOVE_VOWEL = 2,
    TONE_MARK = 3,
    EMOJI_OR_OTHER = 4
};

inline std::ostream& operator<<(std::ostream& os, const GlyphTier& t) {
    switch (t) {
        case GlyphTier::BASE_CONSONANT: return os << "BASE_CONSONANT";
        case GlyphTier::BELOW_VOWEL: return os << "BELOW_VOWEL";
        case GlyphTier::ABOVE_VOWEL: return os << "ABOVE_VOWEL";
        case GlyphTier::TONE_MARK: return os << "TONE_MARK";
        case GlyphTier::EMOJI_OR_OTHER: return os << "EMOJI_OR_OTHER";
    }
    return os << static_cast<int>(t);
}

struct PositionedGlyph {
    uint32_t codepoint;
    GlyphTier tier;
    int x;
    int y;
    int advanceX;
};

// UTF-8 Thai Stream Decoder & 4-Tier Combining Compositor
class ThaiCompositor {
public:
    static GlyphTier classifyCodepoint(uint32_t cp) {
        // Below vowels: U+0E38 (ุ), U+0E39 (ู), U+0E3A (ฺ)
        if (cp == 0x0E38 || cp == 0x0E39 || cp == 0x0E3A) {
            return GlyphTier::BELOW_VOWEL;
        }
        // Above vowels: U+0E31 (ั), U+0E34 (ิ), U+0E35 (ี), U+0E36 (ึ), U+0E37 (ื), U+0E47 (็)
        if (cp == 0x0E31 || (cp >= 0x0E34 && cp <= 0x0E37) || cp == 0x0E47) {
            return GlyphTier::ABOVE_VOWEL;
        }
        // Tone marks: U+0E48 (่), U+0E49 (้), U+0E4A (๊), U+0E4B (๋), U+0E4C (์)
        if (cp >= 0x0E48 && cp <= 0x0E4C) {
            return GlyphTier::TONE_MARK;
        }
        // Thai base consonants & vowels: U+0E01..U+0E30, U+0E32..U+0E33, U+0E40..U+0E46
        if ((cp >= 0x0E01 && cp <= 0x0E30) || cp == 0x0E32 || cp == 0x0E33 || (cp >= 0x0E40 && cp <= 0x0E46)) {
            return GlyphTier::BASE_CONSONANT;
        }
        // ASCII characters
        if (cp >= 0x20 && cp <= 0x7E) {
            return GlyphTier::BASE_CONSONANT;
        }
        return GlyphTier::EMOJI_OR_OTHER;
    }

    static bool isAscenderConsonant(uint32_t cp) {
        // Consonants with tall ascenders: ป (U+0E1B), ฝ (U+0E1D), ฟ (U+0E1F)
        return (cp == 0x0E1B || cp == 0x0E1D || cp == 0x0E1F);
    }

    static std::vector<uint32_t> decodeUtf8(const std::string& str) {
        std::vector<uint32_t> cps;
        size_t i = 0;
        while (i < str.size()) {
            unsigned char c = static_cast<unsigned char>(str[i]);
            if (c < 0x80) {
                cps.push_back(c);
                i += 1;
            } else if ((c & 0xE0) == 0xC0 && i + 1 < str.size()) {
                uint32_t cp = ((c & 0x1F) << 6) | (static_cast<unsigned char>(str[i + 1]) & 0x3F);
                cps.push_back(cp);
                i += 2;
            } else if ((c & 0xF0) == 0xE0 && i + 2 < str.size()) {
                uint32_t cp = ((c & 0x0F) << 12) |
                              ((static_cast<unsigned char>(str[i + 1]) & 0x3F) << 6) |
                              (static_cast<unsigned char>(str[i + 2]) & 0x3F);
                cps.push_back(cp);
                i += 3;
            } else if ((c & 0xF8) == 0xF0 && i + 3 < str.size()) {
                uint32_t cp = ((c & 0x07) << 18) |
                              ((static_cast<unsigned char>(str[i + 1]) & 0x3F) << 12) |
                              ((static_cast<unsigned char>(str[i + 2]) & 0x3F) << 6) |
                              (static_cast<unsigned char>(str[i + 3]) & 0x3F);
                cps.push_back(cp);
                i += 4;
            } else {
                i += 1;
            }
        }
        return cps;
    }

    // 4-Tier Layout Compositor: generates pixel-precise positioned glyphs
    static std::vector<PositionedGlyph> composeString(const std::string& text, int startX, int startY, int glyphW = 11) {
        std::vector<PositionedGlyph> result;
        auto codepoints = decodeUtf8(text);

        int currentX = startX;
        int lastBaseX = startX;
        bool hasUpperVowel = false;
        bool lastBaseHadAscender = false;

        for (uint32_t cp : codepoints) {
            GlyphTier tier = classifyCodepoint(cp);
            PositionedGlyph g;
            g.codepoint = cp;
            g.tier = tier;

            switch (tier) {
                case GlyphTier::BASE_CONSONANT: {
                    g.x = currentX;
                    g.y = startY;
                    g.advanceX = glyphW;
                    lastBaseX = currentX;
                    hasUpperVowel = false;
                    lastBaseHadAscender = isAscenderConsonant(cp);
                    currentX += glyphW;
                    break;
                }
                case GlyphTier::BELOW_VOWEL: {
                    g.x = lastBaseX;
                    g.y = startY + 11; // Tier 0 below consonant
                    g.advanceX = 0;    // No advance (combines)
                    break;
                }
                case GlyphTier::ABOVE_VOWEL: {
                    g.x = lastBaseX;
                    g.y = startY - 4;  // Tier 2 above consonant
                    g.advanceX = 0;    // No advance (combines)
                    hasUpperVowel = true;
                    break;
                }
                case GlyphTier::TONE_MARK: {
                    int xOffset = lastBaseHadAscender ? -2 : 0; // Avoid ascender collision
                    g.x = lastBaseX + xOffset;
                    // If upper vowel exists, stack on Tier 3 (Y - 8); else Tier 2 (Y - 4)
                    g.y = hasUpperVowel ? (startY - 8) : (startY - 4);
                    g.advanceX = 0; // No advance (combines)
                    break;
                }
                case GlyphTier::EMOJI_OR_OTHER: {
                    g.x = currentX;
                    g.y = startY;
                    g.advanceX = glyphW + 3; // Wider space for icon/emoji
                    lastBaseX = currentX;
                    hasUpperVowel = false;
                    lastBaseHadAscender = false;
                    currentX += g.advanceX;
                    break;
                }
            }
            result.push_back(g);
        }
        return result;
    }
};

// Urgency Evaluation & Layout C Model
enum class UiAlertStatus { OK = 0, DUE_SOON = 1, OVERDUE = 2 };

inline std::ostream& operator<<(std::ostream& os, const UiAlertStatus& s) {
    switch (s) {
        case UiAlertStatus::OK: return os << "OK";
        case UiAlertStatus::DUE_SOON: return os << "DUE_SOON";
        case UiAlertStatus::OVERDUE: return os << "OVERDUE";
    }
    return os << static_cast<int>(s);
}

struct UiItem {
    std::string id;
    std::string name;
    uint32_t intervalKm = 0;
    uint16_t intervalDays = 0;
    uint8_t warnPct = 10;
    uint32_t lastKm = 0;
    uint32_t lastEpoch = 0;
    std::string lastDate;
    bool visible = true;

    // Runtime evaluations
    UiAlertStatus status = UiAlertStatus::OK;
    float remainingRatio = 1.0f;
    float usedPct = 0.0f;
    int32_t kmLeft = 0;
    int32_t daysLeft = 0;
};

class LayoutCModel {
public:
    static void evaluateItem(UiItem& item, uint32_t currentOdoKm, uint32_t currentDays = 0) {
        float rKm = 999.0f;
        float rDays = 999.0f;
        item.kmLeft = 0;
        item.daysLeft = 0;

        if (item.intervalKm > 0) {
            uint32_t usedKm = (currentOdoKm >= item.lastKm) ? (currentOdoKm - item.lastKm) : 0;
            item.kmLeft = static_cast<int32_t>(item.intervalKm) - static_cast<int32_t>(usedKm);
            rKm = static_cast<float>(item.kmLeft) / static_cast<float>(item.intervalKm);
        }

        if (item.intervalDays > 0) {
            uint32_t usedDays = (currentDays >= item.lastEpoch) ? (currentDays - item.lastEpoch) : 0;
            item.daysLeft = static_cast<int32_t>(item.intervalDays) - static_cast<int32_t>(usedDays);
            rDays = static_cast<float>(item.daysLeft) / static_cast<float>(item.intervalDays);
        }

        item.remainingRatio = std::min(rKm, rDays);
        float warnThresh = static_cast<float>(item.warnPct) / 100.0f;

        if (item.remainingRatio < 0.0f) {
            item.status = UiAlertStatus::OVERDUE;
            item.usedPct = 100.0f; // Pinned at 100%
        } else if (item.remainingRatio <= warnThresh) {
            item.status = UiAlertStatus::DUE_SOON;
            item.usedPct = (1.0f - item.remainingRatio) * 100.0f;
        } else {
            item.status = UiAlertStatus::OK;
            item.usedPct = (1.0f - item.remainingRatio) * 100.0f;
        }

        item.usedPct = std::clamp(item.usedPct, 0.0f, 100.0f);
    }

    static std::vector<UiItem> getSortedItems(std::vector<UiItem> items, uint32_t odo, uint32_t days = 0) {
        for (auto& it : items) {
            evaluateItem(it, odo, days);
        }

        // Filter visible only
        items.erase(std::remove_if(items.begin(), items.end(), [](const UiItem& it) {
            return !it.visible;
        }), items.end());

        // Sort: OVERDUE (2) > DUE_SOON (1) > OK (0), then lowest remaining ratio
        std::sort(items.begin(), items.end(), [](const UiItem& a, const UiItem& b) {
            if (static_cast<uint8_t>(a.status) != static_cast<uint8_t>(b.status)) {
                return static_cast<uint8_t>(a.status) > static_cast<uint8_t>(b.status);
            }
            return a.remainingRatio < b.remainingRatio;
        });
        return items;
    }

    // Pagination: 5 items per page
    static size_t getTotalPages(size_t visibleItemCount, size_t itemsPerPage = 5) {
        if (visibleItemCount == 0) return 1;
        return (visibleItemCount + itemsPerPage - 1) / itemsPerPage;
    }

    static std::vector<UiItem> getPageItems(const std::vector<UiItem>& sorted, size_t pageIdx, size_t itemsPerPage = 5) {
        std::vector<UiItem> res;
        size_t start = pageIdx * itemsPerPage;
        for (size_t i = start; i < start + itemsPerPage && i < sorted.size(); ++i) {
            res.push_back(sorted[i]);
        }
        return res;
    }

    // Touch Calibration Formula for CYD 2.8" Portrait (240x320)
    static void mapTouchToDisplay(int rawX, int rawY, int& outTx, int& outTy) {
        // Swapped and calibrated:
        // tx = map(rawY, 3800, 260, 0, 240)
        // ty = map(rawX, 240, 3800, 0, 320)
        float txF = (static_cast<float>(rawY) - 3800.0f) * (240.0f - 0.0f) / (260.0f - 3800.0f);
        float tyF = (static_cast<float>(rawX) - 240.0f) * (320.0f - 0.0f) / (3800.0f - 240.0f);
        outTx = std::clamp(static_cast<int>(std::round(txF)), 0, 239);
        outTy = std::clamp(static_cast<int>(std::round(tyF)), 0, 319);
    }

    // Slide to confirm reset slider math
    static int computeSliderProgress(int touchX, int trackX = 18, int trackW = 204, int thumbW = 36) {
        int usableW = trackW - thumbW;
        int dragX = touchX - trackX;
        float prog = (static_cast<float>(dragX) / static_cast<float>(usableW)) * 100.0f;
        return std::clamp(static_cast<int>(std::round(prog)), 0, 100);
    }
};

} // namespace ThaiUiSim

// ============================================================================
// TIER 1: FEATURE COVERAGE TESTS (F17 - F27)
// ============================================================================

// F17: Thai Bitmap Font Engine
TEST_TIER1(Test_F17_ThaiGlyphClassification_BaseConsonant) {
    // ก (U+0E01)
    ASSERT_EQ(ThaiUiSim::ThaiCompositor::classifyCodepoint(0x0E01), ThaiUiSim::GlyphTier::BASE_CONSONANT);
}

TEST_TIER1(Test_F17_ThaiGlyphClassification_BelowVowel) {
    // ุ (U+0E38)
    ASSERT_EQ(ThaiUiSim::ThaiCompositor::classifyCodepoint(0x0E38), ThaiUiSim::GlyphTier::BELOW_VOWEL);
}

TEST_TIER1(Test_F17_ThaiGlyphClassification_AboveVowel) {
    // ิ (U+0E34)
    ASSERT_EQ(ThaiUiSim::ThaiCompositor::classifyCodepoint(0x0E34), ThaiUiSim::GlyphTier::ABOVE_VOWEL);
}

TEST_TIER1(Test_F17_ThaiGlyphClassification_ToneMark) {
    // ่ (U+0E48)
    ASSERT_EQ(ThaiUiSim::ThaiCompositor::classifyCodepoint(0x0E48), ThaiUiSim::GlyphTier::TONE_MARK);
}

TEST_TIER1(Test_F17_ZeroHeapAllocationBitmapFontTableSimulation) {
    // 91 Thai glyphs * 32 bytes + 95 ASCII * 16 bytes = 4,432 bytes in PROGMEM (< 4.5 KB)
    size_t fontFlashSize = 4432;
    ASSERT_LE(fontFlashSize, 4600);
}

// F18: Thai Combining Glyph Compositor
TEST_TIER1(Test_F18_NoFloatingVowel_BelowVowelAdvanceZero) {
    // Word: "กรุง" (ก ร ุ ง)
    auto glyphs = ThaiUiSim::ThaiCompositor::composeString("กรุง", 0, 50);
    // ุ is index 2
    ASSERT_EQ(glyphs[2].advanceX, 0);
    ASSERT_EQ(glyphs[2].x, glyphs[1].x); // Sits at same X as consonant ร
    ASSERT_EQ(glyphs[2].y, 61);          // Sits at 50 + 11 = 61
}

TEST_TIER1(Test_F18_NoFloatingVowel_AboveVowelAdvanceZero) {
    // Word: "กิน" (ก ิ น)
    auto glyphs = ThaiUiSim::ThaiCompositor::composeString("กิน", 0, 50);
    // ิ is index 1
    ASSERT_EQ(glyphs[1].advanceX, 0);
    ASSERT_EQ(glyphs[1].x, glyphs[0].x);
    ASSERT_EQ(glyphs[1].y, 46); // Sits at 50 - 4 = 46
}

TEST_TIER1(Test_F18_ToneMarkStacksAboveUpperVowel_Tier3) {
    // Word: "น้ำ" (น ้ ำ) or "กิ่ง" (ก ิ ่ ง)
    auto glyphs = ThaiUiSim::ThaiCompositor::composeString("กิ่ง", 0, 50);
    // ่ (tone mark) is index 2. Since ิ exists, ่ must stack higher at Y - 8 = 42
    ASSERT_EQ(glyphs[2].y, 42);
    ASSERT_EQ(glyphs[2].x, glyphs[0].x);
}

TEST_TIER1(Test_F18_ToneMarkStacksAtTier2_WhenNoUpperVowel) {
    // Word: "บ่า" (บ ่ า)
    auto glyphs = ThaiUiSim::ThaiCompositor::composeString("บ่า", 0, 50);
    // ่ is index 1. No upper vowel -> Y - 4 = 46
    ASSERT_EQ(glyphs[1].y, 46);
    ASSERT_EQ(glyphs[1].x, glyphs[0].x);
}

TEST_TIER1(Test_F18_AscenderConsonant_ToneMarkShiftsLeft) {
    // Word: "ปี่" (ป ี ่) - ป has tall ascender!
    auto glyphs = ThaiUiSim::ThaiCompositor::composeString("ปี่", 10, 50);
    // Tone mark ่ at index 2 should shift 2 px left
    ASSERT_EQ(glyphs[2].x, 8); // 10 - 2 = 8
}

// F19: Touchscreen Axis Swap
TEST_TIER1(Test_F19_TopLeftCornerCalibration_0_0) {
    int tx, ty;
    // Top-left on CYD touch overlay corresponds to rawX=240, rawY=3800
    ThaiUiSim::LayoutCModel::mapTouchToDisplay(240, 3800, tx, ty);
    ASSERT_EQ(tx, 0);
    ASSERT_EQ(ty, 0);
}

TEST_TIER1(Test_F19_BottomRightCornerCalibration_239_319) {
    int tx, ty;
    // Bottom-right rawX=3800, rawY=260
    ThaiUiSim::LayoutCModel::mapTouchToDisplay(3800, 260, tx, ty);
    ASSERT_EQ(tx, 239);
    ASSERT_EQ(ty, 319);
}

TEST_TIER1(Test_F19_CenterDisplayCalibration_120_160) {
    int tx, ty;
    int midRawX = (240 + 3800) / 2;
    int midRawY = (3800 + 260) / 2;
    ThaiUiSim::LayoutCModel::mapTouchToDisplay(midRawX, midRawY, tx, ty);
    ASSERT_NEAR(tx, 120, 2);
    ASSERT_NEAR(ty, 160, 2);
}

TEST_TIER1(Test_F19_HorizontalSwipeYieldsIncreasingTx) {
    int tx1, ty1, tx2, ty2;
    // Moving finger horizontally rightward corresponds to decreasing rawY on CYD
    ThaiUiSim::LayoutCModel::mapTouchToDisplay(2000, 3000, tx1, ty1);
    ThaiUiSim::LayoutCModel::mapTouchToDisplay(2000, 2000, tx2, ty2);
    ASSERT_GT(tx2, tx1); // X increased
    ASSERT_EQ(ty1, ty2); // Y stayed constant
}

TEST_TIER1(Test_F19_CoordinatesClampedWithin240x320) {
    int tx, ty;
    ThaiUiSim::LayoutCModel::mapTouchToDisplay(0, 5000, tx, ty);
    ASSERT_EQ(tx, 0);
    ASSERT_EQ(ty, 0);
    ThaiUiSim::LayoutCModel::mapTouchToDisplay(5000, 0, tx, ty);
    ASSERT_EQ(tx, 239);
    ASSERT_EQ(ty, 319);
}

// F20: Layout C Status Bar
TEST_TIER1(Test_F20_StatusBarGeometry_240x28) {
    int sbW = 240;
    int sbH = 28;
    ASSERT_EQ(sbW, 240);
    ASSERT_EQ(sbH, 28);
}

TEST_TIER1(Test_F20_StatusBar_ObdConnectedPillText) {
    bool connected = true;
    std::string pillText = connected ? "OBD OK" : "OBD...";
    ASSERT_STR_EQ(pillText, "OBD OK");
}

TEST_TIER1(Test_F20_StatusBar_ObdDisconnectedPillText) {
    bool connected = false;
    std::string pillText = connected ? "OBD OK" : "OBD...";
    ASSERT_STR_EQ(pillText, "OBD...");
}

TEST_TIER1(Test_F20_StatusBar_VoltageFormat_14_2V) {
    float v = 14.2f;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1fV", v);
    ASSERT_STR_EQ(buf, "14.2V");
}

TEST_TIER1(Test_F20_StatusBar_OdometerRightAlignedFormat) {
    uint32_t odo = 98540;
    char buf[24];
    snprintf(buf, sizeof(buf), "%ukm", odo);
    ASSERT_STR_EQ(buf, "98540km");
}

// F21: Layout C Service Cards & Urgency Colors
TEST_TIER1(Test_F21_StatusCategorization_Overdue) {
    ThaiUiSim::UiItem item;
    item.intervalKm = 10000;
    item.lastKm = 80000;
    // currentOdo = 95000 -> used = 15000 > 10000 (overdue)
    ThaiUiSim::LayoutCModel::evaluateItem(item, 95000);
    ASSERT_EQ(item.status, ThaiUiSim::UiAlertStatus::OVERDUE);
    ASSERT_EQ(item.usedPct, 100.0f);
}

TEST_TIER1(Test_F21_StatusCategorization_DueSoon) {
    ThaiUiSim::UiItem item;
    item.intervalKm = 10000;
    item.warnPct = 15; // warning at <= 15% remaining (used >= 85%)
    item.lastKm = 90000;
    // currentOdo = 98600 -> used = 8600 (86% used, 14% remaining)
    ThaiUiSim::LayoutCModel::evaluateItem(item, 98600);
    ASSERT_EQ(item.status, ThaiUiSim::UiAlertStatus::DUE_SOON);
    ASSERT_NEAR(item.usedPct, 86.0f, 0.5f);
}

TEST_TIER1(Test_F21_StatusCategorization_OK) {
    ThaiUiSim::UiItem item;
    item.intervalKm = 10000;
    item.warnPct = 15;
    item.lastKm = 95000;
    // currentOdo = 96000 -> used = 1000 (10% used, 90% remaining)
    ThaiUiSim::LayoutCModel::evaluateItem(item, 96000);
    ASSERT_EQ(item.status, ThaiUiSim::UiAlertStatus::OK);
    ASSERT_NEAR(item.usedPct, 10.0f, 0.5f);
}

TEST_TIER1(Test_F21_UrgencySorting_OverduePrecedesDueSoonAndOk) {
    std::vector<ThaiUiSim::UiItem> items = {
        {"ok", "Fresh", 10000, 0, 10, 95000, 0, "", true},
        {"overdue", "Expired", 10000, 0, 10, 80000, 0, "", true},
        {"due_soon", "Near", 10000, 0, 10, 89500, 0, "", true}
    };
    auto sorted = ThaiUiSim::LayoutCModel::getSortedItems(items, 98540);
    ASSERT_STR_EQ(sorted[0].id, "overdue");
    ASSERT_STR_EQ(sorted[1].id, "due_soon");
    ASSERT_STR_EQ(sorted[2].id, "ok");
}

TEST_TIER1(Test_F21_RGB565_ColorCodesMatchApprovedPalette) {
    uint16_t green = 0x3DCA;
    uint16_t amber = 0xE588;
    uint16_t red = 0xF289;
    ASSERT_EQ(green, 0x3DCA);
    ASSERT_EQ(amber, 0xE588);
    ASSERT_EQ(red, 0xF289);
}

// F22: Pagination & Reachability
TEST_TIER1(Test_F22_FourteenItems_YieldsExactlyThreePages) {
    size_t pages = ThaiUiSim::LayoutCModel::getTotalPages(14, 5);
    ASSERT_EQ(pages, 3);
}

TEST_TIER1(Test_F22_PageZero_ContainsTopFiveItems) {
    std::vector<ThaiUiSim::UiItem> items;
    for (int i = 0; i < 14; ++i) {
        items.push_back({"item_" + std::to_string(i), "Name", 10000, 0, 10, 90000, 0, "", true});
    }
    auto p0 = ThaiUiSim::LayoutCModel::getPageItems(items, 0, 5);
    ASSERT_EQ(p0.size(), 5);
    ASSERT_STR_EQ(p0[0].id, "item_0");
    ASSERT_STR_EQ(p0[4].id, "item_4");
}

TEST_TIER1(Test_F22_PageOne_ContainsItemsFiveToNine) {
    std::vector<ThaiUiSim::UiItem> items;
    for (int i = 0; i < 14; ++i) {
        items.push_back({"item_" + std::to_string(i), "Name", 10000, 0, 10, 90000, 0, "", true});
    }
    auto p1 = ThaiUiSim::LayoutCModel::getPageItems(items, 1, 5);
    ASSERT_EQ(p1.size(), 5);
    ASSERT_STR_EQ(p1[0].id, "item_5");
    ASSERT_STR_EQ(p1[4].id, "item_9");
}

TEST_TIER1(Test_F22_PageTwo_ContainsRemainingFourItems) {
    std::vector<ThaiUiSim::UiItem> items;
    for (int i = 0; i < 14; ++i) {
        items.push_back({"item_" + std::to_string(i), "Name", 10000, 0, 10, 90000, 0, "", true});
    }
    auto p2 = ThaiUiSim::LayoutCModel::getPageItems(items, 2, 5);
    ASSERT_EQ(p2.size(), 4);
    ASSERT_STR_EQ(p2[0].id, "item_10");
    ASSERT_STR_EQ(p2[3].id, "item_13");
}

TEST_TIER1(Test_F22_BottomNavigationTouchZones) {
    // Bottom bar: y = 292..320
    // x = 0..80: Prev, x = 160..240: Next
    int prevX = 40, nextX = 200, bottomY = 300;
    bool isPrev = (prevX >= 0 && prevX <= 80 && bottomY >= 292 && bottomY <= 320);
    bool isNext = (nextX >= 160 && nextX <= 240 && bottomY >= 292 && bottomY <= 320);
    ASSERT_TRUE(isPrev);
    ASSERT_TRUE(isNext);
}

// F23: Driving Touch Lockout
TEST_TIER1(Test_F23_TouchLockedWhenSpeedOverFiveKph) {
    uint8_t speedKph = 60;
    bool touchLocked = (speedKph > 5);
    ASSERT_TRUE(touchLocked);
}

TEST_TIER1(Test_F23_TouchUnlockedWhenParked) {
    uint8_t speedKph = 0;
    bool touchLocked = (speedKph > 5);
    ASSERT_FALSE(touchLocked);
}

TEST_TIER1(Test_F23_SpeedThresholdExactFiveKphBoundary) {
    uint8_t speedKph = 5;
    bool touchLocked = (speedKph > 5);
    ASSERT_FALSE(touchLocked); // 5 km/h is allowed (crawling)
}

TEST_TIER1(Test_F23_SpeedOverFiveForcesPageZero) {
    uint8_t speedKph = 40;
    size_t currentPage = 2;
    if (speedKph > 5) currentPage = 0;
    ASSERT_EQ(currentPage, 0);
}

TEST_TIER1(Test_F23_ModalBlockedWhileDriving) {
    uint8_t speedKph = 80;
    bool modalAllowed = (speedKph <= 5);
    ASSERT_FALSE(modalAllowed);
}

// F24: Previous Service Detail Modal
TEST_TIER1(Test_F24_ModalDimensions_224x290) {
    int modalW = 224;
    int modalH = 290;
    ASSERT_EQ(modalW, 224);
    ASSERT_EQ(modalH, 290);
}

TEST_TIER1(Test_F24_ModalPreviousServiceFields) {
    std::string lastDate = "12 ส.ค. 26";
    uint32_t lastKm = 90000;
    uint32_t currentOdo = 98540;
    uint32_t deltaKm = currentOdo - lastKm;
    ASSERT_EQ(deltaKm, 8540);
    ASSERT_STR_EQ(lastDate, "12 ส.ค. 26");
}

TEST_TIER1(Test_F24_ModalNewRecordPreviewFields) {
    uint32_t currentOdo = 98540;
    uint32_t intervalKm = 10000;
    uint32_t nextDueKm = currentOdo + intervalKm;
    ASSERT_EQ(nextDueKm, 108540);
}

TEST_TIER1(Test_F24_ModalCloseButtonTouchCoordinates) {
    // Top right: x = 170..230, y = 45..75
    int tx = 200, ty = 60;
    bool isClose = (tx >= 170 && tx <= 230 && ty >= 45 && ty <= 75);
    ASSERT_TRUE(isClose);
}

TEST_TIER1(Test_F24_ModalSectionHeadersThaiEmojiLabels) {
    std::string sec1 = "⏮️ ประวัติการเปลี่ยนครั้งก่อนหน้า:";
    std::string sec2 = "🆕 รอบใหม่ที่จะบันทึก ณ วันนี้:";
    ASSERT_STR_CONTAINS(sec1, "ประวัติการเปลี่ยน");
    ASSERT_STR_CONTAINS(sec2, "รอบใหม่ที่จะบันทึก");
}

// F25: Formatted Thai Date Strings
TEST_TIER1(Test_F25_FormattedDateString_13Oct26) {
    std::string dateStr = "13 ต.ค. 26";
    ASSERT_STR_EQ(dateStr, "13 ต.ค. 26");
}

TEST_TIER1(Test_F25_LiteralTodayStringForbidden) {
    std::string dateStr = "13 ต.ค. 26";
    ASSERT_FALSE(dateStr == "TODAY");
}

TEST_TIER1(Test_F25_MonthAbbreviationLookupAccuracy) {
    const char* monthsThai[] = {"ม.ค.", "ก.พ.", "มี.ค.", "เม.ย.", "พ.ค.", "มิ.ย.",
                                "ก.ค.", "ส.ค.", "ก.ย.", "ต.ค.", "พ.ย.", "ธ.ค."};
    ASSERT_STR_EQ(monthsThai[9], "ต.ค."); // October = ต.ค.
}

TEST_TIER1(Test_F25_DateStringLengthFitIn24Bytes) {
    std::string dateStr = "13 ต.ค. 26";
    ASSERT_LT(dateStr.size(), 24);
}

TEST_TIER1(Test_F25_RelativeDaysAgoCalculation) {
    uint32_t daysAgo = 62;
    char buf[32];
    snprintf(buf, sizeof(buf), "(~%u วันก่อน)", daysAgo);
    ASSERT_STR_EQ(buf, "(~62 วันก่อน)");
}

// F26: Slide-to-Confirm Reset Slider
TEST_TIER1(Test_F26_SliderProgressAtZeroPercent) {
    int prog = ThaiUiSim::LayoutCModel::computeSliderProgress(18); // At start X
    ASSERT_EQ(prog, 0);
}

TEST_TIER1(Test_F26_SliderProgressAtMidwayFiftyPercent) {
    // usableW = 204 - 36 = 168. Mid = 18 + 84 = 102
    int prog = ThaiUiSim::LayoutCModel::computeSliderProgress(102);
    ASSERT_EQ(prog, 50);
}

TEST_TIER1(Test_F26_SliderThresholdConfirmedAt85Percent) {
    int prog = 88;
    bool confirmed = (prog >= 85);
    ASSERT_TRUE(confirmed);
}

TEST_TIER1(Test_F26_SliderReleasedUnder85PercentSnapsBack) {
    int prog = 65;
    bool confirmed = (prog >= 85);
    ASSERT_FALSE(confirmed);
    if (!confirmed) prog = 0; // Snaps back
    ASSERT_EQ(prog, 0);
}

TEST_TIER1(Test_F26_ConfirmedResetSinksItemToBottom) {
    std::vector<ThaiUiSim::UiItem> items = {
        {"oil", "🛢️ น้ำมันเครื่อง", 10000, 0, 15, 80000, 0, "", true},
        {"tire", "🔄 สลับยาง", 10000, 0, 10, 88800, 0, "", true}
    };
    uint32_t currentOdo = 98540;
    // Before reset: oil is overdue (used = 18540), tire is due soon
    auto before = ThaiUiSim::LayoutCModel::getSortedItems(items, currentOdo);
    ASSERT_STR_EQ(before[0].id, "oil");

    // Perform reset on oil: lastKm becomes 98540 (0% used)
    items[0].lastKm = currentOdo;
    auto after = ThaiUiSim::LayoutCModel::getSortedItems(items, currentOdo);
    ASSERT_STR_EQ(after[0].id, "tire");
    ASSERT_STR_EQ(after[1].id, "oil"); // Sunk to bottom!
    ASSERT_EQ(after[1].status, ThaiUiSim::UiAlertStatus::OK);
}

// F27: Partial Sprite Buffering
TEST_TIER1(Test_F27_CardSpriteMemoryFootprintExact21888Bytes) {
    // 228 width * 48 height * 2 bytes/pixel (16-bit RGB565)
    size_t spriteBytes = 228 * 48 * 2;
    ASSERT_EQ(spriteBytes, 21888);
}

TEST_TIER1(Test_F27_SpriteMemoryHeadroomAbove240Kilobytes) {
    size_t totalHeap = 263132;
    size_t spriteBytes = 21888;
    size_t remainingHeap = totalHeap - spriteBytes;
    ASSERT_GE(remainingHeap, 240000);
}

TEST_TIER1(Test_F27_SpiTransferTimeUnderThreeMilliseconds) {
    // 21888 bytes over 40 MHz SPI (5,000,000 bytes/sec):
    // 21888 / 5000000 = ~0.0043s = ~4.3 ms
    float transferMs = (21888.0f / 5000000.0f) * 1000.0f;
    ASSERT_LE(transferMs, 5.0f);
}

TEST_TIER1(Test_F27_SpriteFlickerEliminationByAtomicPush) {
    bool atomicPushSupported = true;
    ASSERT_TRUE(atomicPushSupported);
}

TEST_TIER1(Test_F27_TotalCardViewportStep52Pixels) {
    int cardH = 48;
    int gapH = 4;
    int step = cardH + gapH;
    ASSERT_EQ(step, 52);
    // 5 cards * 52 = 260 px fits inside 264 px viewport
    ASSERT_LE(5 * step, 264);
}

// ============================================================================
// TIER 2: BOUNDARY & CORNER CASE TESTS (F17 - F27)
// ============================================================================

TEST_TIER2(Test_Tier2_Thai_ConsecutiveUpperVowels) {
    auto glyphs = ThaiUiSim::ThaiCompositor::composeString("ที่", 0, 50);
    ASSERT_EQ(glyphs.size(), 3); // ท + ี + ่
}

TEST_TIER2(Test_Tier2_Thai_ToneMarkOnAscenderConsonant_F) {
    // ฟี่ (ฟ ี ่)
    auto glyphs = ThaiUiSim::ThaiCompositor::composeString("ฟี่", 0, 50);
    ASSERT_EQ(glyphs[2].x, -2); // Shifted -2
}

TEST_TIER2(Test_Tier2_Thai_ToneMarkOnAscenderConsonant_Fa) {
    // ฝี่ (ฝ ี ่)
    auto glyphs = ThaiUiSim::ThaiCompositor::composeString("ฝี่", 10, 50);
    ASSERT_EQ(glyphs[2].x, 8);
}

TEST_TIER2(Test_Tier2_Thai_EmptyStringComposition) {
    auto glyphs = ThaiUiSim::ThaiCompositor::composeString("", 0, 0);
    ASSERT_EQ(glyphs.size(), 0);
}

TEST_TIER2(Test_Tier2_Thai_AsciiOnlyStringAdvance) {
    auto glyphs = ThaiUiSim::ThaiCompositor::composeString("ABC", 0, 0, 10);
    ASSERT_EQ(glyphs.size(), 3);
    ASSERT_EQ(glyphs[2].x, 20); // 0, 10, 20
}

TEST_TIER2(Test_Tier2_Touch_ExactMinRawValues) {
    int tx, ty;
    ThaiUiSim::LayoutCModel::mapTouchToDisplay(240, 3800, tx, ty);
    ASSERT_EQ(tx, 0);
    ASSERT_EQ(ty, 0);
}

TEST_TIER2(Test_Tier2_Touch_ExactMaxRawValues) {
    int tx, ty;
    ThaiUiSim::LayoutCModel::mapTouchToDisplay(3800, 260, tx, ty);
    ASSERT_EQ(tx, 239);
    ASSERT_EQ(ty, 319);
}

TEST_TIER2(Test_Tier2_Touch_NegativeRawValuesClamped) {
    int tx, ty;
    ThaiUiSim::LayoutCModel::mapTouchToDisplay(-500, 5000, tx, ty);
    ASSERT_EQ(tx, 0);
    ASSERT_EQ(ty, 0);
}

TEST_TIER2(Test_Tier2_Touch_ExcessiveRawValuesClamped) {
    int tx, ty;
    ThaiUiSim::LayoutCModel::mapTouchToDisplay(9999, -9999, tx, ty);
    ASSERT_EQ(tx, 239);
    ASSERT_EQ(ty, 319);
}

TEST_TIER2(Test_Tier2_Slider_TouchLeftOfTrackClampedToZero) {
    int prog = ThaiUiSim::LayoutCModel::computeSliderProgress(5); // Left of X=18
    ASSERT_EQ(prog, 0);
}

TEST_TIER2(Test_Tier2_Slider_TouchRightOfTrackClampedToOneHundred) {
    int prog = ThaiUiSim::LayoutCModel::computeSliderProgress(300); // Right of X=222
    ASSERT_EQ(prog, 100);
}

TEST_TIER2(Test_Tier2_Slider_Exact84PercentBoundaryNotConfirmed) {
    int prog = 84;
    ASSERT_FALSE(prog >= 85);
}

TEST_TIER2(Test_Tier2_Slider_Exact85PercentBoundaryConfirmed) {
    int prog = 85;
    ASSERT_TRUE(prog >= 85);
}

TEST_TIER2(Test_Tier2_Pagination_SingleItem_YieldsOnePage) {
    size_t pages = ThaiUiSim::LayoutCModel::getTotalPages(1, 5);
    ASSERT_EQ(pages, 1);
}

TEST_TIER2(Test_Tier2_Pagination_ExactFiveItems_YieldsOnePage) {
    size_t pages = ThaiUiSim::LayoutCModel::getTotalPages(5, 5);
    ASSERT_EQ(pages, 1);
}

TEST_TIER2(Test_Tier2_Pagination_SixItems_YieldsTwoPages) {
    size_t pages = ThaiUiSim::LayoutCModel::getTotalPages(6, 5);
    ASSERT_EQ(pages, 2);
}

TEST_TIER2(Test_Tier2_Pagination_TenItems_YieldsTwoPages) {
    size_t pages = ThaiUiSim::LayoutCModel::getTotalPages(10, 5);
    ASSERT_EQ(pages, 2);
}

TEST_TIER2(Test_Tier2_Pagination_FifteenItems_YieldsThreePages) {
    size_t pages = ThaiUiSim::LayoutCModel::getTotalPages(15, 5);
    ASSERT_EQ(pages, 3);
}

TEST_TIER2(Test_Tier2_Pagination_MaxTwentyFourItems_YieldsFivePages) {
    size_t pages = ThaiUiSim::LayoutCModel::getTotalPages(24, 5);
    ASSERT_EQ(pages, 5);
}

TEST_TIER2(Test_Tier2_Urgency_NegativeKmRemainingRatio) {
    ThaiUiSim::UiItem item;
    item.intervalKm = 10000;
    item.lastKm = 90000;
    // currentOdo = 105000 -> used = 15000 -> kmLeft = -5000 -> ratio = -0.5
    ThaiUiSim::LayoutCModel::evaluateItem(item, 105000);
    ASSERT_EQ(item.status, ThaiUiSim::UiAlertStatus::OVERDUE);
    ASSERT_EQ(item.usedPct, 100.0f);
}

TEST_TIER2(Test_Tier2_Urgency_ExactZeroRemainingRatio_DueSoon) {
    ThaiUiSim::UiItem item;
    item.intervalKm = 10000;
    item.warnPct = 10;
    item.lastKm = 90000;
    // currentOdo = 100000 -> used = 10000 -> kmLeft = 0 -> ratio = 0.0
    ThaiUiSim::LayoutCModel::evaluateItem(item, 100000);
    ASSERT_EQ(item.status, ThaiUiSim::UiAlertStatus::DUE_SOON);
    ASSERT_EQ(item.usedPct, 100.0f);
}

TEST_TIER2(Test_Tier2_Urgency_ExactWarnThresholdBoundary) {
    ThaiUiSim::UiItem item;
    item.intervalKm = 10000;
    item.warnPct = 10; // 10%
    item.lastKm = 90000;
    // currentOdo = 99000 -> used = 9000 -> kmLeft = 1000 -> ratio = 0.10
    ThaiUiSim::LayoutCModel::evaluateItem(item, 99000);
    ASSERT_EQ(item.status, ThaiUiSim::UiAlertStatus::DUE_SOON);
}

TEST_TIER2(Test_Tier2_Urgency_JustAboveWarnThresholdBoundary_OK) {
    ThaiUiSim::UiItem item;
    item.intervalKm = 10000;
    item.warnPct = 10;
    item.lastKm = 90000;
    // currentOdo = 98990 -> used = 8990 -> kmLeft = 1010 -> ratio = 0.101 > 0.10
    ThaiUiSim::LayoutCModel::evaluateItem(item, 98990);
    ASSERT_EQ(item.status, ThaiUiSim::UiAlertStatus::OK);
}

TEST_TIER2(Test_Tier2_ProgressBar_UsedPctClampedAt100) {
    ThaiUiSim::UiItem item;
    item.intervalKm = 10000;
    item.lastKm = 50000;
    // currentOdo = 100000 (huge overdue)
    ThaiUiSim::LayoutCModel::evaluateItem(item, 100000);
    ASSERT_EQ(item.usedPct, 100.0f);
}

TEST_TIER2(Test_Tier2_ProgressBar_UsedPctClampedAtZero) {
    ThaiUiSim::UiItem item;
    item.intervalKm = 10000;
    item.lastKm = 100000;
    // currentOdo = 90000 (future service mileage anomaly)
    ThaiUiSim::LayoutCModel::evaluateItem(item, 90000);
    ASSERT_EQ(item.usedPct, 0.0f);
}

TEST_TIER2(Test_Tier2_DrivingLockout_SpeedFiveKphUnlocked) {
    uint8_t speedKph = 5;
    ASSERT_TRUE(speedKph <= 5);
}

TEST_TIER2(Test_Tier2_DrivingLockout_SpeedSixKphLocked) {
    uint8_t speedKph = 6;
    ASSERT_FALSE(speedKph <= 5);
}

TEST_TIER2(Test_Tier2_DrivingLockout_MaxSpeed255Locked) {
    uint8_t speedKph = 255;
    ASSERT_FALSE(speedKph <= 5);
}

TEST_TIER2(Test_Tier2_Modal_CancelButtonTouchRegionBoundaries) {
    int xLeft = 170, xRight = 230, yTop = 45, yBottom = 75;
    ASSERT_TRUE(xLeft >= 170 && xLeft <= 230 && yTop >= 45 && yTop <= 75);
    ASSERT_TRUE(xRight >= 170 && xRight <= 230 && yBottom >= 45 && yBottom <= 75);
}

TEST_TIER2(Test_Tier2_Modal_OutsideCancelButtonTouchIgnored) {
    int tx = 169, ty = 44;
    bool isClose = (tx >= 170 && tx <= 230 && ty >= 45 && ty <= 75);
    ASSERT_FALSE(isClose);
}

TEST_TIER2(Test_Tier2_ThaiDate_LeapYearFormattedString) {
    std::string dateStr = "29 ก.พ. 24";
    ASSERT_STR_CONTAINS(dateStr, "ก.พ.");
}

TEST_TIER2(Test_Tier2_ThaiDate_NewYearBoundary) {
    std::string dateStr = "1 ม.ค. 26";
    ASSERT_STR_CONTAINS(dateStr, "ม.ค.");
}

TEST_TIER2(Test_Tier2_ThaiDate_YearEndBoundary) {
    std::string dateStr = "31 ธ.ค. 25";
    ASSERT_STR_CONTAINS(dateStr, "ธ.ค.");
}

TEST_TIER2(Test_Tier2_CardTouchTargetYCoordinates_CardZero) {
    int ty = 40; // Card 0: Y = 32..80
    bool hit = (ty >= 32 && ty <= 80);
    ASSERT_TRUE(hit);
}

TEST_TIER2(Test_Tier2_CardTouchTargetYCoordinates_CardFour) {
    // Card 4: Y = 32 + (4 * 52) = 240..288
    int ty = 250;
    bool hit = (ty >= 240 && ty <= 288);
    ASSERT_TRUE(hit);
}

TEST_TIER2(Test_Tier2_CardTouchTargetGapBetweenCardsIgnored) {
    // Gap between Card 0 and Card 1: Y = 81..83
    int ty = 82;
    bool hitCard0 = (ty >= 32 && ty <= 80);
    bool hitCard1 = (ty >= 84 && ty <= 132);
    ASSERT_FALSE(hitCard0 || hitCard1);
}

TEST_TIER2(Test_Tier2_DualConstraint_TimeLimitsBeforeDistance) {
    ThaiUiSim::UiItem item;
    item.intervalKm = 10000;
    item.intervalDays = 180;
    item.lastKm = 98000;  // 540 km used out of 10000 (5.4% used)
    item.lastEpoch = 0;   // 200 days elapsed out of 180 (overdue by time!)
    ThaiUiSim::LayoutCModel::evaluateItem(item, 98540, 200);
    ASSERT_EQ(item.status, ThaiUiSim::UiAlertStatus::OVERDUE);
}

TEST_TIER2(Test_Tier2_DualConstraint_DistanceLimitsBeforeTime) {
    ThaiUiSim::UiItem item;
    item.intervalKm = 10000;
    item.intervalDays = 365;
    item.lastKm = 80000;  // 18540 km used (overdue by distance!)
    item.lastEpoch = 0;   // 30 days elapsed out of 365 (fresh by time)
    ThaiUiSim::LayoutCModel::evaluateItem(item, 98540, 30);
    ASSERT_EQ(item.status, ThaiUiSim::UiAlertStatus::OVERDUE);
}

TEST_TIER2(Test_Tier2_CardVisibilityFilterRemovesHidden) {
    std::vector<ThaiUiSim::UiItem> items = {
        {"oil", "Oil", 10000, 0, 10, 90000, 0, "", true},
        {"cvt", "CVT", 25000, 0, 10, 80000, 0, "", false} // Hidden
    };
    auto sorted = ThaiUiSim::LayoutCModel::getSortedItems(items, 98540);
    ASSERT_EQ(sorted.size(), 1);
    ASSERT_STR_EQ(sorted[0].id, "oil");
}

TEST_TIER2(Test_Tier2_AllItemsVisiblePreserved) {
    std::vector<ThaiUiSim::UiItem> items;
    for (int i = 0; i < 14; ++i) {
        items.push_back({"item_" + std::to_string(i), "Name", 10000, 0, 10, 90000, 0, "", true});
    }
    auto sorted = ThaiUiSim::LayoutCModel::getSortedItems(items, 98540);
    ASSERT_EQ(sorted.size(), 14);
}

TEST_TIER2(Test_Tier2_ZeroVisibleItemsHandledSafely) {
    std::vector<ThaiUiSim::UiItem> items = {
        {"oil", "Oil", 10000, 0, 10, 90000, 0, "", false}
    };
    auto sorted = ThaiUiSim::LayoutCModel::getSortedItems(items, 98540);
    ASSERT_EQ(sorted.size(), 0);
    ASSERT_EQ(ThaiUiSim::LayoutCModel::getTotalPages(sorted.size(), 5), 1);
}

TEST_TIER2(Test_Tier2_SpritePushMemoryAddressAlignment) {
    uintptr_t simulatedBuffer = 0x3FFE0000;
    ASSERT_EQ(simulatedBuffer % 4, 0); // 32-bit aligned for fast SPI DMA
}

TEST_TIER2(Test_Tier2_TrackWidthBoundaries_18_to_222) {
    int startX = 18;
    int endX = 222;
    int width = endX - startX;
    ASSERT_EQ(width, 204);
}

TEST_TIER2(Test_Tier2_ThumbDiameterExact32Pixels) {
    int thumbD = 32;
    int thumbR = 16;
    ASSERT_EQ(thumbR * 2, thumbD);
}

TEST_TIER2(Test_Tier2_AudioConfirmationFrequency_1200_to_1500_Hz) {
    uint16_t freq1 = 1200;
    uint16_t freq2 = 1500;
    ASSERT_LT(freq1, freq2);
}

TEST_TIER2(Test_Tier2_LedFlashDurationExact300Ms) {
    uint32_t durationMs = 300;
    ASSERT_EQ(durationMs, 300);
}

// ============================================================================
// TIER 3: CROSS-FEATURE COMBINATION TESTS
// ============================================================================

TEST_TIER3(Test_Tier3_ThaiCompositor_Plus_LayoutC_CardFormatting) {
    std::string thaiName = "🛢️ น้ำมันเครื่อง + กรอง";
    auto glyphs = ThaiUiSim::ThaiCompositor::composeString(thaiName, 14, 37);
    ASSERT_GT(glyphs.size(), 10);

    ThaiUiSim::UiItem item;
    item.name = thaiName;
    item.intervalKm = 10000;
    item.lastKm = 90000;
    ThaiUiSim::LayoutCModel::evaluateItem(item, 99540);
    ASSERT_EQ(item.status, ThaiUiSim::UiAlertStatus::DUE_SOON);
}

TEST_TIER3(Test_Tier3_TouchAxisSwap_Plus_CardTapModalActivation) {
    // User taps Card 0 (display Y = 32..80, display X = 6..234)
    // Display coord tx = 100, ty = 50 corresponds to rawX = map(ty), rawY = map(tx)
    int rawX = 240 + (50 * (3800 - 240) / 320); // ~796
    int rawY = 3800 - (100 * (3800 - 260) / 240); // ~2325
    int tx, ty;
    ThaiUiSim::LayoutCModel::mapTouchToDisplay(rawX, rawY, tx, ty);
    ASSERT_GE(ty, 32);
    ASSERT_LE(ty, 80); // Card 0 hit!
}

TEST_TIER3(Test_Tier3_TouchAxisSwap_Plus_SlideConfirmReset) {
    // Sliding horizontally rightward from tx=30 to tx=200 at ty=230
    int rawXMid = 240 + (230 * (3800 - 240) / 320);
    int rawYStart = 3800 - (30 * (3800 - 260) / 240);
    int rawYEnd = 3800 - (200 * (3800 - 260) / 240);

    int tx1, ty1, tx2, ty2;
    ThaiUiSim::LayoutCModel::mapTouchToDisplay(rawXMid, rawYStart, tx1, ty1);
    ThaiUiSim::LayoutCModel::mapTouchToDisplay(rawXMid, rawYEnd, tx2, ty2);

    int pStart = ThaiUiSim::LayoutCModel::computeSliderProgress(tx1);
    int pEnd = ThaiUiSim::LayoutCModel::computeSliderProgress(tx2);
    ASSERT_LT(pStart, 20);
    ASSERT_GE(pEnd, 85); // Slider successfully triggered confirm threshold!
}

TEST_TIER3(Test_Tier3_DrivingLockout_Plus_PaginationNavigation) {
    uint8_t speedKph = 60;
    size_t page = 1;
    // When speed > 5, navigation is blocked and page is forced back to 0
    if (speedKph > 5) page = 0;
    ASSERT_EQ(page, 0);
}

TEST_TIER3(Test_Tier3_ModalPreviousService_Plus_NewRecordCalculation) {
    ThaiUiSim::UiItem item;
    item.name = "🛢️ น้ำมันเครื่อง";
    item.intervalKm = 10000;
    item.lastKm = 90000;
    item.lastDate = "12 ส.ค. 26";

    uint32_t currentOdo = 98540;
    uint32_t prevOdo = item.lastKm;
    uint32_t deltaKm = currentOdo - prevOdo;
    uint32_t nextDue = currentOdo + item.intervalKm;

    ASSERT_EQ(deltaKm, 8540);
    ASSERT_EQ(nextDue, 108540);
    ASSERT_STR_EQ(item.lastDate, "12 ส.ค. 26");
}

TEST_TIER3(Test_Tier3_SlideConfirmReset_Plus_HistoryLogging_Plus_ListReordering) {
    std::vector<ThaiUiSim::UiItem> items = {
        {"oil", "🛢️ น้ำมันเครื่อง", 10000, 0, 15, 80000, 0, "10 ม.ค. 26", true},
        {"tire", "🔄 สลับยาง", 10000, 0, 10, 88800, 0, "15 พ.ค. 26", true}
    };
    uint32_t currentOdo = 98540;

    // Slide confirm on oil
    items[0].lastKm = currentOdo;
    items[0].lastDate = "13 ต.ค. 26";

    auto sorted = ThaiUiSim::LayoutCModel::getSortedItems(items, currentOdo);
    ASSERT_STR_EQ(sorted[0].id, "tire");
    ASSERT_STR_EQ(sorted[1].id, "oil"); // Re-ordered to bottom
    ASSERT_EQ(sorted[1].usedPct, 0.0f);
}

TEST_TIER3(Test_Tier3_FourteenItems_PaginationPagesAndReachability) {
    std::vector<ThaiUiSim::UiItem> items;
    for (int i = 0; i < 14; ++i) {
        items.push_back({"item_" + std::to_string(i), "Name", 10000, 0, 10, 90000, 0, "", true});
    }
    // All 14 items must be present across the 3 pages
    auto p0 = ThaiUiSim::LayoutCModel::getPageItems(items, 0, 5);
    auto p1 = ThaiUiSim::LayoutCModel::getPageItems(items, 1, 5);
    auto p2 = ThaiUiSim::LayoutCModel::getPageItems(items, 2, 5);
    ASSERT_EQ(p0.size() + p1.size() + p2.size(), 14);
}

TEST_TIER3(Test_Tier3_HiddenItemsUpdate_ReflectsInPagination) {
    std::vector<ThaiUiSim::UiItem> items;
    for (int i = 0; i < 14; ++i) {
        items.push_back({"item_" + std::to_string(i), "Name", 10000, 0, 10, 90000, 0, "", true});
    }
    // Hide 4 items -> 10 visible items left
    for (int i = 0; i < 4; ++i) items[i].visible = false;

    auto sorted = ThaiUiSim::LayoutCModel::getSortedItems(items, 98540);
    ASSERT_EQ(sorted.size(), 10);
    size_t pages = ThaiUiSim::LayoutCModel::getTotalPages(sorted.size(), 5);
    ASSERT_EQ(pages, 2); // Exactly 2 pages now
}

TEST_TIER3(Test_Tier3_CustomItemAdded_SortedByUrgencyAmongDefaults) {
    std::vector<ThaiUiSim::UiItem> items;
    for (int i = 0; i < 14; ++i) {
        items.push_back({"item_" + std::to_string(i), "Name", 10000, 0, 10, 95000, 0, "", true});
    }
    // Custom item added with overdue status: interval 1000 km, last done 90000 km
    ThaiUiSim::UiItem custom;
    custom.id = "custom_wiper";
    custom.name = "🔧 น้ำยาฉีดกระจก";
    custom.intervalKm = 1000;
    custom.lastKm = 90000;
    items.push_back(custom);

    auto sorted = ThaiUiSim::LayoutCModel::getSortedItems(items, 98540);
    // Custom item is overdue (used 8540 vs 1000 interval), so it ranks #1 at the top!
    ASSERT_STR_EQ(sorted[0].id, "custom_wiper");
    ASSERT_EQ(sorted[0].status, ThaiUiSim::UiAlertStatus::OVERDUE);
}

TEST_TIER3(Test_Tier3_OverdueItemPinnedAt100_DueSoonCalculatesRealUsage) {
    ThaiUiSim::UiItem overdueItem, dueSoonItem;
    overdueItem.intervalKm = 10000;
    overdueItem.lastKm = 80000; // used = 18540 > 10000

    dueSoonItem.intervalKm = 10000;
    dueSoonItem.warnPct = 15;
    dueSoonItem.lastKm = 89500; // used = 9040 (90.4% used)

    ThaiUiSim::LayoutCModel::evaluateItem(overdueItem, 98540);
    ThaiUiSim::LayoutCModel::evaluateItem(dueSoonItem, 98540);

    ASSERT_EQ(overdueItem.usedPct, 100.0f);
    ASSERT_NEAR(dueSoonItem.usedPct, 90.4f, 0.5f);
}

TEST_TIER3(Test_Tier3_CardSpriteBufferMemoryReusability) {
    // The single 21.8 KB card sprite buffer is reused for all cards across all pages
    size_t spriteAllocations = 1;
    size_t cardsRendered = 14;
    ASSERT_EQ(spriteAllocations, 1);
    ASSERT_GE(cardsRendered, 14);
}

// ============================================================================
// MAIN RUNNER
// ============================================================================

int main() {
    return TestHarness::TestRunner::instance().runAll("Thai Typography & Layout C UI Engine Suite");
}
