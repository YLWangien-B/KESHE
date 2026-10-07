/* =============================================================================
 *  selfcheck.cpp  —  数据结构调试程序
 *
 *  这个程序只做一件事：把 ds.h / ds_sort.h 里自己实现的数据结构拿出来，
 *  用一组可预期的输入去调用它，把实际结果和预期结果比一比。
 *
 *  —— 它和「医院」「科室」「医生」这些实际使用没有关系 ——
 *  这里没有数据文件、没有业务逻辑，每个结构都用自然构造的小例子验证：
 *  数、字符、简单的结构体。全部通过就说明这个数据结构是对的。
 *
 *  被验证的结构（在 ds.h / ds_sort.h 里）：
 *    SeqList         顺序表      —— 静态数组 + 长度
 *    RingQueue       环形队列    —— 先进先出，出队位置循环复用
 *    HashMap         散列表      —— 链地址法，字符串键 -> 下标
 *    ScheduleTable   二维数组    —— 按「行 × 列」两个下标定位
 *    WeightedGraph   带权图      —— 邻接矩阵 + Floyd 最短路
 *    BipartiteGraph  带权二部图  —— 两侧顶点 + 前向星邻接表
 *    insertSorted    插入排序    —— 有序插入，容量满时按规则决定是否挤入
 *
 *  用法：
 *    selfcheck                       跑全部
 *    selfcheck SeqList               只跑顺序表
 *    selfcheck SeqList HashMap       跑指定的几个
 *    selfcheck list                  看有哪些结构可以测
 *
 *  退出码：0 全部通过，1 有失败项。
 * ========================================================================== */
#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "ds.h"
#include "ds_sort.h"

// =============================================================================
//  自检框架
//
//  一个计数器 + 一个断言宏。通过的不打印；失败的连「期望什么、实际得到什么、
//  在源码哪一行」一起打出来，便于直接定位。
// =============================================================================
namespace check {

int gPassed = 0;
int gFailed = 0;
bool gStructFailed = false;  // 当前这个结构是否已经出现过失败

// 开一个结构的检查
void begin(const char* name) {
  gStructFailed = false;
  std::printf("\n%s\n", "============================================================");
  std::printf("  %s\n", name);
  std::printf("%s\n", "============================================================");
}

// 记一条结论
void report(bool ok, const char* what, const char* expect, const char* actual, const char* file, int line) {
  if (ok) {
    ++gPassed;
    return;
  }
  ++gFailed;
  gStructFailed = true;
  std::printf("  [失败] %s\n", what);
  std::printf("         期望：%s\n", expect);
  std::printf("         实际：%s\n", actual);
  std::printf("         位置：%s : %d\n", file, line);
}

void end() {
  if (!gStructFailed) std::printf("  -> 全部通过\n");
}

// 一句说明，交代这个结构在验什么
void note(const char* text) { std::printf("  · %s\n", text); }

}  // namespace check

// -----------------------------------------------------------------------------
//  断言宏
//
//  表达式必须只求值一次。一开始写成「比较时算一次、打印时再算一次」，
//  于是 a.push(x) 这种带副作用的调用被执行了两遍，看起来像是被检的顺序表
//  出了错，实际是断言宏自己的毛病。所以一律先存进局部常量。
// -----------------------------------------------------------------------------
#define EXPECT_INT(what, expect, actual)                                                          \
  do {                                                                                            \
    const long long e_ = (long long)(expect);                                                     \
    const long long a_ = (long long)(actual);                                                     \
    char es_[48], as_[48];                                                                        \
    std::snprintf(es_, sizeof(es_), "%lld", e_);                                                  \
    std::snprintf(as_, sizeof(as_), "%lld", a_);                                                  \
    check::report(e_ == a_, what, es_, as_, __FILE__, __LINE__);                                  \
  } while (0)

#define EXPECT_TRUE(what, cond)                                                                   \
  do {                                                                                            \
    const bool ok_ = (cond) ? true : false;                                                       \
    check::report(ok_, what, "true", ok_ ? "true" : "false", __FILE__, __LINE__);                 \
  } while (0)

#define EXPECT_FALSE(what, cond)                                                                  \
  do {                                                                                            \
    const bool ok_ = (cond) ? true : false;                                                       \
    check::report(!ok_, what, "false", ok_ ? "true" : "false", __FILE__, __LINE__);               \
  } while (0)

#define EXPECT_STR(what, expect, actual)                                                          \
  do {                                                                                            \
    const char* e_ = (expect);                                                                    \
    const char* a_ = (actual);                                                                    \
    check::report(std::strcmp(e_, a_) == 0, what, e_, a_, __FILE__, __LINE__);                    \
  } while (0)

