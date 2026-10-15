/* =============================================================================
 *  panel_queue.cpp  —  功能 7：候诊队列
 *
 *  界面流程：患者签到 -> 医生叫号 -> （过号 / 就诊完成）。
 *  队列本身是 ds.h 里的环形队列 RingQueue<Waiting>，操作在 service_queue.h 的 Queue 类，
 *  这里一个算法都没重写，只是把 Queue 返回的 QueueResult 枚举翻译成一句给用户看的话。
 *
 *  为什么用枚举返回值而不是 bool：每一种失败都要给患者一句准确的话 ——
 *  「已经在队列里了，当前第 N 位」和「队列已满，请稍后再来」是完全不同的提示。
 * ========================================================================== */
#include <cstdio>
#include <cstring>

#include "imgui.h"
#include "gui_common.h"
#include "gui_state.h"
#include "load.h"
#include "panels.h"
#include "service_queue.h"

namespace gui {

namespace {

// 某医生当前的候诊列表
void drawDoctorQueue(int doctor) {
  const Hospital& h = db();
  const int count = Queue::countOf(h, doctor);

  ImGui::Text("%s %s（%s）的候诊队列，共 %d 人", h.doctors[doctor].code, h.doctors[doctor].name,
              h.doctors[doctor].title, count);
  ImGui::SameLine();
  ImGui::TextDisabled("  诊室：%s", roomName(h, h.doctors[doctor].room));

  if (count == 0) {
    ImGui::TextDisabled("当前没有患者候诊。");
    return;
  }

  if (ImGui::BeginTable("queueList", 5,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("排队号", ImGuiTableColumnFlags_WidthFixed, 72.0f);
    ImGui::TableSetupColumn("患者编号", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("姓名", ImGuiTableColumnFlags_WidthFixed, 100.0f);
    ImGui::TableSetupColumn("状态", ImGuiTableColumnFlags_WidthFixed, 240.0f);
    ImGui::TableSetupColumn("前面还有", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableHeadersRow();

    int shown = 0;
    for (int i = 0; i < h.waiting.size() && shown < count; ++i) {
      const Waiting& w = h.waiting.at(i);
      if (w.doctor != doctor) continue;
      ImGui::TableNextRow();

      ImGui::TableSetColumnIndex(0);
      ImGui::Text("%d", w.queueNo);

      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(h.patients[w.patient].code);

      ImGui::TableSetColumnIndex(2);
      ImGui::TextUnformatted(h.patients[w.patient].name);

      ImGui::TableSetColumnIndex(3);
      if (w.calledCount == 0) {
        ImGui::TextColored(ImVec4(0.65f, 0.75f, 0.90f, 1.0f), "等待中");
      } else if (w.calledCount == 1) {
        ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.52f, 1.0f), "已叫号，请进诊室");
      } else {
        ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.35f, 1.0f), "已过号 %d 次，再叫不到就重新排队",
                           w.calledCount - 1);
      }

      ImGui::TableSetColumnIndex(4);
      // 「前面还有几人」只数还没被叫号的：正在诊室里的人不算「在等的人」
      ImGui::Text("%d 人", Queue::waitingAhead(h, w.patient, doctor));
      ++shown;
    }
    ImGui::EndTable();
  }
  ImGui::TextDisabled("「前面还有」只统计还没被叫号的人；正在诊室里的那几位不算，否则患者会以为还要等很久。");
}

// 患者下拉框
bool patientCombo(const char* id, const Hospital& h, int* value) {
  bool changed = false;
  char cur[64];
  if (*value >= 0 && *value < h.patients.size())
    std::snprintf(cur, sizeof(cur), "%s %s", h.patients[*value].code, h.patients[*value].name);
  else
    std::snprintf(cur, sizeof(cur), "（请选择）");
  ImGui::SetNextItemWidth(comboWidth(cur));
  if (ImGui::BeginCombo(id, cur)) {
    for (int i = 0; i < h.patients.size(); ++i) {
      char label[64];
      std::snprintf(label, sizeof(label), "%s %s", h.patients[i].code, h.patients[i].name);
      if (ImGui::Selectable(label, *value == i)) {
        *value = i;
        changed = true;
      }
    }
    ImGui::EndCombo();
  }
  return changed;
}

// 医生下拉框
bool doctorCombo(const char* id, const Hospital& h, int* value) {
  bool changed = false;
  char cur[128];
  if (*value >= 0 && *value < h.doctors.size())
    std::snprintf(cur, sizeof(cur), "%s %s（%s · %s）", h.doctors[*value].code,
                  h.doctors[*value].name, h.doctors[*value].title,
                  deptName(h, h.doctors[*value].dept));
  else
    std::snprintf(cur, sizeof(cur), "（请选择）");
  ImGui::SetNextItemWidth(comboWidth(cur));
  if (ImGui::BeginCombo(id, cur)) {
    for (int i = 0; i < h.doctors.size(); ++i) {
      char label[128];
      std::snprintf(label, sizeof(label), "%s %s（%s · %s）", h.doctors[i].code, h.doctors[i].name,
                    h.doctors[i].title, deptName(h, h.doctors[i].dept));
      if (ImGui::Selectable(label, *value == i)) {
        *value = i;
        changed = true;
      }
    }
    ImGui::EndCombo();
  }
  return changed;
}

}  // namespace

