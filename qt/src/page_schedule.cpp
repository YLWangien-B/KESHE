/* =============================================================================
 *  page_schedule.cpp  —  排班展示页
 * ========================================================================== */
#include "page_schedule.h"

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <QMessageBox>
#include <QPushButton>

#include "qt_app.h"
#include "qt_role.h"
#include "service_extra.h"
#include "qt_common.h"

namespace qt {

SchedulePage::SchedulePage(QWidget* parent) : PageBase(parent) {
  auto* layout = new QVBoxLayout(this);

  layout->addWidget(makePageTitle(QStringLiteral("功能 5  排班展示")));
  layout->addWidget(makeHint(QStringLiteral(
      "排班用二维数组存（每位医生一张 日期 × 时段 的表），两个下标就是主键，"
      "查询 O(1)。格内是「余号 / 总数」，颜色越绿越好约；点任意格子可以直接带到预约页。")));

  // ---- 筛选行 ----
  auto* filterRow = new QHBoxLayout();
  filterRow->addWidget(new QLabel(QStringLiteral("视角："), this));
  modeCombo_ = new QComboBox(this);
  modeCombo_->addItem(QStringLiteral("按医生（一个人一周的号源）"));
  modeCombo_->addItem(QStringLiteral("按科室汇总（全科医生对比）"));
  filterRow->addWidget(modeCombo_);

  doctorCombo_ = new QComboBox(this);
  doctorCombo_->setMinimumWidth(260);
  filterRow->addWidget(doctorCombo_);

  deptCombo_ = new QComboBox(this);
  deptCombo_->setMinimumWidth(160);
  filterRow->addWidget(deptCombo_);

  filterRow->addStretch(1);
  hintLabel_ = makeHint(QString());
  filterRow->addWidget(hintLabel_);
  layout->addLayout(filterRow);

  // ---- 管理端才有的一行：停诊 / 恢复出诊（扩展要求第 2 条）----
  auto* adminRow = new QHBoxLayout();
  selLabel_ = makeHint(QStringLiteral("未选中格子。点一下号源格子选中它，再点「设为停诊」。"));
  adminRow->addWidget(selLabel_, 1);
  stopBtn_ = new QPushButton(QStringLiteral("设为停诊"), this);
  stopBtn_->setEnabled(false);
  stopBtn_->setToolTip(QStringLiteral("把选中的时段停掉，并通知该时段已预约的患者改约"));
  adminRow->addWidget(stopBtn_);
  resumeBtn_ = new QPushButton(QStringLiteral("恢复出诊"), this);
  resumeBtn_->setEnabled(false);
  adminRow->addWidget(resumeBtn_);
  layout->addLayout(adminRow);
  // 患者端没有这两个按钮
  const bool adminVisible = isAdmin();
  stopBtn_->setVisible(adminVisible);
  resumeBtn_->setVisible(adminVisible);
  selLabel_->setVisible(adminVisible);

  connect(stopBtn_, &QPushButton::clicked, this, &SchedulePage::onStopClicked);
  connect(resumeBtn_, &QPushButton::clicked, this, &SchedulePage::onResumeClicked);

  // ---- 两张表放在 QStackedWidget 里，按视角切换 ----
  views_ = new QStackedWidget(this);

  doctorTable_ = new QTableWidget(this);
  doctorTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  doctorTable_->setSelectionMode(QAbstractItemView::SingleSelection);
  doctorTable_->horizontalHeader()->setStretchLastSection(false);
  // 按医生视图正好 7 天，等分列宽最整齐
  doctorTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  doctorTable_->verticalHeader()->setVisible(true);
  views_->addWidget(doctorTable_);

  deptTable_ = new QTableWidget(this);
  deptTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  deptTable_->setSelectionMode(QAbstractItemView::SingleSelection);
  deptTable_->horizontalHeader()->setStretchLastSection(false);
  // 按科室汇总是 15 列，按内容自适应
  deptTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  views_->addWidget(deptTable_);

  // 不拉伸：按医生视图只有 2 行，撑满整个高度会留下大片空白
  layout->addWidget(views_);
  layout->addStretch(1);

  connect(modeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &SchedulePage::onModeChanged);
  connect(doctorCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &SchedulePage::onDoctorChanged);
  connect(deptCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &SchedulePage::onDeptChanged);
  connect(doctorTable_, &QTableWidget::cellClicked, this, &SchedulePage::onCellActivated);
  connect(deptTable_, &QTableWidget::cellClicked, this, &SchedulePage::onCellActivated);
}

void SchedulePage::refreshPage() {
  rebuildDoctorCombos();
  rebuildDeptCombo();
  onModeChanged(modeCombo_->currentIndex());
}

// 医生下拉框：按「编号 姓名（科室 · 职称）」列出
void SchedulePage::rebuildDoctorCombos() {
  Hospital& h = db();
  const int keep = doctorCombo_->currentIndex();
  QSignalBlocker block(doctorCombo_);
  doctorCombo_->clear();
  for (int i = 0; i < h.doctors.size(); ++i) {
    doctorCombo_->addItem(QStringLiteral("%1  %2（%3 · %4）")
                              .arg(QString::fromUtf8(h.doctors[i].code),
                                   QString::fromUtf8(h.doctors[i].name), deptName(h, h.doctors[i].dept),
                                   QString::fromUtf8(h.doctors[i].title)));
  }
  if (keep >= 0 && keep < doctorCombo_->count())
    doctorCombo_->setCurrentIndex(keep);
  else if (doctorCombo_->count() > 0)
    doctorCombo_->setCurrentIndex(0);
}

void SchedulePage::rebuildDeptCombo() {
  Hospital& h = db();
  const int keep = deptCombo_->currentIndex();
  QSignalBlocker block(deptCombo_);
  deptCombo_->clear();
  for (int i = 0; i < h.depts.size(); ++i)
    deptCombo_->addItem(QStringLiteral("%1  %2").arg(QString::fromUtf8(h.depts[i].code),
                                                     deptName(h, i)));
  if (keep >= 0 && keep < deptCombo_->count())
    deptCombo_->setCurrentIndex(keep);
  else if (deptCombo_->count() > 0)
    deptCombo_->setCurrentIndex(0);
}

void SchedulePage::onModeChanged(int index) {
  const bool byDoctor = (index == 0);
  doctorCombo_->setVisible(byDoctor);
  deptCombo_->setVisible(!byDoctor);
  views_->setCurrentIndex(byDoctor ? 0 : 1);
  if (byDoctor)
    fillByDoctor();
  else
    fillByDept();
}

void SchedulePage::onDoctorChanged(int) { fillByDoctor(); }
void SchedulePage::onDeptChanged(int) { fillByDept(); }

// 按医生：2 行（上午/下午）× 7 天
void SchedulePage::fillByDoctor() {
  Hospital& h = db();
  QtState& st = state();
  const int doctor = doctorCombo_->currentIndex();
  if (doctor < 0 || doctor >= h.doctors.size()) return;
  st.doctor = doctor;

  // 列 = 7 天，行 = 2 个时段。时段名放在行标题里，
  // 不再额外占一列「时段」——否则行标题和第一列会重复显示「上午/下午」。
  QStringList headers;
  for (int d = 0; d < kMaxDay; ++d) headers << dayText(d);
  const int oldRow = doctorTable_->currentRow();
  doctorTable_->setRowCount(2);
  doctorTable_->setColumnCount(kMaxDay);
  doctorTable_->setHorizontalHeaderLabels(headers);
  doctorTable_->setVerticalHeaderLabels({slotText(0), slotText(1)});
  doctorTable_->verticalHeader()->setVisible(true);
  doctorTable_->horizontalHeader()->setStretchLastSection(true);

  // 文案要跟角色一致：管理端点格子只是选中，并不会跳到预约页
  hintLabel_->setText(isAdmin()
                          ? QStringLiteral("点格子选中后，用下方按钮停诊 / 恢复出诊")
                          : QStringLiteral("提示：点任意格子 → 直接到预约页"));

  for (int s = 0; s < 2; ++s) {
    for (int d = 0; d < kMaxDay; ++d) {
      const SlotState state1 = slotState(h, doctor, d, s);
      QString text;
      if (state1 == SLOT_NONE)
        text = QStringLiteral("不出诊");
      else if (state1 == SLOT_STOPPED)
        text = QStringLiteral("停诊");
      else
        text = QStringLiteral("余 %1 / 共 %2").arg(h.remaining(doctor, d, s)).arg(
            h.slot(doctor, d, s).quota);

      auto* item = new QTableWidgetItem(text);
      item->setTextAlignment(Qt::AlignCenter);
      const QColor c = slotColor(state1);
      if (state1 == SLOT_NONE || state1 == SLOT_STOPPED) {
        item->setForeground(QColor(0x9A, 0xA0, 0xA6));
      } else {
        item->setBackground(QColor(c.red(), c.green(), c.blue(), 60));
      }
      doctorTable_->setItem(s, d, item);
    }
  }
  doctorTable_->resizeRowsToContents();
  if (oldRow >= 0 && oldRow < 2) doctorTable_->selectRow(oldRow);
}

// 按科室汇总：每行一位医生，每列一个「周X + 时段」，格内余号
void SchedulePage::fillByDept() {
  Hospital& h = db();
  const int dept = deptCombo_->currentIndex();
  if (dept < 0 || dept >= h.depts.size()) return;

  QVector<int> docs;
  for (int i = 0; i < h.doctors.size(); ++i)
    if (h.doctors[i].dept == dept) docs.append(i);

  QStringList headers = {QStringLiteral("医生")};
  for (int d = 0; d < kMaxDay; ++d)
    for (int s = 0; s < 2; ++s) headers << dayText(d) + slotText(s);

  const int oldRow = deptTable_->currentRow();
  deptTable_->setRowCount(docs.size());
  deptTable_->setColumnCount(headers.size());
  deptTable_->setHorizontalHeaderLabels(headers);
  deptTable_->setVerticalHeaderLabels(QStringList());  // 医生名放第一列

  hintLabel_->setText(
      QStringLiteral("%1 共 %2 名医生。格内是余号数，\"—\" 表示不出诊。%3")
          .arg(deptName(h, dept))
          .arg(docs.size())
          .arg(isAdmin() ? QStringLiteral("点格子选中后可用下方按钮停诊")
                         : QStringLiteral("点格子 → 预约页")));

  for (int r = 0; r < docs.size(); ++r) {
    const int doc = docs[r];
    auto* nameItem = new QTableWidgetItem(QStringLiteral("%1 %2").arg(
        QString::fromUtf8(h.doctors[doc].code), QString::fromUtf8(h.doctors[doc].name)));
    deptTable_->setItem(r, 0, nameItem);
    for (int d = 0; d < kMaxDay; ++d) {
      for (int s = 0; s < 2; ++s) {
        const int col = 1 + d * 2 + s;
        const SlotState stt = slotState(h, doc, d, s);
        QString text;
        if (stt == SLOT_NONE || stt == SLOT_STOPPED)
          text = QStringLiteral("—");
        else
          text = QString::number(h.remaining(doc, d, s));
        auto* item = new QTableWidgetItem(text);
        item->setTextAlignment(Qt::AlignCenter);
        // 把「哪位医生 / 哪天 / 哪时段」记在格子上，点击时要用
        item->setData(Qt::UserRole, doc);
        item->setData(Qt::UserRole + 1, d);
        item->setData(Qt::UserRole + 2, s);
        if (stt != SLOT_NONE && stt != SLOT_STOPPED) {
          const QColor c = slotColor(stt);
          item->setBackground(QColor(c.red(), c.green(), c.blue(), 50));
        } else {
          item->setForeground(QColor(0x9A, 0xA0, 0xA6));
        }
        deptTable_->setItem(r, col, item);
      }
    }
  }
  deptTable_->resizeColumnsToContents();
  if (oldRow >= 0 && oldRow < docs.size()) deptTable_->selectRow(oldRow);
}

// 点格子：按角色分流。
//   患者端 —— 记下医生/日期/时段，跳到预约页去约（原来的行为）
//   管理端 —— 只把这一格选起来，供「设为停诊 / 恢复出诊」使用。
//             管理端不该被带到预约页（预约是患者的事）。
void SchedulePage::onCellActivated(int row, int col) {
  QtState& st = state();
  Hospital& h = db();
  int doctor = -1, day = -1, slot = -1;

  if (views_->currentIndex() == 0) {
    // 按医生视图：行 = 时段，列 = 天
    doctor = doctorCombo_->currentIndex();
    slot = row;
    day = col;
  } else {
    // 按科室视图：格子上记了医生/日/时段
    QTableWidgetItem* item = deptTable_->item(row, col);
    if (!item || !item->data(Qt::UserRole).isValid()) return;
    doctor = item->data(Qt::UserRole).toInt();
    day = item->data(Qt::UserRole + 1).toInt();
    slot = item->data(Qt::UserRole + 2).toInt();
  }
  if (doctor < 0 || day < 0 || slot < 0) return;

  const SlotState ss = slotState(h, doctor, day, slot);

  // ---- 管理端：记住选中，启停诊/恢复按钮 ----
  if (isAdmin()) {
    selDoctor_ = doctor;
    selDay_ = day;
    selSlot_ = slot;
    const bool assigned = (ss != SLOT_NONE);
    stopBtn_->setEnabled(assigned && ss != SLOT_STOPPED);
    resumeBtn_->setEnabled(assigned && ss == SLOT_STOPPED);
    selLabel_->setText(QStringLiteral("已选中：%1 · %2%3%4")
                           .arg(doctorName(h, doctor), dayText(day), slotText(slot),
                                ss == SLOT_STOPPED ? QStringLiteral("（当前已停诊）")
                                                   : QString()));
    return;
  }

  // ---- 患者端：不出诊/停诊的格子点了没意义，直接提示 ----
  if (ss == SLOT_NONE) {
    notifier().warn(QStringLiteral("%1 %2 不出诊，无法预约").arg(dayText(day), slotText(slot)));
    return;
  }
  if (ss == SLOT_STOPPED) {
    notifier().warn(QStringLiteral("%1 %2 已停诊").arg(dayText(day), slotText(slot)));
    return;
  }

  // 记下跳转意图，预约页 refreshPage 时会用它预填
  st.jumpToBooking = true;
  st.jumpDoctor = doctor;
  st.jumpDay = day;
  st.jumpSlot = slot;
  notifier().info(QStringLiteral("已选择 %1 %2 %3，跳到预约页")
                      .arg(doctorName(h, doctor), dayText(day), slotText(slot)));
  if (auto* w = qobject_cast<MainWindow*>(window())) w->switchPage(PAGE_BOOK);
}

// =============================================================================
//  扩展要求第 2 条：医生临时停诊时通知已预约患者
// =============================================================================
void SchedulePage::onStopClicked() {
  Hospital& h = db();
  if (selDoctor_ < 0) return;

  const QString when = dayText(selDay_) + slotText(selSlot_);
  const int booked = h.slot(selDoctor_, selDay_, selSlot_).booked;

  // 把影响说清楚再让操作者确认 —— 停诊会取消别人的预约，不该一点就生效
  const QString detail =
      QStringLiteral("确定把 <b>%1</b> %2 设为停诊吗？<br><br>"
                     "该时段当前有 <b>%3</b> 条已预约记录，"
                     "停诊后它们会被标记为「因停诊取消」并通知患者改约。<br><br>"
                     "（号源不会自动恢复给这些患者 —— 停诊期间号已经被别人约走了，"
                     "只能让他们重新预约。）")
          .arg(doctorName(h, selDoctor_), when)
          .arg(booked);

  if (QMessageBox::question(this, QStringLiteral("设为停诊"), detail,
                            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) !=
      QMessageBox::Yes)
    return;

  // ② 停诊的同时把受影响的患者改到替代时段 —— 只"通知已停诊"是不够的，
  //    患者还得自己从头重约一遍。改约成功会生成新凭证，原记录留痕。
  const StopClinic::StopResult r =
      StopClinic::applyAndReschedule(h, selDoctor_, selDay_, selSlot_);
  if (!r.valid) {
    notifier().error(QStringLiteral("该时段没有排班，无法停诊"));
    return;
  }
  if (r.affected == 0) {
    notifier().ok(QStringLiteral("已停诊：%1 %2%3（该时段本来没有预约，无需通知）")
                      .arg(doctorName(h, selDoctor_), dayText(selDay_), slotText(selSlot_)));
  } else if (r.failed == 0) {
    notifier().ok(QStringLiteral("已停诊：%1 %2%3。%4 位患者已自动改约到其它时段"
                                 "（新凭证见其预约管理）")
                      .arg(doctorName(h, selDoctor_), dayText(selDay_), slotText(selSlot_))
                      .arg(r.rescheduled));
  } else {
    notifier().warn(QStringLiteral("已停诊：%1 %2%3。%4 位自动改约成功，%5 位没找到"
                                   "合适时段，需患者自行重约")
                        .arg(doctorName(h, selDoctor_), dayText(selDay_), slotText(selSlot_))
                        .arg(r.rescheduled)
                        .arg(r.failed));
  }

  // 刷新界面
  onModeChanged(modeCombo_->currentIndex());
}

void SchedulePage::onResumeClicked() {
  Hospital& h = db();
  if (selDoctor_ < 0) return;

  if (StopClinic::resume(h, selDoctor_, selDay_, selSlot_)) {
    notifier().ok(QStringLiteral("已恢复出诊：%1 %2%3")
                      .arg(doctorName(h, selDoctor_), dayText(selDay_), slotText(selSlot_)));
    onModeChanged(modeCombo_->currentIndex());
  } else {
    notifier().warn(QStringLiteral("该时段本来就不是停诊状态"));
  }
}

}  // namespace qt
