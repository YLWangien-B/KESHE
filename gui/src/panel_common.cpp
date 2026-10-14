/* =============================================================================
 *  panel_common.cpp  —  几个页面共用的绘图小工具
 *
 *  号源格子（slotCell）在「在线预约」和「排班展示」两个页面都要用，
 *  所以放到这里，不重复写两遍。
 * ========================================================================== */
#include "panels.h"

#include <cstdio>

#include "imgui.h"
#include "gui_common.h"

namespace gui {

namespace {

// 号源状态 -> 格子底色。用「绿-黄-红-灰」几档，一眼能看出哪里还能挂上号。
ImVec4 slotColor(SlotState st) {
  switch (st) {
    case SLOT_OK:      return ImVec4(0.22f, 0.48f, 0.30f, 1.0f);  // 号源充足
    case SLOT_TIGHT:   return ImVec4(0.55f, 0.45f, 0.14f, 1.0f);  // 号不多了
    case SLOT_FULL:    return ImVec4(0.55f, 0.20f, 0.20f, 1.0f);  // 已满
    case SLOT_STOPPED: return ImVec4(0.38f, 0.24f, 0.38f, 1.0f);  // 停诊
    default:           return ImVec4(0.20f, 0.21f, 0.24f, 1.0f);  // 不出诊
  }
}

// 把格子里的文字准备出来
void cellText(Hospital& h, int doctor, int day, int slot, SlotState st, char* buf, int cap) {
  switch (st) {
    case SLOT_NONE:    std::snprintf(buf, cap, "%s", "不出诊"); break;
    case SLOT_STOPPED: std::snprintf(buf, cap, "%s", "停诊"); break;
    case SLOT_FULL:    std::snprintf(buf, cap, "%s", "已满"); break;
    default:
      std::snprintf(buf, cap, "余 %d/%d", h.remaining(doctor, day, slot),
                    h.slot(doctor, day, slot).quota);
      break;
  }
}

}  // namespace

bool slotCell(const char* id, int doctor, int day, int slot, float width) {
  Hospital& h = db();
  const SlotState st = slotState(h, doctor, day, slot);

  char text[40];
  cellText(h, doctor, day, slot, st, text, sizeof(text));

  const float height = ImGui::GetFrameHeight() + 6.0f;
  ImGui::PushID(id);
  ImGui::InvisibleButton("##cell", ImVec2(width, height));
  const bool clicked = ImGui::IsItemClicked();
  const bool hovered = ImGui::IsItemHovered();

  const ImVec2 min = ImGui::GetItemRectMin();
  const ImVec2 max = ImGui::GetItemRectMax();
  ImVec4 bg = slotColor(st);
  if (hovered && st != SLOT_NONE) bg = ImVec4(bg.x + 0.10f, bg.y + 0.10f, bg.z + 0.10f, 1.0f);

  ImGui::GetWindowDrawList()->AddRectFilled(min, max, ImGui::GetColorU32(bg), 3.0f);
  const ImVec2 ts = ImGui::CalcTextSize(text);
  ImGui::GetWindowDrawList()->AddText(
      ImVec2(min.x + (width - ts.x) * 0.5f, min.y + (height - ts.y) * 0.5f),
      ImGui::GetColorU32(ImVec4(0.95f, 0.96f, 0.97f, 1.0f)), text);

  if (hovered) {
    ImGui::BeginTooltip();
    ImGui::Text("%s %s", dayText(day), slotText(slot));
    ImGui::Text("状态：%s", slotStateText(h, doctor, day, slot));
    if (st == SLOT_OK || st == SLOT_TIGHT)
      ImGui::Text("剩余号源：%d / %d", h.remaining(doctor, day, slot),
                  h.slot(doctor, day, slot).quota);
    ImGui::EndTooltip();
  }
  ImGui::PopID();
  return clicked && st != SLOT_NONE;
}

// 在图例里说明几种颜色代表什么，界面下面统一画一条
void drawSlotLegend() {
  struct Item {
    SlotState st;
    const char* text;
  };
  static const Item kItems[] = {
      {SLOT_OK, "号源充足"},  {SLOT_TIGHT, "号源紧张"}, {SLOT_FULL, "号已满"},
      {SLOT_STOPPED, "已停诊"}, {SLOT_NONE, "不出诊"},
  };
  for (int i = 0; i < static_cast<int>(sizeof(kItems) / sizeof(kItems[0])); ++i) {
    if (i) ImGui::SameLine();
    ImGui::ColorButton("##lg", slotColor(kItems[i].st), ImGuiColorEditFlags_NoTooltip,
                       ImVec2(14.0f, 14.0f));
    ImGui::SameLine();
    ImGui::TextUnformatted(kItems[i].text);
  }
}

}  // namespace gui
