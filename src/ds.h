/* =============================================================================
 *  ds.h  —  自行实现的数据结构（不使用模板）
 *
 *  课程要求：不能使用现成的数据结构库，也不使用模板（因为考查的就是数据结构
 *            与算法本身）；字符串、数学等库不受限制，因此本文件只用到 <cstring>。
 *
 *  本文件所在的分层：
 *      ds_base.h  基础工具（散列表）
 *      ds_list.h  实体定义 + 六个顺序表
 *      ds.h       本文件：环形队列、二维数组、带权图、带权二部图
 *      model.h    数据总库 Hospital
 *
 *  本系统用到的数据结构（六个 + 一个排序算法）：
 *      顺序表      ds_list.h   —— 科室、医生、患者、预约、房间等表的存储
 *      环形队列    WaitingQueue —— 候诊队列
 *      散列表      HashMap      —— 编号/名字到下标的索引（ds_base.h）
 *      二维数组    ScheduleTable—— 医生排班，按「日期 × 时段」索引
 *      带权图      WeightedGraph —— 就诊路网（邻接矩阵 + Floyd）
 *      带权二部图  BipartiteGraph—— 症状与科室的关联关系
 *      插入排序    insertSorted —— 导诊结果排序（ds_sort.h）
 *
 *  ---------------------------------------------------------------------------
 *  这份代码不使用任何模板
 *
 *  原先是 `template <typename T, int N> class RingQueue` 这类写法，
 *  展开后与手写的具体类型在生成的机器码上完全等价，但「模板」两个字在
 *  课程的禁用名单里。现在每个结构都写成具体类型，代价是少量重复代码，
 *  换来的是名单上没有任何可指摘的地方。
 *  ---------------------------------------------------------------------------
 * ========================================================================== */
#pragma once

#include <cstdio>
#include <cstring>

#include "ds_base.h"
#include "ds_list.h"

// =============================================================================
//  环形队列：先进先出。候诊队列用它，出队位置可以被后续入队循环使用。
//
//  元素固定是 Waiting（候诊患者），容量固定是 kMaxWaiting（64），所以不用模板。
//
//  为什么必须额外记 n_（而不是只靠 head_/tail_ 判断空满）：
//      空队列：head == tail（都指向同一个位置，还没放东西）
//      满队列：head == tail（tail 绕一圈追上了 head）
//    两种情况的关系完全一样，无法区分。解决办法有两种：
//      留一个空位：最多只放 N-1 个，用 (tail+1)%N == head 判满 —— 浪费一个槽
//      记长度 n_ ：判空 n_==0，判满 n_==N                    —— 多一个 int
//    这里选后者：多花 4 字节换「容量不浪费 + 判空判满都是一次比较」。
// =============================================================================
class WaitingQueue {
 public:
  bool empty() const { return n_ == 0; }
  bool full() const { return n_ >= kMaxWaiting; }
  int size() const { return n_; }
  int capacity() const { return kMaxWaiting; }

  bool push(const Waiting& v) {
    if (n_ >= kMaxWaiting) return false;
    data_[tail_] = v;
    tail_ = (tail_ + 1) % kMaxWaiting;  // 走到末尾就绕回 0，这就是「环形」
    ++n_;
    return true;
  }

  bool pop(Waiting& out) {
    if (n_ == 0) return false;
    out = data_[head_];
    head_ = (head_ + 1) % kMaxWaiting;
    --n_;
    return true;
  }

  // 按排队次序访问第 i 个元素（0 为队首），用于查看排队进度。
  // 注意 (head_ + i) % N：i=0 是队首，不是 data_[0] —— 写错了就会出现
  // 「界面显示的排队次序和真实次序不一致」这种很难查的问题。
  const Waiting& at(int i) const { return data_[(head_ + i) % kMaxWaiting]; }

