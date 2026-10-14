/* =============================================================================
 *  panel_route.cpp  —  功能 6：就诊路线
 *
 *  算法完全复用 service.h 的 Route::between()：它返回一个 RouteResult，
 *  里面是「途经的房间下标序列 + 总距离 + 换层次数」。
 *  这里的活儿只有两件：把两个房间选出来，把结果排版画出来。
 *
 *  与字符界面的区别：字符界面只能把节点序列打成一张竖表；
 *  图形界面把这条路线同步高亮到「楼层平面图」上，跨层的地方一眼能看出来。
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

// 房间下拉框：按楼层分组显示，避免 48 个房间堆成一条长列表
bool roomCombo(const char* id, const Hospital& h, int* value) {
  bool changed = false;
  char cur[128];
  if (*value >= 0 && *value < h.rooms.size())
    std::snprintf(cur, sizeof(cur), "%s  %s", h.rooms[*value].code, h.rooms[*value].name);
  else
    std::snprintf(cur, sizeof(cur), "（请选择房间）");

  // 宽度按当前显示的房间名量出来，别写死像素
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

}  // namespace

// 查询两个房间之间的路线。逻辑抽成一个函数，与按钮的绘制分开。
static void applyRouteQuery() {
  GuiState& st = state();
  Hospital& h = db();
  if (st.routeFrom == st.routeTo) {
    st.notify(NOTICE_WARN, "起点与终点是同一个房间，无需移动。");
    st.routeValid = false;
    st.routeHighlightCount = 0;
    std::printf("[GUI] 路线查询：起点与终点相同\n");
    return;
  }
  st.route = Route::between(h, st.routeFrom, st.routeTo);
  st.routeValid = true;
  st.routeHighlightCount = 0;
  if (!st.route.found) {
    st.notify(NOTICE_ERROR, "两点之间不连通（路网里没有把它们连起来的通道）。");
    std::printf("[GUI] 路线查询：%s -> %s 不连通\n", h.rooms[st.routeFrom].code,
                h.rooms[st.routeTo].code);
    return;
  }
  for (int i = 0; i < st.route.stepCount && i < kMaxRouteSteps; ++i)
    st.routeHighlight[st.routeHighlightCount++] = st.route.steps[i];
  st.floorHighlightFrom = st.routeFrom;
  st.floorHighlightTo = st.routeTo;
  st.notify(NOTICE_OK, "全程约 %d 米，途经 %d 个节点，换层 %d 次。已同步高亮到楼层平面图。",
            st.route.totalDistance, st.route.stepCount, st.route.floorChanges);

  std::printf("[GUI] 路线查询：%s -> %s，全程 %d 米，途经 %d 个节点，换层 %d 次\n",
              h.rooms[st.routeFrom].code, h.rooms[st.routeTo].code, st.route.totalDistance,
              st.route.stepCount, st.route.floorChanges);
  for (int i = 0; i < st.route.stepCount; ++i) {
    const Room& r = h.rooms[st.route.steps[i]];
    std::printf("        节点 %2d  %s  %s\n", r.node, r.code, r.name);
  }
}

void drawRoute() {
  GuiState& st = state();
  Hospital& h = db();

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.72f, 0.95f, 1.0f));
  ImGui::TextUnformatted("功能 6  就诊路线");
  ImGui::PopStyleColor();
  ImGui::TextWrapped("把每个房间当图的一个顶点、能走过去就画一条边、边权是距离；"
                      "装载时用 Floyd 一次算出任意两点最短路，所以这里的查询是 O(1) 查表 + 还原路径。");
  ImGui::Separator();

  if (st.routeFrom < 0 || st.routeFrom >= h.rooms.size()) st.routeFrom = 0;
  if (st.routeTo < 0 || st.routeTo >= h.rooms.size()) st.routeTo = h.rooms.size() - 1;

  // ------------------------------ 选起点终点 ------------------------------
  ImGui::TextUnformatted("起点：");
  ImGui::SameLine();
  roomCombo("##from", h, &st.routeFrom);

  ImGui::TextUnformatted("终点：");
  ImGui::SameLine();
  roomCombo("##to", h, &st.routeTo);

  ImGui::Spacing();
  if (ImGui::Button("查询路线", ImVec2(buttonWidth("查询路线"), 0.0f))) applyRouteQuery();
  ImGui::SameLine();
  if (ImGui::Button("交换起终点", ImVec2(buttonWidth("交换起终点"), 0.0f))) {
    const int t = st.routeFrom;
    st.routeFrom = st.routeTo;
    st.routeTo = t;
    st.routeValid = false;
    st.routeHighlightCount = 0;
  }
  ImGui::SameLine();
  if (ImGui::Button("在平面图上查看", ImVec2(buttonWidth("在平面图上查看"), 0.0f))) {
    st.floorHighlightFrom = st.routeFrom;
    st.floorHighlightTo = st.routeTo;
    st.page = PAGE_FLOOR;
  }

  if (!st.routeValid) {
    ImGui::Spacing();
    ImGui::TextDisabled("提示：可以在「楼层平面图」页直接点房间选起点，按住 Shift 点则设为终点。");
    return;
  }
  if (!st.route.found) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.45f, 1.0f), "两点之间不连通。");
    return;
  }

  // ------------------------------ 路线摘要 ------------------------------
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.85f, 0.62f, 1.0f));
  ImGui::Text("全程约 %d 米，途经 %d 个节点，换层 %d 次", st.route.totalDistance, st.route.stepCount,
              st.route.floorChanges);
  ImGui::PopStyleColor();

  // ------------------------------ 节点序列 ------------------------------
  const int startNode = h.rooms[st.route.steps[0]].node;
  if (ImGui::BeginTable("routeSteps", 5,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 34.0f);
    ImGui::TableSetupColumn("节点", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("房间", ImGuiTableColumnFlags_WidthFixed, 300.0f);
    ImGui::TableSetupColumn("所在楼层", ImGuiTableColumnFlags_WidthFixed, 110.0f);
    ImGui::TableSetupColumn("累计距离", ImGuiTableColumnFlags_WidthFixed, 100.0f);
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

  // ------------------------------ 换层指引 ------------------------------
  ImGui::Spacing();
  ImGui::SeparatorText("换层指引");
  if (st.route.floorChanges == 0) {
    ImGui::TextUnformatted("全程在同一层，无需上下楼。");
  } else {
    for (int i = 1; i < st.route.stepCount; ++i) {
      const Room& prev = h.rooms[st.route.steps[i - 1]];
      const Room& cur = h.rooms[st.route.steps[i]];
      if (prev.floor == cur.floor) continue;
      // 换层发生在通道处，按房间名里有没有「电梯」判断患者用的是电梯还是楼梯
      const bool byLift = (std::strstr(prev.name, "电梯") != nullptr) ||
                          (std::strstr(cur.name, "电梯") != nullptr);
      ImGui::Text("  %d. 在「%s」%s，到达 %d 层的「%s」", i, prev.name, byLift ? "乘电梯" : "走楼梯",
                  cur.floor + 1, cur.name);
    }
  }

  ImGui::Spacing();
  ImGui::TextWrapped("路网共 %d 个顶点（模型上限 kMaxNodes = %d），Floyd 预计算后每次查询只查表。",
                     h.road.nodeCount(), WeightedGraph::kMaxNodes);
}

}  // namespace gui
