/* =============================================================================
 *  floor_plan.h  —  楼层平面图（QGraphicsView / QGraphicsScene 实现）
 *
 *  这张图直接对应 WeightedGraph 里的路网：
 *      · 一行 = 一层楼，房间按路网节点号从小到大排开
 *      · 同一个科室的房间同一种颜色，于是「科室分布」一眼可见
 *      · 查询出的就诊路线用亮色描出来
 *      · 点击房间可以直接把它设成起点（Shift+点击设终点）
 *
 *  为什么用 QGraphicsView 而不是自绘 QWidget：
 *    QGraphicsView 自带缩放、拖拽、坐标系与命中测试（itemAt），
 *    每个房间是一个图元，以后想加「悬停高亮/放大镜/动画」都很容易。
 *    自绘 paintEvent 虽然更快，但这些交互都得自己算。
 *
 *  数据来源全部是 rooms 表与 road 图，坐标由节点号算出来，没有硬编码。
 * ========================================================================== */
#pragma once

#include <QGraphicsView>
#include <QWidget>

#include "qt_state.h"

class QGraphicsScene;

namespace qt {

class FloorPlanView : public QGraphicsView {
  Q_OBJECT

 public:
  explicit FloorPlanView(QWidget* parent = nullptr);

  // 按当前 Hospital 数据与 state().route 重建整张图。
  // 48 个房间加上文字也就一百多个图元，重建一次可以忽略不计，
  // 所以不做增量更新 —— 简单、不会出现「改了数据但图没更新」的问题。
  void rebuild();

  // 房间方块里显示房间号（false 则显示科室名）
  void setShowCode(bool on);
  bool showCode() const { return showCode_; }

 Q_SIGNALS:
  // 用户点了某个房间（下标是 h.rooms 里的下标）
  void roomClicked(int roomIndex, bool shift);

 protected:
  void mousePressEvent(QMouseEvent* event) override;
  // 控件尺寸变化时重新把整张图缩放到可见范围。
  // 必须有这个：rebuild() 可能发生在控件还没拿到实际尺寸的时候（构造期、
  // 或者刚被放进 QSplitter 还没布局），那时 fitInView 算出来的缩放是退化的，
  // 表现为「平面图一片空白」。尺寸定下来之后这里再拟合一次就正常了。
  void resizeEvent(QResizeEvent* event) override;

 private:
  void fitScene();

  QGraphicsScene* scene_ = nullptr;
  bool showCode_ = true;
  bool rebuilding_ = false;
};

// 科室配色图例：一行色块 + 科室名。窄的时候自动换行（Qt 布局天然支持）。
class FloorLegend : public QWidget {
  Q_OBJECT

 public:
  explicit FloorLegend(QWidget* parent = nullptr);

 private:
  void paintEvent(QPaintEvent* event) override;
  int heightForWidth(int w) const override;
  bool hasHeightForWidth() const override { return true; }
};

}  // namespace qt
