#include "ChatColors.h"

namespace pluto {
namespace gui {

// Palette — mutable defaults for light theme; _InitPalette() overwrites.
rgb_color kColUserRole   = {  40,  95, 200, 255 };
rgb_color kColUserBody   = {  30,  30,  30, 255 };
rgb_color kColAsstRole   = {  30, 140,  70, 255 };
rgb_color kColAsstBody   = {  20,  70,  30, 255 };
rgb_color kColToolRole   = { 140,  60, 160, 255 };
rgb_color kColToolBody   = { 110, 110, 110, 255 };
rgb_color kColErr        = { 200,  50,  50, 255 };
rgb_color kColWarn       = { 190, 140,  20, 255 };
rgb_color kColDim        = { 130, 130, 130, 255 };
rgb_color kColShimmer    = {  60, 160, 100, 255 };

rgb_color kColSynKeyword = {   0,   0, 180, 255 };
rgb_color kColSynString  = { 180,  50,  20, 255 };
rgb_color kColSynComment = { 100, 130, 100, 255 };
rgb_color kColSynNumber  = {   0, 130, 130, 255 };
rgb_color kColSynPreproc = { 130,  70,   0, 255 };
rgb_color kColSynType    = {   0, 120, 100, 255 };

const char* kArrowOpen  = "\xE2\x96\xBE";  // ▾
const char* kArrowClose = "\xE2\x96\xB8";  // ▸
const char* kCursorChar = "\xE2\x96\x88";  // █
const int32 kUtf3       = 3;

// kMsgToolToggle is inline constexpr in ChatColors.h.

}  // namespace gui
}  // namespace pluto
