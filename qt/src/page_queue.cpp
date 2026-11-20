/* =============================================================================
 *  page_queue.cpp  —  候诊队列
 *
 *  队列用环形队列实现，四个动作全部走 service_queue.h 的 Queue 类。
 *
 *  两端界面不同（见 page_queue.h 的说明）：
 *    管理端 = 完整队列管理（选患者、选医生、签到、叫号、过号、完成）
 *    患者端 = 「我的候诊」卡片（只看自己的预约与排队状态，只有一个签到按钮）
 * ========================================================================== */
#include "page_queue.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "qt_common.h"
#include "qt_role.h"
#include "service_queue.h"

namespace qt {

namespace {
// 在该医生的队伍里找患者的排队记录；找不到返回 nullptr
const Waiting* findWaiting(const Hospital& h, int patient, int doctor) {
  for (int i = 0; i < h.waiting.size(); ++i) {
    const Waiting& w = h.waiting.at(i);
    if (w.patient == patient && w.doctor == doctor) return &w;
  }
  return nullptr;
}
}  // namespace

// =============================================================================
//  构造
// =============================================================================
QueuePage::QueuePage(QWidget* parent) : PageBase(parent) {
  auto* layout = new QVBoxLayout(this);

  layout->addWidget(makePageTitle(QStringLiteral("候诊队列")));

  if (isAdmin()) {
    // ---------------------------------------------------------------------
    //  管理端：完整队列管理
    // ---------------------------------------------------------------------
    layout->addWidget(makeHint(QStringLiteral(
        "队列用<b>环形队列</b>实现，入队出队都是 O(1)，出队腾出的位置给后来的患者循环使用。"
        "「先来先看」由 Queue::callNext 守住：队首已被叫号就不许越过他去叫后面的人；"
        "过号记满 2 次重新排到队尾。")));

    auto* row = new QHBoxLayout();
    row->addWidget(new QLabel(QStringLiteral("患者："), this));
    patientCombo_ = new QComboBox(this);
    patientCombo_->setMinimumWidth(180);
    row->addWidget(patientCombo_);

    row->addWidget(new QLabel(QStringLiteral("医生："), this));
    doctorCombo_ = new QComboBox(this);
    doctorCombo_->setMinimumWidth(200);
    row->addWidget(doctorCombo_);

    // 默认只列「该患者预约过的医生」——36 个医生里翻找太费劲。
    // 但叫号时可能需要看别的医生的队伍，所以留一个开关可以放开。
    showAllDoctors_ = new QCheckBox(QStringLiteral("显示全部医生"), this);
    row->addWidget(showAllDoctors_);

    auto* inBtn = new QPushButton(QStringLiteral("患者签到排队"), this);
    inBtn->setDefault(true);
    row->addWidget(inBtn);
    auto* callBtn = new QPushButton(QStringLiteral("叫下一位"), this);
    row->addWidget(callBtn);
    auto* passBtn = new QPushButton(QStringLiteral("过号处理"), this);
    row->addWidget(passBtn);
    auto* finishBtn = new QPushButton(QStringLiteral("就诊完成"), this);
    row->addWidget(finishBtn);

    row->addStretch(1);
    layout->addLayout(row);

    // 统计单独一行 —— 放在按钮行末尾会被挤成竖排（加复选框之后尤其明显）
    summary_ = makeHint(QString());
    layout->addWidget(summary_);

    table_ = new QTableWidget(this);
    setupReadOnlyTable(table_, 0, 6,
                       {QStringLiteral("排队号"), QStringLiteral("患者"), QStringLiteral("状态"),
                        QStringLiteral("已叫号"), QStringLiteral("前面还有"),
                        QStringLiteral("排到次序")});
    table_->setColumnWidth(0, 80);
    table_->setColumnWidth(1, 160);
    table_->setColumnWidth(2, 200);
    table_->setColumnWidth(3, 90);
    table_->setColumnWidth(4, 100);
    table_->setColumnWidth(5, 100);
    layout->addWidget(table_, 1);

    connect(inBtn, &QPushButton::clicked, this, &QueuePage::onCheckIn);
    connect(callBtn, &QPushButton::clicked, this, &QueuePage::onCallNext);
    connect(passBtn, &QPushButton::clicked, this, &QueuePage::onPass);
    connect(finishBtn, &QPushButton::clicked, this, &QueuePage::onFinish);
    connect(doctorCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &QueuePage::onDoctorChanged);
    connect(patientCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &QueuePage::onPatientChanged);
    connect(showAllDoctors_, &QCheckBox::toggled, this, [this](bool) {
      rebuildDoctorCombo();
      fillQueueTable();
    });
    return;
  }

  // -------------------------------------------------------------------------
  //  患者端：只有「我的候诊」卡片
  // -------------------------------------------------------------------------
  layout->addWidget(makeHint(QStringLiteral(
      "到院后选中下面的一条预约，点「<b>签到排队</b>」即可进入候诊队列。"
      "排队号、前面还有几人、有没有叫到你，这里会实时显示。"
      "叫号与就诊由医生在诊室操作。")));

  // ---- 卡片 1：我的预约 ----
  auto* myBox = new QGroupBox(QStringLiteral("我的预约"), this);
  auto* myLayout = new QVBoxLayout(myBox);
  myTable_ = new QTableWidget(myBox);
  setupReadOnlyTable(myTable_, 0, 5,
                     {QStringLiteral("凭证号"), QStringLiteral("科室 / 医生"),
                      QStringLiteral("就诊时间"), QStringLiteral("就诊地点"),
                      QStringLiteral("排队状态")},
                     false);
  myTable_->setColumnWidth(0, 100);
  myTable_->setColumnWidth(1, 240);
  myTable_->setColumnWidth(2, 120);
  myTable_->setColumnWidth(3, 260);
  myTable_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
  myLayout->addWidget(myTable_, 1);

  auto* btnRow = new QHBoxLayout();
  inBtn_ = new QPushButton(QStringLiteral("签到排队"), myBox);
  inBtn_->setDefault(true);
  inBtn_->setMinimumWidth(130);
  btnRow->addWidget(inBtn_);
  btnRow->addStretch(1);
  myLayout->addLayout(btnRow);
  layout->addWidget(myBox, 1);

  // ---- 卡片 2：我的候诊状态 ----
  auto* statusBox = new QGroupBox(QStringLiteral("我的候诊状态"), this);
  auto* statusLayout = new QVBoxLayout(statusBox);
  queueStatus_ = new QLabel(statusBox);
  queueStatus_->setTextFormat(Qt::RichText);
  queueStatus_->setWordWrap(true);
  queueStatus_->setMinimumHeight(72);
  queueStatus_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  statusLayout->addWidget(queueStatus_);
  layout->addWidget(statusBox);

  connect(inBtn_, &QPushButton::clicked, this, &QueuePage::onCheckIn);
  connect(myTable_, &QTableWidget::cellClicked, this, &QueuePage::onMyBookingClicked);
}

// =============================================================================
//  刷新
// =============================================================================
void QueuePage::refreshPage() {
  if (!isAdmin()) {
    fillMyBookings();
    updateMyQueueStatus();
    return;
  }

  Hospital& h = db();
  QtState& st = state();

  const int keepP = patientCombo_->currentIndex();
  {
    QSignalBlocker b(patientCombo_);
    patientCombo_->clear();
    for (int i = 0; i < h.patients.size(); ++i) patientCombo_->addItem(patientLabel(h, i));
    int want = (keepP >= 0 && keepP < patientCombo_->count()) ? keepP : st.patient;
    if (want < 0 || want >= patientCombo_->count()) want = 0;
    patientCombo_->setCurrentIndex(want);
    st.patient = want;
  }

  rebuildDoctorCombo();
  fillQueueTable();
}

// =============================================================================
//  管理端：该医生的队伍
// =============================================================================
// 医生下拉框：默认只列「该患者预约过（或在排队）的医生」，
// 勾上「显示全部医生」才列全部 36 位（叫号时可能需要）。
void QueuePage::rebuildDoctorCombo() {
  Hospital& h = db();
  QtState& st = state();
  const int patient = patientCombo_->currentIndex();

  QVector<int> related;
  if (patient >= 0) {
    for (int i = 0; i < h.bookings.size(); ++i) {
      const Booking& b = h.bookings[i];
      if (b.patient != patient) continue;
      if (b.status != BOOKED && b.status != FINISHED) continue;
      if (!related.contains(b.doctor)) related.append(b.doctor);
    }
    for (int i = 0; i < h.waiting.size(); ++i) {
      const Waiting& w = h.waiting.at(i);
      if (w.patient == patient && !related.contains(w.doctor)) related.append(w.doctor);
    }
  }

  // 该患者没有任何相关医生时退回全部列表，否则下拉框会是空的
  const bool filtered = (showAllDoctors_ == nullptr || !showAllDoctors_->isChecked()) &&
                        !related.isEmpty();

  const int keepD = st.doctor;
  QSignalBlocker b(doctorCombo_);
  doctorCombo_->clear();
  auto addOne = [&](int d) {
    doctorCombo_->addItem(QStringLiteral("%1  %2（%3）")
                              .arg(QString::fromUtf8(h.doctors[d].code),
                                   QString::fromUtf8(h.doctors[d].name),
                                   deptName(h, h.doctors[d].dept)),
                          d);  // 存真实医生下标
  };
  if (filtered)
    for (int d : related) addOne(d);
  else
    for (int i = 0; i < h.doctors.size(); ++i) addOne(i);

  // 恢复选中：按真实医生下标找，不能用位置
  int target = 0;
  for (int k = 0; k < doctorCombo_->count(); ++k)
    if (doctorCombo_->itemData(k).toInt() == keepD) target = k;
  if (doctorCombo_->count() > 0) doctorCombo_->setCurrentIndex(target);
  st.doctor = doctorCombo_->count() > 0 ? doctorCombo_->currentData().toInt() : 0;
}

void QueuePage::onPatientChanged(int index) {
  if (index >= 0) state().patient = index;
  rebuildDoctorCombo();
  fillQueueTable();
}

void QueuePage::onDoctorChanged(int) {
  if (doctorCombo_->count() > 0) state().doctor = doctorCombo_->currentData().toInt();
  fillQueueTable();
}

void QueuePage::fillQueueTable() {
  Hospital& h = db();
  const int doctor = doctorCombo_->count() > 0 ? doctorCombo_->currentData().toInt() : -1;
  if (doctor < 0) return;

  QVector<int> order;
  for (int i = 0; i < h.waiting.size(); ++i)
    if (h.waiting.at(i).doctor == doctor) order.append(i);

  table_->setRowCount(order.size());
  for (int r = 0; r < order.size(); ++r) {
    const Waiting& w = h.waiting.at(order[r]);
    const int called = w.calledCount;
    QString status;
    QColor color(0xC8, 0xCE, 0xD6);
    if (called == 0) {
      status = QStringLiteral("等待中");
    } else if (called == 1) {
      status = QStringLiteral("已叫号，请进诊室");
      color = QColor(0x6E, 0xC8, 0x7E);
    } else {
      status = QStringLiteral("已过号 %1 次，再叫不到就重新排队").arg(called - 1);
      color = QColor(0xD8, 0xB0, 0x50);
    }
    const QStringList cells = {
        QString::number(w.queueNo),
        patientLabel(h, w.patient),
        status,
        QString::number(called),
        QString::number(Queue::waitingAhead(h, w.patient, doctor)),
        QStringLiteral("第 %1 位").arg(Queue::positionOf(h, w.patient, doctor)),
    };
    for (int c = 0; c < cells.size(); ++c) {
      auto* item = new QTableWidgetItem(cells[c]);
      if (c != 1) item->setTextAlignment(Qt::AlignCenter);
      if (c == 2) item->setForeground(color);
      table_->setItem(r, c, item);
    }
  }
  table_->resizeRowsToContents();

  int allWaiting = 0;
  for (int i = 0; i < h.waiting.size(); ++i) ++allWaiting;
  summary_->setText(QStringLiteral("%1 的候诊人数：%2（全院共 %3 人）")
                        .arg(doctorName(h, doctor))
                        .arg(order.size())
                        .arg(allWaiting));
}

// =============================================================================
//  患者端：我的预约（含排队状态）
// =============================================================================
void QueuePage::fillMyBookings() {
  Hospital& h = db();
  const int patient = state().patient;
  if (patient < 0 || patient >= h.patients.size()) {
    myTable_->setRowCount(0);
    return;
  }

  // 只列「已预约」和「已完成」的（取消/作废的不再出现在候诊视角里）
  QVector<int> rows;
  for (int i = 0; i < h.bookings.size(); ++i) {
    const Booking& b = h.bookings[i];
    if (b.patient != patient) continue;
    if (b.status != BOOKED && b.status != FINISHED) continue;
    rows.append(i);
  }

  const int keepRow = myTable_->currentRow();
  myTable_->setRowCount(rows.size());
  for (int r = 0; r < rows.size(); ++r) {
    const Booking& b = h.bookings[rows[r]];

    QString queueText;
    QColor queueColor(0x8A, 0x91, 0x99);
    const Waiting* w = findWaiting(h, patient, b.doctor);
    if (b.status == FINISHED) {
      queueText = QStringLiteral("已完成");
      queueColor = QColor(0x6E, 0xC8, 0x7E);
    } else if (w != nullptr) {
      if (w->calledCount == 0) {
        queueText = QStringLiteral("已签到 · 排队号 %1 · 前面 %2 人")
                        .arg(w->queueNo)
                        .arg(Queue::waitingAhead(h, patient, b.doctor));
        queueColor = QColor(0x6E, 0xC8, 0x7E);
      } else {
        queueText = QStringLiteral("已叫号 · 请进诊室（排队号 %1）").arg(w->queueNo);
        queueColor = QColor(0xD8, 0xB0, 0x50);
      }
    } else {
      queueText = QStringLiteral("未签到");
    }

    const QStringList cells = {
        QString::fromUtf8(b.ticket),
        QStringLiteral("%1 %2").arg(deptName(h, h.doctors[b.doctor].dept),
                                    QString::fromUtf8(h.doctors[b.doctor].name)),
        QStringLiteral("%1 %2").arg(dayText(b.day), slotText(b.slot)),
        roomName(h, h.doctors[b.doctor].room),
        queueText,
    };
    for (int c = 0; c < cells.size(); ++c) {
      auto* item = new QTableWidgetItem(cells[c]);
      if (c == 2 || c == 4) item->setTextAlignment(Qt::AlignCenter);
      if (c == 4) item->setForeground(queueColor);
      myTable_->setItem(r, c, item);
    }
  }
  myTable_->resizeRowsToContents();
  if (keepRow >= 0 && keepRow < myTable_->rowCount()) myTable_->selectRow(keepRow);
}

// 下方「我的候诊状态」卡片
void QueuePage::updateMyQueueStatus() {
  Hospital& h = db();
  const int patient = state().patient;
  const int row = myTable_->currentRow();
  if (patient < 0 || row < 0) {
    queueStatus_->setText(QStringLiteral(
        "<span style='color:#8a9199;'>选中上面的一条预约，再点「签到排队」。</span>"));
    inBtn_->setEnabled(false);
    return;
  }

  // 表格里的行号和预约记录不是一回事，重新找一遍（和 fillMyBookings 同序）
  QVector<int> rows;
  for (int i = 0; i < h.bookings.size(); ++i) {
    const Booking& b = h.bookings[i];
    if (b.patient != patient) continue;
    if (b.status != BOOKED && b.status != FINISHED) continue;
    rows.append(i);
  }
  if (row >= rows.size()) {
    inBtn_->setEnabled(false);
    return;
  }

  const Booking& b = h.bookings[rows[row]];
  const Doctor& d = h.doctors[b.doctor];

  if (b.status == FINISHED) {
    queueStatus_->setText(QStringLiteral(
        "<span style='color:#6ec87e;'>这次就诊已完成。</span><br>"
        "可以到「预约管理」里给 %1 医生评价。")
                              .arg(QString::fromUtf8(d.name)));
    inBtn_->setEnabled(false);
    return;
  }

  const Waiting* w = findWaiting(h, patient, b.doctor);
  if (w == nullptr) {
    queueStatus_->setText(
        QStringLiteral("<b>还没签到。</b><br>%1 %2 · %3 %4<br>"
                       "点「签到排队」进入 %5 的候诊队列。")
            .arg(QString::fromUtf8(b.ticket), QStringLiteral("%1 %2")
                                                  .arg(deptName(h, d.dept),
                                                       QString::fromUtf8(d.name)),
                 dayText(b.day), slotText(b.slot))
            .arg(QString::fromUtf8(d.name)));
    inBtn_->setEnabled(true);
    return;
  }

  const int ahead = Queue::waitingAhead(h, patient, b.doctor);
  const int pos = Queue::positionOf(h, patient, b.doctor);
  if (w->calledCount == 0) {
    queueStatus_->setText(
        QStringLiteral("<span style='color:#6ec87e;'><b>已签到</b></span> · "
                       "排队号 <b>%1</b> · 前面还有 <b>%2</b> 人（第 %3 位）<br>"
                       "医生：%4 · %5<br>请在候诊区等候，叫到名字再进诊室。")
            .arg(w->queueNo)
            .arg(ahead)
            .arg(pos)
            .arg(QString::fromUtf8(d.name), roomName(h, d.room)));
  } else {
    queueStatus_->setText(
        QStringLiteral("<span style='color:#d8b050;'><b>已经叫到你了！</b></span> · "
                       "排队号 <b>%1</b>（第 %2 位）<br>"
                       "请立刻到 %3 找 %4 医生。")
            .arg(w->queueNo)
            .arg(pos)
            .arg(roomName(h, d.room), QString::fromUtf8(d.name)));
  }
  inBtn_->setEnabled(false);  // 已签到，不能再签一次
}

void QueuePage::onMyBookingClicked(int row, int) {
  Q_UNUSED(row);
  updateMyQueueStatus();
}

// =============================================================================
//  签到（两端共用入口，患者端按选中的预约取医生）
// =============================================================================
void QueuePage::onCheckIn() {
  Hospital& h = db();

  int patient = -1;
  int doctor = -1;

  if (isAdmin()) {
    patient = patientCombo_->currentIndex();
    doctor = doctorCombo_->currentData().toInt();
  } else {
    // 患者端：用当前选中的那条预约的医生
    patient = state().patient;
    const int row = myTable_->currentRow();
    QVector<int> rows;
    for (int i = 0; i < h.bookings.size(); ++i) {
      const Booking& b = h.bookings[i];
      if (b.patient != patient) continue;
      if (b.status != BOOKED && b.status != FINISHED) continue;
      rows.append(i);
    }
    if (row < 0 || row >= rows.size()) {
      notifier().warn(QStringLiteral("请先在上面选中一条预约"));
      return;
    }
    doctor = h.bookings[rows[row]].doctor;
  }

  if (doctor < 0 || patient < 0) return;

  const QueueResult r = Queue::checkIn(db(), patient, doctor);
  switch (r) {
    case QUEUE_OK:
      notifier().ok(QStringLiteral("签到成功：%1 → %2，排队号 %3（前面 %4 人）")
                        .arg(patientName(db(), patient), doctorName(db(), doctor))
                        .arg(Queue::queueNoOf(db(), patient, doctor))
                        .arg(Queue::waitingAhead(db(), patient, doctor)));
      break;
    case QUEUE_DUP:
      notifier().warn(QStringLiteral("%1 已经在 %2 的队列里了（当前第 %3 位）")
                          .arg(patientName(db(), patient), doctorName(db(), doctor))
                          .arg(Queue::positionOf(db(), patient, doctor)));
      break;
    case QUEUE_FULL:
      notifier().error(QStringLiteral("候诊队列已满（容量 %1），请稍后再签到").arg(kMaxWaiting));
      break;
    default:
      notifier().error(QStringLiteral("签到失败"));
      break;
  }

  if (isAdmin())
    fillQueueTable();
  else {
    fillMyBookings();
    updateMyQueueStatus();
  }
}

// =============================================================================
//  管理端：叫号 / 过号 / 就诊完成
// =============================================================================
void QueuePage::onCallNext() {
  Hospital& h = db();
  const int doctor = doctorCombo_->currentData().toInt();
  if (doctor < 0) return;

  int patient = -1;
  const QueueResult r = Queue::callNext(h, doctor, patient);
  switch (r) {
    case QUEUE_OK:
      lastCalledPatient_ = patient;
      patientCombo_->setCurrentIndex(patient);
      notifier().ok(QStringLiteral("叫号：%1 到 %2 就诊（排队号 %3）")
                        .arg(patientName(h, patient), doctorName(h, doctor))
                        .arg(Queue::queueNoOf(h, patient, doctor)));
      break;
    case QUEUE_FIRST_CALLED:
      notifier().warn(QStringLiteral("队首的人已被叫号还未处理完，不能越过他叫下一位。\n"
                                     "请先「过号处理」或「就诊完成」。"));
      break;
    case QUEUE_EMPTY:
      notifier().info(QStringLiteral("%1 的候诊队列为空").arg(doctorName(h, doctor)));
      break;
    default:
      notifier().error(QStringLiteral("叫号失败"));
      break;
  }
  fillQueueTable();
}

void QueuePage::onPass() {
  Hospital& h = db();
  const int doctor = doctorCombo_->currentData().toInt();
  const int patient = lastCalledPatient_ >= 0 ? lastCalledPatient_
                                              : patientCombo_->currentIndex();
  if (doctor < 0 || patient < 0) {
    notifier().warn(QStringLiteral("请先叫号，或在下拉框里选择要记过号的患者"));
    return;
  }
  if (Queue::pass(h, patient, doctor))
    notifier().info(QStringLiteral("%1 已过号 %2 次，重新排到队尾（新的排队号 %3）")
                        .arg(patientName(h, patient))
                        .arg(kMaxPass)
                        .arg(Queue::queueNoOf(h, patient, doctor)));
  else
    notifier().warn(QStringLiteral("%1 记一次过号（还没到 %2 次）")
                        .arg(patientName(h, patient))
                        .arg(kMaxPass));
  fillQueueTable();
}

void QueuePage::onFinish() {
  Hospital& h = db();
  const int doctor = doctorCombo_->currentData().toInt();
  const int patient = lastCalledPatient_ >= 0 ? lastCalledPatient_
                                              : patientCombo_->currentIndex();
  if (doctor < 0 || patient < 0) return;

  const QueueResult r = Queue::finish(h, patient, doctor);
  if (r == QUEUE_OK) {
    notifier().ok(QStringLiteral("就诊完成：%1 已离开 %2 的队列，对应预约标记为「已完成」")
                      .arg(patientName(h, patient), doctorName(h, doctor)));
    lastCalledPatient_ = -1;
  } else {
    notifier().warn(QStringLiteral("该患者不在 %1 的队列里").arg(doctorName(h, doctor)));
  }
  fillQueueTable();
}

}  // namespace qt
