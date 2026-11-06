/* =============================================================================
 *  qt_common.h  —  Qt 界面层的公共约定
 *
 *  这一层是「界面层」的第三种实现（字符界面 / Dear ImGui / Qt）。
 *  它只做两件事：读 Hospital 里的数据（只读）、把数据翻译成界面能直接用的文字与颜色。
 *  数据结构层（ds.h / ds_sort.h）与业务层（service.h / service_queue.h）一行都没改。
 *
 *  与 ImGui 版的差别：
 *    · 返回 QString 而不是 const char*，省掉一次次 QString::fromUtf8 调用
 *    · 颜色用 QColor，不再依赖 ImGui 的 IM_COL32
 *    · 不再需要字体装载 —— Qt 自带完整的 Unicode 字体回退，
 *      中文零配置就能显示（ImGui 那边要自己找到系统字体并登记字形范围）
 * ========================================================================== */
#pragma once

#include <QColor>
#include <QString>

#include "model.h"

namespace qt {

// ------------------------------ 下标 -> 名字 ------------------------------
// 全部读 Hospital，不做任何修改。下标非法时返回「未知XX」，界面不会显示空白。

QString deptName(const Hospital& h, int dept);
QString doctorName(const Hospital& h, int doctor);
QString patientName(const Hospital& h, int patient);
QString roomName(const Hospital& h, int room);

// 统一写法：编号 + 姓名，如 "D1 王秀英"
QString patientLabel(const Hospital& h, int patient);
QString doctorLabel(const Hospital& h, int doctor);
QString roomLabel(const Hospital& h, int room);

// 日期 0..6 -> 周一..周日；时段 0..3 -> 上午/下午/前夜/后夜
QString dayText(int day);
QString slotText(int slot);
QString timeText(int day, int slot);

// 楼层 0..3 -> 门诊楼1层 / 门诊楼2层 / 内科楼1层 / 内科楼2层
QString floorText(int floor);

// 职称列表（装载的数据里只会出现前四种，界面用作下拉框选项）
QStringList titleList();

// ------------------------------ 号源状态 ------------------------------
// 界面按「还有多少号」上色，统一在这里判定，避免各处自己比较 quota - booked。
enum SlotState {
  SLOT_NONE = 0,  // 该时段不出诊
  SLOT_FULL,      // 号已满
  SLOT_TIGHT,     // 号不多了（余量 <= 3）
  SLOT_OK,        // 号源充足
  SLOT_STOPPED,   // 已停诊
};

// 注意：Hospital::slot() 返回可写引用（它不是 const 方法），
// 所以这些辅助函数用非 const 的 Hospital& —— 界面只读它，但类型上必须匹配。
SlotState slotState(Hospital& h, int doctor, int day, int slot);

// 状态 -> 一句给患者看的话
QString slotStateText(Hospital& h, int doctor, int day, int slot);

// 状态 -> 颜色（与 ImGui 版同一套配色，便于两份界面对照）
QColor slotColor(SlotState s);

// 12 个科室各一种颜色，保证同科室同色（用在楼层平面图与图例上）
QColor deptColor(int dept);

// ------------------------------ 排版辅助 ------------------------------
// 把「字号 22px 的标题」「说明文字」这类重复样式集中定义，
// 页面里不再各写一份 stylesheet。
QString styleTitle();
QString styleHint();
QString styleDanger();

}  // namespace qt
