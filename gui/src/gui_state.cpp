/* =============================================================================
 *  gui_state.cpp  —  GUI 状态实例与提示条
 * ========================================================================== */
#include "gui_state.h"

#include <cstdarg>

namespace gui {

GuiState& state() {
  static GuiState s;
  return s;
}

// 写一条提示。variadic 版本照 printf 用，和界面层原来的 printf 习惯一致。
void GuiState::notify(NoticeKind kind, const char* fmt, ...) {
  noticeKind = kind;
  va_list args;
  va_start(args, fmt);
  std::vsnprintf(notice, sizeof(notice), fmt, args);
  va_end(args);
}

}  // namespace gui
