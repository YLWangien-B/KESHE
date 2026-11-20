/* =============================================================================
 *  page_route.cpp  —  就诊路线页（含楼层平面图）
 * ========================================================================== */
#include "page_route.h"

#include <QCheckBox>
#include <QComboBox>
#include <QInputDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

#include "floor_plan.h"
#include "qt_common.h"
#include "qt_role.h"
#include "service.h"

namespace qt {

RoutePage::RoutePage(QWidget* parent) : PageBase(parent) {
  auto* layout = new QVBoxLayout(this);

  layout->addWidget(makePageTitle(QStringLiteral("就诊路线")));
  layout->addWidget(makeHint(QStringLiteral(
      "把每个房间当图的一个顶点、能走过去就画一条边、边权是距离；"
      "装载时用 Floyd 一次算出任意两点最短路，所以查询是查表 + 还原路径。"
      "左边的明细和右边的平面图是同一份数据：查询出的路线会直接描在图上。")));

  // ---- 查询行 ----
  auto* row = new QHBoxLayout();
  row->addWidget(new QLabel(QStringLiteral("起点："), this));
  fromCombo_ = new QComboBox(this);
  fromCombo_->setMinimumWidth(240);
  row->addWidget(fromCombo_);

  row->addWidget(new QLabel(QStringLiteral("终点："), this));
  toCombo_ = new QComboBox(this);
  toCombo_->setMinimumWidth(240);
  row->addWidget(toCombo_);

  auto* queryBtn = new QPushButton(QStringLiteral("查询路线"), this);
  queryBtn->setDefault(true);
  row->addWidget(queryBtn);
  auto* swapBtn = new QPushButton(QStringLiteral("交换起终点"), this);
  row->addWidget(swapBtn);

  // 患者端专用：从自己的预约里选一条，终点自动填成那位医生的诊室。
  // 省得患者再去 48 个房间里翻"我该去哪个诊室"。
  if (isPatient()) {
    auto* importBtn = new QPushButton(QStringLiteral("导入目的地"), this);
    importBtn->setToolTip(
        QStringLiteral("从「我的预约」里选一条，自动把该医生的诊室设为终点并算出路线"));
    row->addWidget(importBtn);
    connect(importBtn, &QPushButton::clicked, this, &RoutePage::onImportDestination);
  }
  row->addStretch(1);

  showCodeCheck_ = new QCheckBox(QStringLiteral("方块显示房间号"), this);
  showCodeCheck_->setChecked(true);
  row->addWidget(showCodeCheck_);
  layout->addLayout(row);

  // ---- 明细 + 平面图（可拖动分隔）----
  auto* splitter = new QSplitter(Qt::Horizontal, this);

  // 左：路线明细
  auto* left = new QWidget(splitter);
  auto* leftLayout = new QVBoxLayout(left);
  leftLayout->setContentsMargins(0, 0, 0, 0);
  summary_ = new QLabel(left);
  summary_->setWordWrap(true);
  summary_->setStyleSheet(QStringLiteral("font-weight:bold;color:#6ec87e;"));
  leftLayout->addWidget(summary_);

  stepTable_ = new QTableWidget(left);
  setupReadOnlyTable(stepTable_, 0, 5,
                     {QStringLiteral(""), QStringLiteral("节点"), QStringLiteral("房间"),
                      QStringLiteral("楼层"), QStringLiteral("累计距离")});
  stepTable_->setColumnWidth(0, 50);
  stepTable_->setColumnWidth(1, 70);
  stepTable_->setColumnWidth(2, 240);
  stepTable_->setColumnWidth(3, 110);
  stepTable_->setColumnWidth(4, 110);
  leftLayout->addWidget(stepTable_, 1);

  floorGuide_ = new QLabel(left);
  floorGuide_->setWordWrap(true);
  floorGuide_->setStyleSheet(styleHint());
  leftLayout->addWidget(floorGuide_);

  splitter->addWidget(left);

  // 右：平面图 + 图例
  auto* right = new QWidget(splitter);
  auto* rightLayout = new QVBoxLayout(right);
  rightLayout->setContentsMargins(0, 0, 0, 0);
  plan_ = new FloorPlanView(right);
  plan_->setMinimumHeight(300);
  rightLayout->addWidget(plan_, 1);
  legend_ = new FloorLegend(right);
  rightLayout->addWidget(legend_, 0);

  auto* tip = makeHint(QStringLiteral("直接点击房间 = 设为起点    Shift+点击 = 设为终点"));
  rightLayout->addWidget(tip);
  splitter->addWidget(right);
  splitter->setStretchFactor(0, 4);
  splitter->setStretchFactor(1, 6);

  layout->addWidget(splitter, 1);

  connect(queryBtn, &QPushButton::clicked, this, &RoutePage::onQuery);
  connect(swapBtn, &QPushButton::clicked, this, &RoutePage::onSwap);
  connect(fromCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &RoutePage::onFromChanged);
  connect(toCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &RoutePage::onToChanged);
  connect(plan_, &FloorPlanView::roomClicked, this, &RoutePage::onRoomClicked);
  connect(showCodeCheck_, &QCheckBox::toggled, this, &RoutePage::onShowCodeToggled);
}

void RoutePage::refreshPage() {
  QtState& st = state();

  fillRoomCombos();

  // 恢复状态里的起终点
  // 下拉框按楼层分组排列，所以「第几项」!= 「房间在 h.rooms 里的下标」。
  // 一律通过 itemData 换算，别用 currentIndex()。
  auto setComboByRoom = [](QComboBox* c, int room) {
    for (int k = 0; k < c->count(); ++k)
      if (c->itemData(k).toInt() == room) {
        c->setCurrentIndex(k);
        return;
      }
    if (c->count() > 0) c->setCurrentIndex(0);
  };

  syncing_ = true;
  setComboByRoom(fromCombo_, st.routeFrom);
  setComboByRoom(toCombo_, st.routeTo);
  if (fromCombo_->count() > 1 && fromCombo_->currentData().toInt() == toCombo_->currentData().toInt()) {
    // 起终点相同：把终点挪到列表最后一项（下标换算后仍然不同）
    toCombo_->setCurrentIndex(toCombo_->count() - 1);
  }
  syncing_ = false;

  st.routeFrom = fromCombo_->currentData().toInt();
  st.routeTo = toCombo_->currentData().toInt();

  fillRouteTable();
  syncPlanToState();
}

// 起终点下拉框：按楼层分组列出全部房间
void RoutePage::fillRoomCombos() {
  Hospital& h = db();
  const int keepFrom = fromCombo_->currentData().toInt();
  const int keepTo = toCombo_->currentData().toInt();

  QSignalBlocker b1(fromCombo_);
  QSignalBlocker b2(toCombo_);
  fromCombo_->clear();
  toCombo_->clear();
  for (int floor = 0; floor < kMaxFloor; ++floor) {
    for (int i = 0; i < h.rooms.size(); ++i) {
      if (h.rooms[i].floor != floor) continue;
      const QString text = QStringLiteral("%1 · %2 %3").arg(floorText(floor),
                                                            QString::fromUtf8(h.rooms[i].code),
                                                            QString::fromUtf8(h.rooms[i].name));
      fromCombo_->addItem(text, i);
      toCombo_->addItem(text, i);
    }
  }

  auto restore = [](QComboBox* c, int want) {
    if (c->count() == 0) return;
    int target = 0;
    for (int k = 0; k < c->count(); ++k)
      if (c->itemData(k).toInt() == want) target = k;
    c->setCurrentIndex(target);
  };
  restore(fromCombo_, keepFrom);
  restore(toCombo_, keepTo);
}

void RoutePage::onFromChanged(int) {
  if (syncing_) return;
  state().routeFrom = fromCombo_->currentData().toInt();
  state().routeValid = false;  // 起终点变了，旧结果作废
  fillRouteTable();
  syncPlanToState();
}

void RoutePage::onToChanged(int) {
  if (syncing_) return;
  state().routeTo = toCombo_->currentData().toInt();
  state().routeValid = false;
  fillRouteTable();
  syncPlanToState();
}

void RoutePage::onSwap() {
  QtState& st = state();
  const int a = fromCombo_->currentIndex();
  fromCombo_->setCurrentIndex(toCombo_->currentIndex());
  toCombo_->setCurrentIndex(a);
  st.routeFrom = fromCombo_->currentData().toInt();
  st.routeTo = toCombo_->currentData().toInt();
  st.routeValid = false;
  fillRouteTable();
  syncPlanToState();
}

// 一键导入就诊目的地：列出当前患者「已预约」的记录，选中后把该医生诊室设为终点
void RoutePage::onImportDestination() {
  Hospital& h = db();
  const int patient = state().patient;
  if (patient < 0 || patient >= h.patients.size()) {
    notifier().warn(QStringLiteral("请先登录"));
    return;
  }

  QVector<int> cand;
  for (int i = 0; i < h.bookings.size(); ++i) {
    const Booking& b = h.bookings[i];
    if (b.patient != patient) continue;
    if (b.status != BOOKED) continue;  // 只看还没就诊的
    if (b.doctor < 0 || b.doctor >= h.doctors.size()) continue;
    if (h.doctors[b.doctor].room < 0) continue;  // 该医生没安排诊室，导不了
    cand.append(i);
  }
  if (cand.isEmpty()) {
    notifier().warn(QStringLiteral("你还没有「已预约」的记录（或预约的医生未安排诊室）"));
    return;
  }

  int pick = cand.first();
  if (cand.size() > 1) {
    QStringList items;
    for (int idx : cand) {
      const Booking& b = h.bookings[idx];
      const Doctor& d = h.doctors[b.doctor];
      items << QStringLiteral("%1 · %2 %3 · %4%5")
                   .arg(QString::fromUtf8(b.ticket), deptName(h, d.dept),
                        QString::fromUtf8(d.name), dayText(b.day), slotText(b.slot));
    }
    bool ok = false;
    const QString chosen = QInputDialog::getItem(
        this, QStringLiteral("导入目的地"),
        QStringLiteral("选一条预约，把它的诊室设为终点："), items, 0, false, &ok);
    if (!ok) return;
    const int pos = items.indexOf(chosen);
    if (pos < 0) return;
    pick = cand[pos];
  }

  const Booking& b = h.bookings[pick];
  const Doctor& d = h.doctors[b.doctor];
  const int room = d.room;

  state().routeTo = room;
  state().routeValid = false;
  // 下拉框按楼层分组，要用 itemData 找回位置，不能拿房间下标当下拉位置
  for (int k = 0; k < toCombo_->count(); ++k) {
    if (toCombo_->itemData(k).toInt() == room) {
      toCombo_->setCurrentIndex(k);
      break;
    }
  }
  applyRouteQuery();  // 立刻把路线算出来并画在图上
  notifier().ok(QStringLiteral("已把 %1 医生的诊室（%2）设为目的地")
                    .arg(QString::fromUtf8(d.name), roomName(h, room)));
}

void RoutePage::onQuery() { applyRouteQuery(); }

void RoutePage::applyRouteQuery() {
  Hospital& h = db();
  QtState& st = state();
  st.routeFrom = fromCombo_->currentData().toInt();
  st.routeTo = toCombo_->currentData().toInt();

  if (st.routeFrom == st.routeTo) {
    notifier().warn(QStringLiteral("起点与终点是同一个房间，无需移动"));
    st.routeValid = false;
    fillRouteTable();
    return;
  }

  st.route = Route::between(h, st.routeFrom, st.routeTo);
  st.routeValid = true;
  if (!st.route.found) {
    notifier().error(QStringLiteral("两点之间不连通（路网里没有把它们连起来的通道）"));
    fillRouteTable();
    syncPlanToState();
    return;
  }

  notifier().ok(QStringLiteral("路线查询：%1 → %2，全程 %3 米，途经 %4 个节点，换层 %5 次")
                    .arg(QString::fromUtf8(h.rooms[st.routeFrom].code),
                         QString::fromUtf8(h.rooms[st.routeTo].code))
                    .arg(st.route.totalDistance)
                    .arg(st.route.stepCount)
                    .arg(st.route.floorChanges));
  fillRouteTable();
  syncPlanToState();
}

void RoutePage::fillRouteTable() {
  Hospital& h = db();
  QtState& st = state();

  if (!st.routeValid) {
    summary_->setText(QStringLiteral("选好起点和终点后点「查询路线」，或直接点右侧平面图上的房间。"));
    stepTable_->setRowCount(0);
    floorGuide_->setText(QString());
    return;
  }
  if (!st.route.found) {
    summary_->setText(QStringLiteral("两点之间不连通。"));
    stepTable_->setRowCount(0);
    floorGuide_->setText(QString());
    return;
  }

  summary_->setText(QStringLiteral("全程约 %1 米，途经 %2 个节点，换层 %3 次")
                        .arg(st.route.totalDistance)
                        .arg(st.route.stepCount)
                        .arg(st.route.floorChanges));

  const int startNode = h.rooms[st.route.steps[0]].node;
  stepTable_->setRowCount(st.route.stepCount);
  for (int i = 0; i < st.route.stepCount; ++i) {
    const Room& r = h.rooms[st.route.steps[i]];
    const bool isEnd = (i == 0 || i == st.route.stepCount - 1);
    const int acc = h.road.distance(startNode, r.node);  // Floyd 已算好，O(1) 查表

    const QStringList cells = {
        isEnd ? (i == 0 ? QStringLiteral("起") : QStringLiteral("终")) : QStringLiteral("·"),
        QString::number(r.node),
        QString::fromUtf8(r.name),
        floorText(r.floor),
        QStringLiteral("%1 米").arg(acc),
    };
    for (int c = 0; c < cells.size(); ++c) {
      auto* item = new QTableWidgetItem(cells[c]);
      if (c == 0 || c == 1 || c == 3 || c == 4) item->setTextAlignment(Qt::AlignCenter);
      if (isEnd) item->setForeground(QColor(0xF0, 0xD0, 0x60));
      stepTable_->setItem(i, c, item);
    }
  }
  stepTable_->resizeRowsToContents();
  stepTable_->resizeColumnsToContents();
  stepTable_->horizontalHeader()->setStretchLastSection(true);

  // 换层指引
  if (st.route.floorChanges == 0) {
    floorGuide_->setText(QStringLiteral("换层指引：全程在同一层，无需上下楼。"));
    return;
  }
  QStringList lines;
  for (int i = 1; i < st.route.stepCount; ++i) {
    const Room& prev = h.rooms[st.route.steps[i - 1]];
    const Room& cur = h.rooms[st.route.steps[i]];
    if (prev.floor == cur.floor) continue;
    const bool byLift = QString::fromUtf8(prev.name).contains(QStringLiteral("电梯")) ||
                        QString::fromUtf8(cur.name).contains(QStringLiteral("电梯"));
    lines << QStringLiteral("在「%1」%2，到 %3 层的「%4」")
                 .arg(QString::fromUtf8(prev.name),
                      byLift ? QStringLiteral("乘电梯") : QStringLiteral("走楼梯"),
                      QString::number(cur.floor + 1), QString::fromUtf8(cur.name));
  }
  floorGuide_->setText(QStringLiteral("<b>换层指引：</b><br>") + lines.join(QStringLiteral("<br>")));
}

void RoutePage::syncPlanToState() {
  QtState& st = state();
  st.floorShowCode = showCodeCheck_->isChecked();
  plan_->setShowCode(st.floorShowCode);
  plan_->rebuild();
}

void RoutePage::onRoomClicked(int roomIndex, bool shift) {
  Hospital& h = db();
  QtState& st = state();
  if (roomIndex < 0 || roomIndex >= h.rooms.size()) return;

  syncing_ = true;
  if (shift) {
    st.routeTo = roomIndex;
    for (int k = 0; k < toCombo_->count(); ++k)
      if (toCombo_->itemData(k).toInt() == roomIndex) toCombo_->setCurrentIndex(k);
    notifier().info(QStringLiteral("终点已设为 %1（%2）").arg(QString::fromUtf8(h.rooms[roomIndex].code),
                                                             QString::fromUtf8(h.rooms[roomIndex].name)));
  } else {
    st.routeFrom = roomIndex;
    for (int k = 0; k < fromCombo_->count(); ++k)
      if (fromCombo_->itemData(k).toInt() == roomIndex) fromCombo_->setCurrentIndex(k);
    notifier().info(QStringLiteral("起点已设为 %1（%2）").arg(QString::fromUtf8(h.rooms[roomIndex].code),
                                                             QString::fromUtf8(h.rooms[roomIndex].name)));
  }
  st.routeValid = false;  // 起终点变了，旧结果作废
  syncing_ = false;

  fillRouteTable();
  plan_->rebuild();
}

void RoutePage::onShowCodeToggled(bool on) {
  state().floorShowCode = on;
  plan_->setShowCode(on);
}

}  // namespace qt
