/* =============================================================================
 *  panel_booking.cpp  —  功能 3：在线预约
 *
 *  对应题目【实现提示】第 4 条「预约时检查并扣减剩余号源」。
 *  这里是整个系统里唯一会改动号源的地方，因此把一段关键逻辑原样保留：
 *
 *      停诊检查 -> 余量检查 -> 扣减 -> 换版本号
 *
 *  这四步连着做完，中间不让出执行权，所以这一段是原子的；
 *  槽里的版本号换来的是「此前看到的余号信息立刻作废」，也就是乐观锁。
 *  图形界面下鼠标可能会点得很快，这段逻辑一个字都不能省。
 * ========================================================================== */
#include <cstdio>
#include <cstring>

#include "imgui.h"
#include "gui_common.h"
#include "gui_state.h"
#include "panels.h"

namespace gui {

namespace {

// 医生下拉框里显示的文字
void doctorLabel(const Hospital& h, int d, char* buf, int cap) {
  std::snprintf(buf, cap, "%s %s（%s · %s）", h.doctors[d].code, h.doctors[d].name,
                h.doctors[d].title, deptName(h, h.doctors[d].dept));
}

}  // namespace

// 生成预约凭证：界面上的按钮与自检请求都调它，逻辑只有一份。
// 这一段与字符界面 menu.h 里完全一致：三步检查与扣减连着做完，
// 中间不让出执行权；扣减后立刻换一个新版本号，让此前看到的余号信息作废。
static void applyBooking() {
  GuiState& st = state();
  Hospital& h = db();
  const int doctor = st.bookDoctor;
  if (doctor < 0 || doctor >= h.doctors.size()) return;
  const Doctor& doc = h.doctors[doctor];

  ScheduleSlot& s = h.slot(doctor, st.bookDay, st.bookSlot);
  if (s.stopped) {
    st.notify(NOTICE_ERROR, "该时段已停诊，预约失败。");
    std::printf("[GUI] 预约失败：该时段已停诊\n");
    return;
  }
  if (s.quota - s.booked <= 0) {
    st.notify(NOTICE_ERROR, "该时段号源已满，预约失败。");
    std::printf("[GUI] 预约失败：号源已满\n");
    return;
  }

  ++s.booked;
  s.version = ++h.versionClock;

  Booking b;
  std::snprintf(b.ticket, sizeof(b.ticket), "TK%04d", ++h.ticketSeq);
  b.patient = st.bookPatient;
  b.doctor = doctor;
  b.day = st.bookDay;
  b.slot = st.bookSlot;
  b.status = BOOKED;
  h.bookings.push(b);

  // 把凭证内容抄进界面状态，供弹窗显示
  std::snprintf(st.ticketNo, sizeof(st.ticketNo), "%s", b.ticket);
  std::snprintf(st.ticketPatient, sizeof(st.ticketPatient), "%s %s", h.patients[st.bookPatient].code,
                h.patients[st.bookPatient].name);
  std::snprintf(st.ticketDept, sizeof(st.ticketDept), "%s", deptName(h, doc.dept));
  std::snprintf(st.ticketDoctor, sizeof(st.ticketDoctor), "%s %s（%s）", doc.code, doc.name,
                doc.title);
  timeText(st.bookDay, st.bookSlot, st.ticketTime, sizeof(st.ticketTime));
  std::snprintf(st.ticketRoom, sizeof(st.ticketRoom), "%s", roomName(h, doc.room));
  st.ticketLeft = h.remaining(doctor, st.bookDay, st.bookSlot);
  st.showTicket = true;
  st.notify(NOTICE_OK, "预约成功，凭证号 %s，该时段剩余号源 %d。", b.ticket, st.ticketLeft);

  std::printf("[GUI] 预约成功：%s 患者 %s 医生 %s %s %s %s，该时段剩余 %d\n", b.ticket,
              h.patients[st.bookPatient].name, doc.code, doc.name, dayText(st.bookDay),
              slotText(st.bookSlot), st.ticketLeft);
}

void drawBooking() {
  GuiState& st = state();
  Hospital& h = db();

  // 自检参数：requestInt[0]=患者 requestInt[1]=医生 requestInt[2]=日期+时段*7
  if (st.requestInt[0] >= 0) {
    st.bookPatient = st.requestInt[0];
    st.requestInt[0] = -1;
  }
  if (st.requestInt[1] >= 0) {
    st.bookDoctor = st.requestInt[1];
    st.requestInt[1] = -1;
  }
  if (st.requestInt[2] >= 0) {
    st.bookDay = st.requestInt[2] % kMaxDay;
    st.bookSlot = st.requestInt[2] / kMaxDay;
    st.requestInt[2] = -1;
  }

  // 自检请求：把「生成预约凭证」当作刚被点过
  if (st.isRequested(ACT_BOOK_SUBMIT)) {
    st.takeRequest(ACT_BOOK_SUBMIT);
    applyBooking();
  }

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.72f, 0.95f, 1.0f));
  ImGui::TextUnformatted("功能 3  在线预约");
  ImGui::PopStyleColor();
  ImGui::TextWrapped("排班用「每位医生一张 日期×时段 二维数组」存，两个下标即主键，查询 O(1)；"
                      "预约时检查并扣减号源，不会超卖。");
  ImGui::Separator();

  // ------------------------------ 选患者 ------------------------------
  ImGui::TextUnformatted("① 选择患者：");
  ImGui::SameLine();
  char pLabel[64];
  if (st.bookPatient >= 0 && st.bookPatient < h.patients.size())
    std::snprintf(pLabel, sizeof(pLabel), "%s %s", h.patients[st.bookPatient].code,
                  h.patients[st.bookPatient].name);
  else
    std::snprintf(pLabel, sizeof(pLabel), "（请选择）");
  ImGui::SetNextItemWidth(comboWidth(pLabel));
  if (ImGui::BeginCombo("##patient", pLabel)) {
    for (int i = 0; i < h.patients.size(); ++i) {
      char label[64];
      std::snprintf(label, sizeof(label), "%s %s", h.patients[i].code, h.patients[i].name);
      if (ImGui::Selectable(label, st.bookPatient == i)) st.bookPatient = i;
    }
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  ImGui::TextDisabled("联系电话：%s",
                      (st.bookPatient >= 0 && st.bookPatient < h.patients.size())
                          ? h.patients[st.bookPatient].phone
                          : "—");

  // ------------------------------ 选医生 ------------------------------
  ImGui::Spacing();
  ImGui::TextUnformatted("② 选择医生：");
  ImGui::SameLine();
  const char* deptCur = (st.bookDeptFilter >= 0 && st.bookDeptFilter < h.depts.size())
                            ? h.depts[st.bookDeptFilter].name
                            : "全部科室";
  ImGui::SetNextItemWidth(comboWidth(deptCur));
  if (ImGui::BeginCombo("##deptFilter", deptCur)) {
    if (ImGui::Selectable("全部科室", st.bookDeptFilter < 0)) st.bookDeptFilter = -1;
    for (int i = 0; i < h.depts.size(); ++i) {
      char label[96];
      std::snprintf(label, sizeof(label), "%s  %s", h.depts[i].code, h.depts[i].name);
      if (ImGui::Selectable(label, st.bookDeptFilter == i)) st.bookDeptFilter = i;
    }
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  char dLabel[128];
  const bool doctorOk = (st.bookDoctor >= 0 && st.bookDoctor < h.doctors.size());
  if (doctorOk)
    doctorLabel(h, st.bookDoctor, dLabel, sizeof(dLabel));
  else
    std::snprintf(dLabel, sizeof(dLabel), "（请选择医生）");
  ImGui::SetNextItemWidth(comboWidth(dLabel));
  if (ImGui::BeginCombo("##doctor", dLabel)) {
    for (int i = 0; i < h.doctors.size(); ++i) {
      if (st.bookDeptFilter >= 0 && h.doctors[i].dept != st.bookDeptFilter) continue;
      char label[128];
      doctorLabel(h, i, label, sizeof(label));
      if (ImGui::Selectable(label, st.bookDoctor == i)) {
        st.bookDoctor = i;
        st.bookDay = 0;
        st.bookSlot = 0;
      }
    }
    ImGui::EndCombo();
  }

  if (!doctorOk) {
    ImGui::Spacing();
    ImGui::TextDisabled("先选一位医生，下面会列出他一周的号源。");
    return;
  }

  // ------------------------------ 号源表格 ------------------------------
  ImGui::Spacing();
  ImGui::Separator();
  const Doctor& doc = h.doctors[st.bookDoctor];
  ImGui::Text("③ 选择日期与时段 —— %s %s（%s · %s）", doc.code, doc.name, doc.title,
              deptName(h, doc.dept));
  ImGui::SameLine();
  ImGui::TextDisabled("  诊室：%s", roomName(h, doc.room));

  const float cellW = 96.0f;
  const ImGuiTableFlags tf = ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit;
  if (ImGui::BeginTable("slotGrid", kMaxDay + 1, tf)) {
    ImGui::TableSetupColumn("时段", ImGuiTableColumnFlags_WidthFixed, 64.0f);
    for (int d = 0; d < kMaxDay; ++d)
      ImGui::TableSetupColumn(dayText(d), ImGuiTableColumnFlags_WidthFixed, cellW);
    ImGui::TableHeadersRow();

    for (int s = 0; s < 2; ++s) {  // 当前数据只用上午/下午两个时段
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(slotText(s));
      for (int d = 0; d < kMaxDay; ++d) {
        ImGui::TableSetColumnIndex(d + 1);
        char id[32];
        std::snprintf(id, sizeof(id), "bk%d_%d", d, s);
        if (slotCell(id, st.bookDoctor, d, s, cellW - 8.0f)) {
          st.bookDay = d;
          st.bookSlot = s;
          st.clearNotice();
        }
      }
    }
    ImGui::EndTable();
  }
  drawSlotLegend();

  ImGui::Spacing();
  ImGui::Text("当前选择：%s %s", dayText(st.bookDay), slotText(st.bookSlot));
  ImGui::SameLine();
  const SlotState cs = slotState(h, st.bookDoctor, st.bookDay, st.bookSlot);
  ImGui::PushStyleColor(ImGuiCol_Text, cs == SLOT_OK || cs == SLOT_TIGHT
                                           ? ImVec4(0.45f, 0.85f, 0.52f, 1.0f)
                                           : ImVec4(0.95f, 0.55f, 0.40f, 1.0f));
  ImGui::Text("· %s", slotStateText(h, st.bookDoctor, st.bookDay, st.bookSlot));
  ImGui::PopStyleColor();
  if (cs == SLOT_OK || cs == SLOT_TIGHT) {
    ImGui::SameLine();
    ImGui::TextDisabled("（剩余 %d 个号）", h.remaining(st.bookDoctor, st.bookDay, st.bookSlot));
  }

  ImGui::Spacing();
  const bool canBook = (cs == SLOT_OK || cs == SLOT_TIGHT);
  if (!canBook) ImGui::BeginDisabled();
  if (ImGui::Button("生成预约凭证", ImVec2(buttonWidth("生成预约凭证"), 0.0f))) applyBooking();
  if (!canBook) ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::TextDisabled("（号满或停诊时按钮不可用，避免点出一个失败的预约）");

  // ------------------------------ 预约凭证弹窗 ------------------------------
  if (st.showTicket) {
    ImGui::OpenPopup("预约凭证");
    st.showTicket = false;
  }
  if (ImGui::BeginPopupModal("预约凭证", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.85f, 0.62f, 1.0f));
    ImGui::TextUnformatted("预约成功");
    ImGui::PopStyleColor();
    ImGui::Separator();
    if (ImGui::BeginTable("ticket", 2, ImGuiTableFlags_SizingFixedFit)) {
      const struct { const char* k; const char* v; } rows[] = {
          {"凭证号", st.ticketNo},   {"患者", st.ticketPatient}, {"科室", st.ticketDept},
          {"医生", st.ticketDoctor}, {"就诊时间", st.ticketTime}, {"就诊地点", st.ticketRoom},
      };
      for (const auto& r : rows) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("%s", r.k);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(r.v);
      }
      ImGui::EndTable();
    }
    ImGui::Spacing();
    if (st.ticketLeft <= 0)
      ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.40f, 1.0f), "该时段号源已经用完。");
    else
      ImGui::Text("该时段剩余号源：%d", st.ticketLeft);
    ImGui::Spacing();
    if (ImGui::Button("知道了", ImVec2(buttonWidth("知道了"), 0.0f))) ImGui::CloseCurrentPopup();
    ImGui::SameLine();
    if (ImGui::Button("查看我的预约", ImVec2(buttonWidth("查看我的预约"), 0.0f))) {
      st.managePatient = st.bookPatient;
      st.page = PAGE_MANAGE;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

}  // namespace gui
