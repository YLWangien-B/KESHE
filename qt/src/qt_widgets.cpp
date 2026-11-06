/* =============================================================================
 *  qt_widgets.cpp  —  号源格子与排版小工具的实现
 * ========================================================================== */
#include "qt_widgets.h"

#include <QEvent>
#include <QHeaderView>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QTableWidget>
#include <QToolTip>

#include "qt_common.h"

namespace qt {

// =============================================================================
//  SlotGrid
// =============================================================================
SlotGrid::SlotGrid(QWidget* parent) : QWidget(parent) {
  setMouseTracking(true);  // 不按键也能收到 mouseMove，用于悬停高亮
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void SlotGrid::setDoctor(int doctor) {
  doctor_ = doctor;
  hoverDay_ = hoverSlot_ = -1;
  update();
}

void SlotGrid::setSelection(int day, int slot) {
  if (day < 0 || day >= kMaxDay || slot < 0 || slot >= 2) return;
  selDay_ = day;
  selSlot_ = slot;
  update();
}

QSize SlotGrid::sizeHint() const {
  const int w = kLeftLabelW + kMaxDay * (kCellW + kGap) + kGap;
  const int h = kHeaderH + 2 * (kCellH + kGap) + kGap;
  return QSize(w, h);
}

QRect SlotGrid::cellRect(int day, int slot) const {
  const int x = kLeftLabelW + kGap + day * (kCellW + kGap);
  const int y = kHeaderH + kGap + slot * (kCellH + kGap);
  return QRect(x, y, kCellW, kCellH);
}

bool SlotGrid::cellAt(int x, int y, int* day, int* slot) const {
  for (int d = 0; d < kMaxDay; ++d) {
    for (int s = 0; s < 2; ++s) {
      if (cellRect(d, s).contains(x, y)) {
        *day = d;
        *slot = s;
        return true;
      }
    }
  }
  return false;
}

void SlotGrid::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);

  Hospital& h = db();

  // ---- 顶部表头：周一..周日 ----
  QFont bold = font();
  bold.setBold(true);
  p.setFont(bold);
  p.setPen(QColor(0xC8, 0xCE, 0xD6));
  for (int d = 0; d < kMaxDay; ++d) {
    const QRect r(kLeftLabelW + kGap + d * (kCellW + kGap), 4, kCellW, kHeaderH - 6);
    p.drawText(r, Qt::AlignCenter, dayText(d));
  }

  // ---- 左侧行标签：上午 / 下午 ----
  for (int s = 0; s < 2; ++s) {
    const QRect r(0, kHeaderH + kGap + s * (kCellH + kGap), kLeftLabelW - 8, kCellH);
    p.drawText(r, Qt::AlignRight | Qt::AlignVCenter, slotText(s));
  }

  // ---- 格子 ----
  QFont small = font();
  small.setPointSizeF(font().pointSizeF() * 0.92);
  for (int d = 0; d < kMaxDay; ++d) {
    for (int s = 0; s < 2; ++s) {
      const QRect r = cellRect(d, s);
      const SlotState st = slotState(h, doctor_, d, s);

      // 底色：号源状态决定；未选中时压暗一点，让选中的格子跳出来
      QColor fill = slotColor(st);
      if (st == SLOT_NONE || st == SLOT_STOPPED) {
        fill.setAlpha(70);
      } else {
        fill.setAlpha(235);
      }
      p.setBrush(fill);

      // 边框：选中 > 悬停 > 普通
      const bool selected = (d == selDay_ && s == selSlot_);
      const bool hovered = (d == hoverDay_ && s == hoverSlot_);
      QPen pen;
      if (selected) {
        pen = QPen(QColor(0xF0, 0xD0, 0x60), 2.5);
      } else if (hovered) {
        pen = QPen(QColor(0xFF, 0xFF, 0xFF, 0xB0), 1.5);
      } else {
        pen = QPen(QColor(0x2A, 0x2D, 0x31), 1.0);
      }
      p.setPen(pen);
      p.drawRoundedRect(r.adjusted(1, 1, -1, -1), 5, 5);

      // 文字：有排班就「余/总」，没排班就「不出诊」
      p.setFont(small);
      if (st == SLOT_NONE) {
        p.setPen(QColor(0x9A, 0xA0, 0xA6));
        p.drawText(r, Qt::AlignCenter, QStringLiteral("不出诊"));
      } else if (st == SLOT_STOPPED) {
        p.setPen(QColor(0xD8, 0xD8, 0xD8));
        p.drawText(r, Qt::AlignCenter, QStringLiteral("停诊"));
      } else {
        const int left = h.remaining(doctor_, d, s);
        const int quota = h.slot(doctor_, d, s).quota;
        // 选中格用深色字压在浅色底上，其余用白字
        p.setPen(QColor(0xFF, 0xFF, 0xFF));
        p.drawText(r, Qt::AlignCenter, QStringLiteral("%1 / %2").arg(left).arg(quota));
      }
    }
  }
}

