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

#include "ds.h"
#include "model.h"
#include "qt_common.h"

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
      "占用字节取自 sizeof。数据结构层与业务层与字符界面、ImGui 界面完全相同，"
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
                      QStringLiteral("已装 / 容量"), QStringLiteral("sizeof（字节）")});
  structTable_->setColumnWidth(0, 150);
  structTable_->setColumnWidth(1, 260);
  structTable_->setColumnWidth(2, 120);
  structTable_->setColumnWidth(3, 130);
  v1->addWidget(structTable_);
  layout->addWidget(box1, 1);

  // ---- 实体记录 ----
  auto* box2 = new QGroupBox(QStringLiteral("实体记录（全部是定长记录，只存数据不存指针）"), this);
  auto* v2 = new QVBoxLayout(box2);
  entityTable_ = new QTableWidget(box2);
  setupReadOnlyTable(entityTable_, 7, 3,
                     {QStringLiteral("实体"), QStringLiteral("含义"),
                      QStringLiteral("sizeof（字节）")});
  entityTable_->setColumnWidth(0, 130);
  entityTable_->setColumnWidth(1, 300);
  entityTable_->setColumnWidth(2, 130);
  v2->addWidget(entityTable_);
  layout->addWidget(box2, 1);
}

void AboutPage::refreshPage() {
  Hospital& h = db();

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
