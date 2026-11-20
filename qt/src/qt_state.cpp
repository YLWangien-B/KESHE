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
      "智能导诊", "医生查询", "在线预约", "预约管理",
      "排班与停诊", "就诊路线", "候诊队列", "人员管理", "数据总览",
      "首页",
  };
  return (page >= 0 && page < PAGE_COUNT) ? kTitles[page] : "";
}

Notifier& notifier() {
  static Notifier instance;
  return instance;
}

}  // namespace qt