// =============================================================================
//  一、顺序表 SeqList
//
//  存储：静态数组 + 实际长度。整个结构就是「一段连续的内存 + 用到哪儿了」。
//  验证要点：尾部追加、按下标随机访问、按下标删除后元素前移、容量上限、
//            清空、退化容量、能存结构体。
// =============================================================================
void testSeqList() {
  check::begin("SeqList  顺序表  —— 静态数组 + 长度");
  check::note("访问方式是「整体遍历 + 按下标随机访问」，所以用连续内存最合适。");

  // ---------- 空表 ----------
  SeqList<int, 4> a;
  EXPECT_INT("新表的长度为 0", 0, a.size());
  EXPECT_TRUE("新表是空的", a.empty());
  EXPECT_FALSE("新表不是满的", a.full());
  EXPECT_INT("容量就是模板参数 4", 4, (SeqList<int, 4>::capacity()));

  // ---------- 尾部追加 ----------
  // push 返回的是「新元素的下标」，满了返回 -1
  EXPECT_INT("push(10) 返回新元素下标 0", 0, a.push(10));
  EXPECT_INT("push(20) 返回新元素下标 1", 1, a.push(20));
  EXPECT_INT("push(30) 返回新元素下标 2", 2, a.push(30));
  EXPECT_INT("长度变成 3", 3, a.size());
  EXPECT_FALSE("还没满", a.full());

  // ---------- 按下标随机访问 ----------
  EXPECT_INT("a[0] = 10", 10, a[0]);
  EXPECT_INT("a[1] = 20", 20, a[1]);
  EXPECT_INT("a[2] = 30", 30, a[2]);

  // operator[] 返回的是引用，应当能就地修改
  a[1] = 99;
  EXPECT_INT("a[1] 改成 99 后读回 99", 99, a[1]);

  // ---------- 容量上限 ----------
  EXPECT_INT("push(40) 返回下标 3", 3, a.push(40));
  EXPECT_TRUE("装满后 full() 为真", a.full());
  EXPECT_INT("已满时 push(50) 返回 -1", -1, a.push(50));
  EXPECT_INT("失败的 push 不改变长度", 4, a.size());
  EXPECT_INT("失败的 push 不破坏原有内容", 10, a[0]);

  // ---------- 按下标删除 ----------
  // 删掉中间的元素，后面的元素应当整体前移，长度减一
  EXPECT_TRUE("removeAt(1) 成功", a.removeAt(1));
  EXPECT_INT("删除后长度是 3", 3, a.size());
  EXPECT_INT("a[0] 仍是 10", 10, a[0]);
  EXPECT_INT("a[1] 变成原来的 a[2] = 30", 30, a[1]);
  EXPECT_INT("a[2] 变成原来的 a[3] = 40", 40, a[2]);
  EXPECT_FALSE("越界删除 removeAt(3) 失败", a.removeAt(3));
  EXPECT_FALSE("负数删除 removeAt(-1) 失败", a.removeAt(-1));
  EXPECT_INT("失败的删除不改变长度", 3, a.size());
  EXPECT_TRUE("腾出空位后可以再 push", a.push(50) >= 0);

  // ---------- 删除首尾两个端点 ----------
  SeqList<int, 4> b;
  b.push(1);
  b.push(2);
  b.push(3);
  EXPECT_TRUE("删除第一个元素", b.removeAt(0));
  EXPECT_INT("删头后长度是 2", 2, b.size());
  EXPECT_INT("删头后 b[0] = 2", 2, b[0]);
  EXPECT_INT("删头后 b[1] = 3", 3, b[1]);
  EXPECT_TRUE("删除最后一个元素", b.removeAt(1));
  EXPECT_INT("删尾后长度是 1", 1, b.size());
  EXPECT_INT("删尾后 b[0] = 2", 2, b[0]);

  // ---------- 清空 ----------
  b.clear();
  EXPECT_INT("clear() 后长度为 0", 0, b.size());
  EXPECT_TRUE("clear() 后为空", b.empty());
  EXPECT_INT("clear() 后还能 push", 0, b.push(7));
  EXPECT_INT("重新 push 的值正确", 7, b[0]);

  // ---------- 退化容量 ----------
  SeqList<int, 1> one;
  EXPECT_INT("容量为 1 的表放得下一个", 0, one.push(7));
  EXPECT_INT("第二个就放不下了，返回 -1", -1, one.push(8));
  EXPECT_TRUE("容量为 1 的表满了", one.full());

  // ---------- 存结构体 ----------
  // 顺序表实际要存的是各种记录，所以元素是结构体也要能整体拷贝
  struct Row {
    int id;
    char name[8];
  };
  SeqList<Row, 2> rows;
  Row r;
  r.id = 5;
  std::snprintf(r.name, sizeof(r.name), "%s", "abc");
  rows.push(r);
  EXPECT_INT("结构体表里读回 id", 5, rows[0].id);
  EXPECT_STR("结构体表里读回 name", "abc", rows[0].name);
  rows[0].id = 6;
  EXPECT_INT("结构体表的元素能就地修改", 6, rows[0].id);

  check::end();
}

// =============================================================================
//  二、环形队列 RingQueue
//
//  存储：静态数组 + 头下标 + 尾下标 + 长度。头尾走到数组末尾时绕回开头，
//        于是出队腾出来的位置能被后来的入队循环使用。
//  验证要点：先进先出、回绕、按次序访问、按次序删除、队满队空、清空。
//
//  回绕是这个结构的核心：不做回绕（普通数组队列）的话，出队后前面的位置
//  就永远用不上了。这里特意让 head 和 tail 都绕过一圈来验证。
// =============================================================================
void testRingQueue() {
  check::begin("RingQueue  环形队列  —— 先进先出，出队位置循环复用");
  check::note("头尾下标都对 N 取余，所以绕回去之后次序仍然正确。");

  RingQueue<int, 4> q;
  EXPECT_TRUE("新队列是空的", q.empty());
  EXPECT_FALSE("新队列不满", q.full());
  EXPECT_INT("新队列长度为 0", 0, q.size());
  EXPECT_INT("容量就是模板参数 4", 4, (RingQueue<int, 4>::capacity()));

  // ---------- 先进先出 ----------
  // 注意 push / pop 返回的是「成功与否」，不是下标 —— 别把它当索引用。
  // 容量是 4，所以第 5 个入队会失败。
  EXPECT_TRUE("push(1) 成功", q.push(1));
  EXPECT_TRUE("push(3) 成功", q.push(3));
  EXPECT_TRUE("push(5) 成功", q.push(5));
  EXPECT_INT("长度为 3", 3, q.size());

  int v = 0;
  EXPECT_TRUE("pop 成功", q.pop(v));
  EXPECT_INT("先入先出：第一个出队的是 1", 1, v);
  EXPECT_TRUE("pop 成功", q.pop(v));
  EXPECT_INT("第二个出队的是 3", 3, v);
  EXPECT_INT("出队两次后长度为 1", 1, q.size());

  // 队内按排队次序访问，队首应当是剩下的 5
  EXPECT_INT("at(0) 是队首 5", 5, q.at(0));

  // ---------- 回绕 ----------
  // 现在 head 已经往前走了两格，再入队三次，tail 会绕回数组开头
  EXPECT_TRUE("push(7) 成功", q.push(7));
  EXPECT_TRUE("push(9) 成功", q.push(9));
  EXPECT_TRUE("push(11) 成功（此时 tail 已绕回数组开头）", q.push(11));
  EXPECT_TRUE("队列满了", q.full());
  EXPECT_FALSE("满队列再 push 返回 false", q.push(13));
  EXPECT_INT("失败的 push 不改变长度", 4, q.size());

  // 回绕之后，按排队次序读出来的仍应当是 5 7 9 11（物理位置是错开的）
  EXPECT_INT("回绕后 at(0) = 5", 5, q.at(0));
  EXPECT_INT("回绕后 at(1) = 7", 7, q.at(1));
  EXPECT_INT("回绕后 at(2) = 9", 9, q.at(2));
  EXPECT_INT("回绕后 at(3) = 11", 11, q.at(3));

  // 全部出队，次序仍应是先进先出
  const int expectOrder[4] = {5, 7, 9, 11};
  bool orderOk = true;
  for (int i = 0; i < 4; ++i) {
    int out = -1;
    if (!q.pop(out) || out != expectOrder[i]) orderOk = false;
  }
  EXPECT_TRUE("绕了一圈之后出队次序仍是先进先出", orderOk);
  EXPECT_TRUE("全部出队后队列为空", q.empty());
  EXPECT_INT("全部出队后长度为 0", 0, q.size());
  int nothing = -1;
  EXPECT_FALSE("空队列 pop 返回 false", q.pop(nothing));

  // ---------- 按排队次序删除 ----------
  RingQueue<int, 4> r;
  r.push(10);
  r.push(20);
  r.push(30);
  r.push(40);
  EXPECT_TRUE("removeAt(1)（删掉 20）成功", r.removeAt(1));
  EXPECT_INT("删除后长度是 3", 3, r.size());
  EXPECT_INT("剩下的第 1 个仍是 10", 10, r.at(0));
  EXPECT_INT("剩下的第 2 个是 30（次序没乱）", 30, r.at(1));
  EXPECT_INT("剩下的第 3 个是 40", 40, r.at(2));
  EXPECT_FALSE("越界 removeAt(3) 失败", r.removeAt(3));
  EXPECT_FALSE("负数 removeAt(-1) 失败", r.removeAt(-1));
  EXPECT_INT("失败的删除不改变长度", 3, r.size());
  EXPECT_TRUE("删掉队首", r.removeAt(0));
  EXPECT_INT("删队首后第 1 个是 30", 30, r.at(0));
  EXPECT_INT("删队首后长度是 2", 2, r.size());

  // ---------- 可写访问 ----------
  r.at(0) = 77;
  EXPECT_INT("at(i) 的写版本能就地修改队内元素", 77, r.at(0));
  EXPECT_INT("就地修改不影响长度", 2, r.size());

  // ---------- 清空 ----------
  r.clear();
  EXPECT_TRUE("clear() 后队列为空", r.empty());
  EXPECT_INT("clear() 后长度为 0", 0, r.size());
  EXPECT_TRUE("clear() 后还能重新入队", r.push(1));
  EXPECT_INT("重新入队后长度是 1", 1, r.size());

  // ---------- 反复进出，验证多次回绕不错乱 ----------
  // 小容量 + 多轮进出，能可靠地暴露「回绕写错」这类问题
  RingQueue<int, 8> busy;
  bool fifoOk = true;
  int nextIn = 0, nextOut = 0;
  for (int round = 0; round < 30; ++round) {
    for (int k = 0; k < 3 && !busy.full(); ++k) busy.push(nextIn++);
    int out = -1;
    if (busy.pop(out) && out != nextOut++) fifoOk = false;
  }
  EXPECT_TRUE("小队列反复进出 30 轮（多次回绕）后次序仍正确", fifoOk);

  check::end();
}

