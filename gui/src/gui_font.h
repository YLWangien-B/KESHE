/* =============================================================================
 *  gui_font.h  —  中文字体装载
 *
 *  Dear ImGui 内置的字体（ProggyClean / ProggyForever）只有拉丁字母，
 *  直接显示中文会全是「?」或方块，所以必须显式装载一份含汉字的字体。
 * ========================================================================== */
#pragma once

#include "imgui.h"

namespace gui {

// 装载中文字体。scale 是 DPI 缩放系数（125% 缩放时传 1.25）。
// 找不到系统字体时自动退回内置字体，并在控制台说明原因 —— 不静默失败。
void loadChineseFont(ImGuiIO& io, float scale);

}  // namespace gui
