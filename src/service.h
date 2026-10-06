/* =============================================================================
 *  service.h  —  业务功能
 *
 *  本文件只放「算法的实现」，不放界面。每个函数把结果算出来返回给调用者，
 *  由 main.cpp 决定怎么显示，这样以后换成图形界面时这里一行都不用改。
 *
 *  包含两件事：
 *    Triage  智能导诊：按患者勾选的症状给科室打分并排序
 *    Route   就诊路线：查两个房间之间的最短路
 * ========================================================================== */
#pragma once

#include "model.h"

// =============================================================================
//  智能导诊
//
//  模型：把「症状—科室」关联关系看成一张带权二部图——
//        一侧是症状顶点，另一侧是科室顶点，边权表示该症状指向该科室的强度。
//
//  算法：患者勾选若干症状后，对每个科室把它命中的症状权重加起来：
//            score(科室) = Σ 权重(症状, 科室)
//        然后按 score 从大到小排序，取前几名作为推荐结果。
//        这实际上是二部图上的一次「邻域加权求和」。
//
//  复杂度：O(科室数 × 每科室症状数)，对本系统的规模来说是瞬间完成。
// =============================================================================
constexpr int kMaxTriageResult = 5;  // 最多返回几个推荐科室
constexpr int kMaxPick = 8;          // 一次最多勾选几个症状

struct TriageItem {
  int dept;                             // 科室下标
  int score;                            // 匹配得分
  int matchCount;                       // 命中的症状个数
  int matched[kMaxSympPerDept];         // 命中的症状下标（用于说明推荐理由）
};

struct TriageResult {
  TriageItem items[kMaxTriageResult];
  int count = 0;
};

class Triage {
 public:
  // picked 是患者勾选的症状下标；返回按得分降序排列的推荐科室
  static TriageResult run(const Hospital& h, const int* picked, int pickCount) {
    TriageResult result;

    // 第一步：对每个科室累计命中症状的权重
    for (int d = 0; d < h.depts.size(); ++d) {
      const Department& dept = h.depts[d];
      int sum = 0;
      int hits[kMaxSympPerDept];
      int hitCount = 0;
      for (int i = 0; i < dept.symptomCount; ++i) {
        if (!isPicked(picked, pickCount, dept.symptoms[i])) continue;
        sum += dept.weights[i];
        hits[hitCount++] = dept.symptoms[i];
      }
      if (sum <= 0) continue;

      // 得分大于 0 的科室先收进结果（最多 kMaxTriageResult 个，超出则替换最低分）
      TriageItem item;
      item.dept = d;
      item.score = sum;
      item.matchCount = hitCount;
      for (int k = 0; k < kMaxSympPerDept; ++k) item.matched[k] = (k < hitCount) ? hits[k] : -1;
      insertSorted(result, item);
    }
    return result;
  }

 private:
  static bool isPicked(const int* picked, int pickCount, int symptom) {
    for (int i = 0; i < pickCount; ++i)
      if (picked[i] == symptom) return true;
    return false;
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
    // 已经满了：只有当新项得分高于最后一名时才挤掉它
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
constexpr int kMaxRouteSteps = 16;  // 一条路线最多显示多少个节点

struct RouteResult {
  bool found = false;
  int totalDistance = 0;              // 全程距离（米）
  int steps[kMaxRouteSteps];          // 途经的房间下标（含起点和终点）
  int stepCount = 0;
  int floorChanges = 0;               // 换层次数
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

    // 还原途经的节点号，再翻译回房间下标
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
