/* =============================================================================
 *  page_about.cpp  —  数据总览页
 *
 *  这一页是给答辩时看的：把六种数据结构实际装了多少、占多少字节摆在屏幕上，
 *  全部用 sizeof 与 size() 实测，没有写死任何数字。
 * ========================================================================== */
#include "page_about.h"

#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

#include <QDateTime>
#include <QMessageBox>
#include <QPushButton>

#include "ds.h"
#include "model.h"
#include "qt_common.h"
#include "qt_role.h"
#include "shared_db.h"
#include "service_extra.h"

namespace qt {

namespace {

// 往表格里填一行：类型名 | 用途 | 已装/容量 | 字节数
void addRow(QTableWidget* t, int row, const QString& type, const QString& use, int used, int cap,
            long long bytes) {
  t->setItem(row, 0, new QTableWidgetItem(type));
  t->setItem(row, 1, new QTableWidgetItem(use));
  t->setItem(row, 2, new QTableWidgetItem(QStringLiteral("%1 / %2").arg(used).arg(cap)));
  t->setItem(row, 3, new QTableWidgetItem(QString::number(bytes)));
}

}  // namespace

AboutPage::AboutPage(QWidget* parent) : PageBase(parent) {
  auto* layout = new QVBoxLayout(this);

  layout->addWidget(makePageTitle(QStringLiteral("数据总览")));
  layout->addWidget(makeHint(QStringLiteral(
      "下面所有数字都是程序运行期从当前实例读出来的：容量取自各结构的 capacity()，"
      "占用字节取自 sizeof。数据结构层与业务层与字符界面完全相同，"
      "换界面不改业务。")));

  summary_ = new QLabel(this);
  summary_->setWordWrap(true);
  layout->addWidget(summary_);

  // ---- 数据结构占用 ----
  auto* box1 = new QGroupBox(QStringLiteral("六种数据结构（自行实现，未使用任何容器类库）"), this);
  auto* v1 = new QVBoxLayout(box1);
  structTable_ = new QTableWidget(box1);
  setupReadOnlyTable(structTable_, 9, 4,
                     {QStringLiteral("类型"), QStringLiteral("用途"),
                      QStringLiteral("已装 / 容量"), QStringLiteral("sizeof（字节）")},
                     false);
  // 190 才放得下 SeqList&lt;Department,16&gt; 这种最长的类型名（原来 150 会被截断）
  structTable_->setColumnWidth(0, 190);
  structTable_->setColumnWidth(1, 260);
  structTable_->setColumnWidth(2, 120);
  structTable_->setColumnWidth(3, 130);
  v1->addWidget(structTable_);
  layout->addWidget(box1, 2);

  // ---- 实体记录 ----
  auto* box2 = new QGroupBox(QStringLiteral("实体记录（全部是定长记录，只存数据不存指针）"), this);
  auto* v2 = new QVBoxLayout(box2);
  entityTable_ = new QTableWidget(box2);
  setupReadOnlyTable(entityTable_, 7, 3,
                     {QStringLiteral("实体"), QStringLiteral("含义"),
                      QStringLiteral("sizeof（字节）")},
                     false);
  entityTable_->setColumnWidth(0, 130);
  entityTable_->setColumnWidth(1, 300);
  entityTable_->setColumnWidth(2, 130);
  v2->addWidget(entityTable_);
  layout->addWidget(box2, 2);

  // ---------------- 健壮性：一致性自检（自选功能）----------------
  auto* box3 = new QGroupBox(QStringLiteral("数据一致性自检"), this);
  auto* v3 = new QVBoxLayout(box3);
  healthLabel_ = new QLabel(box3);
  healthLabel_->setWordWrap(true);
  healthLabel_->setTextFormat(Qt::RichText);
  v3->addWidget(healthLabel_);
  auto* btnRow = new QHBoxLayout();
  auto* recheck = new QPushButton(QStringLiteral("重新自检"), box3);
  recheck->setMaximumWidth(120);
  btnRow->addWidget(recheck);
  auto* repair = new QPushButton(QStringLiteral("自检并修复"), box3);
  repair->setMaximumWidth(140);
  repair->setToolTip(QStringLiteral("把各时段的「已约数」按预约表重新对账 —— 只在自检报不一致时用"));
  btnRow->addWidget(repair);
  btnRow->addStretch(1);
  v3->addLayout(btnRow);
  layout->addWidget(box3);

  // ---------------- 健壮性：审计日志（自选功能）----------------
  auto* box4 = new QGroupBox(QStringLiteral("操作审计日志（最近 256 条，环形覆盖）"), this);
  auto* v4 = new QVBoxLayout(box4);
  opsTable_ = new QTableWidget(box4);
  setupReadOnlyTable(opsTable_, 0, 5,
                     {QStringLiteral("序号"), QStringLiteral("时间"), QStringLiteral("操作"),
                      QStringLiteral("患者"), QStringLiteral("医生")});
  opsTable_->setColumnWidth(0, 70);
  opsTable_->setColumnWidth(1, 90);
  opsTable_->setColumnWidth(2, 130);
  opsTable_->setColumnWidth(3, 160);
  opsTable_->setColumnWidth(4, 160);
  v4->addWidget(opsTable_);
  layout->addWidget(box4, 2);

  connect(recheck, &QPushButton::clicked, this, &AboutPage::onRecheck);
  connect(repair, &QPushButton::clicked, this, &AboutPage::onRepair);
}

// 跑一遍一致性自检，把结论写进标签
void AboutPage::fillHealth() {
  Hospital& h = db();
  const Health::Report r = Health::check(h);

  QString html;
  if (r.ok()) {
    html = QStringLiteral("<span style='color:#6ec87e;font-weight:bold;'>✓ 数据一致</span>："
                          "预约表 %1 条（其中「已预约」%2 条），各时段的已约数与预约表"
                          "<b>逐条对上</b>，所有下标引用有效。")
               .arg(r.totalBookings)
               .arg(r.activeBookings);
  } else {
    html = QStringLiteral("<span style='color:#b04a4a;font-weight:bold;'>✗ 发现不一致</span>："
                          "余额对不上 %1 处、无效引用 %2 条、负数余额 %3 处。"
                          "这意味着系统以为还有号、实际已被约走 —— 会超卖，建议点「自检并修复」。")
               .arg(r.slotUnder)
               .arg(r.badBooking + r.badWaiting)
               .arg(r.negativeBooked);
  }
  if (r.slotOver > 0) {
    html += QStringLiteral("<br><span style='color:#8a9199;'>另有 %1 个时段的已约数"
                           "大于本系统里的预约记录 —— 那是数据文件自带的初始已约数，属正常。</span>")
                .arg(r.slotOver);
  }
  if (r.nearCapacity > 0) {
    html += QStringLiteral("<br><span style='color:#d8b050;'>⚠ 有 %1 张表已用超过 90%% 容量，"
                           "再涨就会开始丢记录。</span>")
                .arg(r.nearCapacity);
  }
  if (!shared::isOpen()) {
    html += QStringLiteral("<br><span style='color:#8a9199;'>当前是单进程模式（没传 --shared）；"
                           "共享模式下这项自检更有意义 —— 它能发现两个程序并发写入留下的问题。</span>");
  }
  healthLabel_->setText(html);
}

// 显示共享区里的审计日志（最新的在上面）
void AboutPage::fillOps() {
  const shared::OpEntry* ops = shared::opEntries();
  if (ops == nullptr) {
    opsTable_->setRowCount(0);
    return;
  }
  Hospital& h = db();

  // 收集所有有效记录（seq != 0），按序号从大到小
  QVector<const shared::OpEntry*> list;
  for (int i = 0; i < shared::kOpLogSize; ++i)
    if (ops[i].seq != 0) list.append(&ops[i]);
  std::sort(list.begin(), list.end(),
            [](const shared::OpEntry* a, const shared::OpEntry* b) { return a->seq > b->seq; });

  opsTable_->setRowCount(list.size());
  for (int r = 0; r < list.size(); ++r) {
    const shared::OpEntry* e = list[r];
    // 时间戳只到秒，用 QDateTime 转成可读字符串
    const QDateTime dt = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(e->time));
    const QStringList cells = {
        QString::number(e->seq),
        dt.toString(QStringLiteral("HH:mm:ss")),
        QString::fromUtf8(shared::opName(e->op)),
        e->patient >= 0 ? patientLabel(h, e->patient) : QStringLiteral("—"),
        e->doctor >= 0 ? doctorLabel(h, e->doctor) : QStringLiteral("—"),
    };
    for (int c = 0; c < cells.size(); ++c) {
      auto* item = new QTableWidgetItem(cells[c]);
      if (c != 2) item->setTextAlignment(Qt::AlignCenter);
      opsTable_->setItem(r, c, item);
    }
  }
  opsTable_->resizeColumnsToContents();
}