  // 同上的可写版本。队列内部本来就是数组，按下标取引用不破坏先进先出的约束
  // （约束是「只能从队首出队」，不是「元素内容不可变」），
  // 只是允许就地修改队内元素（例如把「已叫号次数」加一），
  // 省去「逐个出队再放回」的来回搬运。
  Waiting& at(int i) { return data_[(head_ + i) % kMaxWaiting]; }

  // 删除队列中的第 i 个元素（患者看完病离开队列）。
  //
  // 队列的语义本只能从队首出队，但「患者看完病离开」可能发生在队列中间
  // （比如 3 号先看完了，5 号还没叫到）。做法是**全部出队 -> 跳过要删的那个
  // -> 其余按原序放回**。
  //
  // 为什么这样是安全的：出队过程中队列会变空，之后再入队时 tail_ 从 head_
  // 走过的位置重新开始，不会覆盖还没出队的元素。这是这个实现能成立的前提。
  //
  // 为什么不用「把后面的元素前移」：环形队列里「后面」在物理上可能是绕回去的，
  // 前移要分成两段处理，边界条件多。用「出队再入队」把这个复杂性完全绕开了。
  // 代价是 O(n)，队列只有几十人，可以接受。
  bool removeAt(int i) {
    if (i < 0 || i >= n_) return false;
    const int total = n_;
    for (int k = 0; k < total; ++k) {
      Waiting v;
      if (!pop(v)) break;
      if (k == i) continue;  // 这一个丢掉，不放回
      push(v);
    }
    return true;
  }

  void clear() { head_ = tail_ = n_ = 0; }

 private:
  Waiting data_[kMaxWaiting];
  int head_ = 0;
  int tail_ = 0;
  int n_ = 0;
};

// =============================================================================
//  带权图（邻接矩阵）+ Floyd 算法
//
//  就诊路线用的图。顶点 = 房间（含楼梯口、电梯口），边 = 可行走的路线，权 = 距离。
//  用邻接矩阵存边：顶点只有几十个，矩阵很小，而且 Floyd 算法正好需要矩阵。
//
//  为什么用 Floyd 而不是 Dijkstra：
//    Floyd 一次算出「任意两点之间的最短路」，路线查询就变成查表 O(1)；
//    对固定不变的路网来说，这比每次查询都重新跑一遍 Dijkstra 更省时间。
//    代价是 O(V^3) 的预计算，V 只有几十，实际可忽略。
// =============================================================================
class WeightedGraph {
 public:
  static constexpr int kMaxNodes = 128;
  // 「不可达」的表示。为什么不是 INT_MAX：Floyd 的核心语句要做加法
  // dist[i][k] + dist[k][j]，INT_MAX + INT_MAX 会溢出回绕成负数，
  // 于是「不可达」被判成「可达且距离为负」，整张矩阵被污染。
  // 1 亿 + 1 亿 = 2 亿，远小于 INT_MAX（约 21 亿），不会溢出；
  // 而 1 亿米 = 10 万公里，远大于任何院内距离（本项目全程 70 米），
  // 不会和真实距离混淆。
  static constexpr int kInf = 100000000;

  WeightedGraph() { init(0); }

  // 设定顶点个数并清空所有边，权值先全部置为不可达
  void init(int nodeCount) {
    n_ = (nodeCount < 0) ? 0 : (nodeCount > kMaxNodes ? kMaxNodes : nodeCount);
    for (int i = 0; i < kMaxNodes; ++i)
      for (int j = 0; j < kMaxNodes; ++j) dist_[i][j] = (i == j) ? 0 : kInf;
  }

  int nodeCount() const { return n_; }

  // 添加一条无向边，取较小权值。
  // 同一对顶点之间可能有多条路线（数据里写了多次），保留最短的那条 ——
  // 路是「可选」的，当然走近的。
  bool addEdge(int u, int v, int w) {
    if (u < 0 || u >= n_ || v < 0 || v >= n_ || w < 0) return false;
    if (w < dist_[u][v]) {
      dist_[u][v] = w;
      dist_[v][u] = w;  // 无向图，两端都登记；漏了这行就会出现「能去不能回」
    }
    return true;
  }

