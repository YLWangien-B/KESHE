/* =============================================================================
 *  panel_about.cpp  —  附加页：数据总览
 *
 *  把「界面下面的那些数据结构」摊开给答辩看：
 *  每个结构装了多少条数据、容量上限是多少、实测算下来占多少字节。
 *  数字全部取自当前的 Hospital 实例与实际 sizeof，不是写死的。
 * ========================================================================== */
#include <cstdio>

#include "imgui.h"
#include "gui_common.h"
#include "gui_state.h"
#include "panels.h"

namespace gui {

namespace {

// 一行「结构 / 用途 / 实装 / 容量 / 字节数」
void row(const char* type, const char* use, int used, int cap, long long bytes) {
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::TextUnformatted(type);
  ImGui::TableSetColumnIndex(1);
  ImGui::TextUnformatted(use);
  ImGui::TableSetColumnIndex(2);
  if (used >= 0)
    ImGui::Text("%d", used);
  else
    ImGui::TextDisabled("—");
  ImGui::TableSetColumnIndex(3);
  if (cap > 0)
    ImGui::Text("%d", cap);
  else
    ImGui::TextDisabled("—");
  ImGui::TableSetColumnIndex(4);
  ImGui::Text("%lld", bytes);
  ImGui::TableSetColumnIndex(5);
  if (cap > 0 && used >= 0 && used > cap) {
    ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.45f, 1.0f), "超容量！");
  } else if (cap > 0 && used >= 0 && used * 10 >= cap * 9) {
    ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.35f, 1.0f), "快满了");
  } else {
    ImGui::TextDisabled("正常");
  }
}

}  // namespace

