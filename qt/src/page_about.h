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

 private:
  QTableWidget* structTable_ = nullptr;
  QTableWidget* entityTable_ = nullptr;
  QLabel* summary_ = nullptr;
};

}  // namespace qt