// =============================================================================
//  三、散列表 HashMap
//
//  存储：桶数组（211 个桶）+ 结点池（1024 个结点）。每个桶挂一条同义词链，
//        冲突的键就挂在同一条链上，靠 strcmp 分辨。
//  验证要点：插入与查找、同名键覆盖而不是新增、查不到、空键、
//            大量键（远多于桶数，必然冲突）仍能正确查找、清空。
// =============================================================================
void testHashMap() {
  check::begin("HashMap  散列表  —— 链地址法，字符串键 -> 下标");
  check::note("散列函数把字符串逐字符累加后对桶数取余；落进同一个桶的键挂在同一条链上。");

  HashMap m;
  EXPECT_INT("新表结点数为 0", 0, m.size());

  int out = -1;
  EXPECT_FALSE("空表里查不到任何键", m.get("A", out));

  // ---------- 插入与查找 ----------
  EXPECT_TRUE("put(A, 0) 成功", m.put("A", 0));
  EXPECT_TRUE("put(B, 1) 成功", m.put("B", 1));
  EXPECT_TRUE("put(C, 5) 成功", m.put("C", 5));
  EXPECT_INT("结点数为 3", 3, m.size());

  EXPECT_TRUE("能查到 A", m.get("A", out));
  EXPECT_INT("A -> 0", 0, out);
  EXPECT_TRUE("能查到 B", m.get("B", out));
  EXPECT_INT("B -> 1", 1, out);
  EXPECT_TRUE("能查到 C", m.get("C", out));
  EXPECT_INT("C -> 5", 5, out);

  // ---------- 查不到的各种情形 ----------
  out = -1;
  EXPECT_FALSE("没插过的键返回 false", m.get("D", out));
  EXPECT_FALSE("大小写不同算不同的键（A 与 a）", m.get("a", out));
  EXPECT_FALSE("空键查不到", m.get("", out));

  // ---------- 同名键覆盖 ----------
  // 覆盖不能新增结点，否则 size 会一直涨，而且旧值该被新值替掉
  EXPECT_TRUE("重复 put(A, 88) 成功", m.put("A", 88));
  EXPECT_INT("覆盖不新增结点，结点数仍是 3", 3, m.size());
  EXPECT_TRUE("A 仍然查得到", m.get("A", out));
  EXPECT_INT("A 的值已被覆盖为 88", 88, out);

  // ---------- 大量键：必然发生冲突 ----------
  // 桶只有 211 个，插 200 个键不会把桶占满，但一定有落到同一个桶的。
  // 这里验证的是「冲突了也能全部查回来」。
  HashMap big;
  char key[24];
  const int kCount = 200;
  bool putAllOk = true;
  for (int i = 0; i < kCount; ++i) {
    std::snprintf(key, sizeof(key), "K%03d", i);
    if (!big.put(key, i * 2)) putAllOk = false;
  }
  EXPECT_TRUE("连续插入 200 个键都成功", putAllOk);
  EXPECT_INT("结点数正好是 200", kCount, big.size());

  bool findAllOk = true;
  for (int i = 0; i < kCount; ++i) {
    std::snprintf(key, sizeof(key), "K%03d", i);
    if (!big.get(key, out) || out != i * 2) findAllOk = false;
  }
  EXPECT_TRUE("200 个键全部能查回原值（含发生冲突的）", findAllOk);

  out = -1;
  EXPECT_FALSE("这 200 个键之外的名字查不到", big.get("K200", out));

  // ---------- 前缀相同的键 ----------
  // 散列函数是逐字符累加的，名字相近的键要看散列后能不能分开
  HashMap alpha;
  for (int i = 0; i < 26; ++i) {
    char k[2];
    k[0] = static_cast<char>('A' + i);
    k[1] = '\0';
    alpha.put(k, i);
  }
  bool alphaOk = true;
  for (int i = 0; i < 26; ++i) {
    char k[2];
    k[0] = static_cast<char>('A' + i);
    k[1] = '\0';
    if (!alpha.get(k, out) || out != i) alphaOk = false;
  }
  EXPECT_TRUE("26 个单字母键全部正确", alphaOk);
  EXPECT_INT("单字母表结点数是 26", 26, alpha.size());

  // ---------- 清空 ----------
  big.clear();
  EXPECT_INT("clear() 后结点数为 0", 0, big.size());
  out = -1;
  EXPECT_FALSE("clear() 后原来的键查不到", big.get("K000", out));
  EXPECT_TRUE("clear() 后还能重新插入", big.put("K000", 7));
  EXPECT_TRUE("重新插入后查得到", big.get("K000", out));
  EXPECT_INT("查回的值是 7", 7, out);

  check::end();
}

