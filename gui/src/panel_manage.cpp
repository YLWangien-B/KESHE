/* =============================================================================
 *  panel_manage.cpp  —  功能 4：预约管理
 *
 *  查看与取消某个患者的预约记录。
 *  取消时要把号源退回去，并且**递增版本号** —— 这样此前看到的余号信息立刻作废，
 *  与预约时用的是同一套版本号机制。
 *
 *  ---------------------------------------------------------------------------
 *  「确认取消」为什么画在表格行里，而不是用弹窗
 *
 *  最初用的是 ImGui 的模态弹窗（OpenPopup + BeginPopupModal），但实际点下去
 *  弹窗不出现，用户完全没法取消。逐行查过 ImGui 的 OpenPopup / BeginPopupModal /
 *  IsPopupOpen（两处的 ID 都来自 window->GetID(name)，按说应当匹配）、也确认过
 *  调用层级（OpenPopup 在表格里，BeginPopupModal 在同一个子窗口的表外，ID 栈平衡），
 *  仍没能定位。既然成本已经超过收益，就换成**不需要弹窗**的做法：
 *
 *      点「取消预约」-> 该行的操作格子变成「确认取消」+「放弃」两个按钮
 *                        （同时在行内把患者、医生、时间、地点摆出来）
 *
 *  好处有三个：不依赖弹窗机制、不用模态、少一次点击；信息也照样看得全。
 *  ---------------------------------------------------------------------------
 * ========================================================================== */
#include <cstdio>
#include <cstring>

#include "imgui.h"
#include "gui_common.h"
#include "gui_state.h"
#include "panels.h"

namespace gui {

// 清掉「等待确认」的状态（确认完成、放弃、换患者时都调它）
void clearPendingCancel() {
  GuiState& st = state();
  st.manageCancelPatient = -1;
  st.manageCancelTicket[0] = '\0';
}

// 取消一条预约。
//
// 定位记录用的是「点击时记下的患者下标 + 凭证号」这一对，而不是当前的 managePatient ——
// 因为确认的时候用户可能已经切换了患者，如果按当前选中的患者去找，
// 就会莫名其妙地报「找不到凭证号」。
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
      clearPendingCancel();
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
    clearPendingCancel();
    return;
  }

  st.notify(NOTICE_ERROR, "没有找到凭证号 %s（患者 %s 名下）。", st.manageCancelTicket,
            (st.manageCancelPatient >= 0 && st.manageCancelPatient < h.patients.size())
                ? h.patients[st.manageCancelPatient].name
                : "未知");
  std::printf("[GUI] 取消预约失败：找不到凭证号 %s（患者下标 %d）\n", st.manageCancelTicket,
              st.manageCancelPatient);
  clearPendingCancel();
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
      if (ImGui::Selectable(label, st.managePatient == i)) {
        st.managePatient = i;
        clearPendingCancel();  // 换患者就把待确认的放弃掉，避免张冠李戴
      }
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
  // 只露出一个「取」字。现在只给「操作」列一个够放下按钮的最小宽度，
  // 其余各列按权重分剩下的空间，窗口怎么缩放都不会把按钮挤出去。
  const float actionW = buttonWidth("确认取消") + buttonWidth("放弃") + 34.0f;
  const ImGuiTableFlags tf = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                             ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY;
  if (ImGui::BeginTable("myBookings", 7, tf, ImVec2(0.0f, -34.0f))) {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("凭证号", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("科室 / 医生", ImGuiTableColumnFlags_WidthStretch, 2.4f);
    ImGui::TableSetupColumn("就诊时间", ImGuiTableColumnFlags_WidthStretch, 1.2f);
    ImGui::TableSetupColumn("就诊地点", ImGuiTableColumnFlags_WidthStretch, 2.6f);
    ImGui::TableSetupColumn("状态", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("该时段余号", ImGuiTableColumnFlags_WidthStretch, 1.1f);
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

      // 这一行是否正处于「等待确认取消」的状态
      const bool pending = (b.status == BOOKED && st.manageCancelPatient == b.patient &&
                            std::strcmp(st.manageCancelTicket, b.ticket) == 0);

      ImGui::TableSetColumnIndex(4);
      if (pending)
        ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.30f, 1.0f), "待确认");
      else if (b.status == BOOKED)
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

      // ---- 操作列 ----
      // 注意：文字必须在「自己的列」里画。之前把「待确认取消」写在操作列代码里回头去
      // 画状态列，结果和余号列的文字叠在了一起 —— 列内光标不会因为你换了列号就回退。
      ImGui::TableSetColumnIndex(6);
      if (b.status != BOOKED) {
        ImGui::TextDisabled("—");
      } else if (pending) {
        // 这一行正处于「等着确认」的状态：就地给出确认与放弃
        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.62f, 0.24f, 0.24f, 1.0f));
        if (ImGui::Button("确认取消", ImVec2(buttonWidth("确认取消"), 0.0f))) {
          applyCancelBooking();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("放弃", ImVec2(buttonWidth("放弃"), 0.0f))) clearPendingCancel();
        ImGui::PopID();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("点「放弃」保留这条预约");
      } else {
        ImGui::PushID(i);
        if (ImGui::Button("取消预约", ImVec2(buttonWidth("取消预约"), 0.0f))) {
          // 记下「这条记录属于谁」+ 凭证号；下一步就在这一行里确认
          st.manageCancelPatient = b.patient;
          std::snprintf(st.manageCancelTicket, sizeof(st.manageCancelTicket), "%s", b.ticket);
          std::printf("[GUI] 待取消：%s（等待确认）\n", b.ticket);
        }
        ImGui::PopID();
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("点一下之后，这一行会出现「确认取消」按钮");
      }
    }
    ImGui::EndTable();
  }

  if (count == 0) {
    ImGui::TextDisabled("%s 目前没有预约记录。",
                        (st.managePatient >= 0 && st.managePatient < h.patients.size())
                            ? h.patients[st.managePatient].name
                            : "该患者");
    // 这位患者名下没有，但全院还有别的预约 —— 多半是患者选错了，
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
  } else if (st.manageCancelPatient >= 0) {
    ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.30f, 1.0f),
                       "正在等待确认：要取消 %s 吗？点该行的「确认取消」，或点「放弃」保留。",
                       st.manageCancelTicket);
  } else {
    ImGui::Text("共 %d 条记录，其中「已预约」%d 条。", count, bookedCount);
  }
}

}  // namespace gui