  // Floyd 算法：三重循环，逐轮允许经过更多的中间顶点
  //   dist[i][j] 最终为 i 到 j 的最短距离
  //   next[i][j] 为 i 到 j 路径上的下一个顶点，用于还原整条路线
  //
  //  k 必须在最外层，这是正确性前提。Floyd 的状态定义是
  //  「第 k 轮之后，dist[i][j] 表示只允许经过顶点 0..k 作为中转时的最短路」，
  //  所以 k 从小到大逐轮放宽限制。k 放内层就变成「允许重复经过某些中转点」，
  //  算出来的不是最短路。
  void floyd() {
    // 初始化 next：不经过任何中转点时，i 到 j 的第一跳就是 j（直接相连）
    for (int i = 0; i < n_; ++i)
      for (int j = 0; j < n_; ++j) next_[i][j] = (dist_[i][j] < kInf && i != j) ? j : -1;

    for (int k = 0; k < n_; ++k)      // 中转点
      for (int i = 0; i < n_; ++i)    // 起点
        for (int j = 0; j < n_; ++j)  // 终点
          if (dist_[i][k] + dist_[k][j] < dist_[i][j]) {
            dist_[i][j] = dist_[i][k] + dist_[k][j];
            next_[i][j] = next_[i][k];  // 继承 i->k 的第一跳
          }
  }

  int distance(int u, int v) const {
    if (u < 0 || v < 0 || u >= n_ || v >= n_) return kInf;
    return dist_[u][v];
  }

  bool reachable(int u, int v) const { return distance(u, v) < kInf; }

  // 还原 u 到 v 的路线，把途经顶点依次写入 path，返回顶点个数。
  //
  // 只记「第一跳」的 next 表让这里非常短：每次只需要问「从 cur 到 v 的第一跳是谁」。
  // 用迭代而不是递归：栈深度不会随路径长度增长。
  // while 的两个退出条件都是必要的 —— cur != -1 防 next 表异常时死循环，
  // n < cap 防写入超出调用方给的缓冲区。
  int buildPath(int u, int v, int* path, int cap) const {
    if (!reachable(u, v) || cap <= 0) return 0;
    int n = 0;
    int cur = u;
    while (cur != -1 && n < cap) {
      path[n++] = cur;
      if (cur == v) return n;  // 到达终点，成功
      cur = next_[cur][v];
    }
    return 0;  // 没走到 v（异常情况）
  }

 private:
  int dist_[kMaxNodes][kMaxNodes];
  int next_[kMaxNodes][kMaxNodes];
  int n_ = 0;
};

// =============================================================================
//  带权二部图：症状与科室为顶点，关联强度为边权
//
//  这是智能导诊的数学模型（题目实现提示第 1 条）。二部图的特点是顶点分成
//  互不相邻的两部分，边只存在于两部分之间，所以两侧的顶点分开存：
//      症状侧   症状表 symptomName_：每个症状是一个顶点
//      科室侧   由科室表提供，科室下标就是科室顶点号
//      边集     edge_：每条边的两个端点分属两侧，权值是关联强度
//
//  邻接表用「前向星」组织：每条边在边集中占两个结点，两侧各挂一次，
//  于是「症状 -> 关联的科室」和「科室 -> 关联的症状」都是 O(度) 的遍历：
//      symptomHead_[症状] -> 该症状关联的科室链
//      deptHead_[科室]    -> 该科室关联的症状链
//
//  顶点用名字作主键（症状名、科室下标），因此可以按名字加顶点、加边。
// =============================================================================
constexpr int kMaxSymptomName = 24;

class BipartiteGraph {
 public:
  static constexpr int kMaxWeight = 5;        // 关联强度的取值范围 1..5
  static constexpr int kMaxSymptoms = 64;     // 症状侧顶点数上限
  static constexpr int kMaxDepartments = 16;  // 科室侧顶点数上限
  // 边数上限。注意这是「结点数」不是「边数」—— 一条边占两个结点，
  // 所以最多 kMaxEdges/2 条边。
  static constexpr int kMaxEdges = 512;

