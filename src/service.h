/* =============================================================================
 *  service.h  —  智能导诊与就诊路线
 *
 *  本文件只放算法的实现，不放界面。每个函数把结果算出来返回给调用者，
 *  由界面层决定怎么显示，这样将来换成图形界面时这里一行都不用改。
 * ========================================================================== */
#pragma once

#include "model.h"

// =============================================================================
//  智能导诊
//
//  数学模型：症状与科室的加权二部图 G = (S ∪ K, E, W)
//      S 症状顶点集，K 科室顶点集，边权 W(s,k) ∈ [1,5] 表示关联强度
//  （题目实现提示第 1 条）
//
//  算法（题目实现提示第 2 条：把患者症状与科室描述进行匹配）：
//
//    患者从症状表里选出自己的症状（选中的就是二部图症状侧的顶点），
//    然后对每个科室做一次「邻域加权求和」：
//
//        score(k) = Σ W(s, k)        （s 取患者选中的症状）
//
//    也就是从选中的症状顶点出发，沿边走到相邻的科室顶点，把边权累加上去。
//    这正是「患者症状与科室描述匹配」的落地方式 —— 科室描述的语义由与它相连的
//    症状顶点代表，选中症状就等于在描述里挑出了对应的词。
//
//    最后按 score 从大到小排序，取前几名作为推荐结果，并记下每个科室命中了
//    哪些症状，界面据此显示「推荐依据」。
//
//  复杂度：O(科室数 × 每科室症状数 + 科室数²)
//          科室 12 个、每科室至多几个症状，实际是瞬间完成。
// =============================================================================
constexpr int kMaxTriageResult = 5;    // 最多返回几个推荐科室
constexpr int kMaxPickedSymptom = 8;   // 一次最多选几个症状
constexpr int kMaxMatchedPerDept = 8;  // 每个科室最多记几个命中的症状

// 一个推荐科室
struct TriageItem {
  int dept;                         // 科室下标
  int score;                        // 匹配得分（命中症状的权重之和）
  int matchCount;                   // 命中的症状个数
  int matched[kMaxMatchedPerDept];  // 命中的症状顶点号，用于说明推荐理由
};

struct TriageResult {
  int picked[kMaxPickedSymptom];  // 患者选中的症状顶点号，按选中先后排列
  int pickedCount = 0;            // 实际选了几个（已去重）
  TriageItem items[kMaxTriageResult];
  int count = 0;

  bool hasPicked(int symptom) const {
    for (int i = 0; i < pickedCount; ++i)
      if (picked[i] == symptom) return true;
    return false;
  }
};

class Triage {
 public:
  // picked 是患者选中的症状顶点号（调用方已去掉重复）。
  // 返回按得分降序排列的推荐科室。
  static TriageResult run(const Hospital& h, const int* picked, int pickCount) {
    TriageResult result;
    const BipartiteGraph& g = h.symptomGraph;

    // 记下患者选了什么，界面显示「依据：胸闷 气短」时用得上
    for (int i = 0; i < pickCount && result.pickedCount < kMaxPickedSymptom; ++i)
      result.picked[result.pickedCount++] = picked[i];

    // 对每个科室做一次邻域加权求和，命中的按得分插入排序
    for (int d = 0; d < g.departmentCount(); ++d) scoreDept(g, d, result);
    return result;
  }

 private:
  // ---------------------------------------------------------------------
  //  把一个科室的得分算出来，并按插入排序放进结果里
  //
  //  沿「科室 -> 关联症状」这条邻接链走一遍，看哪些症状被患者选中了。
  //  这就是二部图上的邻域加权求和：科室是中心，它的邻居症状里被选中的那些，
  //  把边权贡献给它的得分。
  // ---------------------------------------------------------------------
  static void scoreDept(const BipartiteGraph& g, int dept, TriageResult& result) {
    int score = 0;
    int matched[kMaxMatchedPerDept];
    int matchCount = 0;

    for (int e = g.firstSymptomOf(dept); e != -1; e = g.nextSymptomEdge(e)) {
      const int symptom = g.edgeSymptom(e);
      if (!result.hasPicked(symptom)) continue;
      score += g.edgeWeight(e);
      if (matchCount < kMaxMatchedPerDept) matched[matchCount++] = symptom;
    }
    if (score <= 0) return;  // 一个症状都没命中，不推荐这个科室

    TriageItem item;
    item.dept = dept;
    item.score = score;
    item.matchCount = matchCount;
    for (int k = 0; k < kMaxMatchedPerDept; ++k) item.matched[k] = (k < matchCount) ? matched[k] : -1;

    insertSorted(result, item);
  }

  // 插入排序：结果最多 5 条，直接插入并保持得分降序
  static void insertSorted(TriageResult& result, const TriageItem& item) {
    if (result.count < kMaxTriageResult) {
      int pos = result.count;
      while (pos > 0 && result.items[pos - 1].score < item.score) {
        result.items[pos] = result.items[pos - 1];
        --pos;
      }
      result.items[pos] = item;
      ++result.count;
      return;
    }
    // 已经满了：只有新项得分高于最后一名时才挤掉它
    if (item.score <= result.items[kMaxTriageResult - 1].score) return;
    int pos = kMaxTriageResult - 1;
    while (pos > 0 && result.items[pos - 1].score < item.score) {
      result.items[pos] = result.items[pos - 1];
      --pos;
    }
    result.items[pos] = item;
  }
};

// =============================================================================
//  就诊路线
//
//  路网在装载时已经用 Floyd 算法算好了任意两点之间的最短距离，
//  所以这里的查询是 O(1) 的查表加一次路径还原，非常轻。
// =============================================================================
constexpr int kMaxRouteSteps = 16;  // 一条路线最多显示多少个顶点

struct RouteResult {
  bool found = false;
  int totalDistance = 0;      // 全程距离（米）
  int steps[kMaxRouteSteps];  // 途经的房间下标（含起点和终点）
  int stepCount = 0;
  int floorChanges = 0;  // 换层次数
};

class Route {
 public:
  // 查询两个房间之间的步行路线
  static RouteResult between(const Hospital& h, int fromRoom, int toRoom) {
    RouteResult r;
    if (fromRoom < 0 || toRoom < 0 || fromRoom >= h.rooms.size() || toRoom >= h.rooms.size()) return r;

    const int u = h.rooms[fromRoom].node;
    const int v = h.rooms[toRoom].node;
    if (!h.road.reachable(u, v)) return r;  // 两点不连通

    r.totalDistance = h.road.distance(u, v);

    // 还原途经的顶点号，再翻译回房间下标
    int path[kMaxRouteSteps];
    const int n = h.road.buildPath(u, v, path, kMaxRouteSteps);
    if (n <= 0) return r;

    r.found = true;
    for (int i = 0; i < n && r.stepCount < kMaxRouteSteps; ++i) {
      const int room = h.findRoomByNode(path[i]);
      if (room >= 0) r.steps[r.stepCount++] = room;
    }
    // 数一数换了几次楼层
    for (int i = 1; i < r.stepCount; ++i)
      if (h.rooms[r.steps[i]].floor != h.rooms[r.steps[i - 1]].floor) ++r.floorChanges;
    return r;
  }
};
