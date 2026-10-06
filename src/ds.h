/* =============================================================================
 *  ds.h  —  自行实现的数据结构
 *
 *  课程要求：不能使用现成的数据结构库（因为考查的就是数据结构与算法本身），
 *            字符串、数学等库不受限制。因此本文件只实现课程要求的数据结构，
 *            字符串处理直接使用 <cstring>。
 *
 *  本系统用到 4 种数据结构：
 *    SeqList    顺序表       —— 全部数据表的存储结构
 *    RingQueue  环形队列     —— 候诊队列
 *    HashMap    散列表       —— 编号到下标的索引
 *    RoadNet    邻接矩阵图   —— 就诊路线，用 Floyd 算法求最短路
 * ========================================================================== */
#pragma once

#include <cstdio>
#include <cstring>

// =============================================================================
//  顺序表：静态容量 + 实际长度。全部数据表都用它。
// =============================================================================
template <typename T, int N>
class SeqList {
 public:
  int size() const { return n_; }
  bool empty() const { return n_ == 0; }
  bool full() const { return n_ >= N; }

  T& operator[](int i) { return data_[i]; }
  const T& operator[](int i) const { return data_[i]; }

  // 尾部追加；容量已满返回 -1
  int push(const T& v) {
    if (n_ >= N) return -1;
    data_[n_] = v;
    return n_++;
  }

  // 按下标删除，后续元素前移
  bool removeAt(int i) {
    if (i < 0 || i >= n_) return false;
    for (int k = i; k + 1 < n_; ++k) data_[k] = data_[k + 1];
    --n_;
    return true;
  }

  void clear() { n_ = 0; }

 private:
  T data_[N];
  int n_ = 0;
};

// =============================================================================
//  环形队列：先进先出。候诊队列用它，出队位置可以被后续入队循环使用。
// =============================================================================
template <typename T, int N>
class RingQueue {
 public:
  bool empty() const { return n_ == 0; }
  bool full() const { return n_ >= N; }
  int size() const { return n_; }

  bool push(const T& v) {
    if (n_ >= N) return false;
    data_[tail_] = v;
    tail_ = (tail_ + 1) % N;
    ++n_;
    return true;
  }

  bool pop(T& out) {
    if (n_ == 0) return false;
    out = data_[head_];
    head_ = (head_ + 1) % N;
    --n_;
    return true;
  }

  // 按排队次序访问第 i 个元素（0 为队首），用于查看排队进度
  const T& at(int i) const { return data_[(head_ + i) % N]; }

  // 同上的可写版本。队列内部本来就是数组，按下标取引用不破坏先进先出的约束，
  // 只是允许就地修改队内元素（例如把「已叫号次数」加一），
  // 省去「逐个出队再放回」的来回搬运。
  T& at(int i) { return data_[(head_ + i) % N]; }

  // 删除队列中的第 i 个元素（患者看完病离开队列）。
  // 环形队列本只能从队首出队，这里用「逐个出队再放回」的办法保留其余元素的
  // 相对次序，代价是 O(n)，队列只有几十个人，可以接受。
  bool removeAt(int i) {
    if (i < 0 || i >= n_) return false;
    const int total = n_;
    for (int k = 0; k < total; ++k) {
      T v;
      if (!pop(v)) break;
      if (k == i) continue;  // 这一个丢掉，不放回
      push(v);
    }
    return true;
  }

  void clear() { head_ = tail_ = n_ = 0; }

 private:
  T data_[N];
  int head_ = 0;
  int tail_ = 0;
  int n_ = 0;
};

// =============================================================================
//  散列表：键是字符串编号（如 "K01"），值是数据表中的下标。
//  采用「链地址法」：每个桶挂一条同义词链，删除时不必修补探测链，实现更直观。
// =============================================================================
constexpr int kHashBuckets = 211;  // 取质数，减少冲突

struct HashNode {
  char key[16];
  int value;
  int next;  // 同义词链的下一个结点，-1 表示链尾
};

class HashMap {
 public:
  HashMap() { clear(); }

  void clear() {
    for (int i = 0; i < kHashBuckets; ++i) head_[i] = -1;
    nodeCount_ = 0;
  }

  int size() const { return nodeCount_; }

