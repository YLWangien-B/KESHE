/* =============================================================================
 *  main.cpp  —  程序入口
 *
 *  一个医院门诊预约与智能导诊系统，分三层：
 *      界面层（main.cpp + ui.h + menu.h）—— 菜单、输入、结果显示
 *      业务层（service.h）               —— 智能导诊、就诊路线
 *      数据层（model.h + load.h）         —— 数据表、路网、文件装载
 *  数据结构全部自行实现，见 ds.h。
 *
 *  构建与运行见 README.md；设计说明见 docs/ 目录。
 * ========================================================================== */
#include <cstdio>
#include <cstdlib>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "ds.h"
#include "load.h"
#include "menu.h"
#include "menu_queue.h"
#include "model.h"
#include "service.h"
#include "service_queue.h"
#include "ui.h"

// =============================================================================
//  数据总库
//
//  总库体积大于默认的 1MB 线程栈，不能作为局部变量，因此用函数内静态对象持有。
//  各功能模块通过 db() 访问同一份数据，不会出现状态不一致。
// =============================================================================
Hospital& db() {
  static Hospital instance;
  return instance;
}

void setupConsole() {
#if defined(_WIN32)
  // 源码与数据文件都是 UTF-8，把控制台代码页也切到 UTF-8，否则中文显示为乱码。
  // 注意不要用 system("chcp 65001")：system 会另起一个 cmd.exe，它会把父进程
  // 继承来的标准输入重定向（如 hospital.exe < in.txt）消费掉，导致随后读不到
  // 任何输入，表现为「菜单一闪而过」。
  SetConsoleOutputCP(65001);
  SetConsoleCP(65001);
#endif
  std::setvbuf(stdout, nullptr, _IONBF, 0);
}

void printMenu() {
  std::printf("\n================ 医院门诊预约与智能导诊系统 ================\n");
  std::printf("  1. 智能导诊     按症状推荐科室\n");
  std::printf("  2. 医生查询     按科室 / 职称 / 擅长领域查询\n");
  std::printf("  3. 在线预约     选医生与时段，生成预约凭证\n");
  std::printf("  4. 预约管理     查看与取消本人的预约\n");
  std::printf("  5. 排班展示     查看医生一周排班与余号\n");
  std::printf("  6. 就诊路线     查询两个诊室之间的走法\n");
  std::printf("  7. 候诊队列     签到排队、叫号、过号、就诊完成\n");
  std::printf("  0. 退出\n");
  std::printf("============================================================\n");
  std::printf("请选择：");
}

int main() {
  setupConsole();
  std::printf("\n正在装载数据...\n");

  Hospital& h = db();
  if (!Loader::loadAll(h, "data")) {
    std::printf("[错误] 数据装载失败。请先运行：build\\gen_data.exe data\n");
    return 1;
  }
  std::printf("装载完成：科室 %d 个 / 症状 %d 个 / 医生 %d 名 / 患者 %d 名 / 房间 %d 间\n", h.depts.size(),
              h.symptoms.size(), h.doctors.size(), h.patients.size(), h.rooms.size());
  if (Loader::errorCount() > 0) std::printf("（有 %d 行数据未通过校验，已跳过）\n", Loader::errorCount());

  for (;;) {
    printMenu();
    char buf[32];
    if (!readLine(buf, sizeof(buf))) break;
    const int choice = std::atoi(buf);

    switch (choice) {
      case 0:
        std::printf("\n感谢使用，再见。\n");
        return 0;
      case 1: doTriage(); break;
      case 2: doQueryDoctor(); break;
      case 3: doBook(); break;
      case 4: doManageBooking(); break;
      case 5: doShowSchedule(); break;
      case 6: doRoute(); break;
      case 7: doQueue(); break;
      default: std::printf("\n请输入 0 到 7 之间的序号。\n"); break;
    }
  }
  return 0;
}
