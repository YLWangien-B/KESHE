/* =============================================================================
 *  panel_floor.cpp  —  附加页：楼层平面图（题目要求「以图形化方式展示科室分布」）
 *
 *  这张图不是随便画的，它直接对应 WeightedGraph 里的路网：
 *      · 一行 = 一层楼，房间按路网节点号从小到大排开
 *      · 同一个科室的房间用同一种颜色，于是「科室分布」一眼可见
 *      · 就诊路线查询的结果用亮色描出来，走廊上的上楼下楼也画出来
 *
 *  数据来源全部是 rooms 表与 road 图，一个像素都不是硬编码的。
 * ========================================================================== */
#include <cstdio>
#include <cstring>

#include "imgui.h"
#include "gui_common.h"
#include "gui_state.h"
#include "panels.h"

namespace gui {

namespace {

// 12 个科室配 12 种颜色，保证同科室同色、相邻科室不同色
const ImU32 kDeptColors[] = {
    IM_COL32(0x3E, 0x6B, 0xA8, 0xFF),  // 心血管内科  蓝
    IM_COL32(0x3F, 0x8F, 0x6B, 0xFF),  // 呼吸内科    绿
    IM_COL32(0xA8, 0x7A, 0x3E, 0xFF),  // 消化内科    棕黄
    IM_COL32(0x8A, 0x5B, 0xA8, 0xFF),  // 神经内科    紫
    IM_COL32(0xA8, 0x4E, 0x5B, 0xFF),  // 普通外科    砖红
    IM_COL32(0x3E, 0x8E, 0xA8, 0xFF),  // 骨科        青
    IM_COL32(0x6B, 0x7A, 0x3E, 0xFF),  // 泌尿外科    橄榄
    IM_COL32(0xA8, 0x3E, 0x8A, 0xFF),  // 皮肤科      品红
    IM_COL32(0x4E, 0x6B, 0x8A, 0xFF),  // 眼科        灰蓝
    IM_COL32(0x8A, 0x6B, 0x4E, 0xFF),  // 耳鼻喉科    灰棕
    IM_COL32(0x5B, 0xA8, 0x4E, 0xFF),  // 儿科        亮绿
    IM_COL32(0xA8, 0x8A, 0x3E, 0xFF),  // 中医科      土黄
};
constexpr int kDeptColorCount = static_cast<int>(sizeof(kDeptColors) / sizeof(kDeptColors[0]));

ImU32 roomColor(const Room& r) {
  if (r.dept < 0) {
    // 公共设施：电梯口画成亮灰，楼梯口画成暗灰
    if (std::strstr(r.name, "电梯")) return IM_COL32(0x70, 0x74, 0x7A, 0xFF);
    return IM_COL32(0x4A, 0x4E, 0x54, 0xFF);
  }
  return kDeptColors[r.dept % kDeptColorCount];
}

// 该房间是否在本次查询出的路线上
bool onRoute(const GuiState& st, int room) {
  for (int i = 0; i < st.routeHighlightCount; ++i)
    if (st.routeHighlight[i] == room) return true;
  return false;
}

// 在 [x, x + maxW) 里画一行居中的文字；文字太宽就按比例缩小字号，保证不越出方框。
// 缩到 minSize 还是放不下就不再缩了，直接按最小字号画（宁可略挤，也不要冲出方框）。
void addFittedText(ImDrawList* dl, const char* text, float x, float y, float maxW, float size,
                   float minSize, ImU32 color) {
  const float w0 = ImGui::CalcTextSize(text).x;
  float size2 = size;
  if (w0 > maxW && w0 > 0.0f) {
    size2 = size * (maxW / w0);
    if (size2 < minSize) size2 = minSize;
  }
  const ImVec2 s2 = ImGui::GetFont()->CalcTextSizeA(size2, FLT_MAX, 0.0f, text);
  dl->AddText(ImGui::GetFont(), size2, ImVec2(x + (maxW - s2.x) * 0.5f, y), color, text);
}

// 画一层楼：走廊 + 房间方块。返回这一层占用的高度。
float drawOneFloor(Hospital& h, int floor) {
  GuiState& st = state();

  // 把这一层的房间按路网节点号排序 —— 节点号就是房间在走廊上的位置
  int idx[kMaxRoom];
  int n = 0;
  for (int i = 0; i < h.rooms.size() && n < kMaxRoom; ++i)
    if (h.rooms[i].floor == floor) idx[n++] = i;
  for (int a = 0; a < n; ++a)  // 房间数很少，插入排序足够
    for (int b = a + 1; b < n; ++b)
      if (h.rooms[idx[b]].node < h.rooms[idx[a]].node) {
        const int t = idx[a];
        idx[a] = idx[b];
        idx[b] = t;
      }
  if (n == 0) return 0.0f;

  const float boxW = 104.0f;
  const float boxH = 50.0f;
  const float gap = 8.0f;
  const float leftPad = 100.0f;
  const float needW = leftPad + n * (boxW + gap) + 12.0f;

  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const float rowH = boxH + 50.0f;

  // 楼层名
  dl->AddText(ImVec2(origin.x + 6.0f, origin.y + boxH * 0.5f - 8.0f),
              IM_COL32(0xC8, 0xCE, 0xD6, 0xFF), floorText(floor));

  // 走廊：一条横线，连接本层全部房间
  const float corridorY = origin.y + boxH + 16.0f;
  dl->AddLine(ImVec2(origin.x + leftPad, corridorY),
              ImVec2(origin.x + leftPad + (n - 1) * (boxW + gap) + boxW, corridorY),
              IM_COL32(0x5A, 0x5F, 0x66, 0xFF), 2.0f);

  for (int k = 0; k < n; ++k) {
    const Room& r = h.rooms[idx[k]];
    const float x = origin.x + leftPad + k * (boxW + gap);
    const float y = origin.y;
    const ImVec2 a(x, y);
    const ImVec2 b(x + boxW, y + boxH);

    const bool isFrom = (st.floorHighlightFrom == idx[k]);
    const bool isTo = (st.floorHighlightTo == idx[k]);
    const bool highlighted = isFrom || isTo || onRoute(st, idx[k]);

    ImU32 fill = roomColor(r);
    if (highlighted) fill = IM_COL32(0xE0, 0xA8, 0x30, 0xFF);
    dl->AddRectFilled(a, b, fill, 4.0f);
    dl->AddRect(a, b, highlighted ? IM_COL32(0xFF, 0xE0, 0x80, 0xFF) : IM_COL32(0x22, 0x24, 0x28, 0xFF),
                4.0f, 0, highlighted ? 2.0f : 1.0f);

    // 方块里的字：房间号 + （可选）简短名称。
    // 字号按框宽自动缩：界面字体 27px 时「门诊楼1层」要 5×27=135px 宽，
    // 而框只有 92px，不缩就会冲出方框压到走廊上。
    char line1[24];
    if (st.floorShowCode)
      std::snprintf(line1, sizeof(line1), "%s", r.code);
    else
      std::snprintf(line1, sizeof(line1), "%s", r.dept >= 0 ? deptName(h, r.dept) : "公共设施");
    addFittedText(dl, line1, x, y + 6.0f, boxW, ImGui::GetFontSize(), 11.0f,
                  IM_COL32(0xF2, 0xF4, 0xF6, 0xFF));

    // 第二行：电梯口/楼梯口 或 科室名
    const char* sub = nullptr;
    if (r.dept < 0)
      sub = std::strstr(r.name, "电梯") ? "电梯口" : "楼梯口";
    else
      sub = deptName(h, r.dept);
    addFittedText(dl, sub, x, y + boxH - 17.0f, boxW, ImGui::GetFontSize() - 3.0f, 9.0f,
                  IM_COL32(0xE6, 0xE9, 0xEC, 0xD0));

    // 起点/终点标记
    if (isFrom || isTo) {
      const char* tag = isFrom ? "起" : "终";
      dl->AddRectFilled(ImVec2(x + boxW - 20.0f, y - 2.0f), ImVec2(x + boxW, y + 16.0f),
                        isFrom ? IM_COL32(0x4E, 0xA8, 0x5B, 0xFF) : IM_COL32(0xC0, 0x50, 0x50, 0xFF), 3.0f);
      dl->AddText(ImVec2(x + boxW - 16.0f, y), IM_COL32(0xFF, 0xFF, 0xFF, 0xFF), tag);
    }

    // 走廊竖线（房间接进走廊）
    dl->AddLine(ImVec2(x + boxW * 0.5f, y + boxH), ImVec2(x + boxW * 0.5f, corridorY),
                IM_COL32(0x5A, 0x5F, 0x66, 0xFF), 1.0f);

    // 点击区域
    ImGui::PushID(2000 + idx[k]);
    ImGui::SetCursorScreenPos(a);
    ImGui::InvisibleButton("room", ImVec2(boxW, boxH));
    if (ImGui::IsItemClicked()) {
      if (ImGui::GetIO().KeyShift || st.floorHighlightTo < 0) {
        st.floorHighlightTo = idx[k];
        st.routeTo = idx[k];
      } else {
        st.floorHighlightFrom = idx[k];
        st.routeFrom = idx[k];
      }
      st.notify(NOTICE_INFO, "已选 %s（%s）。按住 Shift 点击 = 设为终点，直接点击 = 设为起点。",
                r.code, r.name);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::Text("%s  %s", r.code, r.name);
      ImGui::Text("楼层：%s   路网节点号：%d", floorText(r.floor), r.node);
      ImGui::Text("所属科室：%s", r.dept >= 0 ? deptName(h, r.dept) : "公共设施");
      if (onRoute(st, idx[k])) ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "在本次查询的路线上");
      ImGui::EndTooltip();
    }
    ImGui::PopID();
  }

