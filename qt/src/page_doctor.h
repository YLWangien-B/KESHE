/* =============================================================================
 *  page_doctor.h  —  医生查询页
 * ========================================================================== */
#pragma once

#include "qt_widgets.h"

class QComboBox;
class QLineEdit;
class QLabel;
class QTableWidget;

namespace qt {

// 三种查询方式：按科室 / 按职称 / 按擅长领域（关键字）
class DoctorPage : public PageBase {
  Q_OBJECT

 public:
  explicit DoctorPage(QWidget* parent = nullptr);
  void refreshPage() override;

 private Q_SLOTS:
  void onModeChanged(int index);
  void runQuery();

 private:
  QComboBox* modeCombo_ = nullptr;
  QComboBox* deptCombo_ = nullptr;
  QComboBox* titleCombo_ = nullptr;
  QLineEdit* keywordEdit_ = nullptr;
  QLabel* countLabel_ = nullptr;
  QTableWidget* table_ = nullptr;
};

}  // namespace qt
