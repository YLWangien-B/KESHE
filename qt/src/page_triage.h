/* =============================================================================
 *  page_triage.h  —  智能导诊页
 * ========================================================================== */
#pragma once

#include "qt_widgets.h"
#include "service.h"

class QCheckBox;
class QGridLayout;
class QLabel;
class QPushButton;
class QTableWidget;
class QWidget;

namespace qt {

// 左侧选症状（25 个复选框），右侧显示推荐科室与推荐依据。
// 算法完全复用 service.h 的 Triage::run —— 二部图邻域加权求和 + 插入排序。
class TriagePage : public PageBase {
  Q_OBJECT

 public:
  explicit TriagePage(QWidget* parent = nullptr);
  void refreshPage() override;

 private Q_SLOTS:
  void onRunTriage();
  void onClearPicked();
  void onResultClicked(int row, int col);
  void onGoBooking();  // 跳到在线预约，并预选当前选中的科室

 private:
  void buildSymptomChecks();

  QWidget* symptomBody_ = nullptr;    // 复选框的父控件（在 QScrollArea 里）
  QGridLayout* symptomGrid_ = nullptr;
  QVector<QCheckBox*> symptomBoxes_;
  QLabel* pickedLabel_ = nullptr;
  QTableWidget* resultTable_ = nullptr;
  QLabel* detailLabel_ = nullptr;
  QPushButton* goBookingBtn_ = nullptr;  // 「去预约该科室」
  int selectedDept_ = -1;                // 结果表里当前选中的科室
  TriageResult lastResult_;  // 上一次的推荐结果（点表格行时用它显示依据）
};

}  // namespace qt
