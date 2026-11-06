/* =============================================================================
 *  qt_common.cpp  —  下标 -> 名字的翻译、号源状态与配色
 * ========================================================================== */
#include "qt_common.h"

#include <QStringList>

namespace qt {

// 数据文件里的字段都是 UTF-8 的 char[]，用 fromUtf8 转成 QString。
// 注意不能用 QString::fromLocal8Bit —— 那会按系统代码页（GBK）解释，中文全乱。
static QString u8(const char* s) { return QString::fromUtf8(s); }

QString deptName(const Hospital& h, int dept) {
  return (dept >= 0 && dept < h.depts.size()) ? u8(h.depts[dept].name) : QStringLiteral("未知科室");
}

QString doctorName(const Hospital& h, int doctor) {
  return (doctor >= 0 && doctor < h.doctors.size()) ? u8(h.doctors[doctor].name)
                                                    : QStringLiteral("未知医生");
}

QString patientName(const Hospital& h, int patient) {
  return (patient >= 0 && patient < h.patients.size()) ? u8(h.patients[patient].name)
                                                       : QStringLiteral("未知患者");
}

QString roomName(const Hospital& h, int room) {
  return (room >= 0 && room < h.rooms.size()) ? u8(h.rooms[room].name)
                                              : QStringLiteral("未安排诊室");
}

QString patientLabel(const Hospital& h, int patient) {
  if (patient < 0 || patient >= h.patients.size()) return QStringLiteral("（未选择）");
  return u8(h.patients[patient].code) + QLatin1Char(' ') + u8(h.patients[patient].name);
}

QString doctorLabel(const Hospital& h, int doctor) {
  if (doctor < 0 || doctor >= h.doctors.size()) return QStringLiteral("（未选择）");
  return u8(h.doctors[doctor].code) + QLatin1Char(' ') + u8(h.doctors[doctor].name);
}

QString roomLabel(const Hospital& h, int room) {
  if (room < 0 || room >= h.rooms.size()) return QStringLiteral("（未选择）");
  return u8(h.rooms[room].code) + QStringLiteral("  ") + u8(h.rooms[room].name);
}

QString dayText(int day) {
  static const char* kNames[] = {"周一", "周二", "周三", "周四", "周五", "周六", "周日"};
  return (day >= 0 && day < kMaxDay) ? u8(kNames[day]) : QStringLiteral("??");
}

QString slotText(int slot) {
  static const char* kNames[] = {"上午", "下午", "前夜", "后夜"};
  return (slot >= 0 && slot < kMaxSlot) ? u8(kNames[slot]) : QStringLiteral("??");
}

QString timeText(int day, int slot) { return dayText(day) + slotText(slot); }

QString floorText(int floor) {
  static const char* kNames[] = {"门诊楼1层", "门诊楼2层", "内科楼1层", "内科楼2层"};
  return (floor >= 0 && floor < kMaxFloor) ? u8(kNames[floor]) : QStringLiteral("?层");
}

QStringList titleList() {
  return {QStringLiteral("主任医师"), QStringLiteral("副主任医师"), QStringLiteral("主治医师"),
          QStringLiteral("住院医师")};
}

// -----------------------------------------------------------------------------
//  号源状态
// -----------------------------------------------------------------------------
SlotState slotState(Hospital& h, int doctor, int day, int slot) {
  if (doctor < 0 || doctor >= h.doctors.size()) return SLOT_NONE;
  if (!h.hasSchedule(doctor, day, slot)) return SLOT_NONE;
  const ScheduleSlot& s = h.slot(doctor, day, slot);
  if (s.stopped) return SLOT_STOPPED;
  const int left = s.quota - s.booked;
  if (left <= 0) return SLOT_FULL;
  if (left <= 3) return SLOT_TIGHT;
  return SLOT_OK;
}

QString slotStateText(Hospital& h, int doctor, int day, int slot) {
  switch (slotState(h, doctor, day, slot)) {
    case SLOT_NONE:
      return QStringLiteral("该时段不出诊");
    case SLOT_STOPPED:
      return QStringLiteral("该时段已停诊");
    case SLOT_FULL:
      return QStringLiteral("该时段号源已满");
    case SLOT_TIGHT:
      return QStringLiteral("号源紧张，请尽快预约");
    case SLOT_OK:
      return QStringLiteral("可以预约");
    default:
      return QString();
  }
}

QColor slotColor(SlotState s) {
  switch (s) {
    case SLOT_OK:
      return QColor(0x4E, 0xA8, 0x5B);  // 绿：号源充足
    case SLOT_TIGHT:
      return QColor(0xC8, 0x9B, 0x30);  // 黄：紧张
    case SLOT_FULL:
      return QColor(0xB0, 0x4A, 0x4A);  // 红：已满
    case SLOT_STOPPED:
      return QColor(0x8A, 0x5B, 0x5B);  // 暗红：停诊
    case SLOT_NONE:
    default:
      return QColor(0x50, 0x54, 0x5A);  // 灰：不出诊
  }
}

// 12 个科室 12 种颜色。同一个科室在平面图、图例、结果表里始终是同一个颜色。
QColor deptColor(int dept) {
  static const QColor kColors[] = {
      QColor(0x3E, 0x6B, 0xA8),  // 心血管内科  蓝
      QColor(0x3F, 0x8F, 0x6B),  // 呼吸内科    绿
      QColor(0xA8, 0x7A, 0x3E),  // 消化内科    棕黄
      QColor(0x8A, 0x5B, 0xA8),  // 神经内科    紫
      QColor(0xA8, 0x4E, 0x5B),  // 普通外科    砖红
      QColor(0x3E, 0x8E, 0xA8),  // 骨科        青
      QColor(0x6B, 0x7A, 0x3E),  // 泌尿外科    橄榄
      QColor(0xA8, 0x3E, 0x8A),  // 皮肤科      品红
      QColor(0x4E, 0x6B, 0x8A),  // 眼科        灰蓝
      QColor(0x8A, 0x6B, 0x4E),  // 耳鼻喉科    灰棕
      QColor(0x5B, 0xA8, 0x4E),  // 儿科        亮绿
      QColor(0xA8, 0x8A, 0x3E),  // 中医科      土黄
  };
  const int n = static_cast<int>(sizeof(kColors) / sizeof(kColors[0]));
  if (dept < 0 || dept >= n) return QColor(0x50, 0x54, 0x5A);
  return kColors[dept];
}

// -----------------------------------------------------------------------------
//  排版辅助：把重复的 stylesheet 集中，页面里只写逻辑
// -----------------------------------------------------------------------------
QString styleTitle() {
  return QStringLiteral("font-size:20px;font-weight:bold;color:#3b6ea8;");
}

QString styleHint() { return QStringLiteral("color:#8a9199;"); }

QString styleDanger() { return QStringLiteral("color:#b04a4a;font-weight:bold;"); }

}  // namespace qt
