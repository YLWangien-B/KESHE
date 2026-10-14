/* =============================================================================
 *  panel_triage.cpp  —  功能 1：智能导诊
 *
 *  界面流程：患者在症状表里点选症状 -> 调 Triage::run() -> 按得分列出推荐科室。
 *  业务算法在 service.h 里，这里只负责「把选中的序号收集起来」和「把结果画出来」。
 *
 *  与字符界面的区别：字符界面让患者敲「4 2 6」这样的序号，
 *  图形界面直接做成可点选的按钮，看到的字就是症状名，不用记序号。
 * ========================================================================== */
#include <cstdio>
#include <cstring>

#include "imgui.h"
#include "gui_common.h"
#include "gui_state.h"
#include "panels.h"
#include "service.h"

namespace gui {

namespace {

// 把已经选中的症状下标收集成数组，交给 Triage::run
int collectPicked(int* out, int cap) {
  const GuiState& st = state();
  int n = 0;
  for (int i = 0; i < BipartiteGraph::kMaxSymptoms && n < cap; ++i)
    if (st.symptomPicked[i]) out[n++] = i;
  return n;
}

}  // namespace

// 执行一个动作。按钮被点、或自检请求了同一个动作时，都走这里 —— 只有一份实现。
static void applyTriage(UiAction act) {
  GuiState& st = state();
  Hospital& h = db();
  if (act == ACT_TRIAGE_CLEAR) {
    for (int i = 0; i < BipartiteGraph::kMaxSymptoms; ++i) st.symptomPicked[i] = false;
    st.symptomCount = 0;
    st.triageValid = false;
    st.lastDept = -1;
    st.clearNotice();
    std::printf("[GUI] 导诊：已清空症状选择\n");
    return;
  }
  if (act != ACT_TRIAGE_RUN) return;

  int picked[BipartiteGraph::kMaxSymptoms];
  const int n = collectPicked(picked, kMaxPickedSymptom);
  st.triage = Triage::run(h, picked, n);
  st.triageValid = true;
  st.lastDept = (st.triage.count > 0) ? st.triage.items[0].dept : -1;
  if (st.triage.count == 0)
    st.notify(NOTICE_WARN, "所选症状与任何科室都没有关联，无法给出推荐。");

  std::printf("[GUI] 导诊：选中 %d 个症状 -> 推荐 %d 个科室\n", n, st.triage.count);
  for (int i = 0; i < st.triage.count; ++i) {
    const TriageItem& it = st.triage.items[i];
    std::printf("        第 %d 名：%s  得分 %d  命中 %d 个症状\n", i + 1, h.depts[it.dept].name,
                it.score, it.matchCount);
  }
}

void drawTriage() {
  GuiState& st = state();
  Hospital& h = db();
  const BipartiteGraph& g = h.symptomGraph;

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.72f, 0.95f, 1.0f));
  ImGui::TextUnformatted("功能 1  智能导诊");
  ImGui::PopStyleColor();
  ImGui::TextWrapped("数学模型：症状—科室加权二部图。选中症状即选中症状侧顶点，"
                      "对每个科室沿邻接链做一次邻域加权求和，按得分推荐。");
  ImGui::Separator();

  // 自检请求：把「开始导诊」或「清空选择」当作刚被点过。
  // 注意只取一个动作，取到就清空 request，避免同一帧里重复触发。
  if (st.isRequested(ACT_TRIAGE_RUN)) {
    st.takeRequest(ACT_TRIAGE_RUN);
    applyTriage(ACT_TRIAGE_RUN);
  } else if (st.isRequested(ACT_TRIAGE_CLEAR)) {
    st.takeRequest(ACT_TRIAGE_CLEAR);
    applyTriage(ACT_TRIAGE_CLEAR);
  }

  // ------------------------------ 症状选择区 ------------------------------
  ImGui::TextUnformatted("请选择您的症状（可多选）：");
  ImGui::Spacing();

  if (ImGui::BeginChild("symptomBox", ImVec2(0.0f, 178.0f), ImGuiChildFlags_Borders)) {
    const float btnW = 116.0f;
    const float availW = ImGui::GetContentRegionAvail().x;
    const int perRow = (availW > btnW) ? static_cast<int>(availW / (btnW + 6.0f)) : 1;
    for (int i = 0; i < g.symptomCount(); ++i) {
      if (i % perRow != 0) ImGui::SameLine();
      const bool on = st.symptomPicked[i];
      if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.50f, 0.72f, 1.0f));
      ImGui::PushID(i);
      if (ImGui::Button(g.symptomName(i), ImVec2(btnW, 0.0f))) {
        st.symptomPicked[i] = !on;
        st.symptomCount += st.symptomPicked[i] ? 1 : -1;
      }
      ImGui::PopID();
      if (on) ImGui::PopStyleColor();
    }
  }
  ImGui::EndChild();

  ImGui::Spacing();
  ImGui::Text("已选症状：%d 个", st.symptomCount);
  ImGui::SameLine();
  const bool canRun = (st.symptomCount > 0);
  if (!canRun) ImGui::BeginDisabled();
  if (ImGui::Button("开始导诊", ImVec2(buttonWidth("开始导诊"), 0.0f))) applyTriage(ACT_TRIAGE_RUN);
  if (!canRun) ImGui::EndDisabled();

  ImGui::SameLine();
  if (ImGui::Button("清空选择", ImVec2(buttonWidth("清空选择"), 0.0f))) {
    for (int i = 0; i < BipartiteGraph::kMaxSymptoms; ++i) st.symptomPicked[i] = false;
    st.symptomCount = 0;
    st.triageValid = false;
    st.lastDept = -1;
    st.clearNotice();
  }
  ImGui::SameLine();
  ImGui::TextDisabled("（打分时最多参考前 %d 个选中的症状，多选的其余症状不参与）", kMaxPickedSymptom);

  if (!st.triageValid) return;

  // ------------------------------ 推荐结果 ------------------------------
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.85f, 0.62f, 1.0f));
  ImGui::TextUnformatted("推荐科室（按匹配得分排序）");
  ImGui::PopStyleColor();

  if (st.triage.count == 0) {
    ImGui::TextDisabled("没有科室与所选症状相关联。");
    return;
  }

  const ImGuiTableFlags tf = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                             ImGuiTableFlags_SizingStretchProp;
  if (ImGui::BeginTable("triageResult", 6, tf)) {
    ImGui::TableSetupColumn("序号", ImGuiTableColumnFlags_WidthFixed, 50.0f);
    ImGui::TableSetupColumn("科室", ImGuiTableColumnFlags_WidthFixed, 130.0f);
    ImGui::TableSetupColumn("得分", ImGuiTableColumnFlags_WidthFixed, 60.0f);
    ImGui::TableSetupColumn("命中的症状", ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableSetupColumn("位置", ImGuiTableColumnFlags_WidthStretch, 1.4f);
    ImGui::TableSetupColumn("直接去预约", ImGuiTableColumnFlags_WidthFixed, 96.0f);
    ImGui::TableHeadersRow();

    for (int i = 0; i < st.triage.count; ++i) {
      const TriageItem& it = st.triage.items[i];
      ImGui::TableNextRow();

      ImGui::TableSetColumnIndex(0);
      if (i == 0) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.85f, 0.62f, 1.0f));
        ImGui::Text("%d  ★首推", i + 1);
        ImGui::PopStyleColor();
      } else {
        ImGui::Text("%d", i + 1);
      }

      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(h.depts[it.dept].name);

      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%d", it.score);

      ImGui::TableSetColumnIndex(3);
      // 命中的症状名拼成一串显示，这就是「推荐依据」
      char basis[256];
      basis[0] = '\0';
      for (int k = 0; k < it.matchCount && k < kMaxMatchedPerDept; ++k) {
        if (it.matched[k] < 0) break;
        if (k) std::strcat(basis, "、");
        std::strcat(basis, g.symptomName(it.matched[k]));
      }
      ImGui::TextUnformatted(basis[0] ? basis : "—");
      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("每命中一个症状，就把该症状与该科室的关联强度累加进得分");
        ImGui::EndTooltip();
      }

      ImGui::TableSetColumnIndex(4);
      ImGui::TextUnformatted(h.depts[it.dept].location);

      ImGui::TableSetColumnIndex(5);
      ImGui::PushID(1000 + i);
      if (ImGui::SmallButton("去预约")) {
        st.bookDeptFilter = it.dept;
        st.page = PAGE_BOOK;
        st.bookDoctor = -1;
        // 默认挑该科室第一位医生，省得用户再找一遍
        for (int d = 0; d < h.doctors.size(); ++d)
          if (h.doctors[d].dept == it.dept) { st.bookDoctor = d; break; }
        st.notify(NOTICE_INFO, "已跳到「在线预约」，并筛出「%s」的医生。", h.depts[it.dept].name);
      }
      ImGui::PopID();
    }
    ImGui::EndTable();
  }

  // ------------------------------ 首推科室的细节 ------------------------------
  const int top = st.triage.items[0].dept;
  ImGui::Spacing();
  ImGui::SeparatorText("首推科室详情");
  ImGui::TextWrapped("该科室主诉范围：%s", h.depts[top].description);

  ImGui::Spacing();
  ImGui::TextUnformatted("该科室的出诊医生：");
  if (ImGui::BeginTable("triageDoctors", 5,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("编号", ImGuiTableColumnFlags_WidthFixed, 60.0f);
    ImGui::TableSetupColumn("姓名", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("职称", ImGuiTableColumnFlags_WidthFixed, 100.0f);
    ImGui::TableSetupColumn("擅长领域", ImGuiTableColumnFlags_WidthFixed, 200.0f);
    ImGui::TableSetupColumn("诊室", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
    int shown = 0;
    for (int d = 0; d < h.doctors.size(); ++d) {
      const Doctor& doc = h.doctors[d];
      if (doc.dept != top) continue;
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(doc.code);
      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(doc.name);
      ImGui::TableSetColumnIndex(2);
      ImGui::TextUnformatted(doc.title);
      ImGui::TableSetColumnIndex(3);
      ImGui::TextUnformatted(doc.skill);
      ImGui::TableSetColumnIndex(4);
      ImGui::TextUnformatted(roomName(h, doc.room));
      ++shown;
    }
    if (shown == 0) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextDisabled("（该科室暂无医生）");
    }
    ImGui::EndTable();
  }
}

}  // namespace gui
