/* =============================================================================
 *  panel_doctor.cpp  —  功能 2：医生查询
 *
 *  三种查询条件对应题目要求的「按科室、职称、擅长领域查询」。
 *  实现方式与字符界面完全一样：顺序表遍历 + 字符串比较 / 子串匹配。
 *  图形界面把三种条件做成三个模式按钮，结果用表格展示。
 * ========================================================================== */
#include <cstring>

#include "imgui.h"
#include "gui_common.h"
#include "gui_state.h"
#include "panels.h"

namespace gui {

namespace {

// 职称候选。查询时用「子串匹配」，所以「主任」能同时匹配「主任医师」
const char* kTitles[] = {"主任医师", "副主任医师", "主治医师", "住院医师"};

// 判断某个医生是否满足当前查询条件
bool doctorMatches(const Hospital& h, const Doctor& d, const GuiState& st) {
  if (st.queryMode == 0) {  // 按科室
    if (st.queryDept < 0 || st.queryDept >= h.depts.size()) return false;
    return d.dept == st.queryDept;
  }
  if (st.queryMode == 1) {  // 按职称（子串匹配，「主任」能命中「主任医师」）
    if (st.queryTitle < 0 || st.queryTitle >= 4) return true;  // 0 表示「全部职称」
    return std::strstr(d.title, kTitles[st.queryTitle - 1]) != nullptr;
  }
  // 按擅长领域（子串匹配）
  if (st.queryKeyword[0] == '\0') return true;
  return std::strstr(d.skill, st.queryKeyword) != nullptr;
}

}  // namespace

void drawDoctorQuery() {
  GuiState& st = state();
  Hospital& h = db();

  // 自检参数：requestInt[0] = 查询方式，requestInt[1] = 要选的科室
  if (st.requestInt[0] >= 0) {
    st.queryMode = st.requestInt[0];
    st.requestInt[0] = -1;
  }
  if (st.requestInt[1] >= 0) {
    st.queryDept = st.requestInt[1];
    st.requestInt[1] = -1;
  }

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.72f, 0.95f, 1.0f));
  ImGui::TextUnformatted("功能 2  医生查询");
  ImGui::PopStyleColor();
  ImGui::TextWrapped("按科室 / 职称 / 擅长领域筛选。数据来源：医生顺序表（%d 名）", h.doctors.size());
  ImGui::Separator();

  // ------------------------------ 查询条件 ------------------------------
  const char* kModes[] = {"按科室查询", "按职称查询", "按擅长领域查询"};
  ImGui::TextUnformatted("查询方式：");
  ImGui::SameLine();
  for (int i = 0; i < 3; ++i) {
    if (i) ImGui::SameLine();
    const bool on = (st.queryMode == i);
    if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.50f, 0.72f, 1.0f));
    if (ImGui::Button(kModes[i], ImVec2(140.0f, 0.0f))) {
      st.queryMode = i;
      st.clearNotice();
    }
    if (on) ImGui::PopStyleColor();
  }

  ImGui::Spacing();
  if (st.queryMode == 0) {
    ImGui::TextUnformatted("科室：");
    ImGui::SameLine();
    const char* cur = (st.queryDept >= 0 && st.queryDept < h.depts.size())
                          ? h.depts[st.queryDept].name
                          : "（请选择）";
    ImGui::SetNextItemWidth(comboWidth(cur));
    if (ImGui::BeginCombo("##dept", cur)) {
      for (int i = 0; i < h.depts.size(); ++i) {
        char label[96];
        std::snprintf(label, sizeof(label), "%s  %s", h.depts[i].code, h.depts[i].name);
        if (ImGui::Selectable(label, st.queryDept == i)) st.queryDept = i;
      }
      ImGui::EndCombo();
    }
  } else if (st.queryMode == 1) {
    ImGui::TextUnformatted("职称：");
    ImGui::SameLine();
    const char* cur = (st.queryTitle == 0) ? "全部职称" : kTitles[st.queryTitle - 1];
    ImGui::SetNextItemWidth(comboWidth(cur));
    if (ImGui::BeginCombo("##title", cur)) {
      if (ImGui::Selectable("全部职称", st.queryTitle == 0)) st.queryTitle = 0;
      for (int i = 0; i < 4; ++i)
        if (ImGui::Selectable(kTitles[i], st.queryTitle == i + 1)) st.queryTitle = i + 1;
      ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("（匹配用子串，所以选「主任医师」不会带出「副主任医师」）");
  } else {
    ImGui::TextUnformatted("擅长领域关键字：");
    ImGui::SameLine();
    // 输入框给大约 16 个汉字的宽度，够看清自己敲了什么
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16.0f);
    ImGui::InputText("##kw", st.queryKeyword, sizeof(st.queryKeyword));
    ImGui::SameLine();
    ImGui::TextDisabled("例如：高血压、咳嗽、骨折、白内障");
  }

  // ------------------------------ 结果表 ------------------------------
  ImGui::Spacing();
  ImGui::Separator();

  int found = 0;
  const ImGuiTableFlags tf = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                             ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;
  if (ImGui::BeginTable("doctorResult", 6, tf, ImVec2(0.0f, -28.0f))) {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("编号", ImGuiTableColumnFlags_WidthFixed, 64.0f);
    ImGui::TableSetupColumn("姓名", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("科室", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("职称", ImGuiTableColumnFlags_WidthFixed, 104.0f);
    ImGui::TableSetupColumn("擅长领域", ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableSetupColumn("诊室", ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableHeadersRow();

    for (int i = 0; i < h.doctors.size(); ++i) {
      const Doctor& d = h.doctors[i];
      if (!doctorMatches(h, d, st)) continue;
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(d.code);
      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(d.name);
      ImGui::TableSetColumnIndex(2);
      ImGui::TextUnformatted(deptName(h, d.dept));
      ImGui::TableSetColumnIndex(3);
      ImGui::TextUnformatted(d.title);
      ImGui::TableSetColumnIndex(4);
      ImGui::TextUnformatted(d.skill);
      ImGui::TableSetColumnIndex(5);
      ImGui::TextUnformatted(roomName(h, d.room));
      ++found;
    }
    ImGui::EndTable();
  }

  ImGui::Text("共找到 %d 名医生。", found);
  ImGui::SameLine();
  if (ImGui::SmallButton("把结果带到「在线预约」")) {
    // 找一个满足条件的医生，直接跳到预约页并选中他
    for (int i = 0; i < h.doctors.size(); ++i) {
      if (!doctorMatches(h, h.doctors[i], st)) continue;
      st.bookDoctor = i;
      st.bookDeptFilter = h.doctors[i].dept;
      st.page = PAGE_BOOK;
      st.notify(NOTICE_INFO, "已跳到「在线预约」，当前医生为 %s %s。", h.doctors[i].code,
                h.doctors[i].name);
      break;
    }
  }
}

}  // namespace gui
