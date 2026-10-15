/* =============================================================================
 *  ds_base.h  —  基础工具：散列表
 *
 *  这是整个数据结构层最底下的一层，只依赖标准库的 <cstring>（字符串比较）。
 *
 *  为什么单独一个文件：散列表在系统里被两处用到 ——
 *    · 数据总库的 4 个索引（编号 -> 表内下标）
 *      （另外带权二部图里还有 1 个，见下）
 *    · 带权二部图的症状名索引（症状名 -> 顶点号）
 *  它是「工具」而不是「业务数据结构」，放在最底层，谁都能 include。
 *
 *  本文件不用模板：键固定是字符串，值固定是 int，没有泛型的必要。
 * ========================================================================== */
#pragma once

#include <cstdio>
#include <cstring>

// =============================================================================
//  散列表：键是字符串（编号或名字），值是数据表中的下标。
//
//  采用「链地址法」：每个桶挂一条同义词链，删除时不必修补探测链，实现更直观。
//
//  为什么选链地址法而不是开放定址法：
//    本系统需要删除（取消预约、重建索引）。开放定址法删元素时不能直接清空 ——
//    那会断掉探测链，让后面的元素再也找不到。要么打「已删除」标记（浪费空间、
//    拖慢查找），要么把后续元素前移（破坏其它键的位置）。链地址法删一个结点
//    只是改两个 next，实现直观得多。
//
//  内存布局的关键：**用「下标」代替指针**
//    head_[b] 存的是 node_[] 里的下标，next 也是下标，-1 表示链尾。
//    于是整个散列表是一大块连续内存：可以整体拷贝、可以放进静态存储区、
//    没有悬空指针。代价是多一次间接寻址，在几十条数据的规模下完全无感。
// =============================================================================
constexpr int kHashBuckets = 211;  // 取质数，减少冲突
constexpr int kHashKeyLen = 24;
constexpr int kHashMaxNodes = 1024;

struct HashNode {
  char key[kHashKeyLen];
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

  // 插入或覆盖；成功返回 true，结点池满返回 false。
  //
  // 同名键「覆盖」而不是「新增」：如果新增，重复装载同一份数据会让结点池
  // 白白翻倍，而且 get 会返回第一个匹配 —— 到底是新值还是旧值取决于头插顺序，
  // 行为不确定。覆盖保证语义明确：同一个键永远只有一份，值是最新的。
  bool put(const char* key, int value) {
    const int b = bucket(key);
    for (int i = head_[b]; i != -1; i = node_[i].next) {
      if (std::strcmp(node_[i].key, key) == 0) {
        node_[i].value = value;  // 已有同名键则覆盖
        return true;
      }
    }
    if (nodeCount_ >= kHashMaxNodes) return false;
    std::snprintf(node_[nodeCount_].key, kHashKeyLen, "%s", key);
    node_[nodeCount_].value = value;
    node_[nodeCount_].next = head_[b];  // 头插法：O(1)，不必遍历到链尾
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
  // 散列函数：把字符串当 31 进制数逐位累加，再对桶数取余。
  //
  // 三个讲究：
  //   1) 用多项式滚动（h*31+c）而不是简单累加 —— 累加会让「字符相同、顺序不同」
  //      的键撞在同一个桶，而本系统的编号（RA101 / RA110）恰恰只差字符顺序；
  //   2) 底数取 31：奇质数，且 h*31 能被编译器优化成 (h<<5)-h；
  //   3) 桶数取质数 211：如果取 2 的幂（如 256），h%256 相当于只取散列值低 8 位，
  //      而 D1~D36 这类编号前缀相同、后缀不同，取低位会让它们全集中到少数几个桶。
  //
  // 为什么要转 unsigned char：char 在 MSVC 上有符号，而中文的 UTF-8 字节值 > 127
  // 会被解释成负数，参与 h*31+c 之后散列值就乱了。
  //
  // **这个转换在本系统里是真的在用，不是防御性摆设**：
  //   · 数据总库那 4 个索引的键是编号（D1 / K01 / RA101），全是 ASCII；
  //   · 但带权二部图的 symptomIndex_ 用的是**中文症状名**作键
  //     （「胸闷」是 3 个汉字 = 6 个字节，每个都 > 127）。
  // 少了这个 cast，「胸闷」的散列会算出负数下标，查症状就查不到了。
  static int bucket(const char* key) {
    unsigned int h = 0;
    for (; *key; ++key) h = h * 31u + static_cast<unsigned char>(*key);
    return static_cast<int>(h % kHashBuckets);
  }

  HashNode node_[kHashMaxNodes];
  int head_[kHashBuckets];
  int nodeCount_ = 0;
};

/* =============================================================================
 *  复杂度与实测负载因子
 *
 *  理论上：查找 / 插入平均 O(1)，最坏 O(n)（全部键撞进一个桶，退化成链表）。
 *
 *  实际上：结点池 1024、桶 211，而系统只用了约 108 个键
 *  （12 科室 + 36 医生 + 12 患者 + 48 房间）：
 *      负载因子 = 108 / 211 ≈ 0.51
 *      平均链长 ≈ 0.51 个结点
 *  平均每条链连一个结点都不到，绝大多数查找是「算一次散列 + 比较一次字符串」就命中。
 * ========================================================================== */