// 自动修复：把 booked 按预约表重算（预约表是事实，booked 只是它的计数缓存）
void AboutPage::onRepair() {
  const Health::Report before = Health::check(db());
  if (before.ok()) {
    notifier().info(QStringLiteral("数据本来就是一致的，不需要修复"));
    return;
  }
  if (QMessageBox::question(
          this, QStringLiteral("自检并修复"),
          QStringLiteral("检测到 %1 处「已约数」与预约表对不上。\n\n"
                         "修复会把每个时段的已约数按预约表重新算一遍并写回。\n"
                         "预约表本身不会被改动。\n\n继续吗？")
              .arg(before.slotUnder),
          QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
    return;

  const int fixed = Health::repair(db());
  fillHealth();
  fillOps();
  if (fixed > 0)
    notifier().ok(QStringLiteral("修复完成：修正了 %1 个时段的已约数").arg(fixed));
  else
    notifier().ok(QStringLiteral("修复完成：没有需要修正的时段"));
}

void AboutPage::onRecheck() {
  fillHealth();
  fillOps();
  const Health::Report r = Health::check(db());
  if (r.ok())
    notifier().ok(QStringLiteral("一致性自检通过：预约表 %1 条，各时段余额全部对上")
                      .arg(r.totalBookings));
  else
    notifier().warn(QStringLiteral("一致性自检发现问题：余额对不上 %1 处、无效引用 %2 条")
                        .arg(r.slotUnder)
                        .arg(r.badBooking + r.badWaiting));
}

void AboutPage::refreshPage() {
  Hospital& h = db();
  fillHealth();
  fillOps();

  const long long total = static_cast<long long>(sizeof(Hospital));
  summary_->setText(QStringLiteral(
                        "<b>数据总库 sizeof(Hospital) = %1 字节（约 %2 KB）</b>，"
                        "全程序只有 db() 这一个实例，放在静态存储区而不是栈上。")
                        .arg(total)
                        .arg(total / 1024));

  const QStringList structs = {
      QStringLiteral("SeqList<Department,16>"), QStringLiteral("SeqList<Doctor,64>"),
      QStringLiteral("SeqList<Patient,128>"),    QStringLiteral("SeqList<Booking,256>"),
      QStringLiteral("SeqList<Room,64>"),
      QStringLiteral("BipartiteGraph"),               QStringLiteral("RingQueue<Waiting,64>"),
      QStringLiteral("WeightedGraph"),                QStringLiteral("HashMap × 4"),
  };
  const QStringList uses = {
      QStringLiteral("科室表"),
      QStringLiteral("医生表（每人含一张 7×4 排班表）"),
      QStringLiteral("患者表"),
      QStringLiteral("预约记录表"),
      QStringLiteral("房间表（含电梯口、楼梯口）"),
      QStringLiteral("症状—科室带权二部图（智能导诊）"),
      QStringLiteral("候诊队列（环形队列）"),
      QStringLiteral("就诊路网（邻接矩阵 + Floyd 预计算）"),
      QStringLiteral("编号 → 下标的四个索引"),
  };
  const QList<QPair<int, int>> usedCap = {
      {h.depts.size(), kMaxDept},
      {h.doctors.size(), kMaxDoctor},
      {h.patients.size(), kMaxPatient},
      {h.bookings.size(), kMaxBook},
      {h.rooms.size(), kMaxRoom},
      {h.symptomGraph.symptomCount(), BipartiteGraph::kMaxSymptoms},
      {h.waiting.size(), kMaxWaiting},
      {h.road.nodeCount(), WeightedGraph::kMaxNodes},
      {h.depts.size() + h.doctors.size() + h.patients.size() + h.rooms.size(), 1024},
  };
  const QList<long long> sizes = {
      static_cast<long long>(sizeof(SeqList<Department, kMaxDept>)),
      static_cast<long long>(sizeof(SeqList<Doctor, kMaxDoctor>)),
      static_cast<long long>(sizeof(SeqList<Patient, kMaxPatient>)),
      static_cast<long long>(sizeof(SeqList<Booking, kMaxBook>)),
      static_cast<long long>(sizeof(SeqList<Room, kMaxRoom>)),
      static_cast<long long>(sizeof(BipartiteGraph)),
      static_cast<long long>(sizeof(RingQueue<Waiting, kMaxWaiting>)),
      static_cast<long long>(sizeof(WeightedGraph)),
      static_cast<long long>(sizeof(HashMap)) * 4,
  };
  for (int i = 0; i < structs.size(); ++i)
    addRow(structTable_, i, structs[i], uses[i], usedCap[i].first, usedCap[i].second, sizes[i]);

  const QStringList ents = {QStringLiteral("Department"), QStringLiteral("Doctor"),
                            QStringLiteral("Room"),       QStringLiteral("Patient"),
                            QStringLiteral("Booking"),    QStringLiteral("Waiting"),
                            QStringLiteral("ScheduleSlot")};
  const QStringList entUse = {
      QStringLiteral("科室：编号 / 名称 / 位置 / 描述"),
      QStringLiteral("医生：编号 / 姓名 / 科室 / 职称 / 擅长 / 房间 / 评分累计"),
      QStringLiteral("房间：编号 / 名称 / 楼层 / 科室 / 路网节点号"),
      QStringLiteral("患者：编号 / 姓名 / 电话"),
      QStringLiteral("预约：凭证号 / 患者 / 医生 / 日期 / 时段 / 状态"),
      QStringLiteral("候诊元素：患者 / 医生 / 排队号 / 已叫号次数"),
      QStringLiteral("排班槽：号源上限 / 已预约 / 版本号 / 是否排班 / 是否停诊"),
  };
  const QList<long long> entSizes = {
      static_cast<long long>(sizeof(Department)), static_cast<long long>(sizeof(Doctor)),
      static_cast<long long>(sizeof(Room)),       static_cast<long long>(sizeof(Patient)),
      static_cast<long long>(sizeof(Booking)),    static_cast<long long>(sizeof(Waiting)),
      static_cast<long long>(sizeof(ScheduleSlot)),
  };
  for (int i = 0; i < ents.size(); ++i) {
    entityTable_->setItem(i, 0, new QTableWidgetItem(ents[i]));
    entityTable_->setItem(i, 1, new QTableWidgetItem(entUse[i]));
    entityTable_->setItem(i, 2, new QTableWidgetItem(QString::number(entSizes[i])));
  }
}

}  // namespace qt
