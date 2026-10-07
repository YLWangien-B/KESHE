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
//  算法（题目实现提示第 2 条：把患者症状与科室描述进行匹配）分三步：
//
//    第一步  从患者的输入里找出已知症状词
//            做法是「词典驱动的正向最长匹配」：从左到右扫描输入串，在每个位置
//            拿全部症状名去比，取能匹配上的最长的那个，命中就跳过该词，否则
//            前进一个字节。取最长的而不是第一个命中的，是为了让「胸闷」这种
//            包含关系的词不会把更长的词切碎。
//
//    第二步  沿二部图把症状的权重累加到科室
//            score(k) = Σ W(s, k)，s 取第一步命中的症状
//            这实际上是二部图上的一次「邻域加权求和」：从命中的症状顶点出发，
//            沿边走到相邻的科室顶点，把边权累加上去。
//            这正是「将患者症状与科室描述进行匹配」的具体做法——
//            科室描述的语义由与它相连的症状顶点代表，匹配就是沿边走一遍。
//
//    第三步  按得分从大到小排序，取前几名作为推荐结果
//            结果里同时记下每个科室命中了哪些症状，界面据此显示「推荐依据」。
//
//  复杂度：O(输入长度 × 症状数 + 科室数 × 症状数 + 科室数²)
//          输入几十个字、症状几十个、科室十几个，实际是瞬间完成。
// =============================================================================
constexpr int kMaxTriageResult = 5;      // 最多返回几个推荐科室
constexpr int kMaxPickedSymptom = 8;     // 一次最多参考几个症状
constexpr int kMaxMatchedPerDept = 8;    // 每个科室最多记几个命中的症状

// 第一步的结果：从患者输入里找出来的症状
struct SymptomMatch {
  int hit[kMaxPickedSymptom];       // 命中的症状顶点号，按在输入中出现的先后排列
  bool exact[kMaxPickedSymptom];    // 命中的是规范词（true）还是同义词（false）
  int aliasIndex[kMaxPickedSymptom];// 命中同义词时是第几个同义词；命中规范词时为 -1
  int count = 0;

  bool full() const { return count >= kMaxPickedSymptom; }

  int indexOf(int symptom) const {
    for (int i = 0; i < count; ++i)
      if (hit[i] == symptom) return i;
    return -1;
  }
  bool has(int symptom) const { return indexOf(symptom) >= 0; }
};

// 一个推荐科室
struct TriageItem {
  int dept;                              // 科室下标
  int score;                             // 匹配得分（命中症状的权重之和）
  int matchCount;                        // 命中的症状个数
  int matched[kMaxMatchedPerDept];       // 命中的症状顶点号，用于说明推荐理由
};

struct TriageResult {
  SymptomMatch found;                    // 从输入里认出哪些症状
  TriageItem items[kMaxTriageResult];
  int count = 0;
};

class Triage {
 public:
  // 主流程：患者输入一句话，返回按得分降序排列的推荐科室
  static TriageResult run(const Hospital& h, const char* text) {
    TriageResult result;
    const BipartiteGraph& g = h.symptomGraph;

    // ---------- 第一步：正向最长匹配，从输入里找出症状词 ----------
    matchSymptoms(g, text, result.found);

    // ---------- 第二步：沿二部图把命中症状的权重累加到科室 ----------
    for (int d = 0; d < g.departmentCount(); ++d) {
      scoreDept(h, g, d, result.found, result);
    }
    return result;
  }

  // 只做第一步：从文本里认出症状。供界面提示「识别到哪些症状」用。
  static SymptomMatch recognize(const Hospital& h, const char* text) {
    SymptomMatch m;
    matchSymptoms(h.symptomGraph, text, m);
    return m;
  }

 private:
  // 在 haystack 里找 needle；用于核对科室描述里有没有写到某个口语说法。
  // 字符串都很短（描述一句、词几个字），朴素查找足够。
  static bool containsText(const char* haystack, const char* needle) {
    if (!needle || !*needle) return false;
    for (int i = 0; haystack[i]; ++i) {
      int k = 0;
      while (needle[k] && haystack[i + k] == needle[k]) ++k;
      if (needle[k] == '\0') return true;
    }
    return false;
  }

