/* =============================================================================
 *  qt_app.cpp  —  主窗口实现
 * ========================================================================== */
#include "qt_app.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QDateTime>
#include <QDockWidget>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>

#include "load.h"
#include "qt_common.h"
#include "qt_role.h"
#include "shared_db.h"
#include "qt_widgets.h"

#include "page_about.h"
#include "page_booking.h"
#include "page_doctor.h"
#include "page_manage.h"
#include "page_queue.h"
#include "page_route.h"
#include "page_schedule.h"
#include "page_triage.h"

namespace qt {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle(QStringLiteral("医院门诊预约与智能导诊系统 —— ") +
                 QString::fromUtf8(roleName()));
  resize(1200, 820);

  stack_ = new QStackedWidget(this);
  setCentralWidget(stack_);

  buildPages();   // 先建页面（页面构造时会读 db()）
  buildMenus();
  buildToolBar();
  buildNavigationDock();
  buildLogDock();
  buildStatusBar();

  // 任何页面调 notifier().xxx() 都会走到这里：写日志 + 更新状态栏
  connect(&notifier(), &Notifier::notified, this, &MainWindow::onNotice);

  // 本角色可用的第一页
  if (!pageOrder_.isEmpty()) switchPage(pageOrder_.first());
  updateStatusSummary();

  // ---- 定时轮询共享区的写计数：另一端改了数据就刷新当前页 ----
  // Qt 是保留模式，控件不会自己跟着别人改的数据变；共享模式下必须主动查。
  lastGeneration_ = shared::generation();
  pollTimer_ = new QTimer(this);
  connect(pollTimer_, &QTimer::timeout, this, [this]() {
    const uint32_t g = shared::generation();
    if (g == lastGeneration_) return;
    lastGeneration_ = g;
    const int page = currentPage();
    if (page >= 0 && pages_.size() > page && pages_[page] != nullptr) {
      pages_[page]->refreshPage();
    }
    updateStatusSummary();
    notifier().info(QStringLiteral("检测到另一端修改了数据，已刷新当前页"));
  });
  // 单进程模式下 generation 恒为 0，定时器不做任何事，没有额外开销。
  pollTimer_->start(700);
}

// =============================================================================
//  页面
// =============================================================================
void MainWindow::buildPages() {
  // pages_ 的下标就是 Page 枚举，本角色用不到的页面留 nullptr。
  pages_ = QVector<PageBase*>(PAGE_COUNT, nullptr);
  pages_[PAGE_TRIAGE] = new TriagePage(this);
  pages_[PAGE_DOCTOR] = new DoctorPage(this);
  pages_[PAGE_BOOK] = new BookingPage(this);
  pages_[PAGE_MANAGE] = new ManagePage(this);
  pages_[PAGE_SCHEDULE] = new SchedulePage(this);
  pages_[PAGE_ROUTE] = new RoutePage(this);
  pages_[PAGE_QUEUE] = new QueuePage(this);
  pages_[PAGE_ABOUT] = new AboutPage(this);

  // 只有本角色能用的页面才进页面栈；pageOrder_ 的顺序就是导航栏与页面栈的顺序。
  pageOrder_.clear();
  for (int p = 0; p < PAGE_COUNT; ++p) {
    if (!pageEnabled(p)) continue;
    PageBase* pg = pages_[p];
    if (pg == nullptr) continue;
    stack_->addWidget(pg);
    pageOrder_.append(p);
  }
  Q_ASSERT(!pageOrder_.isEmpty());
}

void MainWindow::switchPage(int page) {
  if (page < 0 || page >= PAGE_COUNT) return;
  if (!pageEnabled(page) || pages_[page] == nullptr) return;  // 本角色没有这一页

  const int idx = pageOrder_.indexOf(page);
  if (idx < 0) return;

  // 切页时刷新一次：Qt 是保留模式，不刷新就会显示旧数据
  pages_[page]->refreshPage();
  stack_->setCurrentIndex(idx);

  // 菜单勾选状态跟着走
  if (idx < pageActions_.size() && pageActions_[idx]) pageActions_[idx]->setChecked(true);

  // 导航栏选中项跟着走（注意不要反过来再触发一次 switchPage）
  syncingNav_ = true;
  if (nav_) nav_->setCurrentRow(idx);
  syncingNav_ = false;

  if (statusPage_) {
    statusPage_->setText(QStringLiteral("当前：") + QString::fromUtf8(pageTitle(page)));
  }
}

