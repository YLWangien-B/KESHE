/* =============================================================================
 *  gui_state.h  —  GUI 自己的界面状态
 *
 *  重要：这里的字段全部是「界面状态」（选了哪个患者、当前在哪一页、上一条提示），
 *  与 Hospital 里的业务数据是两回事。业务数据仍然只有 db() 那一个实例，
 *  由 hospital（字符界面）与 hospital_gui（图形界面）两个程序共用。
 * ========================================================================== */
#pragma once

#include <cstdio>

#include "gui_common.h"
#include "model.h"
#include "service.h"

namespace gui {

// 左侧七个功能页 + 两个辅助页
enum Page {
  PAGE_TRIAGE = 0,   // 1 智能导诊
  PAGE_DOCTOR,       // 2 医生查询
  PAGE_BOOK,         // 3 在线预约
  PAGE_MANAGE,       // 4 预约管理
  PAGE_SCHEDULE,     // 5 排班展示
  PAGE_ROUTE,        // 6 就诊路线
  PAGE_QUEUE,        // 7 候诊队列
  PAGE_FLOOR,        // 附加：楼层平面图（题目要求「以图形化方式展示科室分布」）
  PAGE_ABOUT,        // 附加：数据总览
  PAGE_COUNT
};

// 提示条的类型，决定颜色
enum NoticeKind { NOTICE_NONE = 0, NOTICE_INFO, NOTICE_OK, NOTICE_WARN, NOTICE_ERROR };

struct GuiState {
  // ------------------------------ 全局 ------------------------------
  Page page = PAGE_TRIAGE;
  char dataDir[128] = "data";
  bool loadOk = false;
  int loadErrors = 0;     // 装载时未通过校验的行数
  int loadOverflows = 0;  // 装载时超出容量上限的处数

  // 提示条：每次操作后写一句，画在页面顶部
  NoticeKind noticeKind = NOTICE_NONE;
  char notice[512] = "";

  void notify(NoticeKind kind, const char* fmt, ...);
  void clearNotice() { noticeKind = NOTICE_NONE; notice[0] = '\0'; }

  // ------------------------------ 功能 1 智能导诊 ------------------------------
  bool symptomPicked[BipartiteGraph::kMaxSymptoms] = {};
  int symptomCount = 0;  // 已选症状数
  int lastPick[BipartiteGraph::kMaxSymptoms] = {};
  int lastPickCount = 0;
  int lastDept = -1;  // 上次推荐排第一的科室，用于顺便列出它的医生
  TriageResult triage;
  bool triageValid = false;

  // ------------------------------ 功能 2 医生查询 ------------------------------
  int queryMode = 0;         // 0 按科室 / 1 按职称 / 2 按擅长领域
  int queryDept = 0;         // 按科室查询时选中的科室下标
  int queryTitle = 0;        // 按职称查询时选中的职称下标
  char queryKeyword[64] = "";  // 按擅长领域查询的关键字

  // ------------------------------ 功能 3 在线预约 ------------------------------
  int bookPatient = 0;   // 选中的患者下标
  int bookDoctor = 0;    // 选中的医生下标
  int bookDay = 0;       // 选中的日期 0..6
  int bookSlot = 0;      // 选中的时段 0..1
  int bookDeptFilter = -1;  // 医生列表按科室筛选，-1 为全部
  // 预约成功后生成的凭证（画成一个弹窗）
  bool showTicket = false;
  char ticketNo[24] = "";
  char ticketPatient[64] = "";
  char ticketDept[64] = "";
  char ticketDoctor[96] = "";
  char ticketTime[32] = "";
  char ticketRoom[96] = "";
  int ticketLeft = 0;

  // ------------------------------ 功能 4 预约管理 ------------------------------
  int managePatient = 0;
  char manageCancelTicket[24] = "";  // 待确认取消的凭证号

  // ------------------------------ 功能 5 排班展示 ------------------------------
  int scheduleMode = 0;    // 0 按医生 / 1 按科室汇总
  int scheduleDoctor = 0;  // 选中的医生
  int scheduleDept = 0;    // 汇总时选中的科室

  // ------------------------------ 功能 6 就诊路线 ------------------------------
  int routeFrom = 0;   // 起点房间下标
  int routeTo = 0;     // 终点房间下标
  RouteResult route;
  bool routeValid = false;
  int routeHighlight[kMaxRouteSteps] = {};  // 要高亮的房间下标（副本，供平面图用）
  int routeHighlightCount = 0;

  // ------------------------------ 功能 7 候诊队列 ------------------------------
  int queuePatient = 0;  // 待签到的患者
  int queueDoctor = 0;   // 接诊医生
  int lastCalledPatient = -1;  // 最近一次叫号叫到的人，用于「就诊完成」的默认值
  int lastCalledDoctor = -1;

  // ------------------------------ 楼层平面图 ------------------------------
  int floorHighlightFrom = -1;
  int floorHighlightTo = -1;
  bool floorShowCode = true;
};

// GUI 状态的唯一实例（与 Hospital 用 db() 是同样的做法）
GuiState& state();

}  // namespace gui
