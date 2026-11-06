/* =============================================================================
 *  qt_state.h  —  Qt 界面层共享状态 + 通知中心
 *
 *  为什么需要它：
 *    在 ImGui 版里所有状态放在一个 GuiState 结构里，因为 ImGui 是立即模式，
 *    每帧都从状态重新画一遍。Qt 是保留模式，控件自己保存大部分状态，
 *    所以这里只留下「**跨页面**真正需要共享」的那部分：
 *      · 当前选中的患者 / 医生（预约、管理、排班、队列几页都要用）
 *      · 路线查询的结果（路线页与平面图共用）
 *      · 「从排班页点某个格子跳到预约页」这类跨页动作
 *
 *  通知中心：
 *    在 ImGui 版里是页面顶部一条提示条（notify + 颜色）。
 *    Qt 版把它升级成「操作日志」停靠窗口：既显示最新一条，也保留历史。
 *    任何页面调 notifier().notify(...)，主窗口就会追加一行并更新状态栏。
 * ========================================================================== */
#pragma once

#include <QObject>
#include <QString>

#include "model.h"
#include "service.h"

namespace qt {

// ------------------------------ 页面 ------------------------------
// 七项功能各一页，其中「就诊路线」把楼层平面图并在一起；
// 再加一页数据总览。顺序与菜单、导航栏一致。
enum Page {
  PAGE_TRIAGE = 0,  // 1 智能导诊
  PAGE_DOCTOR,      // 2 医生查询
  PAGE_BOOK,        // 3 在线预约
  PAGE_MANAGE,      // 4 预约管理
  PAGE_SCHEDULE,    // 5 排班展示
  PAGE_ROUTE,       // 6 就诊路线（含楼层平面图）
  PAGE_QUEUE,       // 7 候诊队列
  PAGE_ABOUT,       // 数据总览
  PAGE_COUNT
};

const char* pageTitle(int page);  // "1  智能导诊" 这种，给菜单与导航栏用

// ------------------------------ 通知 ------------------------------
enum NoticeKind { NOTICE_INFO = 0, NOTICE_OK, NOTICE_WARN, NOTICE_ERROR };

// ------------------------------ 共享状态 ------------------------------
struct QtState {
  // 当前数据目录（支持用命令行参数换一套数据）
  char dataDir[128] = "data";

  // 跨页面共享的选择
  int patient = 0;  // 预约 / 预约管理 / 候诊队列共用
  int doctor = 0;   // 预约 / 排班 / 候诊队列共用
  int day = 0;      // 0..6
  int slot = 0;     // 0 上午 / 1 下午

  // 功能 6：路线
  int routeFrom = 0;
  int routeTo = 0;
  bool routeValid = false;
  RouteResult route;
  bool floorShowCode = true;  // 平面图上房间方块里显示房间号（否则显示科室名）

  // 跨页跳转意图：排班页点了某个格子 -> 预约页带着医生/日期/时段打开
  bool jumpToBooking = false;
  int jumpDoctor = 0;
  int jumpDay = 0;
  int jumpSlot = 0;
};

QtState& state();

// ------------------------------ 通知中心 ------------------------------
// 单例 QObject，用信号把消息广播给主窗口（操作日志 + 状态栏）。
// 页面之间不直接互相调用，都通过它，避免页面之间互相耦合。
class Notifier : public QObject {
  Q_OBJECT

 public:
  // 发一条操作结果。msg 不需要带「成功/失败」前缀，kind 决定颜色与图标。
  void info(const QString& msg) { Q_EMIT notified(NOTICE_INFO, msg); }
  void ok(const QString& msg) { Q_EMIT notified(NOTICE_OK, msg); }
  void warn(const QString& msg) { Q_EMIT notified(NOTICE_WARN, msg); }
  void error(const QString& msg) { Q_EMIT notified(NOTICE_ERROR, msg); }

 Q_SIGNALS:
  void notified(int kind, const QString& msg);
};

Notifier& notifier();

}  // namespace qt