  // text[pos] 处是否以词 word 开头；是则通过 len 传出该词的字节数
  static bool startsWith(const char* text, int pos, const char* word, int& len) {
    len = 0;
    if (!word || !*word) return false;
    int k = 0;
    for (; word[k]; ++k)
      if (!text[pos + k] || text[pos + k] != word[k]) return false;
    len = k;
    return true;
  }

  // ---------------------------------------------------------------------
  //  正向最长匹配
  //
  //  在 text[pos] 处把全部症状名与它们的同义词都拿来比，取匹配上的最长的那个。
  //  取最长而不是第一个命中的，是为了处理包含关系：例如输入「胸口发闷」时，
  //  既能匹配到同义词「胸口」，也能匹配到「胸口发闷」，应当取后者。
  //  命中规范词与命中同义词都算命中，只是后者的置信度低一些（见 scoreDept）。
  // ---------------------------------------------------------------------
  static int longestMatchAt(const BipartiteGraph& g, const char* text, int pos, int& matchedLen, bool& isExact,
                            int& aliasIndex) {
    matchedLen = 0;
    isExact = false;
    aliasIndex = -1;
    int best = -1;

    // 先比规范词，再比同义词：同样长度时规范词优先
    for (int s = 0; s < g.symptomCount(); ++s) {
      int len = 0;
      if (startsWith(text, pos, g.symptomName(s), len) && len > matchedLen) {
        best = s;
        matchedLen = len;
        isExact = true;
        aliasIndex = -1;
      }
    }
    for (int s = 0; s < g.symptomCount(); ++s) {
      for (int a = 0; a < g.aliasCount(s); ++a) {
        int len = 0;
        if (startsWith(text, pos, g.aliasName(s, a), len) && len > matchedLen) {
          best = s;
          matchedLen = len;
          isExact = false;
          aliasIndex = a;
        }
      }
    }
    return best;
  }

  // 扫一遍输入串，把认出来的症状按出现先后记进 found（同一个症状只记一次）
  static void matchSymptoms(const BipartiteGraph& g, const char* text, SymptomMatch& found) {
    found.count = 0;
    if (!text) return;
    const int n = static_cast<int>(std::strlen(text));

    int pos = 0;
    while (pos < n) {
      int len = 0;
      bool exact = false;
      int aliasIndex = -1;
      const int hit = longestMatchAt(g, text, pos, len, exact, aliasIndex);
      if (hit >= 0 && len > 0) {
        if (!found.has(hit) && !found.full()) {
          const int k = found.count++;
          found.hit[k] = hit;
          found.exact[k] = exact;
          found.aliasIndex[k] = aliasIndex;
        }
        pos += len;  // 命中就跳过整个词
        continue;
      }
      ++pos;  // 没命中就前进一个字节（UTF-8 汉字是 3 字节，多走几步而已）
    }
  }

  // ---------------------------------------------------------------------
  //  把某个科室的得分算出来，并按插入排序放进结果里
  // ---------------------------------------------------------------------
  static void scoreDept(const Hospital& h, const BipartiteGraph& g, int dept, const SymptomMatch& found,
                        TriageResult& result) {
    int score = 0;
    int matched[kMaxMatchedPerDept];
    int matchCount = 0;

    // 沿「科室 -> 关联症状」这条邻接链走一遍，看哪些症状被患者提到了。
    //
    // 命中的是规范词，就用边的权重；命中的是同义词（患者说的口语），
    // 就回到这个科室的描述里核对一下：描述里也写到了这个口语词，说明患者说的
    // 就是它主诉范围内的症状，照原权重算；描述里没写，说明只是沾边，
    // 打对折。这一步让「患者症状与科室描述匹配」真正落到了描述文本上，
    // 而不只是走过边表。
    for (int e = g.firstSymptomOf(dept); e != -1; e = g.nextSymptomEdge(e)) {
      const int symptom = g.edgeSymptom(e);
      const int at = found.indexOf(symptom);
      if (at < 0) continue;

      const int w = g.edgeWeight(e);
      bool full = found.exact[at];
      if (!full && found.aliasIndex[at] >= 0) {
        const char* alias = g.aliasName(symptom, found.aliasIndex[at]);
        full = containsText(h.depts[dept].description, alias);
      }
      score += full ? w : ((w + 1) / 2);
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
