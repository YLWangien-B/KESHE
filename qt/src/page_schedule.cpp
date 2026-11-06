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

#include "qt_app.h"
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

  // ---- 两张表放在 QStackedWidget 里，按视角切换 ----
  views_ = new QStackedWidget(this);

  doctorTable_ = new QTableWidget(this);
  doctorTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  doctorTable_->setSelectionMode(QAbstractItemView::SingleSelection);
  doctorTable_->horizontalHeader()->setStretchLastSection(true);
  doctorTable_->verticalHeader()->setVisible(true);
  views_->addWidget(doctorTable_);

  deptTable_ = new QTableWidget(this);
  deptTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  deptTable_->setSelectionMode(QAbstractItemView::SingleSelection);
  deptTable_->horizontalHeader()->setStretchLastSection(true);
  views_->addWidget(deptTable_);

  layout->addWidget(views_, 1);

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

  hintLabel_->setText(QStringLiteral("提示：点任意格子 → 直接到预约页"));

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
  deptTable_->horizontalHeader()->setStretchLastSection(true);

  hintLabel_->setText(
      QStringLiteral("%1 共 %2 名医生。格内是余号数，\"—\" 表示不出诊。点格子 → 预约页")
          .arg(deptName(h, dept))
          .arg(docs.size()));

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

// 点格子 -> 记住医生/日期/时段 -> 跳到预约页
void SchedulePage::onCellActivated(int row, int col) {
  QtState& st = state();
  Hospital& h = db();
  int doctor = -1, day = -1, slot = -1;

  if (views_->currentIndex() == 0) {
    // 按医生视图：行 = 时段，列 = 天
    doctor = doctorCombo_->currentIndex();
    slot = row;
    day = col;
    if (slotState(h, doctor, day, slot) == SLOT_NONE ||
        slotState(h, doctor, day, slot) == SLOT_STOPPED) {
      notifier().warn(QStringLiteral("%1 %2 不出诊，无法预约")
                          .arg(dayText(day), slotText(slot)));
      return;
    }
  } else {
    // 按科室视图：格子上记了医生/日/时段
    QTableWidgetItem* item = deptTable_->item(row, col);
    if (!item || !item->data(Qt::UserRole).isValid()) return;
    doctor = item->data(Qt::UserRole).toInt();
    day = item->data(Qt::UserRole + 1).toInt();
    slot = item->data(Qt::UserRole + 2).toInt();
    if (slotState(h, doctor, day, slot) == SLOT_NONE ||
        slotState(h, doctor, day, slot) == SLOT_STOPPED) {
      notifier().warn(QStringLiteral("该时段不出诊"));
      return;
    }
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

}  // namespace qt
