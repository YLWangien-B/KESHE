/* =============================================================================
 *  gui_main.cpp  —  图形界面程序的入口与主框架
 *
 *  与字符界面的 main.cpp 是同一个系统的两种「界面层」：
 *      main.cpp      字符界面（tools\run.bat 出来的 hospital.exe）
 *      gui_main.cpp  图形界面（本文件，出来的 hospital_gui.exe）
 *  两者共用同一份数据结构层、数据层与业务层，没有任何代码复制。
 *
 *  命令行：
 *      hospital_gui.exe            用当前目录下的 data\
 *      hospital_gui.exe 数据目录    用指定目录（便于换一套数据演示）
 * ========================================================================== */
#include "gui_app.h"

#include <cstdio>
#include <cstring>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "imgui.h"

#include "gui_state.h"
#include "load.h"
#include "panels.h"

namespace gui {

namespace {

// 如果命令行没给数据目录，先看工作目录下有没有 data\；
// 没有的话再看 exe 自己所在目录（双击运行时工作目录可能不是工程根）。
// 返回 true 表示找到了一个像样的数据目录。
bool resolveDataDir(const char* requested, char* out, int cap) {
  auto hasData = [](const char* dir) {
    char probe[MAX_PATH + 32];
    std::snprintf(probe, sizeof(probe), "%s/symptoms.txt", dir);
    if (std::FILE* f = std::fopen(probe, "rb")) {
      std::fclose(f);
      return true;
    }
    return false;
  };

  if (hasData(requested)) {
    std::snprintf(out, cap, "%s", requested);
    return true;
  }

  wchar_t exePath[MAX_PATH] = {};
  if (::GetModuleFileNameW(nullptr, exePath, MAX_PATH) > 0) {
    wchar_t* slash = ::wcsrchr(exePath, L'\\');
    if (slash) {
      *slash = L'\0';
      // exe 在 build\ 下，所以 data\ 可能在上一层，也可能就在 exe 旁边
      const wchar_t* kCandidates[] = {L"\\data", L"\\..\\data"};
      for (const wchar_t* suffix : kCandidates) {
        wchar_t candidate[MAX_PATH] = {};
        ::wcsncpy_s(candidate, MAX_PATH, exePath, _TRUNCATE);
        ::wcsncat_s(candidate, MAX_PATH, suffix, _TRUNCATE);
        char narrow[MAX_PATH * 2] = {};
        const int conv = ::WideCharToMultiByte(CP_UTF8, 0, candidate, -1, narrow, sizeof(narrow),
                                               nullptr, nullptr);
        std::printf("[GUI] 探测 %ls -> %s (conv=%d, hasData=%d)\\n", candidate, narrow, conv,
                    hasData(narrow) ? 1 : 0);
        if (conv > 0 && hasData(narrow)) {
          std::snprintf(out, cap, "%s", narrow);
          return true;
        }
      }
    }
  }
  std::snprintf(out, cap, "%s", requested);
  return false;
}

}  // namespace

// 把装载结果写成一条提示，并在标题栏带上装载统计
void reloadData(const char* dir) {
  GuiState& st = state();

  // 解析出真正存在的数据目录，免得「双击 exe」时因为工作目录不对而装载失败
  char resolved[512];
  const char* want = (dir && *dir) ? dir : "data";
  const bool found = resolveDataDir(want, resolved, sizeof(resolved));
  std::snprintf(st.dataDir, sizeof(st.dataDir), "%s", found ? resolved : want);
  if (found && std::strcmp(want, resolved) != 0)
    std::printf("[GUI] 工作目录下没找到 %s，改用 %s\n", want, resolved);

  Hospital& h = db();
  st.loadOk = Loader::loadAll(h, st.dataDir);
  st.loadErrors = Loader::errorCount();
  st.loadOverflows = Loader::overflowCount();

  std::printf("\n===== 图形界面 装载数据（目录 %s）=====\n", st.dataDir);
  if (!st.loadOk) {
    std::printf("[错误] 数据装载失败。请先运行：build\\gen_data.exe %s\n", st.dataDir);
    st.notify(NOTICE_ERROR, "数据装载失败：打不开 %s 目录下的数据文件。请先运行 build\\gen_data.exe %s",
              st.dataDir, st.dataDir);
    setTitleSuffix("数据装载失败");
    return;
  }

  std::printf("装载完成：科室 %d 个 / 症状 %d 个（二部图 %d 条关联）/ 医生 %d 名 / 患者 %d 名 / 房间 %d 间\n",
              h.depts.size(), h.symptomGraph.symptomCount(), h.symptomGraph.edgeCount(),
              h.doctors.size(), h.patients.size(), h.rooms.size());

  char suffix[256];
  std::snprintf(suffix, sizeof(suffix),
                "科室 %d / 症状 %d / 医生 %d / 患者 %d / 房间 %d", h.depts.size(),
                h.symptomGraph.symptomCount(), h.doctors.size(), h.patients.size(),
                h.rooms.size());
  setTitleSuffix(suffix);

  if (st.loadOverflows > 0) {
    st.notify(NOTICE_WARN, "有 %d 处数据超出了容量上限，部分记录没有被装入；请调大 src/model.h 里的 kMax* 常量",
              st.loadOverflows);
  } else if (st.loadErrors > 0) {
    st.notify(NOTICE_WARN, "有 %d 行数据未通过校验，已跳过（详见控制台输出）", st.loadErrors);
  } else {
    st.notify(NOTICE_OK, "数据装载完成：科室 %d 个、症状 %d 个、医生 %d 名、患者 %d 名、房间 %d 间",
              h.depts.size(), h.symptomGraph.symptomCount(), h.doctors.size(), h.patients.size(),
              h.rooms.size());
  }
}

// -----------------------------------------------------------------------------
//  功能自检（tools\gui_selftest.bat 用）
//
//  为什么需要它：GUI 没法靠模拟鼠标来测试 —— 合成点击的坐标要经过 DPI 缩放、
//  窗口边框、ImGui 逻辑坐标三重换算，非常脆。而「程序化地点一遍界面」只需要
//  直接设好界面状态、再置一个动作请求，面板会走和真实点击完全相同的 apply() 函数。
//
//  做法：脚本里一行一个动作，每帧执行一个，执行前先等 6 帧让界面稳定，
//  结束时报告一共执行了多少个动作。动作的执行结果由各面板 printf 到控制台。
// -----------------------------------------------------------------------------
namespace {

// 选中一个症状（症状顶点下标）。由自检驱动调用，直接设界面状态。
void selfTestPickSymptom(int symptom) {
  GuiState& st = state();
  const int n = db().symptomGraph.symptomCount();
  if (symptom < 0 || symptom >= n) return;
  if (st.symptomPicked[symptom]) return;
  st.symptomPicked[symptom] = true;
  ++st.symptomCount;
  std::printf("        （已选中症状 #%d：%s，当前共 %d 个）\n", symptom + 1,
              db().symptomGraph.symptomName(symptom), st.symptomCount);
}

// 步骤执行前要额外摆好的界面状态。命名成 PREP_* 而不是塞进 requestInt，
// 是因为这些值要在「上一帧的动作生效之后」才算得出来 ——
// 比如取消预约需要知道刚生成的那张凭证号是多少。
enum SelfTestPrep {
  PREP_NONE = 0,
  PREP_MANAGE_LAST_TICKET,  // 把 managePatient 与 manageCancelTicket 指向刚预约的那条记录
  PREP_ROUTE_BY_CODE,       // 用 roomCodeFrom / roomCodeTo 里的房间号查下标，而不是写死下标
};

struct SelfTestStep {
  Page page;
  int action;      // UiAction，或 -1 表示只切页、不触发动作
  int symptom;     // >= 0 时选中该号症状（症状顶点下标），-1 表示不动
  int int0, int1, int2;  // requestInt 的前三个参数，-1 表示不覆盖
  int prep;        // SelfTestPrep
  const char* what;
};

// 路线用房间号指定，而不是写死房间下标 —— 下标会随数据文件的顺序变化，
// 写错了很难发现（第一次自检就把 RB209 写成了 RB211）。
const char* g_routeFromCode = nullptr;
const char* g_routeToCode = nullptr;

// 每一步都写清楚「在测什么」，这样控制台日志本身就是测试记录。
// 症状下标对着 data\symptoms.txt 的行号：0 发热、1 咳嗽、3 胸闷、5 气短。
const SelfTestStep kSelfTest[] = {
    // ---------- 功能 1 智能导诊 ----------
    {PAGE_TRIAGE, -1, 3, -1, -1, -1, PREP_NONE, "选中症状：胸闷"},
    {PAGE_TRIAGE, -1, 5, -1, -1, -1, PREP_NONE, "选中症状：气短"},
    {PAGE_TRIAGE, -1, 1, -1, -1, -1, PREP_NONE, "选中症状：咳嗽"},
    {PAGE_TRIAGE, ACT_TRIAGE_RUN, -1, -1, -1, -1, PREP_NONE, "点「开始导诊」"},
    // ---------- 功能 2 医生查询 ----------
    {PAGE_DOCTOR, -1, -1, 0, -1, -1, PREP_NONE, "切到按科室查询"},
    {PAGE_DOCTOR, -1, -1, -1, 0, -1, PREP_NONE, "科室选 K01（心血管内科）"},
    // ---------- 功能 3 在线预约 ----------
    {PAGE_BOOK, -1, -1, 0, 0, 0, PREP_NONE, "选患者 P1 / 医生 D1 / 周一上午"},
    {PAGE_BOOK, ACT_BOOK_SUBMIT, -1, -1, -1, -1, PREP_NONE, "点「生成预约凭证」"},
    // ---------- 功能 4 预约管理 ----------
    {PAGE_MANAGE, -1, -1, 0, -1, -1, PREP_MANAGE_LAST_TICKET, "查看 P1 的预约记录"},
    {PAGE_MANAGE, ACT_MANAGE_CANCEL_CONFIRM, -1, -1, -1, -1, PREP_MANAGE_LAST_TICKET, "取消刚生成的预约"},
    // ---------- 功能 5 排班展示 ----------
    {PAGE_SCHEDULE, -1, -1, -1, -1, -1, PREP_NONE, "排班展示（按医生一周号源日历）"},
    // ---------- 功能 6 就诊路线 ----------
    {PAGE_ROUTE, -1, -1, -1, -1, -1, PREP_ROUTE_BY_CODE, "起点 RA101、终点 RB209（按房间号查）"},
    {PAGE_ROUTE, ACT_ROUTE_RUN, -1, -1, -1, -1, PREP_NONE, "点「查询路线」"},
    // ---------- 功能 7 候诊队列 ----------
    {PAGE_QUEUE, -1, -1, 0, 0, -1, PREP_NONE, "选患者 P1 / 医生 D1"},
    {PAGE_QUEUE, ACT_QUEUE_CHECKIN, -1, -1, -1, -1, PREP_NONE, "点「患者签到排队」"},
    {PAGE_QUEUE, ACT_QUEUE_CALL, -1, -1, -1, -1, PREP_NONE, "点「叫下一位」"},
    {PAGE_QUEUE, ACT_QUEUE_PASS, -1, -1, -1, -1, PREP_NONE, "点「过号处理」（第 1 次）"},
    {PAGE_QUEUE, ACT_QUEUE_PASS, -1, -1, -1, -1, PREP_NONE, "点「过号处理」（第 2 次，应重新排队）"},
    {PAGE_QUEUE, ACT_QUEUE_FINISH, -1, -1, -1, -1, PREP_NONE, "点「就诊完成」"},
    // ---------- 附加两页 ----------
    {PAGE_FLOOR, -1, -1, -1, -1, -1, PREP_NONE, "楼层平面图"},
    {PAGE_ABOUT, -1, -1, -1, -1, -1, PREP_NONE, "数据总览"},
};
constexpr int kSelfTestCount = static_cast<int>(sizeof(kSelfTest) / sizeof(kSelfTest[0]));

int g_step = -1;      // -1 表示还没开始
int g_waitFrames = 0;
const char* g_lastTicket = nullptr;

// 自检用到的房间号（在 main 里赋上真实值）
void selfTestSetRoute(const char* from, const char* to) {
  g_routeFromCode = from;
  g_routeToCode = to;
}

// 摆好这一步依赖的界面状态
void selfTestPrep(int prep) {
  GuiState& st = state();
  Hospital& h = db();
  switch (prep) {
    case PREP_MANAGE_LAST_TICKET:
      // 指向患者 P1 名下最新的一条预约（也就是上一步刚生成的那张凭证）
      st.managePatient = 0;
      st.manageCancelTicket[0] = '\0';
      for (int i = h.bookings.size() - 1; i >= 0; --i) {
        if (h.bookings[i].patient != 0) continue;
        if (h.bookings[i].status != BOOKED) continue;
        std::snprintf(st.manageCancelTicket, sizeof(st.manageCancelTicket), "%s",
                      h.bookings[i].ticket);
        g_lastTicket = h.bookings[i].ticket;
        std::printf("        （准备取消的凭证号：%s）\n", h.bookings[i].ticket);
        break;
      }
      break;
    case PREP_ROUTE_BY_CODE: {
      // 用房间号查下标，不写死下标
      const int from = h.findRoom(g_routeFromCode ? g_routeFromCode : "");
      const int to = h.findRoom(g_routeToCode ? g_routeToCode : "");
      if (from >= 0 && to >= 0) {
        st.routeFrom = from;
        st.routeTo = to;
        std::printf("        （%s -> %s，房间下标 %d -> %d）\n", h.rooms[from].code,
                    h.rooms[to].code, from, to);
      } else {
        std::printf("        （房间号查不到：%s / %s）\n", g_routeFromCode ? g_routeFromCode : "-",
                    g_routeToCode ? g_routeToCode : "-");
      }
      break;
    }
    default:
      break;
  }
}

// 跑一步自检：返回 false 表示已经跑完
bool runOneSelfTestStep() {
  GuiState& st = state();
  if (g_step < 0) {
    g_step = 0;
    g_waitFrames = 0;
    std::printf("\n########## 图形界面功能自检：共 %d 步 ##########\n", kSelfTestCount);
  }
  if (g_step >= kSelfTestCount) return false;

  // 每步之间留 6 帧，让上一帧的动作生效、界面稳定下来
  if (g_waitFrames > 0) {
    --g_waitFrames;
    return true;
  }

  const SelfTestStep& s = kSelfTest[g_step];
  if (s.symptom >= 0) selfTestPickSymptom(s.symptom);
  if (s.int0 >= 0) st.requestInt[0] = s.int0;
  if (s.int1 >= 0) st.requestInt[1] = s.int1;
  if (s.int2 >= 0) st.requestInt[2] = s.int2;
  selfTestPrep(s.prep);
  st.requestPage = static_cast<int>(s.page);
  if (s.action >= 0) st.request = s.action;

  std::printf("[自检 %2d/%d] %s\n", g_step + 1, kSelfTestCount, s.what);
  ++g_step;
  g_waitFrames = 6;
  return true;
}

}  // namespace

// 自检模式的开关：由命令行参数 --selftest 打开
bool g_selfTestMode = false;
bool g_selfTestDone = false;

// 自检每帧推进一步；全部走完返回 false
bool runSelfTestFrame() {
  if (g_selfTestDone) return false;
  if (!runOneSelfTestStep()) {
    g_selfTestDone = true;
    std::printf("\n########## 功能自检结束 ##########\n");
    std::printf("界面会停在这里，直接关窗口即可退出。\n");
    return false;
  }
  return true;
}

// ------------------------------ 提示条 ------------------------------
// 这两个函数放在 gui 命名空间里（不是匿名命名空间），因为 renderFrame 在命名空间外，
// 需要通过 gui:: 限定名调用它们。
void drawNotice() {
  GuiState& st = state();
  if (st.noticeKind == NOTICE_NONE || st.notice[0] == '\0') return;

  ImVec4 color;
  const char* tag = "";
  switch (st.noticeKind) {
    case NOTICE_OK:    color = ImVec4(0.42f, 0.78f, 0.48f, 1.0f); tag = "成功"; break;
    case NOTICE_WARN:  color = ImVec4(0.95f, 0.75f, 0.30f, 1.0f); tag = "注意"; break;
    case NOTICE_ERROR: color = ImVec4(0.95f, 0.42f, 0.42f, 1.0f); tag = "错误"; break;
    default:           color = ImVec4(0.62f, 0.72f, 0.85f, 1.0f); tag = "提示"; break;
  }
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  // 用 TextWrapped 而不是 Text：装载统计这类提示比较长，不折行的话右边会被切掉
  ImGui::TextWrapped("[%s] %s", tag, st.notice);
  ImGui::PopStyleColor();
  ImGui::Separator();
}

// ------------------------------ 左侧功能导航 ------------------------------
void drawSidebar() {
  GuiState& st = state();

  // 自检模式要求切页：直接改，不经过鼠标点击
  if (st.requestPage >= 0 && st.requestPage < PAGE_COUNT) {
    st.page = static_cast<Page>(st.requestPage);
    st.requestPage = -1;
  }

  ImGui::BeginChild("sidebar", ImVec2(212.0f, 0.0f), ImGuiChildFlags_Borders);
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.72f, 0.95f, 1.0f));
  ImGui::TextUnformatted("医院门诊预约与");
  ImGui::TextUnformatted("智能导诊系统");
  ImGui::PopStyleColor();
  ImGui::Separator();
  ImGui::Spacing();

  static const char* kLabels[PAGE_COUNT] = {
      "1  智能导诊", "2  医生查询", "3  在线预约", "4  预约管理", "5  排班展示",
      "6  就诊路线", "7  候诊队列", "楼层平面图", "数据总览",
  };
  for (int i = 0; i < PAGE_COUNT; ++i) {
    const bool selected = (static_cast<int>(st.page) == i);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.44f, 0.68f, 1.0f));
    if (ImGui::Button(kLabels[i], ImVec2(-FLT_MIN, 0.0f))) st.page = static_cast<Page>(i);
    if (selected) ImGui::PopStyleColor();
    ImGui::PopStyleVar();
  }

  ImGui::Spacing();
  ImGui::Separator();

  // 装载统计：换数据目录后这里的数字会跟着变
  const Hospital& h = db();
  if (st.loadOk) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.62f, 0.66f, 0.70f, 1.0f));
    ImGui::Text("数据目录：%s", st.dataDir);
    ImGui::Text("科室 %d   症状 %d", h.depts.size(), h.symptomGraph.symptomCount());
    ImGui::Text("医生 %d   患者 %d", h.doctors.size(), h.patients.size());
    ImGui::Text("房间 %d", h.rooms.size());
    ImGui::Text("二部图关联 %d 条", h.symptomGraph.edgeCount());
    ImGui::PopStyleColor();
  } else {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.42f, 0.42f, 1.0f));
    ImGui::TextUnformatted("数据未装载");
    ImGui::PopStyleColor();
  }

  ImGui::EndChild();
}

