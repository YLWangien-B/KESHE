/* =============================================================================
 *  panel_schedule.cpp  —  功能 5：排班展示
 *
 *  两种视角，对应题目「以日历形式展示医生的排班情况」：
 *    按医生：一位医生一周的号源日历（周一~周日 × 上午/下午，格内是「余号/总数」）
 *    按科室：一个科室所有医生的号源矩阵，一眼看出哪天哪个人还有号
 *  数据全部来自「每位医生一张 日期×时段 二维数组」，不额外建任何索引。
 * ========================================================================== */
#include <cstdio>

#include "imgui.h"
#include "gui_common.h"
#include "gui_state.h"
#include "panels.h"

namespace gui {

namespace {

// 一位医生的一周排班：7 列（天）× 2 行（上午/下午）
void drawOneDoctorCalendar(int doctor, float cellW) {
  Hospital& h = db();
  const Doctor& doc = h.doctors[doctor];

  ImGui::Text("%s %s（%s · %s）", doc.code, doc.name, doc.title, deptName(h, doc.dept));
  ImGui::SameLine();
  ImGui::TextDisabled("  诊室：%s", roomName(h, doc.room));

  const ImGuiTableFlags tf = ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit;
  if (ImGui::BeginTable("oneDoc", kMaxDay + 1, tf)) {
    ImGui::TableSetupColumn("时段", ImGuiTableColumnFlags_WidthFixed, 64.0f);
    for (int d = 0; d < kMaxDay; ++d)
      ImGui::TableSetupColumn(dayText(d), ImGuiTableColumnFlags_WidthFixed, cellW);
    ImGui::TableHeadersRow();
    for (int s = 0; s < 2; ++s) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(slotText(s));
      for (int d = 0; d < kMaxDay; ++d) {
        ImGui::TableSetColumnIndex(d + 1);
        char id[32];
        std::snprintf(id, sizeof(id), "sc%d_%d", d, s);
        if (slotCell(id, doctor, d, s, cellW - 8.0f)) {
          // 点格子 = 想去这个时段预约，直接跳到预约页并选中
          GuiState& st = state();
          st.bookDoctor = doctor;
          st.bookDeptFilter = h.doctors[doctor].dept;
          st.bookDay = d;
          st.bookSlot = s;
          st.page = PAGE_BOOK;
          st.notify(NOTICE_INFO, "已把「%s %s %s」带到在线预约页。", doc.name, dayText(d), slotText(s));
        }
      }
    }
    ImGui::EndTable();
  }

  // 该医生这一周的汇总
  int slots = 0, quota = 0, left = 0, booked = 0;
  for (int d = 0; d < kMaxDay; ++d)
    for (int s = 0; s < 2; ++s) {
      if (!h.hasSchedule(doctor, d, s)) continue;
      ++slots;
      quota += h.slot(doctor, d, s).quota;
      booked += h.slot(doctor, d, s).booked;
      left += h.remaining(doctor, d, s);
    }
  ImGui::Spacing();
  ImGui::Text("本周出诊 %d 个时段，号源共 %d 个：已约 %d，剩余 %d。", slots, quota, booked, left);
}

// 一个科室的号源矩阵：行是医生，列是「天 × 时段」
void drawDeptMatrix(int dept) {
  Hospital& h = db();
  ImGui::TextUnformatted(deptName(h, dept));
  ImGui::SameLine();
  ImGui::TextDisabled("  %s", (dept >= 0 && dept < h.depts.size()) ? h.depts[dept].location : "");

  const ImGuiTableFlags tf = ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit |
                             ImGuiTableFlags_ScrollX;
  if (ImGui::BeginTable("deptMatrix", kMaxDay * 2 + 1, tf)) {
    ImGui::TableSetupColumn("医生", ImGuiTableColumnFlags_WidthFixed, 130.0f);
    for (int d = 0; d < kMaxDay; ++d)
      for (int s = 0; s < 2; ++s) {
        char head[16];
        std::snprintf(head, sizeof(head), "%s%s", dayText(d), s == 0 ? "上" : "下");
        ImGui::TableSetupColumn(head, ImGuiTableColumnFlags_WidthFixed, 74.0f);
      }
    ImGui::TableHeadersRow();

    int deptSlots = 0, deptQuota = 0, deptLeft = 0;
    for (int i = 0; i < h.doctors.size(); ++i) {
      if (h.doctors[i].dept != dept) continue;
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("%s %s", h.doctors[i].code, h.doctors[i].name);
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s · %s\n%s", h.doctors[i].title, deptName(h, h.doctors[i].dept),
                          h.doctors[i].skill);

      for (int d = 0; d < kMaxDay; ++d)
        for (int s = 0; s < 2; ++s) {
          ImGui::TableSetColumnIndex(1 + d * 2 + s);
          char id[32];
          std::snprintf(id, sizeof(id), "dm%d_%d_%d", i, d, s);
          if (slotCell(id, i, d, s, 66.0f)) {
            GuiState& st = state();
            st.bookDoctor = i;
            st.bookDeptFilter = dept;
            st.bookDay = d;
            st.bookSlot = s;
            st.page = PAGE_BOOK;
            st.notify(NOTICE_INFO, "已把「%s %s %s」带到在线预约页。", h.doctors[i].name, dayText(d),
                      slotText(s));
          }
          if (h.hasSchedule(i, d, s)) {
            ++deptSlots;
            deptQuota += h.slot(i, d, s).quota;
            deptLeft += h.remaining(i, d, s);
          }
        }
    }
    ImGui::EndTable();
    ImGui::Text("本科室本周共 %d 个出诊时段，号源 %d 个，剩余 %d 个。", deptSlots, deptQuota, deptLeft);
  }
}

}  // namespace