// =============================================================================
//  四、二维数组 ScheduleTable
//
//  存储：二维静态数组 slot_[行][列]，两个下标就是主键。
//  验证要点：越界判断、未使用与已使用的区分、按两个下标定位互不干扰、
//            数量增减、余量不为负、清空。
//
//  这一节完全不涉及「排班」这个业务，只看「两个下标的二维表」本身的行为。
// =============================================================================
void testScheduleTable() {
  check::begin("ScheduleTable  二维数组  —— 两个下标直接定位");
  check::note("下标就是主键，定位是一次下标运算；未使用过的格子用标记位区分。");

  ScheduleTable<7, 4> t;
  EXPECT_INT("行数是模板参数 7", 7, (ScheduleTable<7, 4>::days()));
  EXPECT_INT("列数是模板参数 4", 4, (ScheduleTable<7, 4>::slots()));

  // ---------- 下标越界判断 ----------
  EXPECT_TRUE("行 0 合法", (ScheduleTable<7, 4>::validDay(0)));
  EXPECT_TRUE("行 6 合法", (ScheduleTable<7, 4>::validDay(6)));
  EXPECT_FALSE("行 7 越界", (ScheduleTable<7, 4>::validDay(7)));
  EXPECT_FALSE("行 -1 越界", (ScheduleTable<7, 4>::validDay(-1)));
  EXPECT_TRUE("列 0 合法", (ScheduleTable<7, 4>::validSlot(0)));
  EXPECT_TRUE("列 3 合法", (ScheduleTable<7, 4>::validSlot(3)));
  EXPECT_FALSE("列 4 越界", (ScheduleTable<7, 4>::validSlot(4)));
  EXPECT_FALSE("列 -1 越界", (ScheduleTable<7, 4>::validSlot(-1)));

  // ---------- 刚建好时所有格子都是「未使用」 ----------
  // 这是这个结构的关键：必须能区分「没填过」和「填了但值是 0」
  EXPECT_FALSE("新表 (0,0) 未使用", t.assigned(0, 0));
  EXPECT_FALSE("新表 (3,2) 未使用", t.assigned(3, 2));
  EXPECT_FALSE("新表 (6,3) 未使用", t.assigned(6, 3));
  EXPECT_INT("未使用的格子余量按 0 返回", 0, t.remaining(0, 0));

  // ---------- 按两个下标定位，互不干扰 ----------
  t.slot(2, 1).quota = 15;
  t.slot(2, 1).booked = 0;
  t.slot(2, 1).assigned = true;
  t.slot(2, 1).stopped = false;

  EXPECT_TRUE("(2,1) 变成已使用", t.assigned(2, 1));
  EXPECT_INT("(2,1) 的余量 = 15 - 0", 15, t.remaining(2, 1));
  EXPECT_INT("(2,1) 的上限读得回来", 15, t.slot(2, 1).quota);
  EXPECT_FALSE("写 (2,1) 不影响同一行的 (2,2)", t.assigned(2, 2));
  EXPECT_FALSE("写 (2,1) 不影响同一列的 (1,1)", t.assigned(1, 1));
  EXPECT_FALSE("写 (2,1) 不影响 (3,1)", t.assigned(3, 1));

  // ---------- 「未使用」与「已使用但余量为 0」必须分得开 ----------
  t.slot(1, 0).quota = 5;
  t.slot(1, 0).booked = 5;  // 用完了
  t.slot(1, 0).assigned = true;
  t.slot(1, 0).stopped = false;
  EXPECT_TRUE("(1,0) 是已使用（虽然余量为 0）", t.assigned(1, 0));
  EXPECT_INT("用完了的格子余量为 0", 0, t.remaining(1, 0));
  EXPECT_FALSE("(1,1) 仍未使用", t.assigned(1, 1));

  // ---------- 数量增减 ----------
  t.slot(2, 1).booked = 1;
  EXPECT_INT("扣掉 1 个后余量是 14", 14, t.remaining(2, 1));
  t.slot(2, 1).booked = 0;
  EXPECT_INT("退回后余量变回 15", 15, t.remaining(2, 1));
  EXPECT_TRUE("(2,1) 仍然是已使用", t.assigned(2, 1));

  // ---------- 标记位 ----------
  t.slot(5, 3).quota = 20;
  t.slot(5, 3).booked = 0;
  t.slot(5, 3).assigned = true;
  t.slot(5, 3).stopped = true;
  EXPECT_TRUE("置了停用标记的格子仍是「已使用」", t.assigned(5, 3));
  EXPECT_INT("停用的格子余量按 0 返回", 0, t.remaining(5, 3));

  // ---------- 余量不能出现负数 ----------
  // 否则「还有没有余量」的判断会反过来
  t.slot(6, 0).quota = 3;
  t.slot(6, 0).booked = 5;  // 故意超额
  t.slot(6, 0).assigned = true;
  t.slot(6, 0).stopped = false;
  EXPECT_INT("超额时余量按 0 返回，不出现负数", 0, t.remaining(6, 0));

  // ---------- 清空 ----------
  t.clear();
  EXPECT_FALSE("clear() 后 (2,1) 回到未使用", t.assigned(2, 1));
  EXPECT_FALSE("clear() 后 (1,0) 回到未使用", t.assigned(1, 0));
  EXPECT_FALSE("clear() 后 (5,3) 回到未使用", t.assigned(5, 3));
  EXPECT_FALSE("clear() 后 (6,0) 回到未使用", t.assigned(6, 0));
  EXPECT_INT("clear() 后余量为 0", 0, t.remaining(2, 1));

  check::end();
}