// -----------------------------------------------------------------------------
//  候诊队列的四个动作（签到 / 叫号 / 过号 / 就诊完成）。
// -----------------------------------------------------------------------------
void applyCheckIn() {
  GuiState& st = state();
  Hospital& h = db();
  const int patient = st.queuePatient;
  const int doctor = st.queueDoctor;
  switch (Queue::checkIn(h, patient, doctor)) {
    case QUEUE_OK:
      st.notify(NOTICE_OK, "签到成功：%s 排在第 %d 位（前面还有 %d 人），诊室 %s。",
                h.patients[patient].name, Queue::positionOf(h, patient, doctor),
                Queue::waitingAhead(h, patient, doctor), roomName(h, h.doctors[doctor].room));
      std::printf("[GUI] 签到：%s -> %s %s，排队号 %d，前面 %d 人\n", h.patients[patient].code,
                  h.doctors[doctor].code, h.doctors[doctor].name,
                  Queue::queueNoOf(h, patient, doctor), Queue::waitingAhead(h, patient, doctor));
      break;
    case QUEUE_DUP:
      st.notify(NOTICE_WARN, "%s 已经在 %s 的候诊队列里了，当前位置第 %d 位。",
                h.patients[patient].name, h.doctors[doctor].name,
                Queue::positionOf(h, patient, doctor));
      std::printf("[GUI] 签到失败：%s 已在 %s 的队列里（第 %d 位）\n", h.patients[patient].code,
                  h.doctors[doctor].code, Queue::positionOf(h, patient, doctor));
      break;
    case QUEUE_FULL:
      st.notify(NOTICE_ERROR, "候诊队列已满（上限 %d 人），请稍后再签到。", kMaxWaiting);
      std::printf("[GUI] 签到失败：队列已满\n");
      break;
    default:
      st.notify(NOTICE_ERROR, "签到失败：患者或医生不存在。");
      std::printf("[GUI] 签到失败：患者或医生不存在\n");
      break;
  }
}

void applyCallNext() {
  GuiState& st = state();
  Hospital& h = db();
  const int doctor = st.queueDoctor;
  int patient = -1;
  switch (Queue::callNext(h, doctor, patient)) {
    case QUEUE_OK:
      st.lastCalledPatient = patient;
      st.lastCalledDoctor = doctor;
      st.notify(NOTICE_OK, "请 %s %s（排队号 %d）到 %s 就诊。", h.patients[patient].code,
                h.patients[patient].name, Queue::queueNoOf(h, patient, doctor),
                roomName(h, h.doctors[doctor].room));
      std::printf("[GUI] 叫号：%s %s -> 请 %s %s（排队号 %d）到 %s\n", h.doctors[doctor].code,
                  h.doctors[doctor].name, h.patients[patient].code, h.patients[patient].name,
                  Queue::queueNoOf(h, patient, doctor), roomName(h, h.doctors[doctor].room));
      break;
    case QUEUE_FIRST_CALLED:
      st.notify(NOTICE_WARN,
                "队首的患者还没处理完（在等他进诊室，或已过号一次），不能越过他叫后面的人。"
                "请等他就诊完，或先做一次过号处理。");
      std::printf("[GUI] 叫号被拒：队首已叫号未处理完（不能越队）\n");
      break;
    case QUEUE_EMPTY:
      st.notify(NOTICE_INFO, "该医生没有需要叫号的候诊患者了。");
      std::printf("[GUI] 叫号：该医生没有候诊患者\n");
      break;
    default:
      st.notify(NOTICE_ERROR, "叫号失败。");
      std::printf("[GUI] 叫号失败\n");
      break;
  }
}