int MainWindow::currentPage() const {
  if (!stack_) return pageOrder_.isEmpty() ? PAGE_TRIAGE : pageOrder_.first();
  const int idx = stack_->currentIndex();
  if (idx < 0 || idx >= pageOrder_.size()) return pageOrder_.isEmpty() ? PAGE_TRIAGE : pageOrder_.first();
  return pageOrder_[idx];
}

void MainWindow::afterLoad() {
  updateStatusSummary();
  switchPage(PAGE_TRIAGE);
}

// =============================================================================
//  菜单栏
// =============================================================================
void MainWindow::buildMenus() {
  // ---- 文件 ----
  QMenu* fileMenu = menuBar()->addMenu(QStringLiteral("文件(&F)"));

  QAction* reload = fileMenu->addAction(QStringLiteral("重新装载数据(&R)"));
  reload->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
  connect(reload, &QAction::triggered, this, &MainWindow::onReloadData);

  fileMenu->addSeparator();
  QAction* quit = fileMenu->addAction(QStringLiteral("退出(&Q)"));
  quit->setShortcut(QKeySequence::Quit);
  connect(quit, &QAction::triggered, this, &QWidget::close);

  // ---- 功能：只列本角色可用的页面，Ctrl+1..n 直达 ----
  QMenu* pageMenu = menuBar()->addMenu(QStringLiteral("功能(&N)"));
  auto* group = new QActionGroup(this);
  group->setExclusive(true);
  pageActions_.clear();
  for (int k = 0; k < pageOrder_.size(); ++k) {
    const int page = pageOrder_[k];
    QAction* act = pageMenu->addAction(QString::fromUtf8(pageTitle(page)));
    act->setCheckable(true);
    if (k < 9) act->setShortcut(QKeySequence(QStringLiteral("Ctrl+%1").arg(k + 1)));
    group->addAction(act);
    pageActions_.append(act);
    connect(act, &QAction::triggered, this, [this, page]() { switchPage(page); });
  }
  if (!pageActions_.isEmpty()) pageActions_.first()->setChecked(true);

  // ---- 视图：停靠窗口的显示开关由 Qt 自动加在菜单末尾 ----
  viewMenu_ = menuBar()->addMenu(QStringLiteral("视图(&V)"));

  // ---- 帮助 ----
  QMenu* helpMenu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
  QAction* about = helpMenu->addAction(QStringLiteral("关于本程序(&A)"));
  connect(about, &QAction::triggered, this, &MainWindow::onShowAboutDialog);
}

// =============================================================================
//  工具栏
// =============================================================================
void MainWindow::buildToolBar() {
  QToolBar* bar = addToolBar(QStringLiteral("常用"));
  bar->setObjectName(QStringLiteral("mainToolBar"));  // saveState 需要
  bar->setMovable(false);
  bar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

  QAction* reload = bar->addAction(QStringLiteral("重新装载数据"));
  connect(reload, &QAction::triggered, this, &MainWindow::onReloadData);

  QAction* refresh = bar->addAction(QStringLiteral("刷新当前页"));
  refresh->setShortcut(QKeySequence(QStringLiteral("F5")));
  connect(refresh, &QAction::triggered, this, &MainWindow::onRefreshCurrentPage);
}

// =============================================================================
//  停靠窗口
// =============================================================================
void MainWindow::buildNavigationDock() {
  auto* dock = new QDockWidget(QStringLiteral("功能导航"), this);
  dock->setObjectName(QStringLiteral("navDock"));
  dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

  nav_ = new QListWidget(dock);
  // 只列本角色可用的页面，顺序与 pageOrder_ 一致（行号就是页面栈下标）
  for (int k = 0; k < pageOrder_.size(); ++k)
    nav_->addItem(QString::fromUtf8(pageTitle(pageOrder_[k])));
  nav_->setCurrentRow(0);
  nav_->setMinimumWidth(150);

  connect(nav_, &QListWidget::currentRowChanged, this, &MainWindow::onNavRowChanged);

  dock->setWidget(nav_);
  addDockWidget(Qt::LeftDockWidgetArea, dock);
  viewMenu_->addAction(dock->toggleViewAction());
}