void SlotGrid::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) return;
  int day = -1, slot = -1;
  if (!cellAt(event->pos().x(), event->pos().y(), &day, &slot)) return;

  Hospital& h = db();
  const SlotState st = slotState(h, doctor_, day, slot);
  // 不出诊 / 停诊的格子不响应点击 —— 点了也没有可预约的东西
  if (st == SLOT_NONE || st == SLOT_STOPPED) {
    QToolTip::showText(event->globalPosition().toPoint(), slotStateText(h, doctor_, day, slot),
                       this);
    return;
  }
  setSelection(day, slot);
  Q_EMIT slotClicked(day, slot);
}

void SlotGrid::mouseMoveEvent(QMouseEvent* event) {
  int day = -1, slot = -1;
  const bool hit = cellAt(event->pos().x(), event->pos().y(), &day, &slot);
  if (hit) {
    if (day != hoverDay_ || slot != hoverSlot_) {
      hoverDay_ = day;
      hoverSlot_ = slot;
      update();
    }
    Hospital& h = db();
    QString tip = dayText(day) + slotText(slot) + QStringLiteral("：") +
                  slotStateText(h, doctor_, day, slot);
    if (slotState(h, doctor_, day, slot) != SLOT_NONE)
      tip += QStringLiteral("（余 %1，共 %2）")
                 .arg(h.remaining(doctor_, day, slot))
                 .arg(h.slot(doctor_, day, slot).quota);
    QToolTip::showText(event->globalPosition().toPoint(), tip, this);
  } else if (hoverDay_ != -1) {
    hoverDay_ = hoverSlot_ = -1;
    update();
  }
}

void SlotGrid::leaveEvent(QEvent*) {
  hoverDay_ = hoverSlot_ = -1;
  update();
}

// =============================================================================
//  排版小工具
// =============================================================================
QLabel* makePageTitle(const QString& text) {
  auto* label = new QLabel(text);
  label->setStyleSheet(styleTitle());
  return label;
}

QLabel* makeHint(const QString& text) {
  auto* label = new QLabel(text);
  label->setStyleSheet(styleHint());
  label->setWordWrap(true);
  return label;
}

void setupReadOnlyTable(QTableWidget* table, int rows, int cols, const QStringList& headers,
                        bool stretchLast) {
  table->setRowCount(rows);
  table->setColumnCount(cols);
  if (!headers.isEmpty()) table->setHorizontalHeaderLabels(headers);
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);  // 只读
  table->setSelectionBehavior(QAbstractItemView::SelectRows);
  table->setSelectionMode(QAbstractItemView::SingleSelection);
  table->setAlternatingRowColors(true);
  table->verticalHeader()->setVisible(false);  // 不显示行号
  table->horizontalHeader()->setStretchLastSection(stretchLast);
  // 允许拖动列宽调整（默认是 Interactive，这里显式写出来便于理解）
  table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  table->setWordWrap(false);
}

void markRow(QTableWidget* table, int row, bool marked) {
  if (row < 0 || row >= table->rowCount()) return;
  const QColor bg = marked ? QColor(0x5A, 0x46, 0x22) : QColor(Qt::transparent);
  for (int c = 0; c < table->columnCount(); ++c) {
    if (QTableWidgetItem* it = table->item(row, c)) it->setBackground(bg);
  }
}

}  // namespace qt
