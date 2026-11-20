/* =============================================================================
 *  qt_app.cpp  —  主窗口实现
 *
 *  导航设计（见 qt_app.h 的说明）：
 *    患者端 = 手机 App：底部 5 个标签，组内用二级标签
 *    管理端 = 按实际就诊流程的 5 个分页
 * ========================================================================== */
#include "qt_app.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QDateTime>
#include <QDockWidget>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include "load.h"
#include "qt_common.h"
#include "qt_role.h"
#include "qt_widgets.h"
#include "shared_db.h"

#include "page_about.h"
#include "page_booking.h"
#include "page_doctor.h"
#include "page_home.h"
#include "page_manage.h"
#include "page_queue.h"
#include "page_registry.h"
#include "page_route.h"
#include "page_schedule.h"
#include "page_triage.h"

namespace qt {

namespace {
// 患者端底部标签栏的按钮样式：大、平、选中高亮 —— 照着手机 App 的感觉做
const char* kBottomBarStyle =
    "QPushButton{"
    "  border:none; border-top:2px solid transparent; background:transparent;"
    "  color:#8a9199; font-size:13px; padding:6px 2px;"
    "}"
    "QPushButton:hover{ color:#c9d3de; }"
    "QPushButton:checked{"
    "  color:#6fb3ff; border-top:2px solid #4a9eff; font-weight:bold;"
    "}";
}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle(QStringLiteral("医院门诊预约与智能导诊系统 —— ") +
                 QString::fromUtf8(roleName()));
  resize(isPatient() ? 1100 : 1280, 860);

  stack_ = new QStackedWidget(this);

  buildPages();   // 先建页面（页面构造时会读 db()），并把它们分好组
  buildMenus();
  buildToolBar();
  if (isPatient())
    buildPatientNav();   // 底部标签栏（手机 App 模式）
  else
    buildAdminNav();     // 左侧流程导航
  buildLogDock();
  buildStatusBar();

  // 任何页面调 notifier().xxx() 都会走到这里：写日志 + 更新状态栏
  connect(&notifier(), &Notifier::notified, this, &MainWindow::onNotice);

  if (!groups_.isEmpty()) switchPage(groups_.first().first());
  updateStatusSummary();

