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

  // 命令行参数：第一个参数当数据目录（便于用另一套数据演示）
  const char* dataDir = (argc > 1) ? argv[1] : "data";

  std::printf("医院门诊预约与智能导诊系统 —— 图形界面版\n");
  std::printf("数据目录：%s\n", dataDir);

  if (!gui::init(L"医院门诊预约与智能导诊系统", 1360, 840)) {
    std::printf("[错误] 图形界面初始化失败，程序退出。\n");
    return 1;
  }
  gui::reloadData(dataDir);
  gui::run();
  gui::shutdown();
  std::printf("程序正常退出。\n");
  return 0;
}
