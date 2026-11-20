/* =============================================================================
 *  qt_app.h  —  主窗口
 *
 *  导航结构（两端不同）：
 *
 *    患者端 —— 手机 App 模式：底部标签栏，5 个分页
 *        首页 / 导诊 / 预约 / 候诊 / 我的
 *      「预约」与「我的」里各自还有二级标签（在线预约+查医生、我的预约+就诊路线）
 *
 *    管理端 —— 按实际就诊流程分页，不再按课程要求的功能编号：
 *        今日候诊 / 排班管理 / 患者服务 / 资源导航 / 系统数据
 *
 *  页面对象本身仍然是一个 PageBase 一个实例，导航只负责把它们分组装进
 *  QStackedWidget（组内多于 1 页时用 QTabWidget 做二级标签）。
 *
 *  页面刷新策略：
 *    Qt 是保留模式，控件不会自己跟着数据变。所以每次切页都调用
 *    PageBase::refreshPage() 从 Hospital 重新读一遍。见 qt_widgets.h 的说明。
 * ========================================================================== */
#pragma once

#include <QMainWindow>
#include <QString>
#include <QVector>

#include "qt_state.h"

class QAction;
class QDockWidget;
class QLabel;
class QListWidget;
class QMenu;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QTabWidget;
class QTimer;
class QWidget;

namespace qt {

class PageBase;

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;  // 退出时清掉自己的心跳槽

  // 切到某一页（菜单、工具栏、导航栏、跨页跳转都走这里）
  void switchPage(int page);
  int currentPage() const;

  // 数据装载完成后调用：填好状态栏摘要、切换并刷新首页。
  void afterLoad();

 private Q_SLOTS:
  void onNavGroup(int groupIndex);
  void onReloadData();
  void onRefreshCurrentPage();
  void onNotice(int kind, const QString& msg);
  void onShowAboutDialog();

 private:
  void buildMenus();
  void buildToolBar();
  void buildPages();        // 建页面实例 + 按角色分组 + 填 QStackedWidget
  void buildPatientNav();   // 患者端：底部标签栏
  void buildAdminNav();     // 管理端：左侧流程导航
  void buildLogDock();
  void buildStatusBar();
  void updateStatusSummary();
  void highlightNav(int groupIndex);

  QStackedWidget* stack_ = nullptr;
  QListWidget* nav_ = nullptr;      // 管理端左侧导航
  QDockWidget* navDock_ = nullptr;
  QWidget* bottomBar_ = nullptr;    // 患者端底部标签栏
  QPlainTextEdit* log_ = nullptr;
  QMenu* viewMenu_ = nullptr;       // 「视图」菜单，停靠窗口的显示开关加在这里
  QLabel* statusData_ = nullptr;    // 状态栏左侧：数据规模
  QLabel* statusPage_ = nullptr;    // 状态栏右侧：当前页 + 最新提示

  // pages_ 的下标就是 Page 枚举；本角色不可用的页面留 nullptr。
  QVector<PageBase*> pages_;

  // 导航分组：每组是一个分页，组内可以有多页（用二级标签切换）。
  QVector<QVector<int>> groups_;     // 每组包含哪些 PAGE_*
  QVector<QString> groupLabels_;     // 分页名字（按业务流程命名）
  QVector<QTabWidget*> groupTabs_;   // 与 groups_ 平行；单页组为 nullptr
  QVector<QPushButton*> navButtons_; // 患者端底部按钮

  QVector<QAction*> groupActions_;   // 「导航」菜单里的 action
  int currentPageIndex_ = -1;        // 当前显示的页面（PAGE_*）
  bool syncingNav_ = false;          // 防止「代码切页」和「导航选中」互相触发
  uint32_t lastGeneration_ = 0;      // 上次看到的数据写计数（用于感知另一端改了数据）
  QTimer* pollTimer_ = nullptr;      // 定时轮询共享区的写计数
};

}  // namespace qt
