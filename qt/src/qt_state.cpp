/* =============================================================================
 *  qt_state.cpp  —  共享状态的唯一实例 + 页面标题 + 通知中心实例
 * ========================================================================== */
#include "qt_state.h"

namespace qt {

QtState& state() {
  static QtState instance;
  return instance;
}

const char* pageTitle(int page) {
  static const char* kTitles[PAGE_COUNT] = {
      "1  智能导诊", "2  医生查询", "3  在线预约", "4  预约管理",
      "5  排班展示", "6  就诊路线", "7  候诊队列", "数据总览",
  };
  return (page >= 0 && page < PAGE_COUNT) ? kTitles[page] : "";
}

Notifier& notifier() {
  static Notifier instance;
  return instance;
}

}  // namespace qt