void MainWindow::buildLogDock() {
  auto* dock = new QDockWidget(QStringLiteral("操作日志"), this);
  dock->setObjectName(QStringLiteral("logDock"));
  dock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea |
                        Qt::RightDockWidgetArea);

  log_ = new QPlainTextEdit(dock);
  log_->setReadOnly(true);
  log_->setMaximumBlockCount(500);  // 最多留 500 行，防止长时间运行占内存
  log_->setMinimumHeight(110);

  dock->setWidget(log_);
  addDockWidget(Qt::BottomDockWidgetArea, dock);
  viewMenu_->addAction(dock->toggleViewAction());
}

void MainWindow::buildStatusBar() {
  statusData_ = new QLabel(this);
  statusPage_ = new QLabel(this);
  statusBar()->addWidget(statusData_);
  statusBar()->addPermanentWidget(statusPage_);
}

// =============================================================================
//  槽
// =============================================================================
void MainWindow::onNavRowChanged(int row) {
  if (syncingNav_) return;  // 是代码改的选中项，不要反过来再切一次页
  if (row < 0 || row >= pageOrder_.size()) return;
  switchPage(pageOrder_[row]);
}

void MainWindow::onReloadData() {
  const auto ret = QMessageBox::question(
      this, QStringLiteral("重新装载数据"),
      QStringLiteral("重新从数据文件装载会丢弃当前所有改动（预约、候诊队列、排班变更）。\n"
                     "确定继续吗？"),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (ret != QMessageBox::Yes) return;

  Hospital& h = db();
  if (!Loader::loadAll(h, state().dataDir)) {
    QMessageBox::critical(this, QStringLiteral("装载失败"),
                          QStringLiteral("数据装载失败，请检查数据目录：%1")
                              .arg(QString::fromUtf8(state().dataDir)));
    notifier().error(QStringLiteral("数据装载失败"));
    return;
  }

  // 数据变了，所有页面的下拉框内容都可能过期 -> 全部标脏，切到哪页刷新哪页
  updateStatusSummary();
  notifier().ok(QStringLiteral("数据已重新装载（未通过校验 %1 行，容量溢出 %2 处）")
                    .arg(Loader::errorCount())
                    .arg(Loader::overflowCount()));
  switchPage(currentPage());
}

void MainWindow::onRefreshCurrentPage() {
  pages_[currentPage()]->refreshPage();
  notifier().info(QStringLiteral("已刷新「%1」")
                      .arg(QString::fromUtf8(pageTitle(currentPage()))));
}

void MainWindow::onNotice(int kind, const QString& msg) {
  // 时间戳 + 类型标签
  QString tag;
  switch (kind) {
    case NOTICE_OK:
      tag = QStringLiteral("[成功]");
      break;
    case NOTICE_WARN:
      tag = QStringLiteral("[提示]");
      break;
    case NOTICE_ERROR:
      tag = QStringLiteral("[失败]");
      break;
    default:
      tag = QStringLiteral("[信息]");
      break;
  }
  const QString line = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")) +
                       QLatin1Char(' ') + tag + QLatin1Char(' ') + msg;
  if (log_) {
    log_->appendPlainText(line);
    log_->verticalScrollBar()->setValue(log_->verticalScrollBar()->maximum());
  }
  if (statusPage_) {
    statusPage_->setText(QStringLiteral("当前：") +
                         QString::fromUtf8(pageTitle(currentPage())) + QStringLiteral("    ") +
                         msg);
  }
}

void MainWindow::onShowAboutDialog() {
  QMessageBox::about(
      this, QStringLiteral("关于"),
      QStringLiteral("<b>医院门诊预约与智能导诊系统</b><br><br>"
                     "课程设计 —— 第 1 题<br>"
                     "图形界面：Qt %1（Qt Widgets）<br>"
                     "数据结构与业务算法：全部自行实现，见 src/<br><br>"
                     "六种数据结构：顺序表、环形队列、散列表、二维数组、带权图、带权二部图<br>"
                     "一个排序算法：插入排序")
          .arg(QString::fromLatin1(qVersion())));
}

// =============================================================================
//  状态栏摘要
// =============================================================================
void MainWindow::updateStatusSummary() {
  Hospital& h = db();
  if (statusData_) {
    statusData_->setText(QStringLiteral("科室 %1 · 症状 %2（关联 %3） · 医生 %4 · 患者 %5 · 房间 %6")
                             .arg(h.depts.size())
                             .arg(h.symptomGraph.symptomCount())
                             .arg(h.symptomGraph.edgeCount())
                             .arg(h.doctors.size())
                             .arg(h.patients.size())
                             .arg(h.rooms.size()));
  }
}

}  // namespace qt
