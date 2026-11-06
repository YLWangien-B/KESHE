/* =============================================================================
 *  page_triage.cpp  —  智能导诊页
 * ========================================================================== */
#include "page_triage.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QVBoxLayout>

#include "qt_common.h"

namespace qt {

namespace {
constexpr int kMaxPick = 8;  // 一次最多选几个症状（与 service.h 的 kMaxPickedSymptom 一致）
}

TriagePage::TriagePage(QWidget* parent) : PageBase(parent) {
  auto* root = new QVBoxLayout(this);

  root->addWidget(makePageTitle(QStringLiteral("功能 1  智能导诊")));
  root->addWidget(makeHint(QStringLiteral(
      "症状与科室的关联用<b>带权二部图</b>表示（题目实现提示第 1 条）："
      "两侧顶点分别是症状和科室，边权 1..5 是关联强度。"
      "选中症状后，对每个科室做「邻域加权求和」得到得分，再用插入排序取前几名。")));

  auto* body = new QHBoxLayout();

  // ---------------- 左：选症状 ----------------
  auto* leftBox = new QGroupBox(QStringLiteral("选择症状（最多 %1 个）").arg(kMaxPick), this);
  auto* leftLayout = new QVBoxLayout(leftBox);

  auto* scroll = new QScrollArea(leftBox);
  scroll->setWidgetResizable(true);
  symptomBody_ = new QWidget(scroll);
  symptomGrid_ = new QGridLayout(symptomBody_);
  symptomGrid_->setSpacing(6);
  scroll->setWidget(symptomBody_);
  leftLayout->addWidget(scroll, 1);

  pickedLabel_ = makeHint(QStringLiteral("已选 0 个症状"));
  leftLayout->addWidget(pickedLabel_);

  auto* btnRow = new QHBoxLayout();
  auto* runBtn = new QPushButton(QStringLiteral("开始导诊"), leftBox);
  runBtn->setDefault(true);
  auto* clearBtn = new QPushButton(QStringLiteral("清空"), leftBox);
  btnRow->addWidget(runBtn);
  btnRow->addWidget(clearBtn);
  leftLayout->addLayout(btnRow);

  body->addWidget(leftBox, 0);

  // ---------------- 右：推荐结果 ----------------
  auto* rightBox = new QGroupBox(QStringLiteral("推荐科室（按匹配得分排序）"), this);
  auto* rightLayout = new QVBoxLayout(rightBox);

  resultTable_ = new QTableWidget(rightBox);
  setupReadOnlyTable(resultTable_, 0, 6,
                     {QStringLiteral("名次"), QStringLiteral("科室"), QStringLiteral("得分"),
                      QStringLiteral("命中症状"), QStringLiteral("位置"),
                      QStringLiteral("该科室医生")},
                     false);  // 最后一列不拉伸：6 列固定宽度加起来会超出可用宽度
  // 前五列按内容给固定宽度；最后一列吃掉剩余空间并允许换行显示多位医生
  resultTable_->setColumnWidth(0, 52);
  resultTable_->setColumnWidth(1, 120);
  resultTable_->setColumnWidth(2, 56);
  resultTable_->setColumnWidth(3, 190);
  resultTable_->setColumnWidth(4, 130);
  resultTable_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
  rightLayout->addWidget(resultTable_, 1);

  detailLabel_ = makeHint(QStringLiteral(
      "选中一个科室后，这里会列出它的全部出诊医生 —— 导诊只到「科室」这一层，"
      "选哪位医生是下一步的事。"));
  detailLabel_->setMinimumHeight(60);
  rightLayout->addWidget(detailLabel_);

  body->addWidget(rightBox, 1);
  root->addLayout(body, 1);

  connect(runBtn, &QPushButton::clicked, this, &TriagePage::onRunTriage);
  connect(clearBtn, &QPushButton::clicked, this, &TriagePage::onClearPicked);
  connect(resultTable_, &QTableWidget::cellClicked, this, &TriagePage::onResultClicked);

  buildSymptomChecks();
}

// 症状复选框：从二部图的症状侧顶点表生成（症状名就是顶点名）
//
// 这个函数会被反复调用（每次切到本页 refreshPage 都会走一遍）。
// 所以必须做到「数量没变就直接返回」——否则每切一次页就重建 25 个复选框，
// 白白消耗控件对象；而且旧控件如果只 deleteLater 而没有转一圈事件循环，
// 它们仍然挂在父控件下，findChildren 会数出 25、50、75、100 这样的累积值。
void TriagePage::buildSymptomChecks() {
  Hospital& h = db();
  if (!symptomBody_ || !symptomGrid_) return;

  // 数量没变就不用重建（症状是所有数据里最稳定的一类，基本不会变）
  const int want = h.symptomGraph.symptomCount();
  if (symptomBoxes_.size() == want && want > 0) return;

  // 数量变了才重建。这里用 delete 而不是 deleteLater：
  // 立刻销毁，父控件下的子控件数量马上就是对的，不会留下「半删除」的中间态。
  symptomBoxes_.clear();
  while (QLayoutItem* item = symptomGrid_->takeAt(0)) {
    if (QWidget* w = item->widget()) {
      w->hide();
      delete w;
    }
    delete item;
  }

  const int cols = 3;  // 每行三个
  for (int i = 0; i < h.symptomGraph.symptomCount(); ++i) {
    auto* cb = new QCheckBox(QString::fromUtf8(h.symptomGraph.symptomName(i)), symptomBody_);
    symptomBoxes_.append(cb);
    symptomGrid_->addWidget(cb, i / cols, i % cols);
    connect(cb, &QCheckBox::checkStateChanged, this, [this](int) {
      int n = 0;
      for (QCheckBox* b : symptomBoxes_)
        if (b->isChecked()) ++n;
      pickedLabel_->setText(QStringLiteral("已选 %1 个症状（最多 %2 个）").arg(n).arg(kMaxPick));
    });
  }
  symptomGrid_->setRowStretch((h.symptomGraph.symptomCount() + cols - 1) / cols, 1);
}

void TriagePage::refreshPage() {
  buildSymptomChecks();
  onClearPicked();
}

void TriagePage::onClearPicked() {
  for (QCheckBox* cb : symptomBoxes_) cb->setChecked(false);
  resultTable_->setRowCount(0);
  lastResult_ = TriageResult();
  pickedLabel_->setText(QStringLiteral("已选 0 个症状（最多 %1 个）").arg(kMaxPick));
  detailLabel_->setText(QStringLiteral(
      "选中症状后点「开始导诊」。得分 = 所选症状与该科室的关联强度之和；"
      "得分相同的科室按先来后到排，保证同样的输入得到同样的推荐。"));
}

void TriagePage::onRunTriage() {
  Hospital& h = db();

  // 收集选中的症状顶点号
  int picked[BipartiteGraph::kMaxSymptoms];
  int n = 0;
  for (int i = 0; i < symptomBoxes_.size() && n < kMaxPick; ++i)
    if (symptomBoxes_[i]->isChecked()) picked[n++] = i;

  if (n == 0) {
    notifier().warn(QStringLiteral("请先至少选择一个症状"));
    return;
  }

  lastResult_ = Triage::run(h, picked, n);

  // 命中的症状名列表，拼成一句话
  auto matchedText = [&h](const TriageItem& it) {
    QStringList names;
    for (int k = 0; k < it.matchCount; ++k)
      names << QString::fromUtf8(h.symptomGraph.symptomName(it.matched[k]));
    return names.join(QStringLiteral("、"));
  };

  resultTable_->setRowCount(lastResult_.count);
  for (int r = 0; r < lastResult_.count; ++r) {
    const TriageItem& it = lastResult_.items[r];
    // 该科室的医生（按评分从高到低，最多列 3 位）
    QVector<int> docs;
    for (int i = 0; i < h.doctors.size(); ++i)
      if (h.doctors[i].dept == it.dept) docs.append(i);
    std::sort(docs.begin(), docs.end(), [&h](int a, int b) {
      const double ra = h.doctors[a].rankCount > 0
                            ? static_cast<double>(h.doctors[a].rankSum) / h.doctors[a].rankCount
                            : 0.0;
      const double rb = h.doctors[b].rankCount > 0
                            ? static_cast<double>(h.doctors[b].rankSum) / h.doctors[b].rankCount
                            : 0.0;
      if (ra != rb) return ra > rb;
      return a < b;  // 同分按下标，保证次序稳定
    });
    QStringList docNames;
    for (int k = 0; k < docs.size() && k < 3; ++k)
      docNames << QString::fromUtf8(h.doctors[docs[k]].name);

    const QStringList cells = {
        QString::number(r + 1),
        deptName(h, it.dept),
        QString::number(it.score),
        matchedText(it),
        QString::fromUtf8(h.depts[it.dept].location),
        docNames.join(QStringLiteral("、")),
    };
    for (int c = 0; c < cells.size(); ++c) {
      auto* item = new QTableWidgetItem(cells[c]);
      if (c == 0 || c == 2) item->setTextAlignment(Qt::AlignCenter);
      if (c == 1) {
        // 名次列用科室色做标记，和平面图的配色对应
        const QColor dc = deptColor(it.dept);
        item->setForeground(dc.lighter(150));
      }
      resultTable_->setItem(r, c, item);
    }
  }
  resultTable_->resizeRowsToContents();

  if (lastResult_.count == 0) {
    notifier().warn(QStringLiteral("所选症状没有匹配到任何科室"));
  } else {
    notifier().ok(QStringLiteral("导诊完成：推荐 %1 个科室，第一名是 %2（得分 %3）")
                      .arg(lastResult_.count)
                      .arg(deptName(h, lastResult_.items[0].dept))
                      .arg(lastResult_.items[0].score));
    onResultClicked(0, 0);
    resultTable_->selectRow(0);
  }
}

// 点某一行 -> 下方显示这个科室的完整医生列表
void TriagePage::onResultClicked(int row, int) {
  Hospital& h = db();
  if (row < 0 || row >= lastResult_.count) return;
  const TriageItem& it = lastResult_.items[row];

  QStringList lines;
  Hospital& hh = db();
  for (int i = 0; i < hh.doctors.size(); ++i) {
    if (hh.doctors[i].dept != it.dept) continue;
    const Doctor& d = hh.doctors[i];
    QString rank = QStringLiteral("暂无评价");
    if (d.rankCount > 0)
      rank = QStringLiteral("%1 分").arg(
          QString::number(static_cast<double>(d.rankSum) / d.rankCount, 'f', 1));
    lines << QStringLiteral("%1 %2（%3）— %4，评分 %5")
                 .arg(QString::fromUtf8(d.code), QString::fromUtf8(d.name),
                      QString::fromUtf8(d.title), QString::fromUtf8(d.skill), rank);
  }
  detailLabel_->setText(QStringLiteral("<b>%1</b> 的出诊医生（共 %2 位）：<br>%3")
                            .arg(deptName(h, it.dept))
                            .arg(lines.size())
                            .arg(lines.join(QStringLiteral("<br>"))));
}

}  // namespace qt
