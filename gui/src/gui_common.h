/* =============================================================================
 *  gui_common.h  —  GUI 层的公共约定
 *
 *  这一层是「界面层」的一个新实现：它只做两件事 ——
 *      1) 读 Hospital 里的数据（只读，不改）
 *      2) 把数据画到屏幕上
 *  数据结构层（ds.h / ds_sort.h）与业务层（service.h / service_queue.h）一行都没改，
 *  这正好验证了原来「业务层不 printf、结果用 struct 返回」的分层是有效的。
 *
 *  下面六个函数的用途见各自注释；它们把业务层的返回值翻译成界面能直接用的字符串。
 * ========================================================================== */
#pragma once

#include <cstdio>

#include "model.h"

// -----------------------------------------------------------------------------
//  字符串工具：GUI 里到处要把「下标 → 名字」翻出来显示
//  注意：这些函数返回的是**静态缓冲区**的地址，一次只用一个，不要同时存两个返回值。
// -----------------------------------------------------------------------------

// 取科室名；下标非法返回「未知科室」
inline const char* deptName(const Hospital& h, int dept) {
  return (dept >= 0 && dept < h.depts.size()) ? h.depts[dept].name : "未知科室";
}

// 取医生姓名
inline const char* doctorName(const Hospital& h, int doctor) {
  return (doctor >= 0 && doctor < h.doctors.size()) ? h.doctors[doctor].name : "未知医生";
}

// 取患者姓名
inline const char* patientName(const Hospital& h, int patient) {
  return (patient >= 0 && patient < h.patients.size()) ? h.patients[patient].name : "未知患者";
}

// 取房间名；-1 表示还没安排诊室
inline const char* roomName(const Hospital& h, int room) {
  return (room >= 0 && room < h.rooms.size()) ? h.rooms[room].name : "未安排诊室";
}

// 日期：0..6 -> 周一..周日（返回静态缓冲，调用后请立刻用掉）
inline const char* dayText(int day) {
  static const char* kNames[] = {"周一", "周二", "周三", "周四", "周五", "周六", "周日"};
  return (day >= 0 && day < kMaxDay) ? kNames[day] : "??";
}

// 时段：0 上午 / 1 下午（当前数据只用这两个）
inline const char* slotText(int slot) {
  static const char* kNames[] = {"上午", "下午", "前夜", "后夜"};
  return (slot >= 0 && slot < kMaxSlot) ? kNames[slot] : "??";
}

// 楼层显示名：0..3 -> 门诊楼1层 / 门诊楼2层 / 内科楼1层 / 内科楼2层
inline const char* floorText(int floor) {
  static const char* kNames[] = {"门诊楼1层", "门诊楼2层", "内科楼1层", "内科楼2层"};
  return (floor >= 0 && floor < kMaxFloor) ? kNames[floor] : "?层";
}

// 把「周X上午」这种就诊时间写进 buf
inline void timeText(int day, int slot, char* buf, int cap) {
  std::snprintf(buf, cap, "%s%s", dayText(day), slotText(slot));
}

// -----------------------------------------------------------------------------
//  号源状态：界面按「还有多少号」上色，这里统一给一个枚举，避免各处自己比较
// -----------------------------------------------------------------------------
enum SlotState {
  SLOT_NONE = 0,   // 该时段不出诊
  SLOT_FULL,       // 号已满
  SLOT_TIGHT,      // 号不多了（余量 <= 3）
  SLOT_OK,         // 号源充足
  SLOT_STOPPED,    // 已停诊
};

// 注意：Hospital::slot() 返回的是可写引用（它不是 const 方法），
// 所以这几个辅助函数一路都用非 const 的 Hospital& —— 界面只读它，但类型上必须匹配。
inline SlotState slotState(Hospital& h, int doctor, int day, int slot) {
  if (doctor < 0 || doctor >= h.doctors.size()) return SLOT_NONE;
  if (!h.hasSchedule(doctor, day, slot)) return SLOT_NONE;
  const ScheduleSlot& s = h.slot(doctor, day, slot);
  if (s.stopped) return SLOT_STOPPED;
  const int left = s.quota - s.booked;
  if (left <= 0) return SLOT_FULL;
  if (left <= 3) return SLOT_TIGHT;
  return SLOT_OK;
}

// 把号源状态翻译成一句给患者看的话
inline const char* slotStateText(Hospital& h, int doctor, int day, int slot) {
  switch (slotState(h, doctor, day, slot)) {
    case SLOT_NONE: return "该时段不出诊";
    case SLOT_STOPPED: return "该时段已停诊";
    case SLOT_FULL: return "该时段号源已满";
    case SLOT_TIGHT: return "号源紧张，请尽快预约";
    case SLOT_OK: return "可以预约";
    default: return "";
  }
}
