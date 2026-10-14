/* =============================================================================
 *  panel_route.cpp  —  功能 6：就诊路线（含楼层平面图）
 *
 *  这两件事本来就是一体的，所以放在一页里：
 *      · 路线明细：选起点终点 -> 查询 -> 途经节点、总距离、换层指引
 *      · 楼层平面图：把这条路线画出来（经过的房间描成高亮色）
 *      · 双向联动：在平面图上点房间就直接设成起点/终点（Shift+点击 = 终点）
 *
 *  地图不是随便画的，它直接对应 WeightedGraph 里的路网：
 *      · 一行 = 一层楼，房间按路网节点号从小到大排开
 *      · 同一个科室的房间同色，于是「科室分布」一眼可见（题目要求图形化展示）
 *      · 数据来源全部是 rooms 表与 road 图，一个坐标都不是硬编码的
 *
 *  算法完全复用 service.h 的 Route::between()：它返回 RouteResult，
 *  里面是「途经的房间下标序列 + 总距离 + 换层次数」。
 *
 *  ---------------------------------------------------------------------------
 *  布局：默认「地图在上、明细在下」，并且可以切成左右并排
 *
 *  一开始是上下排的，两块的固定高度加起来超出了逻辑视口（150% DPI 下只有 1013
 *  像素高），结果地图最下面那层楼被裁掉了 —— 只有截图才看得出来。
 *  现在两块的高度都收紧到刚好放得下四层楼，并留一个开关：
 *    · 不勾「并排」：地图占满宽度，48 个房间一屏看完（默认）
 *    · 勾上「并排」：明细在左、地图在右，适合窗口拉得很宽的时候
 *  ---------------------------------------------------------------------------
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

// ------------------------------ 房间下拉框 ------------------------------
// 按楼层分组显示，避免 48 个房间堆成一条长列表
bool roomCombo(const char* id, const Hospital& h, int* value) {
  bool changed = false;
  char cur[128];
  if (*value >= 0 && *value < h.rooms.size())
    std::snprintf(cur, sizeof(cur), "%s  %s", h.rooms[*value].code, h.rooms[*value].name);
  else
    std::snprintf(cur, sizeof(cur), "（请选择房间）");

  ImGui::SetNextItemWidth(comboWidth(cur));
  if (ImGui::BeginCombo(id, cur)) {
    for (int f = 0; f < kMaxFloor; ++f) {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.72f, 0.95f, 1.0f));
      ImGui::TextUnformatted(floorText(f));
      ImGui::PopStyleColor();
      for (int i = 0; i < h.rooms.size(); ++i) {
        if (h.rooms[i].floor != f) continue;
        char label[160];
        std::snprintf(label, sizeof(label), "    %s  %s", h.rooms[i].code, h.rooms[i].name);
        if (ImGui::Selectable(label, *value == i)) {
          *value = i;
          changed = true;
        }
      }
      ImGui::Separator();
    }
    ImGui::EndCombo();
  }
  return changed;
}

// -----------------------------------------------------------------------------
//  楼层平面图的绘制
// -----------------------------------------------------------------------------

// 12 个科室配 12 种颜色，保证同科室同色
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

// 一层楼占的高度：方块 42 + 走廊 34。四层楼 304 像素。
constexpr float kBoxH = 42.0f;
constexpr float kRowH = kBoxH + 34.0f;

ImU32 roomColor(const Room& r) {
  if (r.dept < 0) {
    // 公共设施：电梯口亮灰，楼梯口暗灰
    if (std::strstr(r.name, "电梯")) return IM_COL32(0x70, 0x74, 0x7A, 0xFF);
    return IM_COL32(0x4A, 0x4E, 0x54, 0xFF);
  }
  return kDeptColors[r.dept % kDeptColorCount];
}

// 该房间是否在本次查询出的路线上。
// 直接查 RouteResult 的 steps，不再另外维护一份高亮列表 —— 两份数据容易对不上。
bool onRoute(const GuiState& st, int room) {
  if (!st.routeValid || !st.route.found) return false;
  for (int i = 0; i < st.route.stepCount; ++i)
    if (st.route.steps[i] == room) return true;
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

// 画一层楼：走廊 + 房间方块
void drawOneFloor(Hospital& h, int floor) {
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
  if (n == 0) return;

  const float boxW = 104.0f;
  const float gap = 8.0f;
  const float leftPad = 100.0f;
  const float needW = leftPad + n * (boxW + gap) + 12.0f;

  ImDrawList* dl = ImGui::GetWindowDrawList();
  const ImVec2 origin = ImGui::GetCursorScreenPos();

  // 楼层名
  dl->AddText(ImVec2(origin.x + 6.0f, origin.y + kBoxH * 0.5f - 8.0f),
              IM_COL32(0xC8, 0xCE, 0xD6, 0xFF), floorText(floor));

  // 走廊：一条横线，连接本层全部房间
  const float corridorY = origin.y + kBoxH + 12.0f;
  dl->AddLine(ImVec2(origin.x + leftPad, corridorY),
              ImVec2(origin.x + leftPad + (n - 1) * (boxW + gap) + boxW, corridorY),
              IM_COL32(0x5A, 0x5F, 0x66, 0xFF), 2.0f);

  for (int k = 0; k < n; ++k) {
    const Room& r = h.rooms[idx[k]];
    const float x = origin.x + leftPad + k * (boxW + gap);
    const float y = origin.y;
    const ImVec2 a(x, y);
    const ImVec2 b(x + boxW, y + kBoxH);

    const bool isFrom = (st.routeFrom == idx[k]);
    const bool isTo = (st.routeTo == idx[k]);
    const bool highlighted = isFrom || isTo || onRoute(st, idx[k]);

    ImU32 fill = roomColor(r);
    if (highlighted) fill = IM_COL32(0xE0, 0xA8, 0x30, 0xFF);
    dl->AddRectFilled(a, b, fill, 4.0f);
    dl->AddRect(a, b,
                highlighted ? IM_COL32(0xFF, 0xE0, 0x80, 0xFF) : IM_COL32(0x22, 0x24, 0x28, 0xFF),
                4.0f, 0, highlighted ? 2.0f : 1.0f);

    // 第一行：房间号（或科室名）
    // 字号压到 0.78 倍：全尺寸汉字约 27px 高，两行加起来会撑出 42px 的方块。
    // 房间号里还可能带上标（如 RA100³），行高更高，所以这里留足余量。
    char line1[24];
    if (st.floorShowCode)
      std::snprintf(line1, sizeof(line1), "%s", r.code);
    else
      std::snprintf(line1, sizeof(line1), "%s", r.dept >= 0 ? deptName(h, r.dept) : "公共设施");
    const float mainSize = ImGui::GetFontSize() * 0.78f;
    addFittedText(dl, line1, x, y + 2.0f, boxW, mainSize, 10.0f,
                  IM_COL32(0xF2, 0xF4, 0xF6, 0xFF));

    // 第二行：电梯口/楼梯口 或 科室名
    const char* sub = nullptr;
    if (r.dept < 0)
      sub = std::strstr(r.name, "电梯") ? "电梯口" : "楼梯口";
    else
      sub = deptName(h, r.dept);
    addFittedText(dl, sub, x, y + kBoxH - 14.0f, boxW, mainSize * 0.82f, 9.0f,
                  IM_COL32(0xE6, 0xE9, 0xEC, 0xD0));

    // 起点/终点标记
    if (isFrom || isTo) {
      const char* tag = isFrom ? "起" : "终";
      dl->AddRectFilled(ImVec2(x + boxW - 20.0f, y - 2.0f), ImVec2(x + boxW, y + 16.0f),
                        isFrom ? IM_COL32(0x4E, 0xA8, 0x5B, 0xFF) : IM_COL32(0xC0, 0x50, 0x50, 0xFF),
                        3.0f);
      dl->AddText(ImVec2(x + boxW - 16.0f, y), IM_COL32(0xFF, 0xFF, 0xFF, 0xFF), tag);
    }

    // 房间接进走廊的竖线
    dl->AddLine(ImVec2(x + boxW * 0.5f, y + kBoxH), ImVec2(x + boxW * 0.5f, corridorY),
                IM_COL32(0x5A, 0x5F, 0x66, 0xFF), 1.0f);

    // 点击区域：直接改起终点（与上面的两个下拉框是同一份状态，所以双向联动）
    ImGui::PushID(2000 + idx[k]);
    ImGui::SetCursorScreenPos(a);
    ImGui::InvisibleButton("room", ImVec2(boxW, kBoxH));
    if (ImGui::IsItemClicked()) {
      if (ImGui::GetIO().KeyShift) {
        st.routeTo = idx[k];
        st.notify(NOTICE_INFO, "终点已设为 %s（%s）", r.code, r.name);
      } else {
        st.routeFrom = idx[k];
        st.notify(NOTICE_INFO, "起点已设为 %s（%s）", r.code, r.name);
      }
      st.routeValid = false;  // 起终点变了，旧结果作废
    }
    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::Text("%s  %s", r.code, r.name);
      ImGui::Text("楼层：%s   路网节点号：%d", floorText(r.floor), r.node);
      ImGui::Text("所属科室：%s", r.dept >= 0 ? deptName(h, r.dept) : "公共设施");
      if (onRoute(st, idx[k])) ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "在本次查询的路线上");
      ImGui::Separator();
      ImGui::TextDisabled("直接点击 = 设为起点    Shift+点击 = 设为终点");
      ImGui::EndTooltip();
    }
    ImGui::PopID();
  }

  ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + kRowH));
  ImGui::Dummy(ImVec2(needW, 0.0f));
}

// 科室配色图例。
// 关键：所有条目必须待在同一行 —— 一旦换行，行数就随面板宽度变化，
// 窄栏里图例会把下面板的高度（实测踩过）。所以先量总宽：够就单行排，
// 不够就整块降到「只给颜色，悬停看名字」的紧凑版。
void drawDeptLegend(const Hospital& h) {
  const float avail = ImGui::GetContentRegionAvail().x;
  float need = ImGui::CalcTextSize("科室配色：").x;
  for (int d = 0; d < h.depts.size(); ++d)
    need += 14.0f + ImGui::CalcTextSize(h.depts[d].name).x + 26.0f;
  need += 14.0f + ImGui::CalcTextSize("电梯口").x + 26.0f;
  need += 14.0f + ImGui::CalcTextSize("楼梯口").x + 26.0f;
  need += 14.0f + ImGui::CalcTextSize("起终点/路线").x + 26.0f;

  if (need <= avail) {
    ImGui::TextUnformatted("科室配色：");
    for (int d = 0; d < h.depts.size(); ++d) {
      ImGui::SameLine();
      ImGui::ColorButton("##deptlegend",
                         ImGui::ColorConvertU32ToFloat4(kDeptColors[d % kDeptColorCount]),
                         ImGuiColorEditFlags_NoTooltip, ImVec2(14.0f, 14.0f));
      ImGui::SameLine();
      ImGui::TextUnformatted(h.depts[d].name);
    }
    ImGui::SameLine();
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
    ImGui::TextUnformatted("起终点/路线");
    return;
  }

  ImGui::TextUnformatted("配色：");
  for (int d = 0; d < h.depts.size(); ++d) {
    ImGui::SameLine();
    ImGui::ColorButton("##deptlegend",
                       ImGui::ColorConvertU32ToFloat4(kDeptColors[d % kDeptColorCount]),
                       ImGuiColorEditFlags_NoTooltip, ImVec2(14.0f, 14.0f));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", h.depts[d].name);
  }
  ImGui::SameLine();
  ImGui::ColorButton("##lg1", ImGui::ColorConvertU32ToFloat4(IM_COL32(0x70, 0x74, 0x7A, 0xFF)),
                     ImGuiColorEditFlags_NoTooltip, ImVec2(14.0f, 14.0f));
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("电梯口");
  ImGui::SameLine();
  ImGui::ColorButton("##lg2", ImGui::ColorConvertU32ToFloat4(IM_COL32(0x4A, 0x4E, 0x54, 0xFF)),
                     ImGuiColorEditFlags_NoTooltip, ImVec2(14.0f, 14.0f));
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("楼梯口");
  ImGui::SameLine();
  ImGui::ColorButton("##lg3", ImGui::ColorConvertU32ToFloat4(IM_COL32(0xE0, 0xA8, 0x30, 0xFF)),
                     ImGuiColorEditFlags_NoTooltip, ImVec2(14.0f, 14.0f));
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("起点 / 终点 / 本次路线上的房间");
  ImGui::SameLine();
  ImGui::TextDisabled("（悬停看科室名）");
}

// -----------------------------------------------------------------------------
//  两个可复用的区块：明细 与 地图
// -----------------------------------------------------------------------------

// 路线明细：一行摘要 + 途经节点表 + 换层指引。tableH 是节点表的高度。
void drawRouteDetail(Hospital& h, float tableH) {
  GuiState& st = state();

  if (!st.routeValid) {
    ImGui::TextDisabled("选好起点和终点后点「查询路线」，或直接在平面图上点两个房间。");
    return;
  }
  if (!st.route.found) {
    ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.45f, 1.0f), "两点之间不连通。");
    return;
  }

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.85f, 0.62f, 1.0f));
  ImGui::Text("全程约 %d 米，途经 %d 个节点，换层 %d 次", st.route.totalDistance,
              st.route.stepCount, st.route.floorChanges);
  ImGui::PopStyleColor();

  const int startNode = h.rooms[st.route.steps[0]].node;
  if (ImGui::BeginTable("routeSteps", 5,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY,
                        ImVec2(0.0f, tableH))) {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 38.0f);
    ImGui::TableSetupColumn("节点", ImGuiTableColumnFlags_WidthFixed, 62.0f);
    ImGui::TableSetupColumn("房间", ImGuiTableColumnFlags_WidthFixed, 300.0f);
    ImGui::TableSetupColumn("楼层", ImGuiTableColumnFlags_WidthFixed, 126.0f);
    ImGui::TableSetupColumn("累计距离", ImGuiTableColumnFlags_WidthFixed, 110.0f);
    ImGui::TableHeadersRow();

    for (int i = 0; i < st.route.stepCount; ++i) {
      const Room& r = h.rooms[st.route.steps[i]];
      const bool isEnd = (i == 0 || i == st.route.stepCount - 1);
      // 累计距离直接查最短路表：Floyd 已经算好任意两点间的距离，这里是 O(1)
      const int acc = h.road.distance(startNode, r.node);

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      if (isEnd)
        ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.35f, 1.0f), "%s", i == 0 ? "起" : "终");
      else
        ImGui::TextDisabled("·");

      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%d", r.node);

      ImGui::TableSetColumnIndex(2);
      if (isEnd)
        ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.35f, 1.0f), "%s", r.name);
      else
        ImGui::TextUnformatted(r.name);

      ImGui::TableSetColumnIndex(3);
      ImGui::TextUnformatted(floorText(r.floor));

      ImGui::TableSetColumnIndex(4);
      ImGui::Text("%d 米", acc);
    }
    ImGui::EndTable();
  }

  if (st.route.floorChanges == 0) {
    ImGui::TextDisabled("换层指引：全程在同一层，无需上下楼。");
    return;
  }
  ImGui::TextUnformatted("换层指引：");
  for (int i = 1; i < st.route.stepCount; ++i) {
    const Room& prev = h.rooms[st.route.steps[i - 1]];
    const Room& cur = h.rooms[st.route.steps[i]];
    if (prev.floor == cur.floor) continue;
    // 换层发生在通道处，按房间名里有没有「电梯」判断用的是电梯还是楼梯
    const bool byLift = (std::strstr(prev.name, "电梯") != nullptr) ||
                        (std::strstr(cur.name, "电梯") != nullptr);
    ImGui::BulletText("在「%s」%s，到 %d 层的「%s」", prev.name, byLift ? "乘电梯" : "走楼梯",
                      cur.floor + 1, cur.name);
  }
}

// 楼层平面图。canvasH 是地图画布高度；tip 为真时在下面多画一句跨层规则说明。
void drawFloorSection(Hospital& h, float canvasH, bool tip) {  GuiState& st = state();

  ImGui::Checkbox("显示房间号", &st.floorShowCode);
  ImGui::SameLine();
  ImGui::TextDisabled("点房间 = 起点，Shift+点 = 终点");

  if (ImGui::BeginChild("floorCanvas", ImVec2(0.0f, canvasH), ImGuiChildFlags_Borders,
                        ImGuiWindowFlags_HorizontalScrollbar)) {
    for (int f = 0; f < kMaxFloor; ++f) drawOneFloor(h, f);
  }
  ImGui::EndChild();

  drawDeptLegend(h);

  if (tip) {
    ImGui::TextDisabled("跨层规则写在 data\\layout.txt（改数据即可改路线）："
                        "同层相邻诊室 10 米；电梯口与本层楼梯口 30 米；"
                        "电梯口与其他层电梯口 20 米；楼梯口与相邻层楼梯口 20 米。");
  }
}

// 查询两个房间之间的路线。按钮与地图点击都走这里。
void applyRouteQuery() {
  GuiState& st = state();
  Hospital& h = db();
  if (st.routeFrom == st.routeTo) {
    st.notify(NOTICE_WARN, "起点与终点是同一个房间，无需移动。");
    st.routeValid = false;
    std::printf("[GUI] 路线查询：起点与终点相同\n");
    return;
  }
  st.route = Route::between(h, st.routeFrom, st.routeTo);
  st.routeValid = true;
  if (!st.route.found) {
    st.notify(NOTICE_ERROR, "两点之间不连通（路网里没有把它们连起来的通道）。");
    std::printf("[GUI] 路线查询：%s -> %s 不连通\n", h.rooms[st.routeFrom].code,
                h.rooms[st.routeTo].code);
    return;
  }
  st.notify(NOTICE_OK, "全程约 %d 米，途经 %d 个节点，换层 %d 次。平面图上已用亮色标出这条路线。",
            st.route.totalDistance, st.route.stepCount, st.route.floorChanges);

  std::printf("[GUI] 路线查询：%s -> %s，全程 %d 米，途经 %d 个节点，换层 %d 次\n",
              h.rooms[st.routeFrom].code, h.rooms[st.routeTo].code, st.route.totalDistance,
              st.route.stepCount, st.route.floorChanges);
  for (int i = 0; i < st.route.stepCount; ++i) {
    const Room& r = h.rooms[st.route.steps[i]];
    std::printf("        节点 %2d  %s  %s\n", r.node, r.code, r.name);
  }
}

}  // namespace

void drawRoute() {
  GuiState& st = state();
  Hospital& h = db();

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.72f, 0.95f, 1.0f));
  ImGui::TextUnformatted("功能 6  就诊路线");
  ImGui::PopStyleColor();
  ImGui::TextWrapped("把每个房间当图的一个顶点、能走过去就画一条边、边权是距离；装载时用 Floyd "
                     "一次算出任意两点最短路，所以查询是查表 + 还原路径。"
                     "下面的平面图上，这条路线会用亮色描出来。");
  ImGui::Separator();

  if (st.routeFrom < 0 || st.routeFrom >= h.rooms.size()) st.routeFrom = 0;
  if (st.routeTo < 0 || st.routeTo >= h.rooms.size()) st.routeTo = h.rooms.size() - 1;

  // ------------------------------ 选起点终点 ------------------------------
  ImGui::TextUnformatted("起点：");
  ImGui::SameLine();
  const bool fromChanged = roomCombo("##from", h, &st.routeFrom);
  ImGui::SameLine();
  ImGui::TextDisabled("（也可以直接在平面图上点房间）");

  ImGui::TextUnformatted("终点：");
  ImGui::SameLine();
  const bool toChanged = roomCombo("##to", h, &st.routeTo);
  ImGui::SameLine();
  ImGui::TextDisabled("（或按住 Shift 点房间）");
  if (fromChanged || toChanged) st.routeValid = false;  // 改了起终点，旧结果作废

  ImGui::Spacing();
  if (ImGui::Button("查询路线", ImVec2(buttonWidth("查询路线"), 0.0f))) applyRouteQuery();
  ImGui::SameLine();
  if (ImGui::Button("交换起终点", ImVec2(buttonWidth("交换起终点"), 0.0f))) {
    const int t = st.routeFrom;
    st.routeFrom = st.routeTo;
    st.routeTo = t;
    st.routeValid = false;
  }
  ImGui::SameLine();
  ImGui::Checkbox("并排显示", &st.routeSideBySide);
  ImGui::Spacing();

  // 两种排法，见文件头说明。两块高度都按「四层楼刚好放得下」定死。
  if (st.routeSideBySide) {
    if (ImGui::BeginTable("routeLayout", 2,
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadOuterX,
                          ImVec2(0.0f, 0.0f))) {
      ImGui::TableSetupColumn("明细", ImGuiTableColumnFlags_WidthFixed, 620.0f);
      ImGui::TableSetupColumn("地图", ImGuiTableColumnFlags_WidthStretch, 1.0f);
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      drawRouteDetail(h, 300.0f);
      ImGui::TableSetColumnIndex(1);
      drawFloorSection(h, 316.0f, true);
      ImGui::EndTable();
    }
  } else {
    // 地图在上：48 个房间一屏看完，不用横向滚动
    drawFloorSection(h, 316.0f, false);
    ImGui::Spacing();
    drawRouteDetail(h, 196.0f);
    ImGui::TextDisabled("跨层规则写在 data\\layout.txt（改数据即可改路线）："
                        "同层相邻诊室 10 米；电梯口与本层楼梯口 30 米；"
                        "电梯口与其他层电梯口 20 米；楼梯口与相邻层楼梯口 20 米。");
  }
}

}  // namespace gui