// =============================================================================
//  五、带权图 WeightedGraph
//
//  存储：邻接矩阵 dist_[i][j]（不可达用一个很大的数表示）+ 路径矩阵 next_[i][j]。
//  算法：Floyd，三重循环逐轮允许经过更多的中转点。
//  验证要点：不可达的表示、加边取较小权值、Floyd 中转、路径还原、
//            孤立顶点、越界与非法参数、重新 init。
//
//  这里用自然构造的小图，不涉及任何真实路网。
// =============================================================================
void testWeightedGraph() {
  check::begin("WeightedGraph  带权图  —— 邻接矩阵 + Floyd 最短路");
  check::note("Floyd 逐轮放宽「允许经过哪些中转点」，一轮放一个，做完就是任意两点最短路。");

  WeightedGraph g;
  g.init(5);

  EXPECT_INT("顶点数是 5", 5, g.nodeCount());
  EXPECT_INT("顶点到自己的距离是 0", 0, g.distance(0, 0));
  EXPECT_INT("没加边时两点不可达（用一个很大的数表示）", WeightedGraph::kInf, (g.distance(0, 1)));
  EXPECT_FALSE("没加边时两点不连通", g.reachable(0, 1));

  // ---------- 加边 ----------
  EXPECT_TRUE("加边 0-1 距离 10", g.addEdge(0, 1, 10));
  EXPECT_TRUE("加边 1-2 距离 10", g.addEdge(1, 2, 10));
  EXPECT_TRUE("加边 2-3 距离 5", g.addEdge(2, 3, 5));
  EXPECT_TRUE("加边 0-4 距离 100", g.addEdge(0, 4, 100));

  EXPECT_INT("相邻两点距离读回 10", 10, g.distance(0, 1));
  EXPECT_INT("图是无向的：1 到 0 也是 10", 10, g.distance(1, 0));
  EXPECT_TRUE("0 能到 1", g.reachable(0, 1));

  // ---------- 同一对顶点重复加边，保留较小的权值 ----------
  // 现实中同一对顶点之间可能有多条边，留最短的那条
  EXPECT_TRUE("重复加边 0-1 距离 30", g.addEdge(0, 1, 30));
  EXPECT_INT("取较小权值，仍是 10", 10, g.distance(0, 1));
  EXPECT_TRUE("再来一条更短的 0-1 距离 4", g.addEdge(0, 1, 4));
  EXPECT_INT("更短的会覆盖，变成 4", 4, g.distance(0, 1));

  // 此刻的边集是：0-1=4、1-2=10、2-3=5、0-4=100
  // 下面所有期望值都按这个边集算

  // ---------- Floyd：没有直接边的两点靠中转 ----------
  // 只加了边、还没跑 Floyd，此时 0 到 2 应当仍不可达
  EXPECT_FALSE("跑 Floyd 之前 0 到 2 不可达（它们之间没有直接边）", g.reachable(0, 2));
  EXPECT_INT("跑 Floyd 之前 0 到 2 的距离是 kInf", WeightedGraph::kInf, (g.distance(0, 2)));

  g.floyd();
  EXPECT_INT("Floyd 后 0 到 2 = 4 + 10", 14, g.distance(0, 2));
  EXPECT_INT("Floyd 后 0 到 3 = 4 + 10 + 5", 19, g.distance(0, 3));
  EXPECT_INT("Floyd 后 1 到 3 = 10 + 5", 15, g.distance(1, 3));
  EXPECT_INT("直达的 0 到 4 仍是 100（绕路要 119，更贵）", 100, g.distance(0, 4));
  EXPECT_TRUE("Floyd 后 0 能到 3", g.reachable(0, 3));
  EXPECT_INT("点到自己的距离仍是 0", 0, g.distance(2, 2));
  EXPECT_TRUE("距离矩阵是对称的：3 到 0 也是 19", g.distance(3, 0) == 19);

  // 这个图整体连通：3 要经 2、1、0 才能到 4
  EXPECT_TRUE("3 经 2、1、0 能绕到 4", g.reachable(3, 4));
  EXPECT_INT("3 到 4 = 19 + 100", 119, g.distance(3, 4));
  EXPECT_TRUE("1 到 4 = 4 + 100 = 104，且对称", g.distance(1, 4) == 104 && g.distance(4, 1) == 104);

  // ---------- 路径还原 ----------
  int path[8];
  int n = g.buildPath(0, 3, path, 8);
  EXPECT_INT("0 到 3 途经 4 个顶点", 4, n);
  EXPECT_INT("路径第 1 个是起点 0", 0, path[0]);
  EXPECT_INT("路径第 2 个是 1", 1, path[1]);
  EXPECT_INT("路径第 3 个是 2", 2, path[2]);
  EXPECT_INT("路径最后一个是终点 3", 3, path[3]);

  EXPECT_INT("相邻两点的路径只含 2 个顶点", 2, g.buildPath(0, 1, path, 8));
  EXPECT_INT("直达的路径含 2 个顶点", 2, g.buildPath(0, 4, path, 8));

  n = g.buildPath(3, 4, path, 8);
  EXPECT_INT("绕路的路径 3-2-1-0-4 含 5 个顶点", 5, n);
  EXPECT_INT("绕路路径第 1 个是 3", 3, path[0]);
  EXPECT_INT("绕路路径第 2 个是 2", 2, path[1]);
  EXPECT_INT("绕路路径第 3 个是 1", 1, path[2]);
  EXPECT_INT("绕路路径第 4 个是 0", 0, path[3]);
  EXPECT_INT("绕路路径最后一个是 4", 4, path[4]);

  // ---------- 真正的孤立顶点 ----------
  // 上面那个图整体是连通的，要验证「不可达」得另建一个谁也不连的顶点
  WeightedGraph iso;
  iso.init(4);
  iso.addEdge(0, 1, 10);
  iso.addEdge(1, 2, 10);  // 顶点 3 谁也不连
  iso.floyd();
  EXPECT_FALSE("孤立顶点 3 与 0 不连通", iso.reachable(0, 3));
  EXPECT_INT("不连通时距离是 kInf", WeightedGraph::kInf, (iso.distance(0, 3)));
  EXPECT_INT("不连通的反向也是 kInf", WeightedGraph::kInf, (iso.distance(3, 0)));
  EXPECT_INT("不连通时 buildPath 返回 0", 0, iso.buildPath(0, 3, path, 8));
  EXPECT_INT("不连通的反向也返回 0", 0, iso.buildPath(3, 0, path, 8));
  EXPECT_TRUE("孤立顶点到自己是连通的", iso.reachable(3, 3));
  EXPECT_INT("孤立顶点到自己的距离是 0", 0, iso.distance(3, 3));

  // ---------- 更短的绕路胜过更长的直达 ----------
  // 这是最短路的核心
  WeightedGraph h;
  h.init(3);
  h.addEdge(0, 2, 100);  // 直达很贵
  h.addEdge(0, 1, 1);
  h.addEdge(1, 2, 1);  // 绕一下只要 2
  h.floyd();
  EXPECT_INT("绕路 2 胜过直达 100", 2, h.distance(0, 2));
  EXPECT_INT("还原出的路径走的是绕路（3 个顶点）", 3, h.buildPath(0, 2, path, 8));

  // ---------- 越界与非法参数一律拒绝 ----------
  EXPECT_FALSE("加边时顶点越界被拒绝", g.addEdge(0, 9, 5));
  EXPECT_FALSE("加边时负数顶点被拒绝", g.addEdge(-1, 1, 5));
  EXPECT_FALSE("加边时负权值被拒绝", g.addEdge(0, 1, -5));
  EXPECT_INT("越界查询距离返回 kInf", WeightedGraph::kInf, (g.distance(0, 99)));
  EXPECT_FALSE("越界查询 reachable 为假", g.reachable(0, 99));
  EXPECT_FALSE("负数下标的查询也为假", g.reachable(-1, 0));
  EXPECT_FALSE("两端都越界的查询为假", g.reachable(99, 99));
  EXPECT_INT("越界取路径返回 0", 0, g.buildPath(0, 99, path, 8));
  EXPECT_INT("容量不够放整条路径时返回 0", 0, g.buildPath(0, 3, path, 1));

  // ---------- 重新 init 应当把边全部清掉 ----------
  h.init(3);
  EXPECT_INT("重新 init 后顶点数仍是 3", 3, h.nodeCount());
  EXPECT_INT("重新 init 后 0 到 1 不可达", WeightedGraph::kInf, (h.distance(0, 1)));
  g.init(0);
  EXPECT_INT("init(0) 后顶点数是 0", 0, g.nodeCount());
  EXPECT_INT("没有顶点时查询返回 kInf", WeightedGraph::kInf, (g.distance(0, 0)));

  check::end();
}