void applyPass() {
  GuiState& st = state();
  Hospital& h = db();
  const int patient = st.queuePatient;
  const int doctor = st.queueDoctor;
  if (!Queue::isWaiting(h, patient, doctor)) {
    st.notify(NOTICE_ERROR, "%s 不在 %s 的候诊队列里。", h.patients[patient].name,
              h.doctors[doctor].name);
    std::printf("[GUI] 过号失败：该患者不在此医生的队列里\n");
    return;
  }
  const int oldNo = Queue::queueNoOf(h, patient, doctor);
  if (Queue::pass(h, patient, doctor)) {
    st.notify(NOTICE_WARN, "%s（原排队号 %d）已达过号上限 %d 次，重新排到队尾，新排队号 %d。",
              h.patients[patient].name, oldNo, kMaxPass, Queue::queueNoOf(h, patient, doctor));
    std::printf("[GUI] 过号：%s 达上限 %d 次，重新排队（原号 %d -> 新号 %d）\n",
                h.patients[patient].code, kMaxPass, oldNo, Queue::queueNoOf(h, patient, doctor));
  } else {
    st.notify(NOTICE_INFO, "%s 记过一次过号。再叫一次不到就会重新排队。",
              h.patients[patient].name);
    std::printf("[GUI] 过号：%s 记一次（原号 %d）\n", h.patients[patient].code, oldNo);
  }
}

void applyFinish() {
  GuiState& st = state();
  Hospital& h = db();
  const int patient = st.queuePatient;
  const int doctor = st.queueDoctor;
  if (Queue::finish(h, patient, doctor) == QUEUE_NOT_FOUND) {
    st.notify(NOTICE_ERROR, "%s 不在 %s 的候诊队列里。", h.patients[patient].name,
              h.doctors[doctor].name);
    std::printf("[GUI] 就诊完成失败：该患者不在此医生的队列里\n");
  } else {
    st.notify(NOTICE_OK, "%s 已完成就诊，离开候诊队列。该医生队列里还有 %d 人。",
              h.patients[patient].name, Queue::countOf(h, doctor));
    std::printf("[GUI] 就诊完成：%s，该医生队列还剩 %d 人\n", h.patients[patient].code,
                Queue::countOf(h, doctor));
  }
}