// -----------------------------------------------------------------------------
//  每帧画一次的全部界面（由 gui_app.cpp 的主循环调用）
// -----------------------------------------------------------------------------
// 三个主程序共用的数据总库实例定义在 db.cpp 里，这里不需要再定义。

}  // namespace gui

// -----------------------------------------------------------------------------
//  每帧画一次的全部界面。
//  注意：这个函数必须定义在 gui 命名空间**之外** —— gui_app.cpp 里是当作全局函数
//  声明的，放进命名空间会变成内部链接，链接时就找不到定义了。
// -----------------------------------------------------------------------------
void renderFrame() {
  gui::GuiState& st = gui::state();

  // 自检模式：每帧推进一步（直接改界面状态 + 置动作请求，
  // 面板会走和真实点击相同的 apply() 函数）
  if (gui::g_selfTestMode) gui::runSelfTestFrame();
  const ImGuiViewport* vp = ImGui::GetMainViewport();

  // 界面尺寸夹在屏幕工作区内。高 DPI 缩放下主视口可能有半个屏幕在窗口外面，
  // 而 ImGui 是按主视口来排版的 —— 不夹的话表格和按钮会排到看不见的地方去。
  float workW = 0.0f, workH = 0.0f;
  gui::getLogicalWorkArea(&workW, &workH);
  const ImVec2 pos = vp->WorkPos;
  const ImVec2 size(workW < vp->WorkSize.x ? workW : vp->WorkSize.x,
                    workH < vp->WorkSize.y ? workH : vp->WorkSize.y);

  ImGui::SetNextWindowPos(pos);
  ImGui::SetNextWindowSize(size);
  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBringToFrontOnFocus;
  ImGui::Begin("##root", nullptr, flags);

  // ------------------------------ 顶部菜单栏 ------------------------------
  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("系统")) {
      if (ImGui::MenuItem("重新装载数据")) gui::reloadData(st.dataDir);
      ImGui::Separator();
      if (ImGui::MenuItem("退出")) ::PostQuitMessage(0);
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("数据目录")) {
      ImGui::TextDisabled("当前：%s", st.dataDir);
      ImGui::Separator();
      if (ImGui::MenuItem("data（默认数据集）", nullptr, std::strcmp(st.dataDir, "data") == 0))
        gui::reloadData("data");
      ImGui::EndMenu();
    }
    ImGui::TextDisabled("   |   数据结构层与业务层与字符界面程序完全相同，界面层换了实现");
    ImGui::EndMenuBar();
  }

  // ------------------------------ 左侧导航 + 右侧内容 ------------------------------
  gui::drawSidebar();
  ImGui::SameLine();

  ImGui::BeginChild("content", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
  gui::drawNotice();

  if (!st.loadOk) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.55f, 0.45f, 1.0f));
    ImGui::TextUnformatted("数据没有装载成功，七项功能都无法使用。");
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::TextWrapped("数据文件放在程序工作目录的 %s\\ 下。请先运行 build\\gen_data.exe %s 生成测试数据，"
                       "或者用命令行指定另一套数据：hospital_gui.exe 数据目录名",
                       st.dataDir, st.dataDir);
  } else {
    switch (st.page) {
      case gui::PAGE_TRIAGE:   gui::drawTriage(); break;
      case gui::PAGE_DOCTOR:   gui::drawDoctorQuery(); break;
      case gui::PAGE_BOOK:     gui::drawBooking(); break;
      case gui::PAGE_MANAGE:   gui::drawManage(); break;
      case gui::PAGE_SCHEDULE: gui::drawSchedule(); break;
      case gui::PAGE_ROUTE:    gui::drawRoute(); break;
      case gui::PAGE_QUEUE:    gui::drawQueue(); break;
      case gui::PAGE_FLOOR:    gui::drawFloorPlan(); break;
      case gui::PAGE_ABOUT:    gui::drawAbout(); break;
      default: break;
    }
  }
  ImGui::EndChild();
  ImGui::End();
}