// =============================================================================
//  六、带权二部图 BipartiteGraph
//
//  存储：两侧顶点表 + 前向星邻接表（链头数组 + 边集）。
//        每条边在边集中占两个结点，两侧各挂一次。
//  验证要点：两侧顶点各自独立、加边要求两端都存在、权值范围、重复加边取大、
//            两个方向都能遍历、两侧数出的边数一致、清空。
//
//  最后那组「两侧边数一致」的断言是这个结构最容易写错的地方：如果一条边的
//  两个端点结点共用一个 next 字段，两条链会互相覆盖，表现就是「从一侧查得到、
//  从另一侧查不到」。
// =============================================================================
void testBipartiteGraph() {
  check::begin("BipartiteGraph  带权二部图  —— 两侧顶点 + 前向星邻接表");
  check::note("顶点分成互不相邻的两侧，边只存在于两侧之间；两条邻接链各自独立。");

  BipartiteGraph g;
  g.clear();

  EXPECT_INT("新图没有左侧顶点", 0, g.symptomCount());
  EXPECT_INT("新图没有右侧顶点", 0, g.departmentCount());
  EXPECT_INT("新图没有边", 0, g.edgeCount());

  // ---------- 左侧顶点 ----------
  const int s0 = g.addSymptom("alpha");
  const int s1 = g.addSymptom("beta");
  const int s2 = g.addSymptom("gamma");
  const int s3 = g.addSymptom("delta");
  EXPECT_INT("第 1 个左侧顶点号是 0", 0, s0);
  EXPECT_INT("第 2 个是 1", 1, s1);
  EXPECT_INT("第 4 个是 3", 3, s3);
  EXPECT_INT("左侧顶点数是 4", 4, g.symptomCount());
  EXPECT_STR("顶点名读得回来", "alpha", g.symptomName(0));
  EXPECT_STR("第 3 个顶点名是 gamma", "gamma", g.symptomName(2));
  EXPECT_INT("按名字查得到顶点号", 1, g.findSymptom("beta"));
  EXPECT_INT("查不到的名字返回 -1", -1, g.findSymptom("zeta"));
  EXPECT_INT("空名字查不到", -1, g.findSymptom(""));

  // 重复加同名顶点应当返回原有的顶点号，不新增
  EXPECT_INT("重复加同名顶点返回原有顶点号", 0, g.addSymptom("alpha"));
  EXPECT_INT("重复加顶点不增加顶点数", 4, g.symptomCount());

  EXPECT_INT("越界取顶点名得到空串", 0, (int)std::strlen(g.symptomName(99)));
  EXPECT_INT("负数取顶点名得到空串", 0, (int)std::strlen(g.symptomName(-1)));

  // ---------- 右侧顶点数还没设，加边应当失败 ----------
  EXPECT_FALSE("右侧顶点数为 0 时加边被拒绝", g.addEdge(s0, 0, 3));

  g.setDepartmentCount(3);
  EXPECT_INT("右侧顶点数是 3", 3, g.departmentCount());

  // ---------- 加边 ----------
  EXPECT_TRUE("加边 alpha-0 权值 3", g.addEdge(s0, 0, 3));
  EXPECT_TRUE("加边 beta-0 权值 3", g.addEdge(s1, 0, 3));
  EXPECT_TRUE("加边 gamma-0 权值 2", g.addEdge(s2, 0, 2));
  EXPECT_TRUE("加边 delta-1 权值 3", g.addEdge(s3, 1, 3));
  EXPECT_TRUE("加边 gamma-1 权值 2", g.addEdge(s2, 1, 2));
  EXPECT_TRUE("加边 delta-2 权值 2", g.addEdge(s3, 2, 2));
  EXPECT_INT("6 条边在边集中占 12 个结点", 12, g.edgeCount());

  // ---------- 边的权值查询 ----------
  EXPECT_INT("alpha-0 的权值是 3", 3, g.weightOf(s0, 0));
  EXPECT_INT("delta-2 的权值是 2", 2, g.weightOf(s3, 2));
  EXPECT_INT("没有边的两点返回 0（alpha-1）", 0, g.weightOf(s0, 1));

  // ---------- 边界与非法参数 ----------
  EXPECT_FALSE("左侧顶点越界被拒绝", g.addEdge(9, 0, 3));
  EXPECT_FALSE("右侧顶点越界被拒绝", g.addEdge(s0, 9, 3));
  EXPECT_FALSE("负数顶点被拒绝", g.addEdge(-1, 0, 3));
  EXPECT_FALSE("权值为 0 被拒绝（边的权值至少是 1）", g.addEdge(s0, 1, 0));
  EXPECT_FALSE("负权值被拒绝", g.addEdge(s0, 1, -1));
  EXPECT_FALSE("权值超过上限被拒绝", (g.addEdge(s0, 1, BipartiteGraph::kMaxWeight + 1)));

  // ---------- 重复加同一条边：取较大权值，不新增边 ----------
  EXPECT_TRUE("重复加 alpha-0 权值 1", g.addEdge(s0, 0, 1));
  EXPECT_INT("重复加边不新增结点", 12, g.edgeCount());
  EXPECT_INT("取较大权值，仍是 3", 3, g.weightOf(s0, 0));
  EXPECT_TRUE("重复加 alpha-0 权值 5（更大）", g.addEdge(s0, 0, 5));
  EXPECT_INT("权值被更新为 5", 5, g.weightOf(s0, 0));
  EXPECT_TRUE("重复加 alpha-0 权值 2（更小）", g.addEdge(s0, 0, 2));
  EXPECT_INT("较小权值不覆盖较大的", 5, g.weightOf(s0, 0));
  g.addEdge(s0, 0, 3);  // 还原成 3

  // ---------- 遍历方向一：左侧顶点 -> 它关联的右侧顶点 ----------
  int count = 0;
  bool seen[3] = {false, false, false};
  for (int e = g.firstDeptOf(s3); e != -1; e = g.nextDeptEdge(e)) {
    const int d = g.edgeDept(e);
    if (d >= 0 && d < 3) seen[d] = true;
    ++count;
  }
  EXPECT_INT("delta 关联 2 个右侧顶点", 2, count);
  EXPECT_TRUE("delta 关联到 1", seen[1]);
  EXPECT_TRUE("delta 关联到 2", seen[2]);
  EXPECT_FALSE("delta 不关联 0", seen[0]);

  count = 0;
  for (int e = g.firstDeptOf(s1); e != -1; e = g.nextDeptEdge(e)) ++count;
  EXPECT_INT("beta 只关联 1 个右侧顶点", 1, count);

  count = 0;
  for (int e = g.firstDeptOf(s0); e != -1; e = g.nextDeptEdge(e)) ++count;
  EXPECT_INT("alpha 只关联 1 个右侧顶点", 1, count);

  // ---------- 遍历方向二：右侧顶点 -> 它关联的左侧顶点 ----------
  // 这一侧最容易出错：两个 next 共用一个的话，这里会数出 0
  count = 0;
  bool seenSym[4] = {false, false, false, false};
  for (int e = g.firstSymptomOf(0); e != -1; e = g.nextSymptomEdge(e)) {
    const int s = g.edgeSymptom(e);
    if (s >= 0 && s < 4) seenSym[s] = true;
    ++count;
  }
  EXPECT_INT("右侧顶点 0 关联 3 个左侧顶点", 3, count);
  EXPECT_TRUE("关联到 alpha", seenSym[0]);
  EXPECT_TRUE("关联到 beta", seenSym[1]);
  EXPECT_TRUE("关联到 gamma", seenSym[2]);
  EXPECT_FALSE("不关联 delta", seenSym[3]);

  count = 0;
  for (int e = g.firstSymptomOf(1); e != -1; e = g.nextSymptomEdge(e)) ++count;
  EXPECT_INT("右侧顶点 1 关联 2 个左侧顶点", 2, count);
  count = 0;
  for (int e = g.firstSymptomOf(2); e != -1; e = g.nextSymptomEdge(e)) ++count;
  EXPECT_INT("右侧顶点 2 关联 1 个左侧顶点", 1, count);

  // ---------- 两侧数出来的边数必须一致 ----------
  // 这是这个结构最关键的对称性：每条边在两侧各挂一次，
  // 一侧数得少就说明那条链被覆盖了。
  int fromLeft = 0;
  for (int s = 0; s < g.symptomCount(); ++s)
    for (int e = g.firstDeptOf(s); e != -1; e = g.nextDeptEdge(e)) ++fromLeft;
  int fromRight = 0;
  for (int d = 0; d < g.departmentCount(); ++d)
    for (int e = g.firstSymptomOf(d); e != -1; e = g.nextSymptomEdge(e)) ++fromRight;
  EXPECT_INT("从左侧数出的边数 = 从右侧数出的边数", fromLeft, fromRight);
  EXPECT_INT("数出来的边数就是 6", 6, fromLeft);
  EXPECT_INT("边集结点数是边数的两倍", 12, fromLeft * 2);

  // ---------- 越界的遍历起点 ----------
  EXPECT_INT("越界左侧顶点的链为空", -1, g.firstDeptOf(99));
  EXPECT_INT("负数左侧顶点的链为空", -1, g.firstDeptOf(-1));
  EXPECT_INT("越界右侧顶点的链为空", -1, g.firstSymptomOf(99));
  EXPECT_INT("负数右侧顶点的链为空", -1, g.firstSymptomOf(-1));
  EXPECT_INT("越界边的 next 返回 -1", -1, g.nextDeptEdge(999));
  EXPECT_INT("越界边读不到端点", -1, g.edgeDept(999));
  EXPECT_INT("越界边读不到权值", 0, g.edgeWeight(999));

  // ---------- 清空 ----------
  g.clear();
  EXPECT_INT("clear() 后左侧顶点数为 0", 0, g.symptomCount());
  EXPECT_INT("clear() 后右侧顶点数为 0", 0, g.departmentCount());
  EXPECT_INT("clear() 后边数为 0", 0, g.edgeCount());
  EXPECT_INT("clear() 后原来的顶点名查不到", -1, g.findSymptom("alpha"));
  EXPECT_INT("clear() 后可以重新加顶点", 0, g.addSymptom("omega"));

  check::end();
}

