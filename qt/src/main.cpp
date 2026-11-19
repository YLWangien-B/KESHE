/* =============================================================================
 *  main.cpp  —  Qt 图形界面的入口
 *
 *  做四件事：
 *      1) 建 QApplication
 *      2) 找到数据目录（允许多种相对位置，见 resolveDataDir）
 *      3) 装载数据（复用 src/load.h，与字符界面完全同一套）
 *      4) 建主窗口并进入事件循环
 *
 *  子系统保持 Console：装载阶段的数据校验信息、以及运行期的诊断输出
 *  仍然打在控制台上 —— 图形界面不该把诊断信息藏起来。
 *
 *  切换界面层没有改动业务层一行：Triage / Route / Queue 三个算法类、
 *  以及 ds.h 里的六种数据结构，全部原样复用。这是分层的价值。
 * ========================================================================== */
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QString>
#include <cstdio>

#include "load.h"
#include "shared_db.h"
#include "model.h"
#include "qt_app.h"
#include "login_dialog.h"
#include "qt_role.h"
#include "qt_state.h"

namespace {

// 判断某个目录里有没有数据文件（用作「这个目录对不对」的判据）
bool hasData(const QString& dir) {
  return QFile::exists(dir + QStringLiteral("/symptoms.txt")) &&
         QFile::exists(dir + QStringLiteral("/depts.txt"));
}

// 找数据目录。
//
// 程序会被从不同位置启动（双击 exe、命令行、从别的目录调），
// 所以不能只认当前工作目录。依次尝试：
//     <当前目录>/data        ← tools\run_qt.bat 会把工作目录切到工程根
//     <exe 所在目录>/data
//     <exe 所在目录>/../data ← exe 在 build\ 里，数据在工程根
//     <当前目录>/<参数>      ← 允许「用另一套数据」：hospital_qt.exe mydata
//
// 注意返回的是**候选串本身**（通常是相对路径 "data"），不是绝对路径。
// 原因见下面 writeDataDir 的注释：Loader 用 fopen()，而 fopen 走的是系统
// ANSI 代码页，喂给它 UTF-8 编码的中文绝对路径会打不开文件。
QString resolveDataDir(const QString& want) {
  const QString exeDir = QCoreApplication::applicationDirPath();
  const QStringList candidates = {
      want,
      QStringLiteral("data"),
      QStringLiteral("../data"),
      exeDir + QStringLiteral("/data"),
      exeDir + QStringLiteral("/../data"),
      exeDir + QLatin1Char('/') + want,
      exeDir + QStringLiteral("/../") + want,
  };
  for (const QString& c : candidates) {
    if (hasData(c)) return c;
  }
  return want;  // 都没找到就原样返回，让 Loader 打印明确的错误
}

// 把数据目录写进 state().dataDir（char[] 缓冲区）。
//
// 为什么要 toLocal8Bit 而不是 toUtf8：
//   数据装载用的是 src/load.h 里的 fopen()。MSVC 的 fopen 走 Windows ANSI API，
//   路径按**系统代码页（简体中文是 GBK/936）**解释。如果这里给 UTF-8 字节，
//   中文路径就会「系统找不到指定的路径」。所以这里必须转成系统代码页。
//   （这是本项目在中文 Windows 上踩过的真实坑：命令行版一直没事，因为它传的
//     是 ASCII 的 "data"；换成绝对路径以后才暴露。）
void writeDataDir(const QString& dir) {
  const QByteArray local = dir.toLocal8Bit();
  std::snprintf(qt::state().dataDir, sizeof(qt::state().dataDir), "%s", local.constData());
}

}  // namespace

