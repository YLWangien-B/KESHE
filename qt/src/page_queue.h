/* =============================================================================
 *  page_queue.h  —  候诊队列页
 * ========================================================================== */
#pragma once

#include "qt_widgets.h"

class QComboBox;
class QLabel;
class QTableWidget;

namespace qt {

// 功能 7：签到排队、叫号、过号、就诊完成。
// 队列用环形队列实现，四个动作全部走 service_queue.h 的 Queue 类。
class QueuePage : public PageBase {
  Q_OBJECT

 public:
  explicit QueuePage(QWidget* parent = nullptr);
  void refreshPage() override;

 private Q_SLOTS:
  void onCheckIn();
  void onCallNext();
  void onPass();
  void onFinish();
  void onDoctorChanged(int index);

 private:
  void fillQueueTable();

  QComboBox* patientCombo_ = nullptr;
  QComboBox* doctorCombo_ = nullptr;
  QTableWidget* table_ = nullptr;
  QLabel* summary_ = nullptr;
  int lastCalledPatient_ = -1;  // 最近一次叫到的人，「就诊完成」默认操作他
};

}  // namespace qt
