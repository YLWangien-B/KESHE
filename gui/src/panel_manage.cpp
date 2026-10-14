/* =============================================================================
 *  panel_manage.cpp  —  功能 4：预约管理
 *
 *  查看与取消某个患者的预约记录。
 *  取消时要把号源退回去，并且**递增版本号** —— 这样此前看到的余号信息立刻作废，
 *  与预约时用的是同一套版本号机制。
 * ========================================================================== */
#include <cstdio>
#include <cstring>

#include "imgui.h"
#include "gui_common.h"
#include "gui_state.h"
#include "panels.h"

namespace gui {

// 取消一条预约：界面上的「确认取消」按钮与自检请求都调它，逻辑只有一份。
void applyCancelBooking() {
  GuiState& st = state();
  Hospital& h = db();
  for (int i = 0; i < h.bookings.size(); ++i) {
    Booking& b = h.bookings[i];
    if (b.patient != st.managePatient) continue;
    if (std::strcmp(b.ticket, st.manageCancelTicket) != 0) continue;
    if (b.status != BOOKED) {
      st.notify(NOTICE_WARN, "预约 %s 不是「已预约」状态，无法取消。", b.ticket);
      std::printf("[GUI] 取消预约失败：%s 状态不是「已预约」\n", b.ticket);
      return;
    }
    // 退回号源，并递增版本号，让此前看到的余号信息立刻失效
    if (h.hasSchedule(b.doctor, b.day, b.slot)) {
      ScheduleSlot& s = h.slot(b.doctor, b.day, b.slot);
      if (s.booked > 0) {
        --s.booked;
        s.version = ++h.versionClock;
      }
    }
    b.status = CANCELLED;
    st.notify(NOTICE_OK, "预约 %s 已取消，号源已退回。", b.ticket);
    std::printf("[GUI] 取消预约：%s，该时段剩余号源 %d\n", b.ticket,
                h.remaining(b.doctor, b.day, b.slot));
    return;
  }
  st.notify(NOTICE_ERROR, "没有找到凭证号 %s。", st.manageCancelTicket);
  std::printf("[GUI] 取消预约失败：找不到凭证号 %s\n", st.manageCancelTicket);
}

void drawManage() {
  GuiState& st = state();
  Hospital& h = db();

  // 自检参数：requestInt[0] = 要查看的患者
  if (st.requestInt[0] >= 0) {
    st.managePatient = st.requestInt[0];
    st.requestInt[0] = -1;
  }

  // 自检请求：把「确认取消」当作刚被点过
  if (st.isRequested(ACT_MANAGE_CANCEL_CONFIRM)) {
    st.takeRequest(ACT_MANAGE_CANCEL_CONFIRM);
    applyCancelBooking();
  }

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.72f, 0.95f, 1.0f));
  ImGui::TextUnformatted("功能 4  预约管理");
  ImGui::PopStyleColor();
  ImGui::TextWrapped("预约记录存在顺序表里（当前 %d 条，容量 %d）。取消时退回号源并递增版本号。",
                     h.bookings.size(), kMaxBook);
  ImGui::Separator();

  // ------------------------------ 选患者 ------------------------------
  ImGui::TextUnformatted("查看哪位患者的预约：");
  ImGui::SameLine();
  char pLabel[64];
  if (st.managePatient >= 0 && st.managePatient < h.patients.size())
    std::snprintf(pLabel, sizeof(pLabel), "%s %s", h.patients[st.managePatient].code,
                  h.patients[st.managePatient].name);
  else
    std::snprintf(pLabel, sizeof(pLabel), "（请选择）");
  ImGui::SetNextItemWidth(comboWidth(pLabel));
  if (ImGui::BeginCombo("##mp", pLabel)) {
    for (int i = 0; i < h.patients.size(); ++i) {
      char label[64];
      std::snprintf(label, sizeof(label), "%s %s", h.patients[i].code, h.patients[i].name);
      if (ImGui::Selectable(label, st.managePatient == i)) st.managePatient = i;
    }
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  ImGui::TextDisabled("（预约功能里刚预约过的那位患者会自动带过来）");

  // ------------------------------ 记录表 ------------------------------
  ImGui::Spacing();
  ImGui::Separator();

  int count = 0;
  int bookedCount = 0;
  const ImGuiTableFlags tf = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                             ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;
  if (ImGui::BeginTable("myBookings", 8, tf, ImVec2(0.0f, -34.0f))) {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("凭证号", ImGuiTableColumnFlags_WidthFixed, 84.0f);
    ImGui::TableSetupColumn("科室", ImGuiTableColumnFlags_WidthFixed, 118.0f);
    ImGui::TableSetupColumn("医生", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("就诊时间", ImGuiTableColumnFlags_WidthFixed, 100.0f);
    ImGui::TableSetupColumn("就诊地点", ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableSetupColumn("状态", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("该时段余号", ImGuiTableColumnFlags_WidthFixed, 96.0f);
    ImGui::TableSetupColumn("操作", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableHeadersRow();

    for (int i = 0; i < h.bookings.size(); ++i) {
      const Booking& b = h.bookings[i];
      if (b.patient != st.managePatient) continue;
      ++count;
      if (b.status == BOOKED) ++bookedCount;

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(b.ticket);

      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(deptName(h, h.doctors[b.doctor].dept));

      ImGui::TableSetColumnIndex(2);
      ImGui::TextUnformatted(h.doctors[b.doctor].name);

      ImGui::TableSetColumnIndex(3);
      char t[32];
      timeText(b.day, b.slot, t, sizeof(t));
      ImGui::TextUnformatted(t);

      ImGui::TableSetColumnIndex(4);
      ImGui::TextUnformatted(roomName(h, h.doctors[b.doctor].room));

      ImGui::TableSetColumnIndex(5);
      if (b.status == BOOKED)
        ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.52f, 1.0f), "已预约");
      else if (b.status == CANCELLED)
        ImGui::TextDisabled("已取消");
      else
        ImGui::TextDisabled("已完成");

      ImGui::TableSetColumnIndex(6);
      if (h.hasSchedule(b.doctor, b.day, b.slot))
        ImGui::Text("%d", h.remaining(b.doctor, b.day, b.slot));
      else
        ImGui::TextDisabled("—");

      ImGui::TableSetColumnIndex(7);
      if (b.status == BOOKED) {
        ImGui::PushID(i);
        if (ImGui::SmallButton("取消预约")) {
          std::snprintf(st.manageCancelTicket, sizeof(st.manageCancelTicket), "%s", b.ticket);
          ImGui::OpenPopup("确认取消预约");
        }
        ImGui::PopID();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("取消后号源会退回，并让此前的余号信息作废");
      } else {
        ImGui::TextDisabled("—");
      }
    }
    ImGui::EndTable();
  }

  if (count == 0) {
    ImGui::TextDisabled("%s 目前没有预约记录。",
                        (st.managePatient >= 0 && st.managePatient < h.patients.size())
                            ? h.patients[st.managePatient].name
                            : "该患者");
  } else {
    ImGui::Text("共 %d 条记录，其中「已预约」%d 条。", count, bookedCount);
  }

  // ------------------------------ 取消确认弹窗 ------------------------------
  if (ImGui::BeginPopupModal("确认取消预约", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("确定要取消预约 %s 吗？", st.manageCancelTicket);
    ImGui::Spacing();
    ImGui::TextDisabled("取消后该时段的号源会退回，其他人可以重新预约。");
    ImGui::Spacing();
    if (ImGui::Button("确认取消", ImVec2(buttonWidth("确认取消"), 0.0f))) {
      applyCancelBooking();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("再想想", ImVec2(buttonWidth("再想想"), 0.0f))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }
}

}  // namespace gui