  // 插入或覆盖；成功返回 true，结点池满返回 false
  bool put(const char* key, int value) {
    const int b = bucket(key);
    for (int i = head_[b]; i != -1; i = node_[i].next) {
      if (std::strcmp(node_[i].key, key) == 0) {
        node_[i].value = value;  // 已有同名键则覆盖
        return true;
      }
    }
    if (nodeCount_ >= kMaxNodes) return false;
    std::strcpy(node_[nodeCount_].key, key);
    node_[nodeCount_].value = value;
    node_[nodeCount_].next = head_[b];
    head_[b] = nodeCount_++;
    return true;
  }

  // 查找；命中返回 true 并通过 out 传出下标
  bool get(const char* key, int& out) const {
    for (int i = head_[bucket(key)]; i != -1; i = node_[i].next) {
      if (std::strcmp(node_[i].key, key) == 0) {
        out = node_[i].value;
        return true;
      }
    }
    return false;
  }

 private:
  static constexpr int kMaxNodes = 1024;

  // 散列函数：把字符串的每个字符累加起来，再对桶数取余
  static int bucket(const char* key) {
    unsigned int h = 0;
    for (; *key; ++key) h = h * 31u + static_cast<unsigned char>(*key);
    return static_cast<int>(h % kHashBuckets);
  }

  HashNode node_[kMaxNodes];
  int head_[kHashBuckets];
  int nodeCount_ = 0;
};

// =============================================================================
//  邻接矩阵图 + Floyd 算法
//
//  就诊路线用的图。节点 = 房间（含楼梯口、电梯口），边 = 可行走的路线，权值 = 距离。
//  用邻接矩阵存边：节点只有几十个，矩阵很小，而且 Floyd 算法正好需要矩阵。
//
//  为什么用 Floyd 而不是 Dijkstra：
//    Floyd 一次算出「任意两点之间的最短路」，路线查询就变成查表 O(1)；
//    对固定不变的路网来说，这比每次查询都重新跑一遍 Dijkstra 更省时间。
//    代价是 O(V^3) 的预计算，V 只有几十，实际可忽略。
// =============================================================================
class RoadNet {
 public:
  static constexpr int kMaxNodes = 128;
  static constexpr int kInf = 100000000;  // 「不可达」的表示

  RoadNet() { init(0); }

  // 设定节点个数并清空所有边，权值先全部置为不可达
  void init(int nodeCount) {
    n_ = (nodeCount < 0) ? 0 : (nodeCount > kMaxNodes ? kMaxNodes : nodeCount);
    for (int i = 0; i < kMaxNodes; ++i)
      for (int j = 0; j < kMaxNodes; ++j) dist_[i][j] = (i == j) ? 0 : kInf;
  }

  int nodeCount() const { return n_; }

  // 添加一条无向边，取较小权值（同一对节点之间可能有多条路线，保留最短的）
  bool addEdge(int u, int v, int w) {
    if (u < 0 || u >= n_ || v < 0 || v >= n_ || w < 0) return false;
    if (w < dist_[u][v]) {
      dist_[u][v] = w;
      dist_[v][u] = w;
    }
    return true;
  }

  // Floyd 算法：三重循环，逐轮允许经过更多的中间节点
  //   dist[i][j] 最终为 i 到 j 的最短距离
  //   next[i][j] 为 i 到 j 路径上的下一个节点，用于还原整条路线
  void floyd() {
    for (int i = 0; i < n_; ++i)
      for (int j = 0; j < n_; ++j) next_[i][j] = (dist_[i][j] < kInf && i != j) ? j : -1;

    for (int k = 0; k < n_; ++k)             // 中转点
      for (int i = 0; i < n_; ++i)           // 起点
        for (int j = 0; j < n_; ++j)         // 终点
          if (dist_[i][k] + dist_[k][j] < dist_[i][j]) {
            dist_[i][j] = dist_[i][k] + dist_[k][j];
            next_[i][j] = next_[i][k];
          }
  }

  int distance(int u, int v) const {
    if (u < 0 || v < 0 || u >= n_ || v >= n_) return kInf;
    return dist_[u][v];
  }

  bool reachable(int u, int v) const { return distance(u, v) < kInf; }

  // 还原 u 到 v 的路线，把途经节点依次写入 path，返回节点个数（不含 v 则返回 0）
  int buildPath(int u, int v, int* path, int cap) const {
    if (!reachable(u, v) || cap <= 0) return 0;
    int n = 0;
    int cur = u;
    while (cur != -1 && n < cap) {
      path[n++] = cur;
      if (cur == v) return n;
      cur = next_[cur][v];
    }
    return 0;
  }

 private:
  int dist_[kMaxNodes][kMaxNodes];
  int next_[kMaxNodes][kMaxNodes];
  int n_ = 0;
};