  // ---- 定时轮询共享区的写计数：另一端改了数据就刷新当前页 ----
  lastGeneration_ = shared::generation();
  pollTimer_ = new QTimer(this);
  connect(pollTimer_, &QTimer::timeout, this, [this]() {
    shared::beat();  // 顺便报心跳，让另一端知道本进程还活着

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
  pollTimer_->start(700);
}

MainWindow::~MainWindow() {
  // 主动清掉心跳槽：另一端/下一次启动就能立刻看出「本进程是正常退出」而不是崩溃
  shared::leave();
  shared::close();
}

// =============================================================================
//  页面与分组
// =============================================================================
void MainWindow::buildPages() {
  // pages_ 的下标就是 Page 枚举，本角色用不到的页面留 nullptr。
  pages_ = QVector<PageBase*>(PAGE_COUNT, nullptr);
  pages_[PAGE_HOME] = new HomePage(this);
  pages_[PAGE_TRIAGE] = new TriagePage(this);
  pages_[PAGE_DOCTOR] = new DoctorPage(this);
  pages_[PAGE_BOOK] = new BookingPage(this);
  pages_[PAGE_MANAGE] = new ManagePage(this);
  pages_[PAGE_SCHEDULE] = new SchedulePage(this);
  pages_[PAGE_ROUTE] = new RoutePage(this);
  pages_[PAGE_QUEUE] = new QueuePage(this);
  pages_[PAGE_REGISTRY] = new RegistryPage(this);
  pages_[PAGE_ABOUT] = new AboutPage(this);

  groups_.clear();
  groupLabels_.clear();
  groupTabs_.clear();

  // 登记一个分页：组内 1 页就直接进栈，多页则套一层 QTabWidget 做二级标签。
  auto addGroup = [this](const char* label, const QVector<int>& want) {
    QVector<int> keep;
    for (int p : want)
      if (p >= 0 && p < PAGE_COUNT && pageEnabled(p) && pages_[p] != nullptr) keep.append(p);
    if (keep.isEmpty()) return;

    if (keep.size() == 1) {
      stack_->addWidget(pages_[keep.first()]);
      groupTabs_.append(nullptr);
    } else {
      auto* tabs = new QTabWidget(this);
      tabs->setDocumentMode(true);
      for (int p : keep) tabs->addTab(pages_[p], QString::fromUtf8(pageTitle(p)));
      stack_->addWidget(tabs);
      groupTabs_.append(tabs);

      // 用户点二级标签时：刷新该页并记住当前页
      const int gi = groups_.size();
      connect(tabs, &QTabWidget::currentChanged, this, [this, gi](int idx) {
        if (syncingNav_) return;
        if (gi < 0 || gi >= groups_.size() || idx < 0 || idx >= groups_[gi].size()) return;
        const int page = groups_[gi][idx];
        pages_[page]->refreshPage();
        currentPageIndex_ = page;
        if (statusPage_)
          statusPage_->setText(QStringLiteral("当前：") + QString::fromUtf8(pageTitle(page)));
      });
    }
    groups_.append(keep);
    groupLabels_.append(QString::fromUtf8(label));
  };

  if (isPatient()) {
    // 手机 App 的 5 个标签：先看"我的事"，再是工具，最后是"我的"
    addGroup("首页", {PAGE_HOME});
    addGroup("导诊", {PAGE_TRIAGE});
    addGroup("预约", {PAGE_BOOK, PAGE_DOCTOR});
    addGroup("候诊", {PAGE_QUEUE});
    addGroup("我的", {PAGE_MANAGE, PAGE_ROUTE});
  } else {
    // 按实际就诊流程排列：来院 → 叫号 → 排班 → 患者 → 资源 → 系统
    addGroup("今日候诊", {PAGE_QUEUE});
    addGroup("排班管理", {PAGE_SCHEDULE});
    addGroup("患者服务", {PAGE_MANAGE, PAGE_REGISTRY});
    addGroup("资源导航", {PAGE_DOCTOR, PAGE_ROUTE});
    addGroup("系统数据", {PAGE_ABOUT});
  }
  Q_ASSERT(!groups_.isEmpty());
}

// =============================================================================
//  切页
// =============================================================================
void MainWindow::switchPage(int page) {
  if (page < 0 || page >= PAGE_COUNT) return;
  if (!pageEnabled(page) || pages_[page] == nullptr) return;

  int gi = -1, si = 0;
  for (int g = 0; g < groups_.size(); ++g) {
    const int k = groups_[g].indexOf(page);
    if (k >= 0) {
      gi = g;
      si = k;
      break;
    }
  }
  if (gi < 0) return;

  pages_[page]->refreshPage();  // Qt 是保留模式，切页必须重新读数据
  syncingNav_ = true;
  stack_->setCurrentIndex(gi);
  if (groupTabs_[gi] != nullptr) groupTabs_[gi]->setCurrentIndex(si);
  currentPageIndex_ = page;
  syncingNav_ = false;

  highlightNav(gi);

  if (statusPage_)
    statusPage_->setText(QStringLiteral("当前：") + QString::fromUtf8(pageTitle(page)));
}

void MainWindow::highlightNav(int groupIndex) {
  syncingNav_ = true;
  for (int b = 0; b < navButtons_.size(); ++b)
    navButtons_[b]->setChecked(b == groupIndex);
  if (nav_) nav_->setCurrentRow(groupIndex);
  syncingNav_ = false;
  if (groupIndex >= 0 && groupIndex < groupActions_.size() && groupActions_[groupIndex])
    groupActions_[groupIndex]->setChecked(true);
}

int MainWindow::currentPage() const {
  if (currentPageIndex_ >= 0) return currentPageIndex_;
  if (groups_.isEmpty() || groups_.first().isEmpty()) return PAGE_HOME;
  return groups_.first().first();
}

void MainWindow::afterLoad() {
  updateStatusSummary();
  // 患者端落在首页（一眼看清"下次什么时候来"），管理端落在今日候诊
  if (isPatient())
    switchPage(PAGE_HOME);
  else
    switchPage(PAGE_QUEUE);
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

  // ---- 导航：列出本角色的分页，Ctrl+1..n 直达 ----
  QMenu* pageMenu = menuBar()->addMenu(QStringLiteral("导航(&N)"));
  auto* group = new QActionGroup(this);
  group->setExclusive(true);
  groupActions_.clear();
  for (int k = 0; k < groupLabels_.size(); ++k) {
    QAction* act = pageMenu->addAction(groupLabels_[k]);
    act->setCheckable(true);
    if (k < 9) act->setShortcut(QKeySequence(QStringLiteral("Ctrl+%1").arg(k + 1)));
    group->addAction(act);
    groupActions_.append(act);
    const int gi = k;
    connect(act, &QAction::triggered, this, [this, gi]() { onNavGroup(gi); });
  }
  if (!groupActions_.isEmpty()) groupActions_.first()->setChecked(true);

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
  bar->setObjectName(QStringLiteral("mainToolBar"));
  bar->setMovable(false);
  bar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

  QAction* reload = bar->addAction(QStringLiteral("重新装载数据"));
  connect(reload, &QAction::triggered, this, &MainWindow::onReloadData);

  QAction* refresh = bar->addAction(QStringLiteral("刷新当前页"));
  refresh->setShortcut(QKeySequence(QStringLiteral("F5")));
  connect(refresh, &QAction::triggered, this, &MainWindow::onRefreshCurrentPage);
}

// =============================================================================
//  患者端：底部标签栏（手机 App 模式）
// =============================================================================
void MainWindow::buildPatientNav() {
  bottomBar_ = new QWidget(this);
  bottomBar_->setObjectName(QStringLiteral("bottomBar"));
  bottomBar_->setStyleSheet(QStringLiteral(
      "#bottomBar{background:#1b2027;border-top:1px solid #2c333c;}"));

  auto* row = new QHBoxLayout(bottomBar_);
  row->setContentsMargins(4, 2, 4, 2);
  row->setSpacing(0);

  navButtons_.clear();
  for (int k = 0; k < groupLabels_.size(); ++k) {
    auto* b = new QPushButton(groupLabels_[k], bottomBar_);
    b->setCheckable(true);
    b->setAutoExclusive(true);
    b->setMinimumHeight(56);
    b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    b->setStyleSheet(QString::fromLatin1(kBottomBarStyle));
    b->setCursor(Qt::PointingHandCursor);
    row->addWidget(b);
    navButtons_.append(b);
    const int gi = k;
    connect(b, &QPushButton::clicked, this, [this, gi]() { onNavGroup(gi); });
  }

  // 把页面栈和底部栏组合成中心控件
  auto* central = new QWidget(this);
  auto* v = new QVBoxLayout(central);
  v->setContentsMargins(0, 0, 0, 0);
  v->setSpacing(0);
  v->addWidget(stack_, 1);
  v->addWidget(bottomBar_, 0);
  setCentralWidget(central);
}

// =============================================================================
//  管理端：左侧流程导航
// =============================================================================
void MainWindow::buildAdminNav() {
  navDock_ = new QDockWidget(QStringLiteral("工作台"), this);
  navDock_->setObjectName(QStringLiteral("navDock"));
  navDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

  nav_ = new QListWidget(navDock_);
  for (int k = 0; k < groupLabels_.size(); ++k) nav_->addItem(groupLabels_[k]);
  nav_->setCurrentRow(0);
  nav_->setMinimumWidth(170);
  nav_->setStyleSheet(QStringLiteral(
      "QListWidget{border:none;background:#1b2027;color:#c9d3de;font-size:14px;}"
      "QListWidget::item{padding:12px 14px;border-left:3px solid transparent;}"
      "QListWidget::item:selected{background:#232b35;color:#6fb3ff;"
      "border-left:3px solid #4a9eff;}"));

  connect(nav_, &QListWidget::currentRowChanged, this, [this](int row) {
    if (syncingNav_) return;
    onNavGroup(row);
  });

  navDock_->setWidget(nav_);
  addDockWidget(Qt::LeftDockWidgetArea, navDock_);
  viewMenu_->addAction(navDock_->toggleViewAction());

  setCentralWidget(stack_);
}

// =============================================================================
//  操作日志
// =============================================================================
void MainWindow::buildLogDock() {
  auto* dock = new QDockWidget(QStringLiteral("操作日志"), this);
  dock->setObjectName(QStringLiteral("logDock"));
  dock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea |
                        Qt::RightDockWidgetArea);

  log_ = new QPlainTextEdit(dock);
  log_->setReadOnly(true);
  log_->setMaximumBlockCount(500);
  log_->setMinimumHeight(110);

  dock->setWidget(log_);
  addDockWidget(Qt::BottomDockWidgetArea, dock);
  viewMenu_->addAction(dock->toggleViewAction());
  // 患者端做成手机 App 的样子：日志面板默认收起，需要时从「视图」打开。
  // 管理端保持常显 —— 排障和审计都靠它。
  if (isPatient()) dock->hide();
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
void MainWindow::onNavGroup(int groupIndex) {
  if (groupIndex < 0 || groupIndex >= groups_.size()) return;
  if (groups_[groupIndex].isEmpty()) return;
  switchPage(groups_[groupIndex].first());
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

  updateStatusSummary();
  notifier().ok(QStringLiteral("数据已重新装载（未通过校验 %1 行，容量溢出 %2 处）")
                    .arg(Loader::errorCount())
                    .arg(Loader::overflowCount()));
  switchPage(currentPage());
}

void MainWindow::onRefreshCurrentPage() {
  const int page = currentPage();
  if (page >= 0 && page < pages_.size() && pages_[page] != nullptr) pages_[page]->refreshPage();
  notifier().info(QStringLiteral("已刷新「%1」")
                      .arg(QString::fromUtf8(pageTitle(page))));
}

void MainWindow::onNotice(int kind, const QString& msg) {
  QString tag;
  switch (kind) {
    case NOTICE_OK: tag = QStringLiteral("[成功]"); break;
    case NOTICE_WARN: tag = QStringLiteral("[提示]"); break;
    case NOTICE_ERROR: tag = QStringLiteral("[失败]"); break;
    default: tag = QStringLiteral("[信息]"); break;
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
    QString mode;
    if (shared::isOpen()) {
      const int peers = shared::alivePeers();
      if (peers > 0) {
        uint32_t pid = 0, role = 0;
        shared::peerInfo(0, &pid, &role);
        mode = QStringLiteral("[共享 · 对端 %1 个：%2 · 写计数 %3] ")
                   .arg(peers)
                   .arg(QString::fromUtf8(shared::roleText(role)))
                   .arg(shared::generation());
      } else if (shared::hasStalePeer()) {
        mode = QStringLiteral("[共享 · 对端异常退出 · 写计数 %1] ")
                   .arg(shared::generation());
      } else {
        mode = QStringLiteral("[共享 · 暂无对端 · 写计数 %1] ").arg(shared::generation());
      }
    } else {
      mode = QStringLiteral("[单进程] ");
    }
    if (state().loggedIn && state().patient >= 0 && state().patient < db().patients.size()) {
      mode = QStringLiteral("[%1] ").arg(patientLabel(db(), state().patient)) + mode;
    }
    statusData_->setText(
        mode + QStringLiteral("科室 %1 · 症状 %2（关联 %3） · 医生 %4 · 患者 %5 · 房间 %6")
                   .arg(h.depts.size())
                   .arg(h.symptomGraph.symptomCount())
                   .arg(h.symptomGraph.edgeCount())
                   .arg(h.doctors.size())
                   .arg(h.patients.size())
                   .arg(h.rooms.size()));
  }
}

}  // namespace qt
