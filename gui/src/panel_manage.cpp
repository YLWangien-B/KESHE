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

// 取消一条预约。
//
// 定位记录用的是「点击时记下的患者下标 + 凭证号」这一对，而不是当前的 managePatient ——
// 因为确认弹窗弹出之后用户还可能去切换患者，如果按当前选中的患者去找，
// 就会莫名其妙地报「找不到凭证号」。（这是实测发现的一个真实缺陷。）
void applyCancelBooking() {
  GuiState& st = state();
  Hospital& h = db();

  if (st.manageCancelPatient < 0 || st.manageCancelTicket[0] == '\0') {
    st.notify(NOTICE_ERROR, "没有待取消的预约，请先在记录表里点「取消预约」。");
    return;
  }

  for (int i = 0; i < h.bookings.size(); ++i) {
    Booking& b = h.bookings[i];
    if (b.patient != st.manageCancelPatient) continue;
    if (std::strcmp(b.ticket, st.manageCancelTicket) != 0) continue;
    if (b.status != BOOKED) {
      st.notify(NOTICE_WARN, "预约 %s 不是「已预约」状态（%s），无法取消。", b.ticket,
                b.status == CANCELLED ? "已取消" : "已完成");
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
    st.notify(NOTICE_OK, "预约 %s 已取消，号源已退回（该时段剩余 %d 个）。", b.ticket,
              h.remaining(b.doctor, b.day, b.slot));
    std::printf("[GUI] 取消预约：%s（患者 %s），该时段剩余号源 %d\n", b.ticket,
                h.patients[b.patient].code, h.remaining(b.doctor, b.day, b.slot));

    // 用掉之后清掉，避免同一个凭证号被重复确认
    st.manageCancelPatient = -1;
    st.manageCancelTicket[0] = '\0';
    return;
  }
  st.notify(NOTICE_ERROR, "没有找到凭证号 %s（患者 %s 名下）。", st.manageCancelTicket,
            (st.manageCancelPatient >= 0 && st.manageCancelPatient < h.patients.size())
                ? h.patients[st.manageCancelPatient].name
                : "未知");
  std::printf("[GUI] 取消预约失败：找不到凭证号 %s（患者下标 %d）\n", st.manageCancelTicket,
              st.manageCancelPatient);
}

void drawManage() {
  GuiState& st = state();
  Hospital& h = db();

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
  // 列宽用「按比例拉伸」而不是写死像素。
  //
  // 原来这里有一组固定宽度（凭证号 84 + 科室 118 + 医生 90 + 时间 100 +
  // 地点 160 + 状态 80 + 余号 96 + 操作 90 ≈ 818），而 150% DPI 下内容区
  // 实际只有约 790 像素 —— 于是「取消预约」按钮被挤到表格最右边，
  // 只露出一个「取」字，得横向滚动才点得到。这是实测发现的真实缺陷。
  //
  // 现在只给「操作」列一个够放下按钮的最小宽度，其余各列按权重分剩下的空间，
  // 窗口怎么缩放都不会把按钮挤出去。同时把「科室」和「医生」并成一列，
  // 省下一列的空间。
  const float actionW = buttonWidth("取消预约") + 12.0f;
  const ImGuiTableFlags tf = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                             ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;
  if (ImGui::BeginTable("myBookings", 7, tf, ImVec2(0.0f, -34.0f))) {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("凭证号", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("科室 / 医生", ImGuiTableColumnFlags_WidthStretch, 2.6f);
    ImGui::TableSetupColumn("就诊时间", ImGuiTableColumnFlags_WidthStretch, 1.3f);
    ImGui::TableSetupColumn("就诊地点", ImGuiTableColumnFlags_WidthStretch, 2.8f);
    ImGui::TableSetupColumn("状态", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("该时段余号", ImGuiTableColumnFlags_WidthStretch, 1.2f);
    ImGui::TableSetupColumn("操作", ImGuiTableColumnFlags_WidthFixed, actionW);
    ImGui::TableHeadersRow();

    for (int i = 0; i < h.bookings.size(); ++i) {
      const Booking& b = h.bookings[i];
      if (b.patient != st.managePatient) continue;
      ++count;
      if (b.status == BOOKED) ++bookedCount;

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(b.ticket);

      // 科室与医生并成一列，省出横向空间
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%s · %s", deptName(h, h.doctors[b.doctor].dept), h.doctors[b.doctor].name);

      ImGui::TableSetColumnIndex(2);
      char t[32];
      timeText(b.day, b.slot, t, sizeof(t));
      ImGui::TextUnformatted(t);

      ImGui::TableSetColumnIndex(3);
      ImGui::TextUnformatted(roomName(h, h.doctors[b.doctor].room));

      ImGui::TableSetColumnIndex(4);
      if (b.status == BOOKED)
        ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.52f, 1.0f), "已预约");
      else if (b.status == CANCELLED)
        ImGui::TextDisabled("已取消");
      else
        ImGui::TextDisabled("已完成");

      ImGui::TableSetColumnIndex(5);
      if (h.hasSchedule(b.doctor, b.day, b.slot))
        ImGui::Text("%d", h.remaining(b.doctor, b.day, b.slot));
      else
        ImGui::TextDisabled("—");

      ImGui::TableSetColumnIndex(6);
      if (b.status == BOOKED) {
        ImGui::PushID(i);
        // 用带内边距宽度的按钮，不再用 SmallButton —— 它太窄，字会被裁
        if (ImGui::Button("取消预约", ImVec2(buttonWidth("取消预约"), 0.0f))) {
          // 记下「这条记录属于谁」+ 凭证号。之后用户的任何操作都不会改变它，
          // 所以确认弹窗里点「确认取消」一定取消的是这一条。
          st.manageCancelPatient = b.patient;
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
    // 这位患者名下没有，但全院还有别的预约 —— 说明多半是患者选错了，
    // 直接把「谁有预约」指出来，省得用户以为预约丢了。
    int others = 0;
    int firstOther = -1;
    for (int i = 0; i < h.bookings.size(); ++i) {
      if (h.bookings[i].patient == st.managePatient) continue;
      if (h.bookings[i].status != BOOKED) continue;
      ++others;
      if (firstOther < 0) firstOther = h.bookings[i].patient;
    }
    if (others > 0 && firstOther >= 0) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.30f, 1.0f),
                         "  （全院的预约里还有 %d 条「已预约」，例如 %s %s —— 在上面切换患者看看）",
                         others, h.patients[firstOther].code, h.patients[firstOther].name);
    }
  } else {
    ImGui::Text("共 %d 条记录，其中「已预约」%d 条。", count, bookedCount);
  }

  // ------------------------------ 取消确认弹窗 ------------------------------
  if (ImGui::BeginPopupModal("确认取消预约", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    // 把要取消的这条记录的关键信息摆出来，避免用户点错
    const int cp = st.manageCancelPatient;
    ImGui::Text("确定要取消预约 %s 吗？", st.manageCancelTicket);
    if (cp >= 0 && cp < h.patients.size()) {
      for (int i = 0; i < h.bookings.size(); ++i) {
        const Booking& b = h.bookings[i];
        if (b.patient != cp || std::strcmp(b.ticket, st.manageCancelTicket) != 0) continue;
        char t[32];
        timeText(b.day, b.slot, t, sizeof(t));
        ImGui::Spacing();
        ImGui::BulletText("患者：%s %s", h.patients[b.patient].code, h.patients[b.patient].name);
        ImGui::BulletText("医生：%s %s（%s）", h.doctors[b.doctor].code, h.doctors[b.doctor].name,
                          h.doctors[b.doctor].title);
        ImGui::BulletText("时间：%s   地点：%s", t, roomName(h, h.doctors[b.doctor].room));
        break;
      }
    }
    ImGui::Spacing();
    ImGui::TextDisabled("取消后该时段的号源会退回，其他人可以重新预约。");
    ImGui::Spacing();
    if (ImGui::Button("确认取消", ImVec2(buttonWidth("确认取消"), 0.0f))) {
      applyCancelBooking();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("再想想", ImVec2(buttonWidth("再想想"), 0.0f))) {
      // 明确放弃：把待取消的记录清掉，避免它一直挂着
      st.manageCancelPatient = -1;
      st.manageCancelTicket[0] = '\0';
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

}  // namespace gui
