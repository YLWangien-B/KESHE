/* =============================================================================
 *  page_manage.h  —  预约管理页
 * ========================================================================== */
#pragma once

#include "qt_widgets.h"

class QInputDialog;

class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;

namespace qt {

// 查看与取消某个患者的预约。
// 取消时要退回号源并递增版本号 —— 与预约时用的是同一套版本号机制。
class ManagePage : public PageBase {
  Q_OBJECT

 public:
  explicit ManagePage(QWidget* parent = nullptr);
  void refreshPage() override;

  // 预约页生成凭证后，可以带着患者跳到这里
  void showPatient(int patient);

 private Q_SLOTS:
  void onPatientChanged(int index);
  void onCancelClicked();
  void onTableSelectionChanged();
  void onRateClicked();  // 扩展要求第 3 条：对已完成的就诊评价医生

 private:
  void fillTable();
  // 按「患者 + 凭证号」定位并取消
  bool cancelBooking(int patient, const QString& ticket);

  QComboBox* patientCombo_ = nullptr;
  QTableWidget* table_ = nullptr;
  QPushButton* cancelBtn_ = nullptr;
  QPushButton* rateBtn_ = nullptr;  // 评价选中的预约
  QLabel* summary_ = nullptr;
  // 待取消的记录：点「取消预约」的瞬间记下，之后用户切换患者也不影响
  int pendingPatient_ = -1;
  QString pendingTicket_;
};

}  // namespace qt
