/* =============================================================================
 *  qt_widgets.h  —  界面层公共控件
 *
 *  目前有两个：
 *    PageBase  所有页面的基类，规定「切到这一页时怎么刷新」
 *    SlotGrid  号源格子：某位医生一周（7 天 × 2 时段）的号源状态，
 *              按余量上色、可点击选中。预约页与排班页都用它。
 *
 *  ---------------------------------------------------------------------------
 *  为什么需要 PageBase::refreshPage()
 *
 *  Qt 是「保留模式」：控件建好之后会一直待在那儿，不会自己跟着数据变。
 *  如果预约页取消了一条预约，数据变了，但表格还显示着旧内容。
 *
 *  解决办法：主窗口在「切换到某页」时调用它的 refreshPage()。
 *  代价是切换页面时刷新一次，而不是实时同步；对这个系统完全够用，
 *  而且实现简单、不会有「忘了更新某处」的隐蔽 bug。
 *  ---------------------------------------------------------------------------
 * ========================================================================== */
#pragma once

#include <QWidget>

#include "qt_state.h"

class QLabel;
class QTableWidget;

namespace qt {

// ------------------------------ 页面基类 ------------------------------
class PageBase : public QWidget {
  Q_OBJECT

 public:
  explicit PageBase(QWidget* parent = nullptr) : QWidget(parent) {}

  // 页面被切到前台时调用：从 Hospital 重新读取数据、刷新控件。
  // 这是保留模式与立即模式最大的不同，见文件头说明。
  virtual void refreshPage() = 0;
};

// ------------------------------ 号源格子 ------------------------------
// 画成一张表：行 = 时段（上午/下午），列 = 周一..周日。
// 每格显示「余号/总数」，颜色表示状态，点击选中一个格子。
// 鼠标悬停时高亮，并在 tooltip 里给出完整说明。
class SlotGrid : public QWidget {
  Q_OBJECT

 public:
  explicit SlotGrid(QWidget* parent = nullptr);

  // 指定要显示哪位医生（-1 表示未选择，整张表画灰）
  void setDoctor(int doctor);
  int doctor() const { return doctor_; }

  // 当前选中的格子
  void setSelection(int day, int slot);
  int selectedDay() const { return selDay_; }
  int selectedSlot() const { return selSlot_; }

  QSize sizeHint() const override;

 Q_SIGNALS:
  // 用户点了某个格子（只发「可以约」或「已满」的格子；不出诊的格子点了没意义）
  void slotClicked(int day, int slot);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent* event) override;

 private:
  QRect cellRect(int day, int slot) const;
  // 鼠标位置落在哪个格子；不落在格子上返回 false
  bool cellAt(int x, int y, int* day, int* slot) const;

  int doctor_ = -1;
  int selDay_ = 0;
  int selSlot_ = 0;
  int hoverDay_ = -1;
  int hoverSlot_ = -1;

  // 布局常量（逻辑像素；Qt 6 默认已按 DPI 缩放，不需要自己换算）
  static constexpr int kLeftLabelW = 64;   // 左侧「上午/下午」标签列宽
  static constexpr int kHeaderH = 34;      // 顶部「周一..周日」表头高
  static constexpr int kCellW = 96;
  static constexpr int kCellH = 58;
  static constexpr int kGap = 6;
};

// ------------------------------ 排版小工具 ------------------------------
// 页面顶部的标题行：粗体大字的页面名 + 一行灰色说明
QLabel* makePageTitle(const QString& text);
QLabel* makeHint(const QString& text);

// 只读表格的标准配置（整行选中、不显示行号、按内容自适应列宽）
void setupReadOnlyTable(QTableWidget* table, int rows, int cols, const QStringList& headers);

// 给表格某一行设置「本行是当前选中/待确认」的高亮底色
void markRow(QTableWidget* table, int row, bool marked);

}  // namespace qt