// -----------------------------------------------------------------------------
//  入口
// -----------------------------------------------------------------------------
int main(int argc, char** argv) {
  // 只保留一个中文字体在控制台能正常输出的设置：把控制台代码页切成 UTF-8。
  // 不要用 system("chcp 65001")：那会另起一个 cmd.exe，把标准输入重定向吃掉。
  ::SetConsoleOutputCP(65001);
  ::SetConsoleCP(65001);
  // 关掉 stdout 缓冲：否则从管道启动时日志会攒在缓冲区里，进程被结束时一个字都看不到
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // 解析参数：--selftest 进入功能自检模式，其余第一个参数当数据目录
  const char* dataDir = "data";
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--selftest") == 0) {
      gui::g_selfTestMode = true;
    } else {
      dataDir = argv[i];
    }
  }

  std::printf("医院门诊预约与智能导诊系统 —— 图形界面版\n");
  std::printf("数据目录：%s\n", dataDir);
  if (gui::g_selfTestMode)
    std::printf("模式：功能自检（自动点一遍七项功能 + 楼层平面图 + 数据总览）\n");

  if (!gui::init(L"医院门诊预约与智能导诊系统", 1360, 840)) {
    std::printf("[错误] 图形界面初始化失败，程序退出。\n");
    return 1;
  }
  gui::reloadData(dataDir);
  // 自检的路线用房间号指定，避免写死下标（下标会随数据文件顺序变化）
  if (gui::g_selfTestMode) gui::selfTestSetRoute("RA101", "RB209");
  gui::run();
  gui::shutdown();
  std::printf("程序正常退出。\n");
  return 0;
}
