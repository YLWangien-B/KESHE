/* =============================================================================
 *  page_manage.cpp  —  预约管理页
 * ========================================================================== */
#include "page_manage.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "qt_common.h"
#include <QInputDialog>

#include "shared_db.h"
#include "service_extra.h"

namespace qt {

ManagePage::ManagePage(QWidget* parent) : PageBase(parent) {
  auto* layout = new QVBoxLayout(this);

  layout->addWidget(makePageTitle(QStringLiteral("功能 4  预约管理")));
  layout->addWidget(makeHint(QStringLiteral(
      "预约记录存在顺序表里。取消时把该时段的号源退回，并递增槽的版本号 ——"
      "这样此前看到的余号信息立刻作废，与预约时用的是同一套版本号机制。")));

  // ---- 选患者 ----
  auto* row = new QHBoxLayout();
  row->addWidget(new QLabel(QStringLiteral("查看哪位患者的预约："), this));
  patientCombo_ = new QComboBox(this);
  patientCombo_->setMinimumWidth(220);
  row->addWidget(patientCombo_);
  row->addSpacing(12);
  cancelBtn_ = new QPushButton(QStringLiteral("取消选中的预约"), this);
  cancelBtn_->setEnabled(false);
  row->addWidget(cancelBtn_);

  // 扩展要求第 3 条：就诊完成后可以评价医生
  rateBtn_ = new QPushButton(QStringLiteral("评价选中的就诊"), this);
  rateBtn_->setEnabled(false);
  rateBtn_->setToolTip(QStringLiteral("只有状态为「已完成」且还没评价过的记录才能评价"));
  row->addWidget(rateBtn_);
  row->addStretch(1);
  summary_ = makeHint(QString());
  row->addWidget(summary_);
  layout->addLayout(row);

  // ---- 记录表 ----
  table_ = new QTableWidget(this);
  setupReadOnlyTable(table_, 0, 7,
                     {QStringLiteral("凭证号"), QStringLiteral("科室 / 医生"),
                      QStringLiteral("就诊时间"), QStringLiteral("就诊地点"),
                      QStringLiteral("状态"), QStringLiteral("该时段余号"),
                      QStringLiteral("评价")});
  table_->setColumnWidth(0, 96);
  table_->setColumnWidth(1, 220);
  table_->setColumnWidth(2, 110);
  table_->setColumnWidth(3, 260);
  table_->setColumnWidth(4, 90);
  table_->setColumnWidth(5, 110);
  table_->setColumnWidth(6, 110);
  layout->addWidget(table_, 1);

  connect(patientCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &ManagePage::onPatientChanged);
  connect(cancelBtn_, &QPushButton::clicked, this, &ManagePage::onCancelClicked);
  connect(table_, &QTableWidget::itemSelectionChanged, this,
          &ManagePage::onTableSelectionChanged);
  connect(rateBtn_, &QPushButton::clicked, this, &ManagePage::onRateClicked);
}

void ManagePage::refreshPage() {
  Hospital& h = db();
  QtState& st = state();

  const int keep = patientCombo_->currentIndex();
  QSignalBlocker block(patientCombo_);
  patientCombo_->clear();
  for (int i = 0; i < h.patients.size(); ++i)
    patientCombo_->addItem(patientLabel(h, i));
  int want = (keep >= 0 && keep < patientCombo_->count()) ? keep : st.patient;
  if (want < 0 || want >= patientCombo_->count()) want = 0;
  patientCombo_->setCurrentIndex(want);
  st.patient = want;

  fillTable();
}

void ManagePage::showPatient(int patient) {
  if (patient >= 0 && patient < patientCombo_->count()) patientCombo_->setCurrentIndex(patient);
}

void ManagePage::onPatientChanged(int index) {
  if (index < 0) return;
  state().patient = index;
  pendingPatient_ = -1;
  pendingTicket_.clear();
  fillTable();
}

void ManagePage::fillTable() {
  Hospital& h = db();
  const int patient = patientCombo_->currentIndex();

  QVector<int> rows;  // 该患者的预约记录下标
  for (int i = 0; i < h.bookings.size(); ++i)
    if (h.bookings[i].patient == patient) rows.append(i);

  table_->setRowCount(rows.size());
  int booked = 0;
  for (int r = 0; r < rows.size(); ++r) {
    const Booking& b = h.bookings[rows[r]];
    if (b.status == BOOKED) ++booked;

    QString status;
    switch (b.status) {
      case BOOKED: status = QStringLiteral("已预约"); break;
      case FINISHED: status = QStringLiteral("已完成"); break;
      case CANCELLED: status = QStringLiteral("已取消"); break;
      case STOPPED_CANCEL: status = QStringLiteral("因停诊取消"); break;
      default: status = QStringLiteral("未知"); break;
    }
    const int left = h.hasSchedule(b.doctor, b.day, b.slot) ? h.remaining(b.doctor, b.day, b.slot) : -1;

    const QStringList cells = {
        QString::fromUtf8(b.ticket),
        deptName(h, h.doctors[b.doctor].dept) + QStringLiteral(" · ") +
            QString::fromUtf8(h.doctors[b.doctor].name),
        timeText(b.day, b.slot),
        roomName(h, h.doctors[b.doctor].room),
        status,
        left >= 0 ? QString::number(left) : QStringLiteral("—"),
        // 评价列：已完成才显示评分，没评过显示「待评价」
        (b.status == FINISHED)
            ? (b.rated != 0 ? QStringLiteral("已评价") : QStringLiteral("待评价"))
            : QStringLiteral("—"),
    };
    for (int c = 0; c < cells.size(); ++c) {
      auto* item = new QTableWidgetItem(cells[c]);
      item->setData(Qt::UserRole, rows[r]);  // 记住它在 h.bookings 里的下标
      if (b.status == BOOKED && c == 4) item->setForeground(QColor(0x6E, 0xC8, 0x7E));
      if (b.status != BOOKED) item->setForeground(QColor(0x9A, 0xA0, 0xA6));
      table_->setItem(r, c, item);
    }
  }
  table_->resizeRowsToContents();
  table_->resizeColumnsToContents();
  table_->horizontalHeader()->setStretchLastSection(true);

  if (rows.isEmpty()) {
    summary_->setText(QStringLiteral("%1 目前没有预约记录。")
                          .arg(patientName(h, patient)));
    // 全院的预约里还有别的患者，提示一下，免得用户以为预约丢了
    int others = 0;
    int firstOther = -1;
    for (int i = 0; i < h.bookings.size(); ++i) {
      if (h.bookings[i].patient == patient || h.bookings[i].status != BOOKED) continue;
      ++others;
      if (firstOther < 0) firstOther = h.bookings[i].patient;
    }
    if (others > 0)
      summary_->setText(summary_->text() +
                        QStringLiteral("（全院的预约里还有 %1 条「已预约」，例如 %2）")
                            .arg(others)
                            .arg(patientName(h, firstOther)));
  } else {
    summary_->setText(QStringLiteral("共 %1 条记录，其中「已预约」%2 条").arg(rows.size()).arg(booked));
  }
  cancelBtn_->setEnabled(false);
  pendingPatient_ = -1;
  pendingTicket_.clear();
}

void ManagePage::onTableSelectionChanged() {
  const int row = table_->currentRow();
  if (row < 0) {
    cancelBtn_->setEnabled(false);
    return;
  }
  QTableWidgetItem* item = table_->item(row, 0);
  if (!item) return;
  pendingTicket_ = item->text();
  pendingPatient_ = patientCombo_->currentIndex();
  // 只有「已预约」的才能取消
  QTableWidgetItem* status = table_->item(row, 4);
  cancelBtn_->setEnabled(status && status->text() == QStringLiteral("已预约"));
  // 只有「已完成」且还没评过的才能评价
  QTableWidgetItem* rateItem = table_->item(row, 6);
  rateBtn_->setEnabled(status && status->text() == QStringLiteral("已完成") && rateItem &&
                       rateItem->text() == QStringLiteral("待评价"));
}

void ManagePage::onCancelClicked() {
  if (pendingTicket_.isEmpty()) return;
  Hospital& h = db();
  QtState& st = state();
  if (pendingPatient_ < 0) return;

  // 找到这条记录，把关键信息摆出来让用户确认（避免点错）
  const Booking* target = nullptr;
  for (int i = 0; i < h.bookings.size(); ++i) {
    const Booking& b = h.bookings[i];
    if (b.patient == pendingPatient_ && pendingTicket_ == QString::fromUtf8(b.ticket)) {
      target = &b;
      break;
    }
  }
  if (!target) {
    notifier().error(QStringLiteral("没有找到凭证号 %1").arg(pendingTicket_));
    return;
  }

  const QString detail =
      QStringLiteral("确定要取消预约 <b>%1</b> 吗？<br><br>"
                     "患者：%2<br>医生：%3 %4（%5）<br>时间：%6<br>地点：%7<br><br>"
                     "取消后该时段的号源会退回，其他人可以重新预约。")
          .arg(pendingTicket_, patientLabel(h, target->patient),
               QString::fromUtf8(h.doctors[target->doctor].code),
               QString::fromUtf8(h.doctors[target->doctor].name),
               QString::fromUtf8(h.doctors[target->doctor].title), timeText(target->day, target->slot),
               roomName(h, h.doctors[target->doctor].room));

  const auto ret = QMessageBox::question(this, QStringLiteral("确认取消预约"), detail,
                                         QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (ret != QMessageBox::Yes) return;

  if (cancelBooking(pendingPatient_, pendingTicket_)) {
    // 界面层状态：这个患者就是当前选中的
    st.patient = pendingPatient_;
    fillTable();
  }
}

// 取消一条预约：退回号源 + 递增版本号 + 改状态。
// 定位用「患者 + 凭证号」这一对，不用当前选中患者：
// 否则用户中途换了患者，取消的就不是他当初点的那一条了。
bool ManagePage::cancelBooking(int patient, const QString& ticket) {
  Hospital& h = db();
  const QByteArray ticketUtf8 = ticket.toUtf8();

  // 「找到记录 + 退回号源 + 改状态」必须连着做完，中间不能让另一端插进来，
  // 否则号源会被重复退回（两个进程都看到同一条「已预约」记录）。
  // 提示信息不在锁里发 —— notifier 会更新状态栏与日志控件，没必要占着锁。
  enum Outcome { kOk, kNotBooked, kNotFound } outcome = kNotFound;
  int leftAfter = 0;
  {
    shared::Lock lock;  // 跨进程互斥（单进程模式下是空操作）
    for (int i = 0; i < h.bookings.size(); ++i) {
      Booking& b = h.bookings[i];
      if (b.patient != patient) continue;
      if (std::strcmp(b.ticket, ticketUtf8.constData()) != 0) continue;

      if (b.status != BOOKED) {
        outcome = kNotBooked;
        break;
      }
      // 退回号源并递增版本号，让此前看到的余号信息立刻失效
      if (h.hasSchedule(b.doctor, b.day, b.slot)) {
        ScheduleSlot& s = h.slot(b.doctor, b.day, b.slot);
        if (s.booked > 0) {
          --s.booked;
          s.version = ++h.versionClock;
        }
      }
      b.status = CANCELLED;
      leftAfter = h.remaining(b.doctor, b.day, b.slot);
      shared::logOp(shared::OP_CANCEL, patient, b.doctor, 0);
      shared::bump();
      outcome = kOk;
      break;
    }
  }

  switch (outcome) {
    case kOk:
      notifier().ok(QStringLiteral("预约 %1 已取消，号源已退回（该时段剩余 %2 个）")
                        .arg(ticket)
                        .arg(leftAfter));
      return true;
    case kNotBooked:
      notifier().warn(QStringLiteral("预约 %1 不是「已预约」状态，无法取消").arg(ticket));
      return false;
    default:
      notifier().error(QStringLiteral("没有找到凭证号 %1（患者 %2 名下）")
                           .arg(ticket, patientName(h, patient)));
      return false;
  }
}

// 扩展要求第 3 条：对已完成的就诊评价医生。
// 评分写进 Doctor 的 rankSum / rankCount，导诊与预约页会读到它。
void ManagePage::onRateClicked() {
  Hospital& h = db();
  const int row = table_->currentRow();
  if (row < 0) return;

  QTableWidgetItem* ticketItem = table_->item(row, 0);
  QTableWidgetItem* idxItem = table_->item(row, 0);
  if (!ticketItem || !idxItem) return;

  // 第 0 列里用 UserRole 记了预约在 h.bookings 里的下标
  const int bookingIndex = idxItem->data(Qt::UserRole).toInt();
  if (bookingIndex < 0 || bookingIndex >= h.bookings.size()) return;
  const Booking& bk = h.bookings[bookingIndex];
  const QString doctorText = QString::fromUtf8(h.doctors[bk.doctor].name) +
                             QStringLiteral("（") +
                             QString::fromUtf8(h.doctors[bk.doctor].title) + QStringLiteral("）");

  bool ok = false;
  const int stars = QInputDialog::getInt(
      this, QStringLiteral("评价医生"),
      QStringLiteral("给 %1 的就诊打几分？\n\n（1 分很不满意，5 分非常满意）").arg(doctorText), 5,
      Rating::kMinStars, Rating::kMaxStars, 1, &ok);
  if (!ok) return;

  if (!Rating::submit(h, bookingIndex, stars)) {
    notifier().warn(QStringLiteral("评价失败：该记录不是「已完成」状态，或已经评价过了"));
    return;
  }
  char buf[64];
  Rating::text(h.doctors[bk.doctor], buf, sizeof(buf));
  notifier().ok(QStringLiteral("已评价 %1：%2 分。该医生当前评分 %3")
                    .arg(doctorText)
                    .arg(stars)
                    .arg(QString::fromUtf8(buf)));
  fillTable();
}

}  // namespace qt
