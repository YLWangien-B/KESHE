/* =============================================================================
 *  page_about.h  —  数据总览页
 * ========================================================================== */
#pragma once

#include "qt_widgets.h"

class QLabel;
class QTableWidget;

namespace qt {

// 显示各数据结构装了多少条、容量多少、占多少字节，以及总库规模。
// 数字全部取自当前的 Hospital 实例与实测 sizeof，不是写死的。
class AboutPage : public PageBase {
  Q_OBJECT

 public:
  explicit AboutPage(QWidget* parent = nullptr);
  void refreshPage() override;

 private Q_SLOTS:
  void onRecheck();  // 重新跑一遍一致性自检
  void onRepair();   // 按预约表把 booked 重新对账

 private:
  void fillHealth();
  void fillOps();

  QTableWidget* structTable_ = nullptr;
  QTableWidget* entityTable_ = nullptr;
  QLabel* summary_ = nullptr;
  QLabel* healthLabel_ = nullptr;   // 一致性自检结论
  QTableWidget* opsTable_ = nullptr;  // 审计日志
};

}  // namespace qt