int main(int argc, char** argv) {
  // 关闭 stdout 缓冲：装载阶段的诊断信息要立刻出现在控制台上，
  // 而不是等进程退出才刷出来（被 kill 时会丢）。与字符界面版保持一致。
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  QApplication app(argc, argv);
  QApplication::setApplicationName(QStringLiteral("hospital_qt"));
  QApplication::setApplicationDisplayName(QStringLiteral("医院门诊预约与智能导诊系统"));
  // Fusion 风格：在 Windows 上也能得到稳定一致的外观，不受系统主题影响
  QApplication::setStyle(QStringLiteral("Fusion"));

  // ---- 数据目录 ----
  // 参数约定：
  //     hospital_qt.exe                 用 data\
  //     hospital_qt.exe mydata          用 mydata\
  //     hospital_qt.exe --shots         逐页截图（开发/文档用）
  bool shootMode = false;
  bool sharedMode = false;
  QString want = QStringLiteral("data");
  for (int i = 1; i < argc; ++i) {
    const QString arg = QString::fromLocal8Bit(argv[i]);
    if (arg == QStringLiteral("--shots"))
      shootMode = true;
    else if (arg == QStringLiteral("--shared"))
      sharedMode = true;  // 与另一个程序共用同一份数据（见 src/shared_db.h）
    else if (!arg.startsWith(QLatin1Char('-')))
      want = arg;
  }
  const QString resolved = resolveDataDir(want);
  writeDataDir(resolved);
  const QString absForDisplay = QDir(resolved).absolutePath();

  std::printf("\n===== 医院门诊预约与智能导诊系统（Qt 界面）=====\n");
  std::printf("数据目录：%s\n", qt::state().dataDir);

  // ---- 装载数据（与另外两个界面共用同一份 Loader）----
  bool dataReady = false;
  if (sharedMode) {
    // 共享模式：数据在共享内存里（data\state.bin），由第一个启动的程序装载
    std::printf("正在连接共享数据区...\n");
    if (shared::open(qt::state().dataDir)) {
      dataReady = true;
      // 登记心跳：另一端能从共享区看到"谁在跑"，崩溃后也能看出来
      shared::join(qt::isAdmin() ? shared::ROLE_ADMIN : shared::ROLE_PATIENT);
    } else {
      // 健壮性：共享区用不了不该让程序罢工。
      // 退回单进程模式，功能照常，只是看不到另一个程序的改动。
      std::printf("[警告] 共享数据区连接失败，退回单进程模式。\n");
      sharedMode = false;
      QMessageBox::warning(nullptr, QStringLiteral("共享模式不可用"),
                           QStringLiteral("无法连接共享数据区，已退回单进程模式。\n\n"
                                          "本程序仍可正常使用，但看不到另一个程序的改动。\n"
                                          "数据目录：%1")
                               .arg(absForDisplay));
    }
  }
  if (!dataReady && !Loader::loadAll(db(), qt::state().dataDir)) {
    const QString msg = QStringLiteral("数据装载失败。\n\n数据目录：%1\n\n"
                                       "请先运行 tools\\build.bat gen 生成测试数据，"
                                       "或用参数指定正确的数据目录。")
                            .arg(absForDisplay);
    std::printf("[错误] 数据装载失败（目录 %s）\n", qt::state().dataDir);
    QMessageBox::critical(nullptr, QStringLiteral("装载失败"), msg);
    return 1;
  }

  // 注意：这个引用必须在 shared::open() 之后取 —— db() 返回的是「当前模式下的
  // 那份数据」，提前取会拿到本地实例，于是同一个程序里出现两份数据。
  Hospital& h = db();

  std::printf("装载完成：科室 %d 个 / 症状 %d 个（二部图 %d 条关联）/ 医生 %d 名 / "
              "患者 %d 名 / 房间 %d 间\n",
              h.depts.size(), h.symptomGraph.symptomCount(), h.symptomGraph.edgeCount(),
              h.doctors.size(), h.patients.size(), h.rooms.size());
  if (Loader::overflowCount() > 0) {
    std::printf("[警告] 有 %d 处数据超出容量上限，部分记录没有装入。\n",
                Loader::overflowCount());
  } else if (Loader::errorCount() > 0) {
    std::printf("（有 %d 行数据未通过校验，已跳过）\n", Loader::errorCount());
  }

  // ---- 患者端：先用姓名登录（注册由医院在管理端完成，客户端不提供注册入口）----
  if (!qt::isAdmin()) {
    if (shootMode) {
      // 截图模式：先把登录框渲染成一张图（说明书要用），再自动登录
      const QString shotDir = QCoreApplication::applicationDirPath() + QStringLiteral("/manual_shots");
      QDir().mkpath(shotDir);
      {
        qt::LoginDialog dlg(nullptr);
        dlg.show();
        QApplication::processEvents();
        QApplication::processEvents();
        const bool ok = dlg.grab().save(shotDir + QStringLiteral("/login.png"));
        std::printf("[shot] 登录框 -> %s\n", ok ? "ok" : "FAILED");
        dlg.close();
      }
      qt::state().patient = 0;
      qt::state().loggedIn = true;
    } else if (!qt::LoginDialog::runLogin(nullptr)) {
      std::printf("[登录] 用户取消登录，退出。\n");
      return 0;
    }
    if (!shootMode)
      std::printf("[登录] 患者下标 %d 登录成功\n", qt::state().patient);
  }

  // ---- 主窗口 ----
  qt::MainWindow window;
  window.show();
  window.afterLoad();  // 状态栏摘要 + 切到第一页

  // ---- 逐页截图模式（开发与文档用）----
  //   hospital_qt.exe --shots
  // 把八个页面各渲染成一张 PNG 存到 build\qt_shots\。
  // 为什么需要它：GUI 的自动化验证很容易被「窗口抢不到焦点」卡住，
  // 用 QWidget::grab() 直接渲染就绕开了这个问题，而且产物可以直接放文档里。
  if (shootMode) {
    const QString outDir = QCoreApplication::applicationDirPath() + QStringLiteral("/manual_shots");
    QDir().mkpath(outDir);
    int shot = 0;
    for (int page = 0; page < qt::PAGE_COUNT; ++page) {
      if (!qt::pageEnabled(page)) continue;  // 本角色没有这一页
      window.switchPage(page);
      QApplication::processEvents();
      QApplication::processEvents();
      const QString path = outDir + QStringLiteral("/%1_%2.png")
                               .arg(QString::fromUtf8(qt::roleName()))
                               .arg(++shot);
      const bool ok = window.grab().save(path);
      std::printf("[shot] %s %s -> %s\n", qt::roleName(), qt::pageTitle(page),
                  ok ? "ok" : "FAILED");
    }
    std::printf("[shot] 全部存到 %s\n", outDir.toLocal8Bit().constData());
    return 0;
  }

  // 把装载阶段的校验结果也写进操作日志（进去就能看到，不用回头看控制台）
  if (Loader::errorCount() > 0 || Loader::overflowCount() > 0) {
    qt::notifier().warn(QStringLiteral("装载时有 %1 行未通过校验、%2 处容量溢出，详见控制台")
                            .arg(Loader::errorCount())
                            .arg(Loader::overflowCount()));
  } else {
    qt::notifier().ok(QStringLiteral("数据装载完成：科室 %1 / 症状 %2 / 医生 %3 / 患者 %4 / 房间 %5")
                          .arg(h.depts.size())
                          .arg(h.symptomGraph.symptomCount())
                          .arg(h.doctors.size())
                          .arg(h.patients.size())
                          .arg(h.rooms.size()));
  }

  return app.exec();
}