  // 跨层通行的示意线：本层电梯口/楼梯口与其它层同位置的连线由右侧标注说明
  ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + rowH));
  ImGui::Dummy(ImVec2(needW, 0.0f));
  return rowH;
}

}  // namespace

void drawFloorPlan() {
  GuiState& st = state();
  Hospital& h = db();

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.72f, 0.95f, 1.0f));
  ImGui::TextUnformatted("附加页  楼层平面图");
  ImGui::PopStyleColor();
  ImGui::TextWrapped("房间按路网节点号在走廊上排开；同科室房间同色。这张图就是 WeightedGraph 里的路网，"
                      "不是手画的示意图。");
  ImGui::Separator();

  ImGui::Checkbox("显示房间号（不勾则显示科室名）", &st.floorShowCode);
  ImGui::SameLine();
  if (ImGui::Button("清除高亮")) {
    st.floorHighlightFrom = -1;
    st.floorHighlightTo = -1;
    st.routeHighlightCount = 0;
    st.clearNotice();
  }
  ImGui::SameLine();
  ImGui::TextDisabled("直接点击房间 = 设为起点，按住 Shift 点击 = 设为终点；也可在「就诊路线」页选择。");

  ImGui::Spacing();
  ImGui::Separator();

  // 图的宽度：4 层 × 12 间，横向滚动
  if (ImGui::BeginChild("floorCanvas", ImVec2(0.0f, -120.0f), ImGuiChildFlags_Borders,
                        ImGuiWindowFlags_HorizontalScrollbar)) {
    for (int f = 0; f < kMaxFloor; ++f) drawOneFloor(h, f);
  }
  ImGui::EndChild();

  // ------------------------------ 图例 ------------------------------
  ImGui::Spacing();
  ImGui::TextUnformatted("科室配色：");
  for (int d = 0; d < h.depts.size(); ++d) {
    if (d % 6 != 0) ImGui::SameLine(); else ImGui::Spacing();
    ImGui::ColorButton("##deptlegend", ImGui::ColorConvertU32ToFloat4(kDeptColors[d % kDeptColorCount]),
                       ImGuiColorEditFlags_NoTooltip, ImVec2(14.0f, 14.0f));
    ImGui::SameLine();
    ImGui::TextUnformatted(h.depts[d].name);
  }
  ImGui::Spacing();
  ImGui::ColorButton("##lg1", ImGui::ColorConvertU32ToFloat4(IM_COL32(0x70, 0x74, 0x7A, 0xFF)),
                     ImGuiColorEditFlags_NoTooltip, ImVec2(14.0f, 14.0f));
  ImGui::SameLine();
  ImGui::TextUnformatted("电梯口");
  ImGui::SameLine();
  ImGui::ColorButton("##lg2", ImGui::ColorConvertU32ToFloat4(IM_COL32(0x4A, 0x4E, 0x54, 0xFF)),
                     ImGuiColorEditFlags_NoTooltip, ImVec2(14.0f, 14.0f));
  ImGui::SameLine();
  ImGui::TextUnformatted("楼梯口");
  ImGui::SameLine();
  ImGui::ColorButton("##lg3", ImGui::ColorConvertU32ToFloat4(IM_COL32(0xE0, 0xA8, 0x30, 0xFF)),
                     ImGuiColorEditFlags_NoTooltip, ImVec2(14.0f, 14.0f));
  ImGui::SameLine();
  ImGui::TextUnformatted("起点 / 终点 / 本次路线上的房间");

  // ------------------------------ 跨层连接说明 ------------------------------
  ImGui::Spacing();
  ImGui::SeparatorText("跨层怎么走（写进路网里的四条规律）");
  ImGui::TextUnformatted("  1. 同层相邻诊室之间：10 米（沿走廊走一间房的宽度）");
  ImGui::TextUnformatted("  2. 本层电梯口 与 本层楼梯口：30 米（走廊全长）");
  ImGui::TextUnformatted("  3. 电梯口 与 其他各层电梯口：20 米（等电梯 + 乘梯的等效距离）");
  ImGui::TextUnformatted("  4. 楼梯口 与 相邻层楼梯口：20 米（上下楼一层的等效距离）");
  ImGui::TextDisabled("这些距离都写在 data\\layout.txt 里，改数据就能改路线结果，不用改代码。");
}

}  // namespace gui