// =============================================================================
//  七、插入排序 insertSorted
//
//  算法：把新元素插到「已经排好序」的数组里的正确位置，后面的元素整体后移；
//        数组满了之后，只有比最后一名更优先的才挤掉它。
//  验证要点：降序保持、容量满时的挤入与不挤入、相同优先级的稳定性、
//            空数组、容量为 1、换一个比较规则。
//
//  这一节完全不涉及「科室」「得分」这些业务概念，用的都是自然构造的例子。
// =============================================================================
namespace sortdemo {
// 用来验证的简单元素：一个编号 + 一个分数
struct Item {
  int id;
  int score;
};
}  // namespace sortdemo

void testInsertSorted() {
  check::begin("insertSorted  插入排序  —— 有序插入，满了按规则挤入");
  check::note("排序规则由比较函数给出，算法本身不关心元素是什么、往哪个方向排。");

  using sortdemo::Item;
  const int kCap = 5;
  Item a[kCap];
  int n = 0;
  const GreaterScore less;  // 规则：分数低的排到后面（即按分数从大到小）

  // ---------- 空数组插一条 ----------
  Item one{7, 1};
  insertSorted(a, n, kCap, one, less);
  EXPECT_INT("空数组插一条后长度是 1", 1, n);
  EXPECT_INT("那一条的编号正确", 7, a[0].id);
  EXPECT_INT("那一条的分数正确", 1, a[0].score);

  // ---------- 乱序插入，应当保持降序 ----------
  Item v2{2, 9};
  insertSorted(a, n, kCap, v2, less);
  EXPECT_INT("插第二条后长度是 2", 2, n);
  EXPECT_INT("分数高的排在第 1 位", 9, a[0].score);
  EXPECT_INT("分数低的排在第 2 位", 1, a[1].score);

  Item v3{3, 6};
  insertSorted(a, n, kCap, v3, less);
  EXPECT_INT("插第三条后长度是 3", 3, n);
  EXPECT_INT("第 1 名 9", 9, a[0].score);
  EXPECT_INT("第 2 名 6", 6, a[1].score);
  EXPECT_INT("第 3 名 1", 1, a[2].score);
  EXPECT_INT("第 2 名就是刚插进去的那条", 3, a[1].id);

  // ---------- 插满 ----------
  Item v4{4, 8}, v5{5, 3};
  insertSorted(a, n, kCap, v4, less);
  insertSorted(a, n, kCap, v5, less);
  EXPECT_INT("插满后长度是容量 5", 5, n);
  EXPECT_INT("降序：第 1 名 9", 9, a[0].score);
  EXPECT_INT("降序：第 2 名 8", 8, a[1].score);
  EXPECT_INT("降序：第 3 名 6", 6, a[2].score);
  EXPECT_INT("降序：第 4 名 3", 3, a[3].score);
  EXPECT_INT("降序：第 5 名 1", 1, a[4].score);

  // ---------- 满了之后：更好的成绩挤掉最后一名 ----------
  // 这是「只保留前 cap 名」的核心语义：榜单满了，但来了更优先的，
  // 就该把垫底的那个挤掉，而不是把这个更好的丢掉。
  Item better{99, 2};  // 分数 2 比最后一名（1）高
  insertSorted(a, n, kCap, better, less);
  EXPECT_INT("比最后一名优先时长度不变（仍是 5）", 5, n);
  EXPECT_INT("新元素进榜，排在最后一位", 2, a[4].score);
  EXPECT_INT("最后一位就是它", 99, a[4].id);
  bool bottleneckGone = true;
  for (int i = 0; i < n; ++i)
    if (a[i].score == 1) bottleneckGone = false;
  EXPECT_TRUE("原来垫底的那条（分数 1）被挤出去了", bottleneckGone);

  // ---------- 满了之后：更差的成绩挤不进来 ----------
  Item worse{77, 0};  // 分数 0 比现在最后一名（2）低
  insertSorted(a, n, kCap, worse, less);
  EXPECT_INT("比最后一名差时长度不变", 5, n);
  bool noWorse = true;
  for (int i = 0; i < n; ++i)
    if (a[i].id == 77) noWorse = false;
  EXPECT_TRUE("被丢弃的元素没有出现在数组里", noWorse);
  EXPECT_INT("最后一名仍是 2", 2, a[4].score);

  // ---------- 满了之后：最高分挤进第 1 位 ----------
  Item high{88, 100};
  insertSorted(a, n, kCap, high, less);
  EXPECT_INT("比最后一名高，长度仍是容量 5", 5, n);
  EXPECT_INT("它插到了第 1 位", 100, a[0].score);
  EXPECT_INT("第 1 位就是它", 88, a[0].id);
  EXPECT_INT("挤入后仍是降序：第 2 名 9", 9, a[1].score);
  EXPECT_INT("挤入后仍是降序：第 5 名 3", 3, a[4].score);

  // ---------- 相同优先级：先来的保持在前（稳定） ----------
  // 这一点对结果的可复现很重要：同样的输入必须得到同样的次序
  Item tie[kCap];
  int tn = 0;
  for (int i = 0; i < kCap; ++i) {
    Item t{i, 5};
    insertSorted(tie, tn, kCap, t, less);
  }
  EXPECT_INT("同分的插满 5 条", 5, tn);
  EXPECT_INT("同分时第 1 位是先来的", 0, tie[0].id);
  EXPECT_INT("同分时第 2 位是第二个来的", 1, tie[1].id);
  EXPECT_INT("同分时最后一位是第 5 个来的", 4, tie[4].id);
  Item late{100, 5};
  insertSorted(tie, tn, kCap, late, less);
  EXPECT_INT("同分的新元素挤不进来，长度不变", 5, tn);
  EXPECT_INT("同分时先来的仍在第 1 位", 0, tie[0].id);

  // ---------- 容量为 1 的退化情形 ----------
  Item tiny[1];
  int tinyN = 0;
  Item t1{1, 5};
  insertSorted(tiny, tinyN, 1, t1, less);
  EXPECT_INT("容量 1 时能放进一条", 1, tinyN);
  EXPECT_INT("放进去的就是它", 1, tiny[0].id);
  Item t2{2, 1};  // 分数 1 比 5 低
  insertSorted(tiny, tinyN, 1, t2, less);
  EXPECT_INT("容量 1 时更差的挤不进来", 1, tinyN);
  EXPECT_INT("留下的还是原来那条", 1, tiny[0].id);
  Item t3{3, 9};  // 分数 9 比 5 高
  insertSorted(tiny, tinyN, 1, t3, less);
  EXPECT_INT("容量 1 时更好的能替换掉它", 1, tinyN);
  EXPECT_INT("替换后是新的高分那条", 3, tiny[0].id);
  Item t4{4, 5};  // 比当前那条（9）差
  insertSorted(tiny, tinyN, 1, t4, less);
  EXPECT_INT("容量 1 时更差的再挤不进来", 1, tinyN);
  EXPECT_INT("留下的仍是 9 那条", 3, tiny[0].id);

  // ---------- 换一个比较规则，得到相反的次序 ----------
  // 这说明算法和元素类型、排序方向都无关
  int nums[8];
  int nn = 0;
  for (int k = 0; k < 6; ++k) {
    const int value = (k * 7) % 10;  // 依次是 0 7 4 1 8 5
    insertSorted(nums, nn, 8, value, Ascending<int>());
  }
  EXPECT_INT("用升序规则插了 6 个", 6, nn);
  bool ascendingOk = true;
  for (int i = 1; i < nn; ++i)
    if (nums[i - 1] > nums[i]) ascendingOk = false;
  EXPECT_TRUE("换一个比较规则就得到升序（算法与排序方向无关）", ascendingOk);
  EXPECT_INT("升序后第 1 个是最小的 0", 0, nums[0]);
  EXPECT_INT("升序后最后一个是最大的 8", 8, nums[5]);

  check::end();
}

