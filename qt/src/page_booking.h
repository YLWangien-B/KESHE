/* =============================================================================
 *  page_booking.h  —  在线预约页
 * ========================================================================== */
#pragma once

#include "qt_widgets.h"

class QComboBox;
class QLabel;
class QPushButton;

namespace qt {

class SlotGrid;

// 功能 3：选患者 → 选医生 → 在号源格子上点一个时段 → 生成预约凭证。
// 预约的核心是「检查并扣减余号」这一段的原子性（题目实现提示第 4 条）：
// 停诊检查、余量检查、扣减连着做完，中间不让出执行权，所以不会超卖。
class BookingPage : public PageBase {
  Q_OBJECT

 public:
  explicit BookingPage(QWidget* parent = nullptr);
  void refreshPage() override;

 private Q_SLOTS:
  void onPatientChanged(int index);
  void onDeptFilterChanged(int index);
  void onDoctorChanged(int index);
  void onSlotClicked(int day, int slot);
  void onBook();
  void onShowMyBookings();

 private:
  void rebuildDoctorCombo();
  void updateSlotInfo();

  QComboBox* patientCombo_ = nullptr;
  QComboBox* deptFilterCombo_ = nullptr;
  QComboBox* doctorCombo_ = nullptr;
  SlotGrid* grid_ = nullptr;
  QLabel* slotInfo_ = nullptr;
  QPushButton* bookBtn_ = nullptr;
  QPushButton* myBookingsBtn_ = nullptr;
};

}  // namespace qt
