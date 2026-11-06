/* =============================================================================
 *  qt_role.h  —  程序角色：患者端 / 管理端
 *
 *  同一套界面代码编译成两个程序：
 *      patient.exe   患者端：导诊、查医生、预约、取消预约、看路线、签到候诊
 *      admin.exe     管理端：查医生、预约总览、排班与停诊、叫号/过号/完成、看路线
 *
 *  为什么要共用一套源码而不是复制一份：
 *      界面代码里绝大部分（表格、号源格子、平面图）两端是一样的，
 *      复制会把「改一处要改两遍」的问题带进来。用角色在**页面可见性**和
 *      **按钮可见性**上做裁剪，代码只有一份。
 *
 *  区别只在编译期的一个宏（APP_ROLE_ADMIN），见 qt/CMakeLists.txt。
 * ========================================================================== */
#pragma once

namespace qt {

enum class AppRole {
  Patient = 0,  // 患者端
  Admin = 1,    // 管理端（医生 / 医院管理者）
};

// 本程序是哪个角色（编译期确定）
AppRole appRole();

inline bool isAdmin() { return appRole() == AppRole::Admin; }
inline bool isPatient() { return appRole() == AppRole::Patient; }

// 角色名，用于窗口标题与状态栏
const char* roleName();

// 该页面在本角色下是否可用（不可用的页不出现在导航栏与菜单里）
bool pageEnabled(int page);

}  // namespace qt
