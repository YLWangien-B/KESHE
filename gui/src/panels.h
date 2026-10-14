/* =============================================================================
 *  panels.h  —  九个页面的绘制入口
 *
 *  七个功能页一一对应题目要求的功能 1~7，另外两页是为「图形化展示」补的：
 *    drawFloorPlan  楼层平面图 —— 题目要求「以图形化方式展示科室分布」
 *    drawAbout      数据总览   —— 给答辩时看的数据规模与结构说明
 *
 *  每个函数都只画界面，业务算法调用 service.h / service_queue.h 或已有的算法。
 * ========================================================================== */
#pragma once

#include "imgui.h"

namespace gui {

void drawTriage();      // 1 智能导诊
void drawDoctorQuery(); // 2 医生查询
void drawBooking();     // 3 在线预约
void drawManage();      // 4 预约管理
void drawSchedule();    // 5 排班展示
void drawRoute();       // 6 就诊路线
void drawQueue();       // 7 候诊队列
void drawFloorPlan();   // 附加：楼层平面图
void drawAbout();       // 附加：数据总览

// 清掉「等待确认取消」的界面状态（见 panel_manage.cpp 的说明）
void clearPendingCancel();

// -----------------------------------------------------------------------------
//  宽度按字体「量」出来，不写死像素
//
//  界面基准字号 18px，在 150% DPI 缩放下实际是 27px，一个汉字的宽度就接近 27px。
//  一开始按钮宽度是按小字号写死的（比如「生成预约凭证」给了 160px），
//  到了 27px 字体的机器上文字被裁成「生成预约凭」—— 这种缺陷编译期看不出来，
//  只有跑起来截图才看得到。所以统一改成：量出文字宽度再加内边距。
// -----------------------------------------------------------------------------
constexpr float kButtonPad = 26.0f;  // 按钮左右各留的余量

// 算出一个按钮要多宽才装得下 text
inline float buttonWidth(const char* text) {
  return ImGui::CalcTextSize(text).x + kButtonPad;
}

// 算出一个下拉框要多宽才装得下 text（下拉框内部左侧还有一点内边距）
inline float comboWidth(const char* text) { return ImGui::CalcTextSize(text).x + 52.0f; }

// 号源格子：用一个带颜色的按钮表示某个（医生, 日期, 时段）的号源状态。
// 返回 true 表示用户点了这个格子。id 用于区分同一个界面里的多个格子。
bool slotCell(const char* id, int doctor, int day, int slot, float width);

// 画一行颜色图例，说明号源格子的四种颜色分别代表什么
void drawSlotLegend();

}  // namespace gui
