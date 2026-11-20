/* =============================================================================
 *  page_queue.h  —  候诊队列页
 *
 *  两端看到的是完全不同的界面：
 *
 *   管理端（医生 / 分诊台）：完整的队列管理
 *      选医生 → 看该医生队伍 → 签到 / 叫下一位 / 过号 / 就诊完成
 *
 *   患者端：只有「我的候诊」卡片
 *      列出自己的预约 → 选中一条 → 签到排队 → 显示排队号与前面还有几人
 *      没有选患者的框（永远是登录者本人），也没有医生的操作按钮。
 *      现实里患者到院后，要么自己在自助机上签到，要么医生在诊室帮他签，
 *      所以「签到」这一步保留，但叫号之类的动作不该给患者。
 * ========================================================================== */
#pragma once

#include "qt_widgets.h"

class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;

namespace qt {

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
  void onMyBookingClicked(int row, int col);

 private:
  void fillQueueTable();        // 管理端：该医生的队伍
  void fillMyBookings();        // 患者端：我的预约 + 排队状态
  void updateMyQueueStatus();   // 患者端：下方「我的候诊状态」

  // ---- 管理端 ----
  QComboBox* patientCombo_ = nullptr;
  QComboBox* doctorCombo_ = nullptr;
  QTableWidget* table_ = nullptr;
  QLabel* summary_ = nullptr;
  int lastCalledPatient_ = -1;  // 最近一次叫到的人，「就诊完成」默认操作他

  // ---- 患者端 ----
  QTableWidget* myTable_ = nullptr;  // 我的预约
  QLabel* queueStatus_ = nullptr;    // 我的候诊状态
  QPushButton* inBtn_ = nullptr;     // 签到排队
};

}  // namespace qt
