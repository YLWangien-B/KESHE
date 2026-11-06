/* =============================================================================
 *  page_doctor.cpp  —  医生查询页
 * ========================================================================== */
#include "page_doctor.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "qt_common.h"

namespace qt {

DoctorPage::DoctorPage(QWidget* parent) : PageBase(parent) {
  auto* layout = new QVBoxLayout(this);

  layout->addWidget(makePageTitle(QStringLiteral("功能 2  医生查询")));
  layout->addWidget(makeHint(QStringLiteral(
      "按科室 / 职称 / 擅长领域三种方式查。35 名医生放在顺序表里，"
      "「按编号查」走散列表是平均 O(1)，而这里的三类查询是一次线性筛选 —— "
      "规模只有 36 条，线性筛选比维护三个索引更划算。")));

  // ---- 查询条件行 ----
  auto* row = new QHBoxLayout();
  row->addWidget(new QLabel(QStringLiteral("查询方式："), this));
  modeCombo_ = new QComboBox(this);
  modeCombo_->addItem(QStringLiteral("按科室"));
  modeCombo_->addItem(QStringLiteral("按职称"));
  modeCombo_->addItem(QStringLiteral("按擅长领域"));
  row->addWidget(modeCombo_);

  deptCombo_ = new QComboBox(this);
  deptCombo_->setMinimumWidth(160);
  row->addWidget(deptCombo_);

  titleCombo_ = new QComboBox(this);
  titleCombo_->setMinimumWidth(140);
  row->addWidget(titleCombo_);

  keywordEdit_ = new QLineEdit(this);
  keywordEdit_->setPlaceholderText(QStringLiteral("输入关键字，如 高血压、咳嗽"));
  keywordEdit_->setMinimumWidth(200);
  row->addWidget(keywordEdit_);

  auto* btn = new QPushButton(QStringLiteral("查询"), this);
  btn->setDefault(true);
  row->addWidget(btn);

  row->addStretch(1);
  countLabel_ = makeHint(QString());
  row->addWidget(countLabel_);
  layout->addLayout(row);

  // ---- 结果表 ----
  table_ = new QTableWidget(this);
  setupReadOnlyTable(table_, 0, 8,
                     {QStringLiteral("编号"), QStringLiteral("姓名"), QStringLiteral("科室"),
                      QStringLiteral("职称"), QStringLiteral("擅长领域"),
                      QStringLiteral("出诊地点"), QStringLiteral("评分"), QStringLiteral("状态")});
  table_->setColumnWidth(0, 70);
  table_->setColumnWidth(1, 90);
  table_->setColumnWidth(2, 110);
  table_->setColumnWidth(3, 110);
  table_->setColumnWidth(4, 220);
  table_->setColumnWidth(5, 240);
  table_->setColumnWidth(6, 70);
  layout->addWidget(table_, 1);

  connect(modeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &DoctorPage::onModeChanged);
  connect(btn, &QPushButton::clicked, this, &DoctorPage::runQuery);
  connect(keywordEdit_, &QLineEdit::returnPressed, this, &DoctorPage::runQuery);
  connect(deptCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &DoctorPage::runQuery);
  connect(titleCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &DoctorPage::runQuery);
}

void DoctorPage::refreshPage() {
  Hospital& h = db();

  // 科室下拉框
  const int keepDept = deptCombo_->currentIndex();
  QSignalBlocker b1(deptCombo_);
  deptCombo_->clear();
  for (int i = 0; i < h.depts.size(); ++i)
    deptCombo_->addItem(QStringLiteral("%1  %2").arg(QString::fromUtf8(h.depts[i].code),
                                                     deptName(h, i)));
  if (keepDept >= 0 && keepDept < deptCombo_->count()) deptCombo_->setCurrentIndex(keepDept);
  else if (deptCombo_->count() > 0) deptCombo_->setCurrentIndex(0);

  // 职称下拉框（数据里实际出现的几种，避免列出没有医生的职称）
  const int keepTitle = titleCombo_->currentIndex();
  QSignalBlocker b2(titleCombo_);
  titleCombo_->clear();
  for (int i = 0; i < h.doctors.size(); ++i) {
    const QString t = QString::fromUtf8(h.doctors[i].title);
    if (titleCombo_->findText(t) < 0) titleCombo_->addItem(t);
  }
  if (keepTitle >= 0 && keepTitle < titleCombo_->count())
    titleCombo_->setCurrentIndex(keepTitle);
  else if (titleCombo_->count() > 0)
    titleCombo_->setCurrentIndex(0);

  onModeChanged(modeCombo_->currentIndex());
}

void DoctorPage::onModeChanged(int index) {
  deptCombo_->setVisible(index == 0);
  titleCombo_->setVisible(index == 1);
  keywordEdit_->setVisible(index == 2);
  runQuery();
}

void DoctorPage::runQuery() {
  Hospital& h = db();
  const int mode = modeCombo_->currentIndex();
  const int dept = deptCombo_->currentIndex();
  const QString title = titleCombo_->currentText();
  const QString keyword = keywordEdit_->text().trimmed();

  QVector<int> hits;
  for (int i = 0; i < h.doctors.size(); ++i) {
    const Doctor& d = h.doctors[i];
    if (mode == 0 && d.dept != dept) continue;
    if (mode == 1 && QString::fromUtf8(d.title) != title) continue;
    if (mode == 2 && !keyword.isEmpty() &&
        !QString::fromUtf8(d.skill).contains(keyword, Qt::CaseInsensitive))
      continue;
    hits.append(i);
  }

  table_->setRowCount(hits.size());
  for (int r = 0; r < hits.size(); ++r) {
    const int i = hits[r];
    const Doctor& d = h.doctors[i];
    const QStringList cells = {
        QString::fromUtf8(d.code),
        QString::fromUtf8(d.name),
        deptName(h, d.dept),
        QString::fromUtf8(d.title),
        QString::fromUtf8(d.skill),
        roomName(h, d.room),
        d.rankCount > 0
            ? QStringLiteral("%1 分（%2 人）")
                  .arg(QString::number(static_cast<double>(d.rankSum) / d.rankCount, 'f', 1))
                  .arg(d.rankCount)
            : QStringLiteral("暂无评价"),
        d.rankCount > 0 ? QStringLiteral("已评价") : QStringLiteral("—"),
    };
    for (int c = 0; c < cells.size(); ++c) {
      auto* item = new QTableWidgetItem(cells[c]);
      if (c == 6 || c == 7) item->setTextAlignment(Qt::AlignCenter);
      table_->setItem(r, c, item);
    }
  }
  table_->resizeRowsToContents();

  countLabel_->setText(QStringLiteral("共 %1 名医生").arg(hits.size()));
  if (hits.isEmpty())
    notifier().warn(QStringLiteral("没有符合条件的医生"));
  else
    notifier().info(QStringLiteral("查询到 %1 名医生").arg(hits.size()));
}

}  // namespace qt
