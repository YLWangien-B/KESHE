/* =============================================================================
 *  floor_plan.cpp  —  楼层平面图的绘制
 * ========================================================================== */
#include "floor_plan.h"

#include <QFontMetricsF>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QPainter>
#include <QScrollBar>

#include "qt_common.h"

namespace qt {

namespace {

// 图元上挂房间下标用的 key（QGraphicsItem::setData / data）
constexpr int kRoomIndexKey = 0;

constexpr qreal kBoxW = 104.0;
constexpr qreal kBoxH = 50.0;
constexpr qreal kGap = 8.0;
constexpr qreal kLeftPad = 104.0;  // 左侧给「门诊楼1层」留的位置
constexpr qreal kRowGap = 22.0;

// 把文字塞进给定宽度：太宽就按比例缩小字号，保证不越出方框。
// 这是 ImGui 版里踩过的问题（文字溢出方块），这里保留同样的保护。
void addFittedText(QGraphicsScene* scene, QGraphicsItem* parent, const QString& text, qreal x,
                   qreal y, qreal maxW, qreal size, qreal minSize, const QColor& color) {
  auto* item = new QGraphicsSimpleTextItem(text, parent);
  QFont f = item->font();
  f.setPointSizeF(size);
  qreal w = QFontMetricsF(f).horizontalAdvance(text);
  if (w > maxW && w > 0.0) {
    qreal size2 = size * (maxW / w);
    if (size2 < minSize) size2 = minSize;
    f.setPointSizeF(size2);
    item->setFont(f);
    w = QFontMetricsF(f).horizontalAdvance(text);
  }
  item->setBrush(color);
  item->setPos(x + (maxW - w) / 2.0, y);
  scene->addItem(item);  // 忘了这一句图元不会进场景，表现是「方块有、文字没有」
}

}  // namespace

// =============================================================================
//  FloorPlanView
// =============================================================================
FloorPlanView::FloorPlanView(QWidget* parent) : QGraphicsView(parent) {
  scene_ = new QGraphicsScene(this);
  setScene(scene_);
  setRenderHint(QPainter::Antialiasing, true);
  setDragMode(QGraphicsView::ScrollHandDrag);       // 拖动地图
  setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
  setBackgroundBrush(QColor(0x1E, 0x21, 0x24));
  setFrameShape(QFrame::NoFrame);
  // 不显示滚动条会让用户不知道右边还有房间；保留但常驻
  setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

void FloorPlanView::setShowCode(bool on) {
  if (showCode_ == on) return;
  showCode_ = on;
  rebuild();
}

void FloorPlanView::rebuild() {
  rebuilding_ = true;
  scene_->clear();
  Hospital& h = db();
  QtState& st = state();

  // 本次路线上的房间（用于高亮）
  QSet<int> onRoute;
  if (st.routeValid && st.route.found) {
    for (int i = 0; i < st.route.stepCount; ++i) onRoute.insert(st.route.steps[i]);
  }

  for (int floor = 0; floor < kMaxFloor; ++floor) {
    // 取这一层的房间，按路网节点号排序 —— 节点号就是房间在走廊上的位置
    QVector<int> idx;
    for (int i = 0; i < h.rooms.size(); ++i)
      if (h.rooms[i].floor == floor) idx.append(i);
    std::sort(idx.begin(), idx.end(),
              [&h](int a, int b) { return h.rooms[a].node < h.rooms[b].node; });
    if (idx.isEmpty()) continue;

    const qreal y = floor * (kBoxH + kRowGap + 40.0);

    // 楼层名
    auto* floorLabel = new QGraphicsSimpleTextItem(floorText(floor));
    QFont ff = floorLabel->font();
    ff.setPointSizeF(9.5);
    floorLabel->setFont(ff);
    floorLabel->setBrush(QColor(0xC8, 0xCE, 0xD6));
    floorLabel->setPos(4.0, y + kBoxH / 2.0 - 12.0);
    scene_->addItem(floorLabel);

    // 走廊：一条横线，连接本层全部房间
    const qreal corridorY = y + kBoxH + 12.0;
    scene_->addLine(6.0 + kLeftPad, corridorY,
                    kLeftPad + (idx.size() - 1) * (kBoxW + kGap) + kBoxW, corridorY,
                    QPen(QColor(0x5A, 0x5F, 0x66), 2.0));

    for (int k = 0; k < idx.size(); ++k) {
      const int room = idx[k];
      const Room& r = h.rooms[room];
      const qreal x = 6.0 + kLeftPad + k * (kBoxW + kGap);

      const bool isFrom = (st.routeFrom == room);
      const bool isTo = (st.routeTo == room);
      const bool highlighted = isFrom || isTo || onRoute.contains(room);

      // 底色：科室色；路线上的房间改成亮橙
      QColor fill = (r.dept >= 0) ? deptColor(r.dept) : QColor(0x4A, 0x4E, 0x54);
      if (r.dept < 0 && QString::fromUtf8(r.name).contains(QStringLiteral("电梯")))
        fill = QColor(0x70, 0x74, 0x7A);
      if (highlighted) fill = QColor(0xE0, 0xA8, 0x30);

      auto* box = new QGraphicsRectItem(x, y, kBoxW, kBoxH);
      box->setBrush(fill);
      box->setPen(QPen(highlighted ? QColor(0xFF, 0xE0, 0x80) : QColor(0x22, 0x24, 0x28),
                       highlighted ? 2.0 : 1.0));
      box->setData(kRoomIndexKey, room);
      box->setCursor(Qt::PointingHandCursor);
      box->setToolTip(QStringLiteral("%1  %2\n楼层：%3   路网节点号：%4\n所属科室：%5\n"
                                     "直接点击 = 设为起点    Shift+点击 = 设为终点")
                          .arg(QString::fromUtf8(r.code), QString::fromUtf8(r.name),
                               floorText(r.floor))
                          .arg(r.node)
                          .arg(r.dept >= 0 ? deptName(h, r.dept) : QStringLiteral("公共设施")));
      scene_->addItem(box);

      // 第一行：房间号（或科室名）
      const QString line1 = showCode_
                                ? QString::fromUtf8(r.code)
                                : (r.dept >= 0 ? deptName(h, r.dept) : QStringLiteral("公共设施"));
      addFittedText(scene_, nullptr, line1, x, y + 4.0, kBoxW, 8.5, 6.0,
                    QColor(0xF2, 0xF4, 0xF6));

      // 第二行：电梯口/楼梯口，或科室名
      QString sub;
      if (r.dept < 0)
        sub = QString::fromUtf8(r.name).contains(QStringLiteral("电梯")) ? QStringLiteral("电梯口")
                                                                         : QStringLiteral("楼梯口");
      else
        sub = deptName(h, r.dept);
      addFittedText(scene_, nullptr, sub, x, y + kBoxH - 16.0, kBoxW, 7.5, 5.5,
                    QColor(0xE6, 0xE9, 0xEC, 0xD0));

      // 起点 / 终点标记（右上角小色块）
      if (isFrom || isTo) {
        auto* tag = new QGraphicsRectItem(x + kBoxW - 20.0, y - 2.0, 20.0, 16.0);
        tag->setBrush(isFrom ? QColor(0x4E, 0xA8, 0x5B) : QColor(0xC0, 0x50, 0x50));
        tag->setPen(Qt::NoPen);
        scene_->addItem(tag);
        auto* t = new QGraphicsSimpleTextItem(isFrom ? QStringLiteral("起") : QStringLiteral("终"));
        QFont tf = t->font();
        tf.setPointSizeF(8.0);
        t->setFont(tf);
        t->setBrush(Qt::white);
        t->setPos(x + kBoxW - 16.0, y - 2.0);
        scene_->addItem(t);
      }

      // 房间接进走廊的竖线
      scene_->addLine(x + kBoxW / 2.0, y + kBoxH, x + kBoxW / 2.0, corridorY,
                      QPen(QColor(0x5A, 0x5F, 0x66), 1.0));
    }
  }

  // 场景大小：让四层楼都放得下
  scene_->setSceneRect(scene_->itemsBoundingRect().adjusted(-8, -8, 8, 12));
  rebuilding_ = false;
  fitScene();
}

// 把整张图缩放到当前视口。视口尺寸为 0 时什么都不做 —— 等 resizeEvent 再来。
void FloorPlanView::fitScene() {
  if (!scene_) return;
  const QRectF r = scene_->sceneRect();
  if (r.isEmpty() || viewport()->width() <= 0 || viewport()->height() <= 0) return;
  fitInView(r, Qt::KeepAspectRatio);
}

void FloorPlanView::resizeEvent(QResizeEvent* event) {
  QGraphicsView::resizeEvent(event);
  fitScene();
}

void FloorPlanView::mousePressEvent(QMouseEvent* event) {
  if (rebuilding_) return;
  if (event->button() == Qt::LeftButton) {
    const QPointF scenePos = mapToScene(event->pos());
    QGraphicsItem* item = scene_->itemAt(scenePos, transform());
    // 点在文字上时拿到的可能是文字图元，往上找到挂了下标的方块
    while (item && !item->data(kRoomIndexKey).isValid()) item = item->parentItem();
    if (item) {
      const int room = item->data(kRoomIndexKey).toInt();
      Q_EMIT roomClicked(room, event->modifiers() & Qt::ShiftModifier);
      return;  // 命中房间就不要继续走「拖动地图」
    }
  }
  QGraphicsView::mousePressEvent(event);
}

// =============================================================================
//  FloorLegend：科室配色图例
// =============================================================================
FloorLegend::FloorLegend(QWidget* parent) : QWidget(parent) {
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
}

int FloorLegend::heightForWidth(int w) const {
  // 12 个科室 + 3 个公共图例，每个约 92 像素宽，按行数算高度
  Hospital& h = db();
  int perRow = qMax(1, w / 96);
  int items = h.depts.size() + 3;
  int rows = (items + perRow - 1) / perRow;
  return rows * 24 + 4;
}

void FloorLegend::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  Hospital& h = db();

  QFont small = font();
  small.setPointSizeF(font().pointSizeF() * 0.9);
  p.setFont(small);

  int x = 2, y = 4;
  const int rowH = 24;
  auto drawSwatch = [&](const QColor& c, const QString& text) {
    const int textW = QFontMetrics(small).horizontalAdvance(text);
    const int itemW = 14 + 6 + textW + 14;
    if (x + itemW > width() && x > 2) {
      x = 2;
      y += rowH;
    }
    p.setBrush(c);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRect(x, y + 4, 14, 14), 3, 3);
    p.setPen(QColor(0xD0, 0xD4, 0xDA));
    p.drawText(QRect(x + 20, y, textW + 4, rowH), Qt::AlignLeft | Qt::AlignVCenter, text);
    x += itemW;
  };

  for (int d = 0; d < h.depts.size(); ++d) drawSwatch(deptColor(d), deptName(h, d));
  drawSwatch(QColor(0x70, 0x74, 0x7A), QStringLiteral("电梯口"));
  drawSwatch(QColor(0x4A, 0x4E, 0x54), QStringLiteral("楼梯口"));
  drawSwatch(QColor(0xE0, 0xA8, 0x30), QStringLiteral("起点/终点/路线"));
}

}  // namespace qt