void drawAbout() {
  const GuiState& st = state();
  Hospital& h = db();  // 非 const：下面的 h.slot() 不是 const 方法

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.72f, 0.95f, 1.0f));
  ImGui::TextUnformatted("数据总览");
  ImGui::PopStyleColor();
  ImGui::TextWrapped("本程序的全部数据都来自文本文件（目录：%s），程序只负责读。"
                     "运行期产生的预约记录与候诊队列只在内存里，退出即清空。",
                     st.dataDir);
  ImGui::Separator();

  // ------------------------------ 装载统计 ------------------------------
  ImGui::SeparatorText("装载统计");
  int scheduleSlots = 0, quotaSum = 0, leftSum = 0;
  for (int i = 0; i < h.doctors.size(); ++i)
    for (int d = 0; d < kMaxDay; ++d)
      for (int s = 0; s < 2; ++s) {
        if (!h.hasSchedule(i, d, s)) continue;
        ++scheduleSlots;
        quotaSum += h.slot(i, d, s).quota;   // slot() 不是 const 方法，所以 h 用非 const 引用
        leftSum += h.remaining(i, d, s);
      }

  // 路网的边数：邻接矩阵里除了对角线，每一对互达的顶点算一条无向边，所以数上三角
  const int nodes = h.road.nodeCount();
  int roadEdges = 0;
  for (int u = 0; u < nodes; ++u)
    for (int v = u + 1; v < nodes; ++v)
      if (h.road.reachable(u, v)) ++roadEdges;

  if (ImGui::BeginTable("loadStats", 2, ImGuiTableFlags_SizingFixedFit)) {
    const struct { const char* k; int v; } rows[] = {
        {"科室数", h.depts.size()},
        {"医生数", h.doctors.size()},
        {"患者数", h.patients.size()},
        {"房间数", h.rooms.size()},
        {"症状数（二部图症状侧顶点）", h.symptomGraph.symptomCount()},
        {"二部图边集结点数（每条边占 2 个结点）", h.symptomGraph.edgeCount()},
        {"二部图边数（关联关系条数）", h.symptomGraph.edgeCount() / 2},
        {"排班槽数（已排班的 日期×时段 格子）", scheduleSlots},
        {"一周号源总数", quotaSum},
        {"一周剩余号源", leftSum},
        {"路网顶点数", nodes},
        {"路网边数（可达的顶点对）", roadEdges},
        {"预约记录数（运行期）", h.bookings.size()},
        {"候诊队列人数（运行期）", h.waiting.size()},
    };
    for (const auto& r : rows) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextDisabled("%s", r.k);
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%d", r.v);
    }
    ImGui::EndTable();
  }

  if (st.loadErrors > 0 || st.loadOverflows > 0)
    ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.35f, 1.0f),
                       "本次装载：%d 行未通过校验，%d 处超出容量上限。", st.loadErrors,
                       st.loadOverflows);
  else
    ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.52f, 1.0f), "本次装载：全部数据行通过校验。");

  // ------------------------------ 数据结构占用 ------------------------------
  ImGui::Spacing();
  ImGui::SeparatorText("数据结构占用（字节数由 sizeof 实测，不是估算）");
  if (ImGui::BeginTable("dsTable", 6,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("结构", ImGuiTableColumnFlags_WidthFixed, 210.0f);
    ImGui::TableSetupColumn("用途", ImGuiTableColumnFlags_WidthFixed, 300.0f);
    ImGui::TableSetupColumn("实装", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("容量", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("字节数", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("余量", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    row("DepartmentList", "科室表", h.depts.size(), kMaxDept, sizeof(DepartmentList));
    row("DoctorList", "医生表（每人含一张 7×4 排班表）", h.doctors.size(), kMaxDoctor,
        sizeof(DoctorList));
    row("PatientList", "患者表", h.patients.size(), kMaxPatient,
        sizeof(PatientList));
    row("BookingList", "预约记录表", h.bookings.size(), kMaxBook,
        sizeof(BookingList));
    row("RoomList", "房间表", h.rooms.size(), kMaxRoom, sizeof(RoomList));
    row("BipartiteGraph", "症状—科室带权二部图（导诊）", h.symptomGraph.symptomCount(),
        BipartiteGraph::kMaxSymptoms, sizeof(BipartiteGraph));
    row("WaitingQueue", "候诊队列（环形队列）", h.waiting.size(), kMaxWaiting,
        sizeof(WaitingQueue));
    row("WeightedGraph", "就诊路网（邻接矩阵 + Floyd）", h.road.nodeCount(), WeightedGraph::kMaxNodes,
        sizeof(WeightedGraph));
    row("HashMap ×4", "四个编号→下标索引", h.depts.size() + h.doctors.size() + h.patients.size() +
                                              h.rooms.size(),
        1024, sizeof(HashMap) * 4);
    ImGui::EndTable();
  }
  ImGui::Spacing();
  ImGui::Text("数据总库 sizeof(Hospital) = %lld 字节（约 %lld KB），全程序只有 db() 这一个实例。",
              static_cast<long long>(sizeof(Hospital)),
              static_cast<long long>(sizeof(Hospital)) / 1024);

  // ------------------------------ 结构说明 ------------------------------
  ImGui::Spacing();
  ImGui::SeparatorText("七项功能各用了哪种结构");
  if (ImGui::BeginTable("mapTable", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("功能", ImGuiTableColumnFlags_WidthFixed, 140.0f);
    ImGui::TableSetupColumn("数据结构", ImGuiTableColumnFlags_WidthFixed, 260.0f);
    ImGui::TableSetupColumn("算法", ImGuiTableColumnFlags_WidthFixed, 300.0f);
    ImGui::TableSetupColumn("复杂度", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
    const struct { const char* f; const char* ds; const char* alg; const char* c; } rows[] = {
        {"1 智能导诊", "带权二部图（前向星邻接表）", "邻域加权求和 + 插入排序", "O(科室×症状)"},
        {"2 医生查询", "顺序表 + 散列表", "线性筛选（子串匹配）", "O(医生数)"},
        {"3 在线预约", "每位医生一张 日期×时段 二维数组 + 版本号", "检查并扣减号源的原子操作", "O(1)"},
        {"4 预约管理", "顺序表 + 散列表", "按患者筛选遍历", "O(预约数)"},
        {"5 排班展示", "二维数组 + 顺序表", "按日期与时段枚举", "O(日期×时段)"},
        {"6 就诊路线", "带权图（邻接矩阵）", "Floyd 预计算 + 查表还原路径", "预计算 O(V³)，查询 O(1)"},
        {"7 候诊队列", "环形队列", "入队 / 出队 / 过号重排", "O(1)，按位删除 O(n)"},
    };
    for (const auto& r : rows) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(r.f);
      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(r.ds);
      ImGui::TableSetColumnIndex(2);
      ImGui::TextUnformatted(r.alg);
      ImGui::TableSetColumnIndex(3);
      ImGui::TextUnformatted(r.c);
    }
    ImGui::EndTable();
  }

  ImGui::Spacing();
  ImGui::TextWrapped("提示：这个图形界面程序与字符界面程序（hospital.exe）共用同一份数据结构层"
                      "（ds.h / ds_sort.h）、数据层（model.h / load.h）与业务层"
                      "（service.h / service_queue.h），只有界面层是两套实现。");
}

}  // namespace gui
