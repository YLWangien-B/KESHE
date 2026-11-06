/* =============================================================================
 *  page_schedule.h  —  排班展示页
 * ========================================================================== */
#pragma once

#include "qt_widgets.h"

class QComboBox;
class QLabel;
class QStackedWidget;
class QTableWidget;

namespace qt {

// 两种视角：
//   按医生    一位医生一周的号源日历（2 行 × 7 天），格内「余 x / 共 y」
//   按科室汇总 该科室全部医生 × 一周各时段，看哪个时段整体最松
// 点任意格子可以直接带到预约页（带上医生 / 日期 / 时段）。
class SchedulePage : public PageBase {
  Q_OBJECT

 public:
  explicit SchedulePage(QWidget* parent = nullptr);
  void refreshPage() override;

 private Q_SLOTS:
  void onModeChanged(int index);
  void onDoctorChanged(int index);
  void onDeptChanged(int index);
  void onCellActivated(int row, int col);

 private:
  void rebuildDoctorCombos();
  void rebuildDeptCombo();
  void fillByDoctor();
  void fillByDept();

  QComboBox* modeCombo_ = nullptr;
  QComboBox* doctorCombo_ = nullptr;
  QComboBox* deptCombo_ = nullptr;
  QStackedWidget* views_ = nullptr;
  QTableWidget* doctorTable_ = nullptr;  // 按医生
  QTableWidget* deptTable_ = nullptr;    // 按科室汇总
  QLabel* hintLabel_ = nullptr;
};

}  // namespace qt