void drawSchedule() {
  GuiState& st = state();
  Hospital& h = db();

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.72f, 0.95f, 1.0f));
  ImGui::TextUnformatted("功能 5  排班展示");
  ImGui::PopStyleColor();
  ImGui::TextWrapped("格内是「余号/总数」。颜色越绿越好约：绿=号源充足，黄=紧张，红=已满，灰=不出诊。"
                      "点任意格子可直接带到预约页。");
  ImGui::Separator();

  ImGui::TextUnformatted("展示方式：");
  ImGui::SameLine();
  if (st.scheduleMode == 0) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.50f, 0.72f, 1.0f));
  if (ImGui::Button("按医生查看", ImVec2(buttonWidth("按医生查看"), 0.0f))) st.scheduleMode = 0;
  if (st.scheduleMode == 0) ImGui::PopStyleColor();
  ImGui::SameLine();
  if (st.scheduleMode == 1) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.50f, 0.72f, 1.0f));
  if (ImGui::Button("按科室汇总", ImVec2(buttonWidth("按科室汇总"), 0.0f))) st.scheduleMode = 1;
  if (st.scheduleMode == 1) ImGui::PopStyleColor();
  ImGui::SameLine();
  drawSlotLegend();

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  if (st.scheduleMode == 0) {
    ImGui::TextUnformatted("选择医生：");
    ImGui::SameLine();
    if (st.scheduleDoctor < 0 || st.scheduleDoctor >= h.doctors.size()) st.scheduleDoctor = 0;
    char cur[128];
    std::snprintf(cur, sizeof(cur), "%s %s（%s）", h.doctors[st.scheduleDoctor].code,
                  h.doctors[st.scheduleDoctor].name, h.doctors[st.scheduleDoctor].title);
    ImGui::SetNextItemWidth(comboWidth(cur));
    if (ImGui::BeginCombo("##sd", cur)) {
      for (int i = 0; i < h.doctors.size(); ++i) {
        char label[128];
        std::snprintf(label, sizeof(label), "%s %s（%s · %s）", h.doctors[i].code,
                      h.doctors[i].name, h.doctors[i].title, deptName(h, h.doctors[i].dept));
        if (ImGui::Selectable(label, st.scheduleDoctor == i)) st.scheduleDoctor = i;
      }
      ImGui::EndCombo();
    }
    ImGui::Spacing();
    drawOneDoctorCalendar(st.scheduleDoctor, 96.0f);
  } else {
    ImGui::TextUnformatted("选择科室：");
    ImGui::SameLine();
    if (st.scheduleDept < 0 || st.scheduleDept >= h.depts.size()) st.scheduleDept = 0;
    char cur[96];
    std::snprintf(cur, sizeof(cur), "%s %s", h.depts[st.scheduleDept].code,
                  h.depts[st.scheduleDept].name);
    ImGui::SetNextItemWidth(comboWidth(cur));
    if (ImGui::BeginCombo("##sdp", cur)) {
      for (int i = 0; i < h.depts.size(); ++i) {
        char label[96];
        std::snprintf(label, sizeof(label), "%s %s", h.depts[i].code, h.depts[i].name);
        if (ImGui::Selectable(label, st.scheduleDept == i)) st.scheduleDept = i;
      }
      ImGui::EndCombo();
    }
    ImGui::Spacing();
    drawDeptMatrix(st.scheduleDept);
  }

  // ------------------------------ 各科室汇总 ------------------------------
  ImGui::Spacing();
  ImGui::SeparatorText("各科室号源汇总");
  if (ImGui::BeginTable("deptSummary", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("科室", ImGuiTableColumnFlags_WidthFixed, 140.0f);
    ImGui::TableSetupColumn("医生数", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("出诊时段数", ImGuiTableColumnFlags_WidthFixed, 110.0f);
    ImGui::TableSetupColumn("一周总号源 / 剩余", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
    for (int d = 0; d < h.depts.size(); ++d) {
      int docs = 0, slots = 0, quota = 0, left = 0;
      for (int i = 0; i < h.doctors.size(); ++i) {
        if (h.doctors[i].dept != d) continue;
        ++docs;
        for (int day = 0; day < kMaxDay; ++day)
          for (int s = 0; s < 2; ++s) {
            if (!h.hasSchedule(i, day, s)) continue;
            ++slots;
            quota += h.slot(i, day, s).quota;
            left += h.remaining(i, day, s);
          }
      }
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(h.depts[d].name);
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%d", docs);
      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%d", slots);
      ImGui::TableSetColumnIndex(3);
      ImGui::Text("%d / %d", quota, left);
    }
    ImGui::EndTable();
  }
}

}  // namespace gui
