/* =============================================================================
 *  page_queue.cpp  —  候诊队列页
 * ========================================================================== */
#include "page_queue.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "service_queue.h"
#include "qt_common.h"

namespace qt {

QueuePage::QueuePage(QWidget* parent) : PageBase(parent) {
  auto* layout = new QVBoxLayout(this);

  layout->addWidget(makePageTitle(QStringLiteral("功能 7  候诊队列")));
  layout->addWidget(makeHint(QStringLiteral(
      "队列用<b>环形队列</b>实现，入队出队都是 O(1)，出队腾出的位置给后来的患者循环使用。"
      "「先来先看」由 Queue::callNext 守住：队首已被叫号就不许越过他去叫后面的人；"
      "过号记满 2 次重新排到队尾。")));

  // ---- 操作行 ----
  auto* row = new QHBoxLayout();
  row->addWidget(new QLabel(QStringLiteral("患者："), this));
  patientCombo_ = new QComboBox(this);
  patientCombo_->setMinimumWidth(180);
  row->addWidget(patientCombo_);

  row->addWidget(new QLabel(QStringLiteral("医生："), this));
  doctorCombo_ = new QComboBox(this);
  doctorCombo_->setMinimumWidth(240);
  row->addWidget(doctorCombo_);

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
  summary_ = makeHint(QString());
  row->addWidget(summary_);
  layout->addLayout(row);

  // ---- 队列表格 ----
  table_ = new QTableWidget(this);
  setupReadOnlyTable(table_, 0, 6,
                     {QStringLiteral("排队号"), QStringLiteral("患者"), QStringLiteral("状态"),
                      QStringLiteral("已叫号"), QStringLiteral("前面还有"), QStringLiteral("排到次序")});
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
}

void QueuePage::refreshPage() {
  Hospital& h = db();
  QtState& st = state();

  // 患者下拉框
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
  // 患者端已登录：只能给自己签到
  if (st.loggedIn) {
    patientCombo_->setCurrentIndex(st.patient);
    patientCombo_->setEnabled(false);
    patientCombo_->setToolTip(QStringLiteral("当前登录患者（不可切换）"));
  }

  // 医生下拉框
  const int keepD = doctorCombo_->currentIndex();
  {
    QSignalBlocker b(doctorCombo_);
    doctorCombo_->clear();
    for (int i = 0; i < h.doctors.size(); ++i)
      doctorCombo_->addItem(QStringLiteral("%1  %2（%3）")
                                .arg(QString::fromUtf8(h.doctors[i].code),
                                     QString::fromUtf8(h.doctors[i].name),
                                     deptName(h, h.doctors[i].dept)));
    int want = (keepD >= 0 && keepD < doctorCombo_->count()) ? keepD : st.doctor;
    if (want < 0 || want >= doctorCombo_->count()) want = 0;
    doctorCombo_->setCurrentIndex(want);
    st.doctor = want;
  }

  fillQueueTable();
}

void QueuePage::onDoctorChanged(int index) {
  if (index >= 0) state().doctor = index;
  fillQueueTable();
}

void QueuePage::fillQueueTable() {
  Hospital& h = db();
  const int doctor = doctorCombo_->currentIndex();
  if (doctor < 0) return;

  // 只列该医生队伍里的人（按排队次序）
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

void QueuePage::onCheckIn() {
  Hospital& h = db();
  Q_UNUSED(h);
  const int doctor = doctorCombo_->currentIndex();
  const int patient = patientCombo_->currentIndex();
  if (doctor < 0 || patient < 0) return;

  const QueueResult r = Queue::checkIn(db(), patient, doctor);
  switch (r) {
    case QUEUE_OK:
      notifier().ok(QStringLiteral("签到成功：%1 → %2，排队号 %3（前面 %4 人）")
                        .arg(patientName(db(), patient), doctorName(db(), doctor))
                        .arg(Queue::queueNoOf(db(), patient, doctor))
                        .arg(Queue::waitingAhead(db(), patient, doctor)));
      fillQueueTable();
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
}

void QueuePage::onCallNext() {
  Hospital& h = db();
  const int doctor = doctorCombo_->currentIndex();
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
      fillQueueTable();
      break;
    case QUEUE_FIRST_CALLED: {
      // 队首已经被叫过还没处理完 —— 不能越过他叫后面的人
      notifier().warn(QStringLiteral("队首的人已被叫号还未处理完，不能越过他叫下一位。\n"
                                     "请先「过号处理」或「就诊完成」。"));
      break;
    }
    case QUEUE_EMPTY:
      notifier().info(QStringLiteral("%1 当前没有候诊患者").arg(doctorName(h, doctor)));
      break;
    default:
      notifier().error(QStringLiteral("叫号失败"));
      break;
  }
}

void QueuePage::onPass() {
  Hospital& h = db();
  const int doctor = doctorCombo_->currentIndex();
  int patient = lastCalledPatient_;
  // 没叫过号就用当前选中的患者
  if (patient < 0) patient = patientCombo_->currentIndex();
  if (doctor < 0 || patient < 0) return;
  if (!Queue::isWaiting(h, patient, doctor)) {
    notifier().warn(QStringLiteral("%1 不在 %2 的队列里").arg(patientName(h, patient),
                                                             doctorName(h, doctor)));
    return;
  }

  const bool requeued = Queue::pass(h, patient, doctor);
  if (requeued) {
    lastCalledPatient_ = -1;
    notifier().warn(QStringLiteral("过号：%1 达 %2 次上限，已重新排到队尾（新排队号 %3）")
                        .arg(patientName(h, patient))
                        .arg(kMaxPass)
                        .arg(Queue::queueNoOf(h, patient, doctor)));
  } else {
    notifier().info(QStringLiteral("过号：%1 记一次（叫号次数达 %2 次会重新排队）")
                        .arg(patientName(h, patient))
                        .arg(kMaxPass));
  }
  fillQueueTable();
}

void QueuePage::onFinish() {
  Hospital& h = db();
  const int doctor = doctorCombo_->currentIndex();
  int patient = lastCalledPatient_;
  if (patient < 0) patient = patientCombo_->currentIndex();
  if (doctor < 0 || patient < 0) return;

  const QueueResult r = Queue::finish(h, patient, doctor);
  if (r == QUEUE_OK) {
    lastCalledPatient_ = -1;
    notifier().ok(QStringLiteral("就诊完成：%1 已离开 %2 的队列")
                      .arg(patientName(h, patient), doctorName(h, doctor)));
    fillQueueTable();
  } else {
    notifier().warn(QStringLiteral("%1 不在 %2 的队列里").arg(patientName(h, patient),
                                                             doctorName(h, doctor)));
  }
}

}  // namespace qt