void drawQueue() {
  GuiState& st = state();
  Hospital& h = db();

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.72f, 0.95f, 1.0f));
  ImGui::TextUnformatted("功能 7  候诊队列");
  ImGui::PopStyleColor();
  ImGui::TextWrapped("队列用环形队列实现：入队出队 O(1)，出队腾出的位置被后面的患者循环复用。"
                     "全院共用一条 %d 位置的队列，靠医生下标区分队伍。",
                     kMaxWaiting);
  ImGui::Separator();

  if (st.queuePatient < 0 || st.queuePatient >= h.patients.size()) st.queuePatient = 0;
  int queueDoctorDefault = st.queueDoctor;
  if (queueDoctorDefault < 0 || queueDoctorDefault >= h.doctors.size()) queueDoctorDefault = 0;

  // ------------------------------ 操作区 ------------------------------
  // 两列等分：SizingFixedFit 会按内容算列宽，而按钮在列里会被这一列卡住裁掉
  // （「就诊完成，离开队列」就是这么被截成「就诊完成，离」的）。
  if (ImGui::BeginTable("queueOps", 2,
                        ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoBordersInBody)) {
    // ---- 患者签到 ----
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("患者：");
    ImGui::SameLine();
    patientCombo("##qp", h, &st.queuePatient);

    ImGui::TableSetColumnIndex(1);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("接诊医生：");
    ImGui::SameLine();
    if (doctorCombo("##qd", h, &st.queueDoctor)) st.lastCalledDoctor = st.queueDoctor;

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    if (ImGui::Button("患者签到排队", ImVec2(buttonWidth("患者签到排队"), 0.0f))) applyCheckIn();

    // ---- 医生叫号 ----
    ImGui::TableSetColumnIndex(1);
    if (ImGui::Button("叫下一位", ImVec2(buttonWidth("叫下一位"), 0.0f))) applyCallNext();

    // ---- 过号 ----
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    if (ImGui::Button("过号处理", ImVec2(buttonWidth("过号处理"), 0.0f))) applyPass();

    // ---- 就诊完成 ----
    ImGui::TableSetColumnIndex(1);
    if (ImGui::Button("就诊完成，离开队列", ImVec2(buttonWidth("就诊完成，离开队列"), 0.0f))) applyFinish();

    ImGui::EndTable();
  }

  // 如果这位患者有预约，顺手提示几句，省得他再查一遍
  for (int i = 0; i < h.bookings.size(); ++i) {
    const Booking& b = h.bookings[i];
    if (b.patient != st.queuePatient || b.status != BOOKED) continue;
    char t[32];
    timeText(b.day, b.slot, t, sizeof(t));
    ImGui::TextWrapped("提示：%s 在 %s %s 有一条预约（凭证号 %s），就诊时间 %s。",
                       h.patients[st.queuePatient].name, deptName(h, h.doctors[b.doctor].dept),
                       h.doctors[b.doctor].name, b.ticket, t);
    break;
  }

  if (st.lastCalledPatient >= 0)
    ImGui::TextDisabled("最近一次叫号：%s（%s 的队列）", h.patients[st.lastCalledPatient].name,
                        h.doctors[st.lastCalledDoctor].name);

  // ------------------------------ 队列内容 ------------------------------
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  drawDoctorQueue(st.queueDoctor);

  // ------------------------------ 全院汇总 ------------------------------
  ImGui::Spacing();
  ImGui::SeparatorText("全院候诊情况");
  if (ImGui::BeginTable("allDeptQueue", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("科室", ImGuiTableColumnFlags_WidthFixed, 140.0f);
    ImGui::TableSetupColumn("出诊医生数", ImGuiTableColumnFlags_WidthFixed, 110.0f);
    ImGui::TableSetupColumn("候诊人数", ImGuiTableColumnFlags_WidthFixed, 100.0f);
    ImGui::TableSetupColumn("队伍明细", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
    int total = 0;
    for (int d = 0; d < h.depts.size(); ++d) {
      int docs = 0, waiting = 0;
      char detail[256];
      detail[0] = '\0';
      for (int i = 0; i < h.doctors.size(); ++i) {
        if (h.doctors[i].dept != d) continue;
        ++docs;
        const int c = Queue::countOf(h, i);
        waiting += c;
        if (c > 0) {
          char one[64];
          std::snprintf(one, sizeof(one), "%s%s×%d", detail[0] ? "、" : "", h.doctors[i].name, c);
          std::strncat(detail, one, sizeof(detail) - std::strlen(detail) - 1);
        }
      }
      if (waiting == 0) continue;
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(h.depts[d].name);
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%d", docs);
      ImGui::TableSetColumnIndex(2);
      ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.35f, 1.0f), "%d", waiting);
      ImGui::TableSetColumnIndex(3);
      ImGui::TextUnformatted(detail);
      total += waiting;
    }
    ImGui::EndTable();
    if (total == 0)
      ImGui::TextDisabled("全院当前没有患者候诊。");
    else
      ImGui::Text("全院共 %d 人候诊。", total);
  }
}

}  // namespace gui