  void clear() {
    symptomCount_ = 0;
    edgeCount_ = 0;
    deptCount_ = 0;
    symptomIndex_.clear();
    for (int i = 0; i < kMaxSymptoms; ++i) symptomHead_[i] = -1;
    for (int i = 0; i < kMaxDepartments; ++i) deptHead_[i] = -1;
  }

  // ------------------------------ 症状侧顶点 ------------------------------
  int symptomCount() const { return symptomCount_; }

  const char* symptomName(int symptom) const {
    return (symptom >= 0 && symptom < symptomCount_) ? symptomName_[symptom] : "";
  }

  int findSymptom(const char* name) const {
    int idx = -1;
    return symptomIndex_.get(name, idx) ? idx : -1;
  }

  // 新增症状顶点；名字已存在时返回原有顶点号，保证顶点不重复。
  //
  // 这个「幂等」是二部图能正确工作的前提：depts.txt 里多个科室都会关联
  // 「咳嗽」，读到第二个「咳嗽」时必须返回同一个顶点号，否则「咳嗽」会有
  // 多个不同顶点，患者选中其中一个时只有部分科室能匹配上，导诊会漏推荐。
  int addSymptom(const char* name) {
    const int exist = findSymptom(name);
    if (exist >= 0) return exist;
    if (symptomCount_ >= kMaxSymptoms) return -1;
    std::snprintf(symptomName_[symptomCount_], kMaxSymptomName, "%s", name);
    symptomHead_[symptomCount_] = -1;
    symptomIndex_.put(symptomName_[symptomCount_], symptomCount_);
    return symptomCount_++;
  }

  // ------------------------------ 科室侧顶点 ------------------------------
  // 科室顶点由科室表提供，科室下标即科室顶点号。
  //
  // 这个方法只在装载开始时调用一次：确定科室侧有多少个顶点（也就确定了边的
  // 两个端点能取哪些值），并把全部科室的邻接链头清空。
  // 之后装载过程只往图里加边，不再动顶点数 —— 否则会把已经建好的邻接链清掉，
  // 表现就是「从症状查得到科室、从科室查不到症状」。
  void setDepartmentCount(int count) {
    deptCount_ = (count < 0) ? 0 : (count > kMaxDepartments ? kMaxDepartments : count);
    for (int i = 0; i < kMaxDepartments; ++i) deptHead_[i] = -1;
  }

  int departmentCount() const { return deptCount_; }

  // ------------------------------ 边 ------------------------------
  int edgeCount() const { return edgeCount_; }

