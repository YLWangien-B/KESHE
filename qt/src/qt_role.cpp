/* =============================================================================
 *  qt_role.cpp  —  角色判定与页面可见性
 * ========================================================================== */
#include "qt_role.h"

#include "qt_state.h"

namespace qt {

AppRole appRole() {
#ifdef APP_ROLE_ADMIN
  return AppRole::Admin;
#else
  return AppRole::Patient;
#endif
}

const char* roleName() { return isAdmin() ? "管理端" : "患者端"; }

bool pageEnabled(int page) {
  if (isAdmin()) {
    // 管理端：查医生、预约总览与管理、排班与停诊、叫号、路线、数据总览
    switch (page) {
      case PAGE_DOCTOR:
      case PAGE_MANAGE:
      case PAGE_SCHEDULE:
      case PAGE_ROUTE:
      case PAGE_QUEUE:
      case PAGE_ABOUT:
        return true;
      default:
        return false;
    }
  }
  // 患者端：导诊、查医生、预约、取消自己的预约、路线、签到候诊
  switch (page) {
    case PAGE_TRIAGE:
    case PAGE_DOCTOR:
    case PAGE_BOOK:
    case PAGE_MANAGE:
    case PAGE_ROUTE:
    case PAGE_QUEUE:
      return true;
    default:
      return false;
  }
}

}  // namespace qt