// =============================================================================
//  主程序：按名字选择要测的结构
// =============================================================================
namespace {

struct Entry {
  const char* name;
  void (*run)();
  const char* brief;
};

const Entry kTests[] = {
    {"SeqList", testSeqList, "顺序表 —— 静态数组 + 长度"},
    {"RingQueue", testRingQueue, "环形队列 —— 先进先出，出队位置循环复用"},
    {"HashMap", testHashMap, "散列表 —— 链地址法，字符串键 -> 下标"},
    {"ScheduleTable", testScheduleTable, "二维数组 —— 两个下标直接定位"},
    {"WeightedGraph", testWeightedGraph, "带权图 —— 邻接矩阵 + Floyd 最短路"},
    {"BipartiteGraph", testBipartiteGraph, "带权二部图 —— 两侧顶点 + 前向星邻接表"},
    {"insertSorted", testInsertSorted, "插入排序 —— 有序插入，满了按规则挤入"},
};
constexpr int kTestCount = static_cast<int>(sizeof(kTests) / sizeof(kTests[0]));

bool selected[kTestCount];

void listTests() {
  std::printf("可以单独测试的结构：\n\n");
  for (int i = 0; i < kTestCount; ++i) std::printf("  %-16s %s\n", kTests[i].name, kTests[i].brief);
  std::printf("\n用法：\n");
  std::printf("  selfcheck                     跑全部\n");
  std::printf("  selfcheck SeqList             只跑顺序表\n");
  std::printf("  selfcheck SeqList HashMap     跑指定的几个\n");
  std::printf("  selfcheck list                看这份清单\n");
  std::printf("\n名字大小写不敏感。\n");
}

// 名字大小写不敏感地比较
bool sameName(const char* a, const char* b) {
  while (*a && *b) {
    char ca = *a, cb = *b;
    if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca - 'A' + 'a');
    if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb - 'A' + 'a');
    if (ca != cb) return false;
    ++a;
    ++b;
  }
  return *a == '\0' && *b == '\0';
}

}  // namespace

int main(int argc, char** argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
#if defined(_WIN32)
  // 让控制台按 UTF-8 显示中文。
  // 不能用 system("chcp 65001")：system 会另起一个 cmd.exe，它会把父进程继承来的
  // 标准输入重定向消费掉，于是随后的读取拿不到任何输入（表现为「菜单一闪而过」）。
  // 直接调 Win32 API 就没有这个问题。
  SetConsoleOutputCP(65001);
  SetConsoleCP(65001);
#endif

  // ---------- 解析参数：结构名 ----------
  bool anySelected = false;
  for (int i = 1; i < argc; ++i) {
    if (sameName(argv[i], "list") || sameName(argv[i], "-h") || sameName(argv[i], "--help")) {
      listTests();
      return 0;
    }
    bool found = false;
    for (int k = 0; k < kTestCount; ++k) {
      if (sameName(argv[i], kTests[k].name)) {
        selected[k] = true;
        anySelected = true;
        found = true;
        break;
      }
    }
    if (!found) std::printf("[警告] 没有名为 %s 的结构，用 selfcheck list 看清单\n", argv[i]);
  }
  if (!anySelected) {
    for (int k = 0; k < kTestCount; ++k) selected[k] = true;
  }

  // ---------- 表头 ----------
  std::printf("============================================================\n");
  std::printf("  数据结构调试程序\n");
  std::printf("============================================================\n");
  std::printf("  只测数据结构本身，与实际使用（医院业务）无关：\n");
  std::printf("  没有数据文件、没有业务逻辑，每个结构都用自然构造的小例子验证。\n");
  std::printf("\n  本次检查：");
  for (int k = 0; k < kTestCount; ++k)
    if (selected[k]) std::printf(" %s", kTests[k].name);
  std::printf("\n  说明：只打印失败的条目。\n");

  // ---------- 逐个跑 ----------
  for (int k = 0; k < kTestCount; ++k)
    if (selected[k]) kTests[k].run();

  // ---------- 汇总 ----------
  const int total = check::gPassed + check::gFailed;
  std::printf("\n============================================================\n");
  std::printf("  检查结果\n");
  std::printf("============================================================\n");
  std::printf("  通过 %d 项，失败 %d 项，共 %d 项\n", check::gPassed, check::gFailed, total);
  if (check::gFailed == 0) {
    std::printf("\n  全部通过。\n");
    return 0;
  }
  std::printf("\n  有 %d 项未通过，请按上面的「位置」逐条排查。\n", check::gFailed);
  return 1;
}