  // 加一条边（症状顶点 —— 科室顶点），权重 1..kMaxWeight。
  // 同一对顶点重复加边时**取较大权重**，不重复占边集。
  //
  // 注意和 WeightedGraph::addEdge 相反（那里取较小）：语义不同 ——
  //   距离是「越短越好」，多条路可选 -> 取小；
  //   关联强度是「越强越可信」，多处证据 -> 取大。
  bool addEdge(int symptom, int dept, int weight) {
    if (symptom < 0 || symptom >= symptomCount_) return false;
    if (dept < 0 || dept >= deptCount_) return false;
    if (weight < 1 || weight > kMaxWeight) return false;

    for (int e = symptomHead_[symptom]; e != -1; e = edge_[e].symptomNext) {
      if (edge_[e].dept == dept) {  // 这条边已经存在，只在权重更大时更新
        if (weight > edge_[e].weight) {
          edge_[e].weight = weight;
          // 另一侧的结点也要同步改 —— 一条边占两个结点，只改一个就会出现
          // 「从症状看强度是 5、从科室看是 3」这种自相矛盾的数据
          for (int r = deptHead_[dept]; r != -1; r = edge_[r].deptNext)
            if (edge_[r].symptom == symptom) edge_[r].weight = weight;
        }
        return true;  // 没有新增结点，但这是合法操作，不算失败
      }
    }
    if (edgeCount_ + 2 > kMaxEdges) return false;

    // 一条边在边集中占两个结点，两侧邻接链各用各的 next。
    //
    // 这一点必须分开，而且是本文件最容易写错的地方：
    // 症状侧的链是「某个症状关联了哪些科室」，科室侧的链是「某个科室关联了
    // 哪些症状」—— 这是两条独立的链。如果一条边只用一个 next 字段，
    // 那么把它挂到症状链上之后再挂到科室链上，就会覆盖掉前一个 next，
    // 结果是「从症状能查到科室，从科室却查不到症状」。
    //
    // 为什么这个 bug 特别难查：导诊算法走的正是「科室->症状」链，
    // 所以它会静默地给出错误的推荐结果 —— 不崩溃、不报错，只是推荐得不对；
    // 而「从症状查科室」的功能是好的，于是你会以为是算法或数据出了问题。
    const int forSymptom = edgeCount_;
    const int forDept = edgeCount_ + 1;
    edge_[forSymptom].dept = dept;
    edge_[forSymptom].symptom = symptom;
    edge_[forSymptom].weight = weight;
    edge_[forSymptom].deptNext = -1;
    edge_[forSymptom].symptomNext = symptomHead_[symptom];

    edge_[forDept].dept = dept;
    edge_[forDept].symptom = symptom;
    edge_[forDept].weight = weight;
    edge_[forDept].deptNext = deptHead_[dept];
    edge_[forDept].symptomNext = -1;

    symptomHead_[symptom] = forSymptom;
    deptHead_[dept] = forDept;
    edgeCount_ += 2;
    return true;
  }

  // ------------------------------ 邻接表遍历 ------------------------------
  // 症状 -> 它关联的科室（沿症状侧的链走）
  int firstDeptOf(int symptom) const {
    return (symptom >= 0 && symptom < symptomCount_) ? symptomHead_[symptom] : -1;
  }
  int nextDeptEdge(int edge) const {
    return (edge < 0 || edge >= edgeCount_) ? -1 : edge_[edge].symptomNext;
  }
  int edgeDept(int edge) const { return (edge < 0 || edge >= edgeCount_) ? -1 : edge_[edge].dept; }
  int edgeWeight(int edge) const { return (edge < 0 || edge >= edgeCount_) ? 0 : edge_[edge].weight; }

  // 科室 -> 它关联的症状（沿科室侧的链走）
  int firstSymptomOf(int dept) const {
    return (dept >= 0 && dept < deptCount_) ? deptHead_[dept] : -1;
  }
  int nextSymptomEdge(int edge) const {
    return (edge < 0 || edge >= edgeCount_) ? -1 : edge_[edge].deptNext;
  }
  int edgeSymptom(int edge) const {
    return (edge < 0 || edge >= edgeCount_) ? -1 : edge_[edge].symptom;
  }

  // 查两个顶点之间那条边的权值；无边返回 0
  int weightOf(int symptom, int dept) const {
    for (int e = firstDeptOf(symptom); e != -1; e = nextDeptEdge(e))
      if (edgeDept(e) == dept) return edgeWeight(e);
    return 0;
  }

 private:
  // 边结点。两个 next 字段分别服务两条链，绝不能合并成一个（见 addEdge 的说明）。
  struct Edge {
    int dept;         // 该边在科室侧的端点
    int symptom;      // 该边在症状侧的端点
    int weight;       // 关联强度
    int deptNext;     // 科室侧链上的下一条边（这个结点只挂在科室链上）
    int symptomNext;  // 症状侧链上的下一条边（-1 表示这条链到尾巴了）
  };

  Edge edge_[kMaxEdges];
  int symptomHead_[kMaxSymptoms];  // 症状 -> 科室链的链头
  int deptHead_[kMaxDepartments];  // 科室 -> 症状链的链头
  char symptomName_[kMaxSymptoms][kMaxSymptomName];
  HashMap symptomIndex_;  // 症状名 -> 症状顶点号
  int symptomCount_ = 0;
  int deptCount_ = 0;
  int edgeCount_ = 0;
};
