/* =============================================================================
 *  qt_app.h  —  主窗口
 *
 *  Qt 原生风格的骨架：
 *      菜单栏    文件 / 功能 / 视图 / 帮助
 *      工具栏    重新装载数据、刷新当前页
 *      停靠窗口  左侧「功能导航」、底部「操作日志」（都可拖动/关闭，可从视图菜单恢复）
 *      中心区域  QStackedWidget，八个页面
 *      状态栏    左侧数据规模摘要、右侧当前页名与最新提示
 *
 *  为什么用停靠窗口而不是自己画一个左侧栏：
 *    QDockWidget 自带拖动、浮动、关闭、记忆位置，用户还能把导航栏拖到任意边。
 *    这是 Qt 原生程序的标准做法，也省掉大量布局代码。
 *
 *  页面刷新策略：
 *    Qt 是保留模式，控件不会自己跟着数据变。所以每次切页都调用
 *    PageBase::refreshPage() 从 Hospital 重新读一遍。见 qt_widgets.h 的说明。
 * ========================================================================== */
#pragma once

#include <QMainWindow>
#include <QVector>

#include "qt_state.h"

class QActionGroup;
class QLabel;
class QListWidget;
class QMenu;
class QPlainTextEdit;
class QStackedWidget;

namespace qt {

class PageBase;

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);

  // 切到某一页（菜单、工具栏、导航栏、跨页跳转都走这里）
  void switchPage(int page);
  int currentPage() const;

  // 数据装载完成后调用：填好状态栏摘要、切换并刷新第一页。
  void afterLoad();

 private Q_SLOTS:
  void onNavRowChanged(int row);
  void onReloadData();
  void onRefreshCurrentPage();
  void onNotice(int kind, const QString& msg);
  void onShowAboutDialog();

 private:
  void buildMenus();
  void buildToolBar();
  void buildNavigationDock();
  void buildLogDock();
  void buildStatusBar();
  void buildPages();
  void updateStatusSummary();

  QStackedWidget* stack_ = nullptr;
  QListWidget* nav_ = nullptr;
  QPlainTextEdit* log_ = nullptr;
  QMenu* viewMenu_ = nullptr;      // 「视图」菜单，停靠窗口的显示开关加在这里
  QLabel* statusData_ = nullptr;   // 状态栏左侧：数据规模
  QLabel* statusPage_ = nullptr;   // 状态栏右侧：当前页 + 最新提示
  QVector<PageBase*> pages_;
  QVector<QAction*> pageActions_;  // 「功能」菜单里的八个 action
  bool syncingNav_ = false;        // 防止「代码切页」和「导航栏选中」互相触发
};

}  // namespace qt
