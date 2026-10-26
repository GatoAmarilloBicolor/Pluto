#include "ChatColors.h"
namespace pluto {
namespace gui {

// Modern flat design palette — mutable defaults; _InitPalette() overwrites.
// Based on a cohesive design system with consistent saturation/lightness.

// Dark theme (default for Haiku)
rgb_color kColUserRole = {  99, 179, 237, 255 };  // #63B3ED - calm blue
rgb_color kColUserBody = { 226, 232, 240, 255 };  // #E2E8F0 - soft gray
rgb_color kColAsstRole = {  72, 187, 120, 255 };  // #48BB78 - fresh green
rgb_color kColAsstBody = { 237, 247, 237, 255 };  // #EDF7ED - mint cream
rgb_color kColToolRole = { 159, 122, 234, 255 };  // #9F7AE6 - purple
rgb_color kColToolBody = { 245, 243, 255, 255 };  // #F5F3FF - lavender mist
rgb_color kColErr      = { 245, 101, 101, 255 };  // #F56565 - coral red
rgb_color kColWarn     = { 237, 137,  54, 255 };  // #ED8936 - amber
rgb_color kColDim      = { 160, 174, 192, 255 };  // #A0AEB0 - muted slate
rgb_color kColShimmer  = {  56, 178, 172, 255 };  // #38B2AC - teal accent

// Syntax highlighting — dark theme optimized
rgb_color kColSynKeyword = { 129, 180, 255, 255 };  // #81B4FF
rgb_color kColSynString  = { 238, 168, 120, 255 };  // #EEA878
rgb_color kColSynComment = { 113, 128, 150, 255 };  // #718096
rgb_color kColSynNumber  = { 100, 210, 210, 255 };  // #64D2D2
rgb_color kColSynPreproc = { 224, 158, 102, 255 };  // #E09E66
rgb_color kColSynType    = { 100, 210, 180, 255 };  // #64D2B4

// Light theme variants (used when forced light)
rgb_color kLightUserRole = {  30,  90, 170, 255 };  // deeper blue
rgb_color kLightUserBody = {  30,  30,  30, 255 };
rgb_color kLightAsstRole = {  22, 120,  70, 255 };
rgb_color kLightAsstBody = {  30,  30,  30, 255 };
rgb_color kLightToolRole = { 100,  70, 180, 255 };
rgb_color kLightToolBody = {  30,  30,  30, 255 };
rgb_color kLightErr      = { 180,  40,  40, 255 };
rgb_color kLightWarn     = { 170, 110,  20, 255 };
rgb_color kLightDim      = { 100, 110, 120, 255 };
rgb_color kLightShimmer  = {  30, 140, 130, 255 };

// Syntax — light theme
rgb_color kLightSynKeyword = {  40,  80, 180, 255 };
rgb_color kLightSynString  = { 160,  90,  30, 255 };
rgb_color kLightSynComment = { 100, 110, 120, 255 };
rgb_color kLightSynNumber  = {  30, 120, 120, 255 };
rgb_color kLightSynPreproc = { 140,  80,  30, 255 };
rgb_color kLightSynType    = {  30, 130, 100, 255 };

// Background colors
const rgb_color kDarkBg   = {  26,  32,  44, 255 };  // #1A202C - rich dark
const rgb_color kDarkFg   = { 226, 232, 240, 255 };  // #E2E8F0
const rgb_color kDarkCodeBg = {  22,  27,  34, 255 }; // #161B22
const rgb_color kLightBg  = { 255, 255, 255, 255 };
const rgb_color kLightFg  = {  30,  30,  30, 255 };
const rgb_color kLightCodeBg = { 245, 245, 245, 255 };

// UI chrome
const rgb_color kBorderColor   = {  74,  85, 104, 255 };  // #4A5568
const rgb_color kHoverBg       = {  45,  55,  72, 255 };  // #2D3748
const rgb_color kActiveBg      = {  56,  68,  88, 255 };  // #384458
const rgb_color kScrollbarBg   = {  45,  55,  72, 255 };
const rgb_color kScrollbarThumb= {  90, 100, 120, 255 };

// Icons
const char* kArrowOpen = "\xE2\x96\xBC";  // ▼
const char* kArrowClose = "\xE2\x96\xB6"; // ▶
const char* kCursorChar = "\xE2\x96\x88"; // █
const int32 kUtf3 = 3;

} // namespace gui
} // namespace pluto
