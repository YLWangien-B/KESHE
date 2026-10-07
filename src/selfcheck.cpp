/* =============================================================================
 *  selfcheck.cpp  —  数据结构与算法的自检程序
 *
 *  这是干什么的：
 *    把每个数据结构、每个算法各自单独拿出来，用一组可预期的输入去调用，
 *    把实际结果和预期结果比一比。全部通过就说明这部分实现是对的；
 *    有失败就打印出是哪一条、期望什么、实际得到什么。
 *
 *  为什么要有它：
 *    1) 每写完一个结构就能立刻验证，不必等到界面接上去才发现问题；
 *    2) 改动代码之后跑一遍，立刻知道有没有把原来对的东西碰坏；
 *    3) 检查与答辩时，它是「我确实设计并验证过」的直接证据。
 *
 *  分组（每组相互独立，可以单独跑）：
 *    1  顺序表 SeqList
 *    2  环形队列 RingQueue
 *    3  散列表 HashMap
 *    4  二维数组 ScheduleTable
 *    5  带权图 WeightedGraph（含 Floyd 最短路）
 *    6  带权二部图 BipartiteGraph
 *    7  插入排序（导诊结果的排序）
 *    8  智能导诊（用真实数据）
 *    9  就诊路线（用真实数据）
 *   10  预约号源与候诊队列（用真实数据）
 *
 *  用法：
 *    selfcheck            跑全部
 *    selfcheck 3          只跑第 3 组
 *    selfcheck 3 5 7      跑指定的几组
 *    selfcheck data       用指定的数据目录做第 8~10 组（默认 data）
 * ========================================================================== */
#include <cstdio>
#include <cstring>

#include "ds.h"
#include "load.h"
#include "model.h"
#include "service.h"
#include "service_queue.h"

Hospital& db() {
  static Hospital h;
  return h;
}

// =============================================================================
//  自检框架：断言、计数、分组
// =============================================================================
namespace check {

int gPassed = 0;   // 通过的断言数
int gFailed = 0;   // 失败的断言数
int gGroup = 0;    // 当前组号
const char* gGroupName = "";
bool gGroupFailed = false;  // 当前组内是否已经出现过失败

// 开一个检查组
void group(int no, const char* name) {
  gGroup = no;
  gGroupName = name;
  gGroupFailed = false;
  std::printf("\n%s\n", "------------------------------------------------------------");
  std::printf("第 %d 组  %s\n", no, name);
  std::printf("%s\n", "------------------------------------------------------------");
}

// 检查通过就静默（避免刷屏），失败才打印，并给出源文件与行号
void report(bool ok, const char* what, const char* expect, const char* actual, const char* file, int line) {
  if (ok) {
    ++gPassed;
    return;
  }
  ++gFailed;
  gGroupFailed = true;
  std::printf("  [失败] %s\n", what);
  std::printf("         期望：%s\n", expect);
  std::printf("         实际：%s\n", actual);
  std::printf("         位置：%s : %d\n", file, line);
}

// 每组结束时汇报一次
void groupDone() {
  if (!gGroupFailed) std::printf("  -> 本组全部通过\n");
}

// ------------------------------ 断言宏 ------------------------------
// 注意：表达式必须在宏里只求值一次。
//   一开始写成了「比较时算一次、打印时再算一次」，于是 a.push(x) 这种带副作用的
//   表达式被调用了两遍 —— 表面上像是被检的顺序表出了问题，其实是自检程序的毛病。
//   所以这里一律先存进局部变量，后面只用变量。
//
// 两个整数相等
#define EXPECT_INT(what, expect, actual)                                                          \
  do {                                                                                            \
    const long long e_ = (long long)(expect);                                                     \
    const long long a_ = (long long)(actual);                                                     \
    char es_[48], as_[48];                                                                        \
    std::snprintf(es_, sizeof(es_), "%lld", e_);                                                  \
    std::snprintf(as_, sizeof(as_), "%lld", a_);                                                  \
    check::report(e_ == a_, what, es_, as_, __FILE__, __LINE__);                                  \
  } while (0)

// 布尔为真
#define EXPECT_TRUE(what, cond)                                                                   \
  do {                                                                                            \
    const bool ok_ = (cond) ? true : false;                                                       \
    check::report(ok_, what, "true", ok_ ? "true" : "false", __FILE__, __LINE__);                 \
  } while (0)

// 布尔为假
#define EXPECT_FALSE(what, cond)                                                                  \
  do {                                                                                            \
    const bool ok_ = (cond) ? true : false;                                                       \
    check::report(!ok_, what, "false", ok_ ? "true" : "false", __FILE__, __LINE__);               \
  } while (0)

// 字符串相等
#define EXPECT_STR(what, expect, actual)                                                          \
  do {                                                                                            \
    const char* e_ = (expect);                                                                    \
    const char* a_ = (actual);                                                                    \
    check::report(std::strcmp(e_, a_) == 0, what, e_, a_, __FILE__, __LINE__);                    \
  } while (0)

// 打印一条说明（用于交代这一组在验什么）
void note(const char* text) { std::printf("  · %s\n", text); }

}  // namespace check

// =============================================================================
//  第 1 组：顺序表 SeqList
//
//  要验证的：尾部追加、按下标随机访问、按下标删除后元素前移、容量上限、
//            清空、以及容量为 0 这种退化情形。
// =============================================================================
void group1SeqList() {
  check::group(1, "顺序表 SeqList —— 科室/医生/患者/预约/房间等表的存储");
  check::note("顺序表的访问方式是「整体遍历 + 按下标随机访问」，所以用静态数组。");

  SeqList<int, 4> a;
  EXPECT_INT("新建的表长度为 0", 0, a.size());
  EXPECT_TRUE("新建的表是空的", a.empty());
  EXPECT_FALSE("新建的表不是满的", a.full());
  EXPECT_INT("容量为模板参数 4", 4, (SeqList<int, 4>::capacity()));

  // 尾部追加，返回的是新元素的下标
  EXPECT_INT("push(10) 返回下标 0", 0, a.push(10));
  EXPECT_INT("push(20) 返回下标 1", 1, a.push(20));
  EXPECT_INT("push(30) 返回下标 2", 2, a.push(30));
  EXPECT_INT("长度变为 3", 3, a.size());
  EXPECT_FALSE("还没满", a.full());

  EXPECT_INT("按下标访问 a[0] = 10", 10, a[0]);
  EXPECT_INT("按下标访问 a[1] = 20", 20, a[1]);
  EXPECT_INT("按下标访问 a[2] = 30", 30, a[2]);

  // 写入：顺序表应当允许就地修改
  a[1] = 99;
  EXPECT_INT("a[1] 改成 99 后读回 99", 99, a[1]);

  EXPECT_INT("push(40) 返回下标 3", 3, a.push(40));
  EXPECT_TRUE("装满后 full() 为真", a.full());
  EXPECT_INT("已满时 push(50) 返回 -1", -1, a.push(50));
  EXPECT_INT("失败的 push 不改变长度", 4, a.size());

  // 删除中间元素，后面的应当前移
  EXPECT_TRUE("removeAt(1) 成功", a.removeAt(1));
  EXPECT_INT("删除后长度 3", 3, a.size());
  EXPECT_INT("删除后 a[0] = 10", 10, a[0]);
  EXPECT_INT("删除后 a[1] = 30（原来的 a[2] 前移了）", 30, a[1]);
  EXPECT_INT("删除后 a[2] = 40", 40, a[2]);
  EXPECT_FALSE("越界删除 removeAt(3) 失败", a.removeAt(3));
  EXPECT_FALSE("负数删除 removeAt(-1) 失败", a.removeAt(-1));
  EXPECT_INT("失败的删除不改变长度", 3, a.size());
  EXPECT_TRUE("腾出位置后可以再 push", a.push(50) >= 0);

  // 删除首尾两个端点
  SeqList<int, 4> b;
  b.push(1);
  b.push(2);
  b.push(3);
  EXPECT_TRUE("删除第一个元素成功", b.removeAt(0));
  EXPECT_INT("删头后长度 2", 2, b.size());
  EXPECT_INT("删头后 b[0] = 2", 2, b[0]);
  EXPECT_TRUE("删除最后一个元素成功", b.removeAt(1));
  EXPECT_INT("删尾后长度 1", 1, b.size());
  EXPECT_INT("删尾后 b[0] = 2", 2, b[0]);

  b.clear();
  EXPECT_INT("clear() 后长度为 0", 0, b.size());
  EXPECT_TRUE("clear() 后为空", b.empty());

  // 退化容量：容量为 1 时只能放一个
  SeqList<int, 1> one;
  EXPECT_INT("容量 1 的表能放一个", 0, one.push(7));
  EXPECT_INT("容量 1 的表放第二个返回 -1", -1, one.push(8));
  EXPECT_TRUE("容量 1 的表满了", one.full());

  // 顺序表实际存的是结构体，这里验证它能整体拷贝
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

  check::groupDone();
}

// =============================================================================
//  第 2 组：环形队列 RingQueue
//
//  要验证的：先进先出、回绕（出队腾出的位置被后来者复用）、按次序访问、
//            按次序删除、队满队空、清空。
//
//  回绕是环形队列的关键点：如果不做回绕（普通数组队列），出队后位置就浪费了。
//  这里特意让 head 和 tail 都绕过一圈来验证。
// =============================================================================
void group2RingQueue() {
  check::group(2, "环形队列 RingQueue —— 候诊队列的实现基础");
  check::note("环形队列的关键是回绕：出队腾出的数组位置会被后来的入队循环使用。");

  RingQueue<int, 4> q;
  EXPECT_TRUE("新建的队列为空", q.empty());
  EXPECT_FALSE("新建的队列不满", q.full());
  EXPECT_INT("新建的队列长度为 0", 0, q.size());
  EXPECT_INT("容量为模板参数 4", 4, (RingQueue<int, 4>::capacity()));

  // 先进先出。注意 push / pop 返回的是「成功与否」，不是下标 ——
  // 它们的返回值是 bool，同一个表达式里不要写两次（会真的执行两次）。
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

  // 队内按排队次序访问：队首是剩下的 5
  EXPECT_INT("at(0) 是队首 5", 5, q.at(0));

  EXPECT_TRUE("push(7) 成功", q.push(7));
  EXPECT_TRUE("push(9) 成功", q.push(9));
  EXPECT_TRUE("push(11) 成功（此时 tail 已回绕到数组开头）", q.push(11));
  EXPECT_TRUE("队列满（4 个位置已用完）", q.full());
  EXPECT_FALSE("满队列 push(13) 返回 false", q.push(13));
  EXPECT_INT("失败的 push 不改变长度", 4, q.size());

  // 队列里现在是 5 7 9 11，按排队次序读出，验证回绕之后次序没错
  EXPECT_INT("回绕后 at(0) = 5", 5, q.at(0));
  EXPECT_INT("回绕后 at(1) = 7", 7, q.at(1));
  EXPECT_INT("回绕后 at(2) = 9", 9, q.at(2));
  EXPECT_INT("回绕后 at(3) = 11", 11, q.at(3));

  // 全部出队，验证顺序仍然是先进先出
  int expectOrder[4] = {5, 7, 9, 11};
  bool orderOk = true;
  for (int i = 0; i < 4; ++i) {
    int out = -1;
    if (!q.pop(out) || out != expectOrder[i]) orderOk = false;
  }
  EXPECT_TRUE("回绕一圈之后出队次序仍然是先进先出", orderOk);
  EXPECT_TRUE("全部出队后队列为空", q.empty());
  EXPECT_INT("全部出队后长度为 0", 0, q.size());
  int nothing = -1;
  EXPECT_FALSE("空队列 pop 返回 false", q.pop(nothing));

  // 按排队次序删除：候诊队列里「看完病离开队列」用得到
  RingQueue<int, 4> r;
  r.push(10);
  r.push(20);
  r.push(30);
  r.push(40);
  EXPECT_TRUE("removeAt(1)（删掉 20）成功", r.removeAt(1));
  EXPECT_INT("删除后长度 3", 3, r.size());
  EXPECT_INT("剩下的第 1 个仍是 10", 10, r.at(0));
  EXPECT_INT("剩下的第 2 个是 30（次序没乱）", 30, r.at(1));
  EXPECT_INT("剩下的第 3 个是 40", 40, r.at(2));
  EXPECT_FALSE("越界 removeAt(3) 失败", r.removeAt(3));
  EXPECT_FALSE("负数 removeAt(-1) 失败", r.removeAt(-1));
  EXPECT_INT("失败的删除不改变长度", 3, r.size());

  EXPECT_TRUE("删除队首成功", r.removeAt(0));
  EXPECT_INT("删队首后剩下的第 1 个是 30", 30, r.at(0));

  // 可写访问：候诊队列里把「已叫号次数」加一用得到
  r.at(0) = 77;
  EXPECT_INT("at(i) 的写版本能就地修改队内元素", 77, r.at(0));
  EXPECT_INT("修改不影响长度", 2, r.size());

  r.clear();
  EXPECT_TRUE("clear() 后队列为空", r.empty());
  EXPECT_TRUE("clear() 后还能重新入队", r.push(1));

  // 候诊队列的实际用法：60 个患者轮流进出，验证不会因为回绕而错乱
  RingQueue<int, 8> busy;
  bool fifoOk = true;
  int nextIn = 0, nextOut = 0;
  for (int round = 0; round < 30; ++round) {
    for (int k = 0; k < 3 && !busy.full(); ++k) busy.push(nextIn++);
    int out = 0;
    if (busy.pop(out) && out != nextOut++) fifoOk = false;
  }
  EXPECT_TRUE("反复进出 30 轮（多次回绕）后次序仍然正确", fifoOk);

  check::groupDone();
}

// =============================================================================
//  第 3 组：散列表 HashMap
//
//  要验证的：插入与查找、同名键覆盖而不是新增、查不到、空键、
//            散列函数把不同键分散开（减少冲突）、大量键仍能正确查找。
// =============================================================================
void group3HashMap() {
  check::group(3, "散列表 HashMap —— 编号到表内下标的索引");
  check::note("链地址法：每个桶挂一条同义词链，删除时不必修补探测链。");

  HashMap m;
  EXPECT_INT("新建的散列表结点数为 0", 0, m.size());

  int out = -1;
  EXPECT_FALSE("空表里查不到任何键", m.get("K01", out));

  EXPECT_TRUE("put(K01, 0) 成功", m.put("K01", 0));
  EXPECT_TRUE("put(K02, 1) 成功", m.put("K02", 1));
  EXPECT_TRUE("put(D1, 5) 成功", m.put("D1", 5));
  EXPECT_INT("结点数为 3", 3, m.size());

  EXPECT_TRUE("能查到 K01", m.get("K01", out));
  EXPECT_INT("K01 -> 0", 0, out);
  EXPECT_TRUE("能查到 K02", m.get("K02", out));
  EXPECT_INT("K02 -> 1", 1, out);
  EXPECT_TRUE("能查到 D1", m.get("D1", out));
  EXPECT_INT("D1 -> 5", 5, out);

  out = -1;
  EXPECT_FALSE("查不到的键返回 false", m.get("K99", out));
  EXPECT_FALSE("大小写不同算不同的键（K01 与 k01）", m.get("k01", out));
  EXPECT_FALSE("空键查不到", m.get("", out));

  // 同名键覆盖，不能新增结点（否则 size 会一直涨，且旧值查不到）
  EXPECT_TRUE("重复 put(K01, 88) 成功", m.put("K01", 88));
  EXPECT_INT("覆盖不新增结点，结点数仍是 3", 3, m.size());
  EXPECT_TRUE("查 K01 得到新值", m.get("K01", out));
  EXPECT_INT("K01 已被覆盖为 88", 88, out);

  // 大量键（远多于桶数，必然产生冲突），验证链地址法能正确查找
  HashMap big;
  char key[24];
  const int kCount = 200;
  bool putAllOk = true;
  for (int i = 0; i < kCount; ++i) {
    std::snprintf(key, sizeof(key), "K%03d", i);
    if (!big.put(key, i * 2)) putAllOk = false;
  }
  EXPECT_TRUE("连续插入 200 个键都成功（桶 211 个，结点池 1024）", putAllOk);
  EXPECT_INT("结点数正好是 200", kCount, big.size());

  bool findAllOk = true;
  for (int i = 0; i < kCount; ++i) {
    std::snprintf(key, sizeof(key), "K%03d", i);
    if (!big.get(key, out) || out != i * 2) findAllOk = false;
  }
  EXPECT_TRUE("200 个键全部能查回原值（含发生冲突的那些）", findAllOk);

  out = -1;
  EXPECT_FALSE("这 200 个键之外的名字查不到", big.get("K200", out));

  // 长度相近的键应当落到不同的桶，否则散列函数没起到分散作用
  // （这里通过「插入 26 个首字母不同的键后再全部查回」间接验证）
  HashMap alpha;
  for (int i = 0; i < 26; ++i) {
    char k[4];
    k[0] = static_cast<char>('A' + i);
    k[1] = '\0';
    alpha.put(k, i);
  }
  bool alphaOk = true;
  for (int i = 0; i < 26; ++i) {
    char k[4];
    k[0] = static_cast<char>('A' + i);
    k[1] = '\0';
    if (!alpha.get(k, out) || out != i) alphaOk = false;
  }
  EXPECT_TRUE("26 个单字母键全部正确（首字母不同应当分散到不同链）", alphaOk);

  big.clear();
  EXPECT_INT("clear() 后结点数为 0", 0, big.size());
  EXPECT_FALSE("clear() 后原键查不到", big.get("K000", out));
  EXPECT_TRUE("clear() 后还能重新插入", big.put("K000", 7));
  EXPECT_TRUE("重新插入后能查到", big.get("K000", out));
  EXPECT_INT("查回的值是 7", 7, out);

  check::groupDone();
}

// =============================================================================
//  第 4 组：二维数组 ScheduleTable
//
//  要验证的：未排班与已排班的区分、剩余号源的计算、停诊、日期与时段越界、
//            清空。这是排班与预约的存储基础。
// =============================================================================
void group4ScheduleTable() {
  check::group(4, "二维数组 ScheduleTable —— 医生排班（日期 × 时段）");
  check::note("排班的主键就是「日期 + 时段」，二维数组的下标天然就是主键，O(1) 定位。");

  ScheduleTable<7, 4> t;
  EXPECT_INT("日期数 7", 7, (ScheduleTable<7, 4>::days()));
  EXPECT_INT("时段数 4", 4, (ScheduleTable<7, 4>::slots()));

  // 越界检查
  EXPECT_TRUE("日期 0 合法", (ScheduleTable<7, 4>::validDay(0)));
  EXPECT_TRUE("日期 6 合法", (ScheduleTable<7, 4>::validDay(6)));
  EXPECT_FALSE("日期 7 越界", (ScheduleTable<7, 4>::validDay(7)));
  EXPECT_FALSE("日期 -1 越界", (ScheduleTable<7, 4>::validDay(-1)));
  EXPECT_TRUE("时段 0 合法", (ScheduleTable<7, 4>::validSlot(0)));
  EXPECT_TRUE("时段 3 合法", (ScheduleTable<7, 4>::validSlot(3)));
  EXPECT_FALSE("时段 4 越界", (ScheduleTable<7, 4>::validSlot(4)));

  // 刚建好时全都没排班
  EXPECT_FALSE("新表 (0,0) 未排班", t.assigned(0, 0));
  EXPECT_FALSE("新表 (3,2) 未排班", t.assigned(3, 2));
  EXPECT_INT("未排班的时段剩余号源为 0", 0, t.remaining(0, 0));

  // 排班：周三下午（下标 2, 1）放 15 个号
  t.slot(2, 1).quota = 15;
  t.slot(2, 1).booked = 0;
  t.slot(2, 1).version = 1;
  t.slot(2, 1).assigned = true;
  t.slot(2, 1).stopped = false;

  EXPECT_TRUE("(2,1) 已排班", t.assigned(2, 1));
  EXPECT_INT("剩余号源 = 15 - 0", 15, t.remaining(2, 1));

  // 区分「没排班」与「排班了但号源为 0」——这是 assigned 标记的意义
  t.slot(1, 0).quota = 5;
  t.slot(1, 0).booked = 5;  // 约满了
  t.slot(1, 0).version = 2;
  t.slot(1, 0).assigned = true;
  t.slot(1, 0).stopped = false;
  EXPECT_TRUE("(1,0) 已排班（虽然号源为 0）", t.assigned(1, 0));
  EXPECT_INT("约满的时段剩余号源为 0", 0, t.remaining(1, 0));

  // 扣减与退回
  t.slot(2, 1).booked = 1;
  EXPECT_INT("扣掉 1 个号后剩余 14", 14, t.remaining(2, 1));
  t.slot(2, 1).booked = 0;
  EXPECT_INT("退回后剩余又变回 15", 15, t.remaining(2, 1));
  EXPECT_TRUE("(2,1) 仍然已排班", t.assigned(2, 1));

  // 停诊：排班还在，但不再放号
  t.slot(5, 3).quota = 20;
  t.slot(5, 3).booked = 0;
  t.slot(5, 3).assigned = true;
  t.slot(5, 3).stopped = true;
  EXPECT_TRUE("停诊的时段仍是「已排班」", t.assigned(5, 3));
  EXPECT_INT("停诊的时段剩余号源为 0", 0, t.remaining(5, 3));

  // 负剩余要归零，不能出现负数（否则「号源已满」的判断会失灵）
  t.slot(6, 0).quota = 3;
  t.slot(6, 0).booked = 5;  // 故意造成超额
  t.slot(6, 0).assigned = true;
  t.slot(6, 0).stopped = false;
  EXPECT_INT("超额预约时剩余号源按 0 返回，不出现负数", 0, t.remaining(6, 0));

  EXPECT_INT("未排班的时段剩余号源恒为 0", 0, t.remaining(4, 2));

  // 清空：全部回到未排班
  t.clear();
  EXPECT_FALSE("clear() 后 (2,1) 回到未排班", t.assigned(2, 1));
  EXPECT_FALSE("clear() 后 (1,0) 回到未排班", t.assigned(1, 0));
  EXPECT_INT("clear() 后剩余号源为 0", 0, t.remaining(2, 1));

  check::groupDone();
}

// =============================================================================
//  第 5 组：带权图 WeightedGraph
//
//  要验证的：不可达的表示、加边、取较小权值、Floyd 求最短路（含中转）、
//            路径还原、越界拒绝、以及「先有更短的路再被更长的覆盖」这类情形。
// =============================================================================
void group5WeightedGraph() {
  check::group(5, "带权图 WeightedGraph —— 就诊路网（邻接矩阵 + Floyd）");
  check::note("路网建成后不变，用 Floyd 一次算好任意两点最短路，之后每次查询 O(1) 查表。");

  WeightedGraph g;
  g.init(5);

  EXPECT_INT("顶点数为 5", 5, g.nodeCount());
  EXPECT_INT("点到自己的距离是 0", 0, g.distance(0, 0));
  EXPECT_INT("没加边时两点不可达", WeightedGraph::kInf, (g.distance(0, 1)));
  EXPECT_FALSE("没加边时两点不连通", g.reachable(0, 1));

  EXPECT_TRUE("加边 0-1 距离 10", g.addEdge(0, 1, 10));
  EXPECT_TRUE("加边 1-2 距离 10", g.addEdge(1, 2, 10));
  EXPECT_TRUE("加边 2-3 距离 5", g.addEdge(2, 3, 5));
  EXPECT_TRUE("加边 0-4 距离 100", g.addEdge(0, 4, 100));

  EXPECT_INT("直接相连的距离读回 10", 10, g.distance(0, 1));
  EXPECT_INT("边是无向的：1 到 0 也是 10", 10, g.distance(1, 0));
  EXPECT_TRUE("0 能到 1", g.reachable(0, 1));
  EXPECT_FALSE("3 到 4 不连通", g.reachable(3, 4));

  // 重复加同一条边取较小权值（现实中同一对房间之间可能有多条路线）
  EXPECT_TRUE("重复加边 0-1 距离 30", g.addEdge(0, 1, 30));
  EXPECT_INT("取较小权值，仍是 10", 10, g.distance(0, 1));

  // Floyd：0 到 3 没有直接边，要经过 1 和 2
  g.floyd();
  EXPECT_INT("Floyd 后 0 到 2 = 10 + 10", 20, g.distance(0, 2));
  EXPECT_INT("Floyd 后 0 到 3 = 10 + 10 + 5", 25, g.distance(0, 3));
  EXPECT_INT("Floyd 后 1 到 3 = 10 + 5", 15, g.distance(1, 3));
  EXPECT_INT("直达的 0 到 4 仍是 100（比绕路 0-1-2-3-4 不通，所以还是 100）", 100, g.distance(0, 4));
  EXPECT_TRUE("Floyd 后 0 能到 3", g.reachable(0, 3));
  EXPECT_INT("点到自己的距离仍是 0", 0, g.distance(2, 2));

  // 这个图整体是连通的：3 要经 2、1、0 才能到 4，距离 10+10+5 的反向 + 100
  EXPECT_TRUE("3 能绕到 4（3-2-1-0-4）", g.reachable(3, 4));
  EXPECT_INT("3 到 4 的最短路 = 25 + 100", 125, g.distance(3, 4));

  // 真正的「不连通」要另建一个孤立的顶点
  WeightedGraph iso;
  iso.init(4);
  iso.addEdge(0, 1, 10);
  iso.addEdge(1, 2, 10);  // 顶点 3 谁也不连
  iso.floyd();
  EXPECT_FALSE("孤立顶点 3 与 0 不连通", iso.reachable(0, 3));
  EXPECT_INT("不连通时距离为 kInf", WeightedGraph::kInf, iso.distance(0, 3));
  EXPECT_INT("不连通时距离为 kInf（反向）", WeightedGraph::kInf, iso.distance(3, 0));

  // 路径还原
  int path[8];
  int n = g.buildPath(0, 3, path, 8);
  EXPECT_INT("0 到 3 途经 4 个顶点", 4, n);
  EXPECT_INT("路径第 1 个是起点 0", 0, path[0]);
  EXPECT_INT("路径第 2 个是 1", 1, path[1]);
  EXPECT_INT("路径第 3 个是 2", 2, path[2]);
  EXPECT_INT("路径最后一个是终点 3", 3, path[3]);

  n = g.buildPath(0, 1, path, 8);
  EXPECT_INT("相邻两点路径只含 2 个顶点", 2, n);
  n = g.buildPath(0, 4, path, 8);
  EXPECT_INT("直达的路径含 2 个顶点", 2, n);

  // 绕路 3-2-1-0-4 共 5 个顶点
  n = g.buildPath(3, 4, path, 8);
  EXPECT_INT("绕路的路径含 5 个顶点", 5, n);
  EXPECT_INT("绕路路径第 1 个是 3", 3, path[0]);
  EXPECT_INT("绕路路径第 2 个是 2", 2, path[1]);
  EXPECT_INT("绕路路径第 3 个是 1", 1, path[2]);
  EXPECT_INT("绕路路径第 4 个是 0", 0, path[3]);
  EXPECT_INT("绕路路径最后一个是 4", 4, path[4]);

  EXPECT_INT("真正不可达时 buildPath 返回 0", 0, iso.buildPath(0, 3, path, 8));
  EXPECT_INT("不可达的反向也返回 0", 0, iso.buildPath(3, 0, path, 8));

  // 越界与非法参数一律拒绝，不能让它们污染矩阵
  EXPECT_FALSE("加边时顶点越界被拒绝", g.addEdge(0, 9, 5));
  EXPECT_FALSE("加边时负数顶点被拒绝", g.addEdge(-1, 1, 5));
  EXPECT_FALSE("加边时负权值被拒绝", g.addEdge(0, 1, -5));
  EXPECT_INT("越界查询距离返回不可达", WeightedGraph::kInf, (g.distance(0, 99)));
  EXPECT_FALSE("越界查询 reachable 为假", g.reachable(0, 99));

  // 「更短的绕路」应当胜过「更长的直达」——这是最短路的核心
  WeightedGraph h;
  h.init(3);
  h.addEdge(0, 2, 100);  // 直达很贵
  h.addEdge(0, 1, 1);
  h.addEdge(1, 2, 1);  // 绕一下只要 2
  h.floyd();
  EXPECT_INT("绕路 2 胜过直达 100", 2, h.distance(0, 2));
  h.buildPath(0, 2, path, 8);
  EXPECT_INT("还原出的路径走的是绕路（3 个顶点）", 3, h.buildPath(0, 2, path, 8));

  // 重新 init 应当把边全部清掉
  h.init(3);
  EXPECT_INT("重新 init 后 0 到 1 不可达", WeightedGraph::kInf, (h.distance(0, 1)));
  EXPECT_INT("重新 init 后仍保留顶点数", 3, h.nodeCount());

  // 实际的楼宇场景：两层楼之间靠电梯口连通
  WeightedGraph b;
  b.init(4);
  b.addEdge(0, 1, 10);  // 1 层走廊两头
  b.addEdge(1, 2, 20);  // 1 层到 2 层（电梯）
  b.addEdge(2, 3, 10);  // 2 层走廊两头
  b.floyd();
  EXPECT_INT("跨层：0 -> 3 = 10 + 20 + 10", 40, b.distance(0, 3));
  b.buildPath(0, 3, path, 8);
  EXPECT_INT("跨层路径经过 4 个顶点", 4, b.buildPath(0, 3, path, 8));

  check::groupDone();
}

// =============================================================================
//  第 6 组：带权二部图 BipartiteGraph
//
//  要验证的：两侧顶点各自独立编号、加边时两端都要存在、边权范围、
//            从两侧遍历邻接链、重复加边取较大权值、去重、清空。
//
//  重点在最后两条：一个症状关联多个科室、一个科室关联多个症状，
//  两个方向都必须能遍历到。这是二部图最容易写错的地方（一个 next 走两条链）。
// =============================================================================
void group6BipartiteGraph() {
  check::group(6, "带权二部图 BipartiteGraph —— 症状与科室的关联关系");
  check::note("两侧顶点互不相邻，边只存在于两侧之间；两条邻接链各自独立。");

  BipartiteGraph g;
  g.clear();

  // --- 症状侧顶点 ---
  EXPECT_INT("新建的图没有症状顶点", 0, g.symptomCount());
  EXPECT_INT("新建的图没有科室顶点", 0, g.departmentCount());
  EXPECT_INT("新建的图没有边", 0, g.edgeCount());
  EXPECT_INT("空图里查不到症状", -1, g.findSymptom("胸闷"));

  const int s0 = g.addSymptom("胸闷");
  const int s1 = g.addSymptom("心悸");
  const int s2 = g.addSymptom("气短");
  const int s3 = g.addSymptom("咳嗽");
  EXPECT_INT("第 1 个症状顶点号是 0", 0, s0);
  EXPECT_INT("第 2 个症状顶点号是 1", 1, s1);
  EXPECT_INT("第 4 个症状顶点号是 3", 3, s3);
  EXPECT_INT("症状顶点数为 4", 4, g.symptomCount());
  EXPECT_STR("顶点名读回正确", "胸闷", g.symptomName(0));
  EXPECT_INT("按名字查得到顶点号", 1, g.findSymptom("心悸"));
  EXPECT_INT("查不到的名字返回 -1", -1, g.findSymptom("肚子疼"));

  const int dup = g.addSymptom("胸闷");
  EXPECT_INT("重复加同名症状返回原有顶点号", 0, dup);
  EXPECT_INT("重复加症状不会新增顶点", 4, g.symptomCount());

  EXPECT_INT("越界取顶点名得到空串", 0, (int)std::strlen(g.symptomName(99)));
  EXPECT_INT("负数取顶点名得到空串", 0, (int)std::strlen(g.symptomName(-1)));

  // --- 边：科室顶点还没登记，加边必须失败 ---
  EXPECT_FALSE("科室顶点数为 0 时加边被拒绝", g.addEdge(s0, 0, 3));

  g.setDepartmentCount(3);  // 科室 0 心血管 / 1 呼吸 / 2 儿科
  EXPECT_INT("科室顶点数为 3", 3, g.departmentCount());

  // --- 加边 ---
  EXPECT_TRUE("加边 胸闷-心血管 权重 3", g.addEdge(s0, 0, 3));
  EXPECT_TRUE("加边 心悸-心血管 权重 3", g.addEdge(s1, 0, 3));
  EXPECT_TRUE("加边 气短-心血管 权重 2", g.addEdge(s2, 0, 2));
  EXPECT_TRUE("加边 咳嗽-呼吸 权重 3", g.addEdge(s3, 1, 3));
  EXPECT_TRUE("加边 气短-呼吸 权重 2", g.addEdge(s2, 1, 2));
  EXPECT_TRUE("加边 咳嗽-儿科 权重 2", g.addEdge(s3, 2, 2));
  EXPECT_INT("6 条边在边集中占 12 个结点", 12, g.edgeCount());

  // --- 边权查询 ---
  EXPECT_INT("查边权 胸闷-心血管 = 3", 3, g.weightOf(s0, 0));
  EXPECT_INT("查边权 咳嗽-儿科 = 2", 2, g.weightOf(s3, 2));
  EXPECT_INT("没边的一对返回 0（胸闷-呼吸）", 0, g.weightOf(s0, 1));

  // --- 边界与非法参数 ---
  EXPECT_FALSE("症状顶点越界被拒绝", g.addEdge(9, 0, 3));
  EXPECT_FALSE("科室顶点越界被拒绝", g.addEdge(s0, 9, 3));
  EXPECT_FALSE("负数顶点被拒绝", g.addEdge(-1, 0, 3));
  EXPECT_FALSE("权值为 0 被拒绝", g.addEdge(s0, 1, 0));
  EXPECT_FALSE("权值为负被拒绝", g.addEdge(s0, 1, -1));
  EXPECT_FALSE("权值超过上限被拒绝", (g.addEdge(s0, 1, BipartiteGraph::kMaxWeight + 1)));

  // --- 重复加边：取较大权值，不新增边 ---
  EXPECT_TRUE("重复加边 胸闷-心血管 权重 1", g.addEdge(s0, 0, 1));
  EXPECT_INT("重复加边不新增结点", 12, g.edgeCount());
  EXPECT_INT("取较大权值，仍是 3", 3, g.weightOf(s0, 0));
  EXPECT_TRUE("重复加边 胸闷-心血管 权重 5（更大）", g.addEdge(s0, 0, 5));
  EXPECT_INT("权值被更新为 5", 5, g.weightOf(s0, 0));
  EXPECT_TRUE("重复加边 胸闷-心血管 权重 2（更小）", g.addEdge(s0, 0, 2));
  EXPECT_INT("较小权值不覆盖较大的", 5, g.weightOf(s0, 0));
  g.addEdge(s0, 0, 3);  // 还原成 3，后面按 3 计算

  // --- 遍历方向一：症状 -> 关联的科室 ---
  int deptCount = 0;
  bool seenDept[3] = {false, false, false};
  for (int e = g.firstDeptOf(s3); e != -1; e = g.nextDeptEdge(e)) {
    const int d = g.edgeDept(e);
    if (d >= 0 && d < 3) seenDept[d] = true;
    ++deptCount;
  }
  EXPECT_INT("咳嗽关联 2 个科室", 2, deptCount);
  EXPECT_TRUE("咳嗽关联到呼吸内科", seenDept[1]);
  EXPECT_TRUE("咳嗽关联到儿科", seenDept[2]);
  EXPECT_FALSE("咳嗽不关联心血管内科", seenDept[0]);

  int aloneCount = 0;
  for (int e = g.firstDeptOf(s1); e != -1; e = g.nextDeptEdge(e)) ++aloneCount;
  EXPECT_INT("心悸只关联 1 个科室", 1, aloneCount);

  // --- 遍历方向二：科室 -> 关联的症状 ---
  // 心血管内科关联 胸闷/心悸/气短 三个；科室侧链断了这里就会是 0，
  // 那正是「两个 next 共用一个」时出现的症状（症状查得到科室、科室查不到症状）。
  int symCount = 0;
  bool seenSym[4] = {false, false, false, false};
  for (int e = g.firstSymptomOf(0); e != -1; e = g.nextSymptomEdge(e)) {
    const int s = g.edgeSymptom(e);
    if (s >= 0 && s < 4) seenSym[s] = true;
    ++symCount;
  }
  EXPECT_INT("心血管内科关联 3 个症状", 3, symCount);
  EXPECT_TRUE("心血管内科关联到胸闷", seenSym[0]);
  EXPECT_TRUE("心血管内科关联到心悸", seenSym[1]);
  EXPECT_TRUE("心血管内科关联到气短", seenSym[2]);
  EXPECT_FALSE("心血管内科不关联咳嗽", seenSym[3]);

  symCount = 0;
  for (int e = g.firstSymptomOf(1); e != -1; e = g.nextSymptomEdge(e)) ++symCount;
  EXPECT_INT("呼吸内科关联 2 个症状", 2, symCount);
  symCount = 0;
  for (int e = g.firstSymptomOf(2); e != -1; e = g.nextSymptomEdge(e)) ++symCount;
  EXPECT_INT("儿科关联 1 个症状", 1, symCount);

  // 一个症状关联多个科室（气短同时指向心血管和呼吸），两个方向都要能看到
  deptCount = 0;
  for (int e = g.firstDeptOf(s2); e != -1; e = g.nextDeptEdge(e)) ++deptCount;
  EXPECT_INT("气短关联 2 个科室（跨科室的症状）", 2, deptCount);
  EXPECT_INT("气短-心血管 的权值是 2", 2, g.weightOf(s2, 0));
  EXPECT_INT("气短-呼吸 的权值是 2", 2, g.weightOf(s2, 1));

  // 两侧的遍历结点数应当一致：每条边在两侧各挂一次
  int totalFromSymptom = 0;
  for (int s = 0; s < g.symptomCount(); ++s)
    for (int e = g.firstDeptOf(s); e != -1; e = g.nextDeptEdge(e)) ++totalFromSymptom;
  int totalFromDept = 0;
  for (int d = 0; d < g.departmentCount(); ++d)
    for (int e = g.firstSymptomOf(d); e != -1; e = g.nextSymptomEdge(e)) ++totalFromDept;
  EXPECT_INT("从症状侧数出的边数 = 从科室侧数出的边数", totalFromSymptom, totalFromDept);
  EXPECT_INT("数出来的边数就是 6", 6, totalFromSymptom);

  // 越界的遍历起点返回 -1，不会崩
  EXPECT_INT("越界症状的邻接链为空", -1, g.firstDeptOf(99));
  EXPECT_INT("越界科室的邻接链为空", -1, g.firstSymptomOf(99));
  EXPECT_INT("越界边的下一个返回 -1", -1, g.nextDeptEdge(999));

  // --- 清空 ---
  g.clear();
  EXPECT_INT("clear() 后症状顶点数为 0", 0, g.symptomCount());
  EXPECT_INT("clear() 后科室顶点数为 0", 0, g.departmentCount());
  EXPECT_INT("clear() 后边数为 0", 0, g.edgeCount());
  EXPECT_INT("clear() 后原症状名查不到", -1, g.findSymptom("胸闷"));
  EXPECT_INT("clear() 后可以重新加症状顶点", 0, g.addSymptom("头痛"));

  check::groupDone();
}

// =============================================================================
//  第 7 组：插入排序（导诊结果的排序）
//
//  Triage 内部的插入排序没有单独暴露接口，这里把同样的逻辑抽出来验证：
//  「结果最多 5 条，满了之后只有更高分才能挤掉最后一名」。
//  这是导诊结果正确性的关键 —— 如果这里写错，会漏掉本该推荐的科室。
// =============================================================================
namespace sortdemo {

constexpr int kMax = 5;

struct Item {
  int id;
  int score;
};

struct Result {
  Item items[kMax];
  int count = 0;
};

// 与 service.h 里 Triage::insertSorted 完全相同的逻辑
void insert(Result& r, const Item& v) {
  if (r.count < kMax) {
    int pos = r.count;
    while (pos > 0 && r.items[pos - 1].score < v.score) {
      r.items[pos] = r.items[pos - 1];
      --pos;
    }
    r.items[pos] = v;
    ++r.count;
    return;
  }
  if (v.score <= r.items[kMax - 1].score) return;  // 不够高，挤不掉最后一名
  int pos = kMax - 1;
  while (pos > 0 && r.items[pos - 1].score < v.score) {
    r.items[pos] = r.items[pos - 1];
    --pos;
  }
  r.items[pos] = v;
}

}  // namespace sortdemo

void group7InsertSort() {
  check::group(7, "插入排序 —— 导诊推荐结果的排序与截断");
  check::note("结果最多留 5 条，靠插入排序保持降序；满了之后只有更高分才能挤进榜。");

  sortdemo::Result r;
  int scores[3] = {3, 9, 6};
  for (int i = 0; i < 3; ++i) {
    sortdemo::Item v;
    v.id = i;
    v.score = scores[i];
    sortdemo::insert(r, v);
  }
  EXPECT_INT("收了 3 条", 3, r.count);
  EXPECT_INT("第 1 名得分 9", 9, r.items[0].score);
  EXPECT_INT("第 2 名得分 6", 6, r.items[1].score);
  EXPECT_INT("第 3 名得分 3", 3, r.items[2].score);
  EXPECT_INT("第 1 名是得分 9 的那条", 1, r.items[0].id);

  // 填满 5 条
  int more[2] = {7, 8};
  for (int i = 0; i < 2; ++i) {
    sortdemo::Item v;
    v.id = 10 + i;
    v.score = more[i];
    sortdemo::insert(r, v);
  }
  EXPECT_INT("收满 5 条", 5, r.count);
  EXPECT_INT("降序：第 1 名 9", 9, r.items[0].score);
  EXPECT_INT("降序：第 2 名 8", 8, r.items[1].score);
  EXPECT_INT("降序：第 3 名 7", 7, r.items[2].score);
  EXPECT_INT("降序：第 4 名 6", 6, r.items[3].score);
  EXPECT_INT("降序：第 5 名 3", 3, r.items[4].score);

  // 更低分挤不进来
  sortdemo::Item low;
  low.id = 99;
  low.score = 2;
  sortdemo::insert(r, low);
  EXPECT_INT("低于最后一名，条数不变", 5, r.count);
  EXPECT_INT("最后一名仍是 3", 3, r.items[4].score);

  // 更高分应当挤掉最后一名
  sortdemo::Item high;
  high.id = 88;
  high.score = 100;
  sortdemo::insert(r, high);
  EXPECT_INT("高于最后一名，条数仍是上限 5", 5, r.count);
  EXPECT_INT("第 1 名换成 100", 100, r.items[0].score);
  EXPECT_INT("第 1 名是刚插入的那条", 88, r.items[0].id);
  EXPECT_INT("原来最后一名 3 被挤掉", 6, r.items[4].score);

  // 刚好等于最后一名时也不挤（保证结果稳定，不因为插入顺序而变）
  sortdemo::Result r2;
  for (int i = 0; i < 5; ++i) {
    sortdemo::Item v;
    v.id = i;
    v.score = 5;
    sortdemo::insert(r2, v);
  }
  sortdemo::Item tie;
  tie.id = 100;
  tie.score = 5;
  sortdemo::insert(r2, tie);
  EXPECT_INT("得分相同挤不进来，条数不变", 5, r2.count);
  EXPECT_INT("得分相同时先来的仍在第 1 位", 0, r2.items[0].id);

  // 只插一条
  sortdemo::Result r3;
  sortdemo::Item one;
  one.id = 7;
  one.score = 1;
  sortdemo::insert(r3, one);
  EXPECT_INT("只插一条时条数为 1", 1, r3.count);
  EXPECT_INT("那一条的内容正确", 7, r3.items[0].id);

  check::groupDone();
}

// =============================================================================
//  第 8~10 组：接上真实数据，验证业务算法
// =============================================================================
static bool loadRealData(Hospital& h, const char* dir) {
  if (Loader::loadAll(h, dir)) return true;
  std::printf("  [失败] 装载数据失败，请先运行：build\\gen_data.exe %s\n", dir);
  ++check::gFailed;
  check::gGroupFailed = true;
  return false;
}

// 第 8 组：智能导诊
void group8Triage(const char* dir) {
  check::group(8, "智能导诊 —— 症状与科室描述匹配 + 邻域加权求和");
  check::note("沿「科室 → 关联症状」这条邻接链走一遍，把患者选中症状的边权累加。");

  Hospital& h = db();
  if (!loadRealData(h, dir)) return;

  const BipartiteGraph& g = h.symptomGraph;
  EXPECT_INT("症状顶点 25 个", 25, g.symptomCount());
  EXPECT_INT("科室顶点 12 个", 12, h.depts.size());
  EXPECT_INT("二部图有 31 条边（62 个边结点）", 62, g.edgeCount());

  // 症状序号 4 = 胸闷，2 = 咳嗽，6 = 气短（数据文件里的顺序）
  EXPECT_STR("第 4 个症状是胸闷", "胸闷", g.symptomName(3));
  EXPECT_STR("第 2 个症状是咳嗽", "咳嗽", g.symptomName(1));
  EXPECT_STR("第 6 个症状是气短", "气短", g.symptomName(5));

  int picked[3] = {3, 1, 5};
  TriageResult r = Triage::run(h, picked, 3);
  EXPECT_INT("选 3 个症状，记下了 3 个", 3, r.pickedCount);
  EXPECT_TRUE("推荐结果非空", r.count > 0);
  EXPECT_INT("心血管内科得分 = 胸闷3 + 气短2 = 5", 5, r.items[0].score);
  EXPECT_STR("第 1 名是心血管内科", "心血管内科", h.depts[r.items[0].dept].name);
  EXPECT_STR("第 2 名是呼吸内科", "呼吸内科", h.depts[r.items[1].dept].name);
  EXPECT_INT("呼吸内科得分 = 咳嗽3 + 气短2 = 5", 5, r.items[1].score);
  EXPECT_TRUE("推荐结果按得分降序", r.items[0].score >= r.items[1].score && r.items[1].score >= r.items[2].score);

  // 只选一个症状
  int only[1] = {3};
  r = Triage::run(h, only, 1);
  EXPECT_INT("只选胸闷时，第 1 名是心血管内科", 0, r.items[0].dept);
  EXPECT_INT("只选胸闷时得分为边权 3", 3, r.items[0].score);
  EXPECT_INT("只选胸闷时命中 1 个症状", 1, r.items[0].matchCount);

  // 选一个与任何科室都无关的症状不存在（25 个症状都在边表里），
  // 这里验证「空输入」这种退化情形不会崩
  r = Triage::run(h, picked, 0);
  EXPECT_INT("一个症状都不选时推荐数为 0", 0, r.count);

  // 结果最多 5 条
  int many[8] = {0, 1, 2, 3, 4, 5, 6, 7};
  r = Triage::run(h, many, 8);
  EXPECT_TRUE("选 8 个症状时推荐数不超过 5", r.count <= kMaxTriageResult);

  // 同一症状的权重只算一次（hasPicked 去重）
  int twice[2] = {3, 3};
  r = Triage::run(h, twice, 2);
  EXPECT_INT("同一症状重复出现只算一次，得分仍是 3", 3, r.items[0].score);

  check::groupDone();
}

// 第 9 组：就诊路线
void group9Route(const char* dir) {
  check::group(9, "就诊路线 —— 查表 O(1) + 路径还原");
  check::note("Floyd 在建图时算好了任意两点最短路，查询只是查表和顺 next 链还原。");

  Hospital& h = db();
  if (!loadRealData(h, dir)) return;

  EXPECT_INT("房间 48 间", 48, h.rooms.size());

  const int ra101 = h.findRoom("RA101");
  const int rb209 = h.findRoom("RB209");
  EXPECT_TRUE("找得到 RA101", ra101 >= 0);
  EXPECT_TRUE("找得到 RB209", rb209 >= 0);

  RouteResult r = Route::between(h, ra101, rb209);
  EXPECT_TRUE("两点连通", r.found);
  EXPECT_INT("路程 70 米", 70, r.totalDistance);
  EXPECT_INT("途经 5 个顶点", 5, r.stepCount);
  EXPECT_INT("换层 1 次", 1, r.floorChanges);
  EXPECT_INT("路径第 1 个是起点", ra101, r.steps[0]);
  EXPECT_INT("路径最后一个是终点", rb209, r.steps[r.stepCount - 1]);

  // 起点终点相同
  r = Route::between(h, ra101, ra101);
  EXPECT_TRUE("同一点也认为连通", r.found);
  EXPECT_INT("同一点距离为 0", 0, r.totalDistance);
  EXPECT_INT("同一点只途经 1 个顶点（它自己）", 1, r.stepCount);
  EXPECT_INT("同一点换层 0 次", 0, r.floorChanges);

  // 不存在的房间
  r = Route::between(h, -1, rb209);
  EXPECT_FALSE("起点不存在时不返回结果", r.found);
  r = Route::between(h, ra101, 999);
  EXPECT_FALSE("终点不存在时不返回结果", r.found);
  r = Route::between(h, 999, 999);
  EXPECT_FALSE("两端都不存在时不返回结果", r.found);

  // 同层相邻诊室
  const int ra102 = h.findRoom("RA102");
  if (ra101 >= 0 && ra102 >= 0) {
    r = Route::between(h, ra101, ra102);
    EXPECT_TRUE("相邻诊室连通", r.found);
    EXPECT_INT("相邻诊室距离 10 米", 10, r.totalDistance);
    EXPECT_INT("相邻诊室不换层", 0, r.floorChanges);
  }

  // 同一间诊室来回都应当得到相同的距离（图是无向的）
  if (ra101 >= 0 && rb209 >= 0) {
    const RouteResult forth = Route::between(h, ra101, rb209);
    const RouteResult back = Route::between(h, rb209, ra101);
    EXPECT_INT("往返距离相同（路网是无向的）", forth.totalDistance, back.totalDistance);
    EXPECT_INT("往返途经顶点数相同", forth.stepCount, back.stepCount);
  }

  check::groupDone();
}

// 第 10 组：预约号源 + 候诊队列
void group10BookingQueue(const char* dir) {
  check::group(10, "预约号源与候诊队列 —— 检查并扣减、签到叫号过号完成");
  check::note("号源扣减与退回要保持一致；候诊队列守住「先来先看」。");

  Hospital& h = db();
  if (!loadRealData(h, dir)) return;

  // ---------- 号源 ----------
  EXPECT_TRUE("D1 在周一上午有排班", h.hasSchedule(0, 0, 0));
  const int before = h.remaining(0, 0, 0);
  EXPECT_TRUE("排班余号大于 0", before > 0);

  // 扣减
  ScheduleSlot& s = h.slot(0, 0, 0);
  const int versionBefore = s.version;
  ++s.booked;
  s.version = ++h.versionClock;
  EXPECT_INT("扣减一号后余号少 1", before - 1, h.remaining(0, 0, 0));
  EXPECT_TRUE("扣减后版本号变了（旧信息作废）", s.version != versionBefore);

  // 退回
  --s.booked;
  s.version = ++h.versionClock;
  EXPECT_INT("退回后余号恢复", before, h.remaining(0, 0, 0));

  // 没排班的时段余号为 0
  EXPECT_FALSE("D1 周一前夜（下标 2）没有排班", (h.hasSchedule(0, 0, 2)));
  EXPECT_INT("没排班的时段余号为 0", 0, h.remaining(0, 0, 2));
  EXPECT_INT("不存在的医生余号为 0", 0, h.remaining(999, 0, 0));

  // ---------- 候诊队列 ----------
  EXPECT_TRUE("候诊队列初始为空", h.waiting.empty());

  // 签到：P1（下标 0）与 P3（下标 2）都找 D1（下标 0）
  EXPECT_INT("P1 签到成功", QUEUE_OK, (Queue::checkIn(h, 0, 0)));
  EXPECT_INT("P3 签到成功", QUEUE_OK, (Queue::checkIn(h, 2, 0)));
  EXPECT_INT("队列里有 2 人", 2, h.waiting.size());
  EXPECT_INT("P1 排在第 1 位", 1, Queue::positionOf(h, 0, 0));
  EXPECT_INT("P3 排在第 2 位", 2, Queue::positionOf(h, 2, 0));
  EXPECT_INT("P1 的排队号是 1", 1, Queue::queueNoOf(h, 0, 0));
  EXPECT_INT("P3 的排队号是 2", 2, Queue::queueNoOf(h, 2, 0));
  EXPECT_INT("P1 前面 0 人", 0, Queue::waitingAhead(h, 0, 0));
  EXPECT_INT("P3 前面 1 人", 1, Queue::waitingAhead(h, 2, 0));

  // 重复签到应当被拒绝
  EXPECT_INT("P1 重复签到返回 QUEUE_DUP", QUEUE_DUP, (Queue::checkIn(h, 0, 0)));
  EXPECT_INT("重复签到不改变人数", 2, h.waiting.size());

  // 不存在的患者/医生
  EXPECT_INT("不存在的患者签到被拒", QUEUE_NO_DOCTOR, (Queue::checkIn(h, 999, 0)));
  EXPECT_INT("不存在的医生签到被拒", QUEUE_NO_DOCTOR, (Queue::checkIn(h, 0, 999)));

  // 叫号：第 1 次叫 P1
  int called = -1;
  EXPECT_INT("叫号成功", QUEUE_OK, (Queue::callNext(h, 0, called)));
  EXPECT_INT("叫到的是 P1", 0, called);
  EXPECT_INT("P1 已叫号，前面剩 0 人（已叫号的不算在「前面等的人」里）", 0, Queue::waitingAhead(h, 2, 0));

  // 队首已叫号但没离开，不能再叫后面的人
  EXPECT_INT("队首未处理完时叫号返回 QUEUE_FIRST_CALLED", QUEUE_FIRST_CALLED, (Queue::callNext(h, 0, called)));

  // 过号：P1 没来，记两次后重新排队
  EXPECT_FALSE("第 1 次过号不重新排队", Queue::pass(h, 0, 0));
  EXPECT_TRUE("第 2 次过号触发重新排队", Queue::pass(h, 0, 0));
  EXPECT_INT("重新排队后人数仍是 2", 2, h.waiting.size());
  EXPECT_INT("P1 的排队号变成了 3（排到队尾）", 3, Queue::queueNoOf(h, 0, 0));
  EXPECT_INT("P3 升到第 1 位", 1, Queue::positionOf(h, 2, 0));
  EXPECT_INT("P1 退到第 2 位", 2, Queue::positionOf(h, 0, 0));

  // 现在队首是 P3，可以叫了
  EXPECT_INT("队首换成 P3 后可以正常叫号", QUEUE_OK, (Queue::callNext(h, 0, called)));
  EXPECT_INT("叫到的是 P3", 2, called);

  // 就诊完成离开队列
  EXPECT_INT("P3 就诊完成", QUEUE_OK, (Queue::finish(h, 2, 0)));
  EXPECT_INT("离开后队列剩 1 人", 1, h.waiting.size());
  EXPECT_INT("P3 已不在队列里", 0, Queue::positionOf(h, 2, 0));
  EXPECT_FALSE("P3 不在队列里", Queue::isWaiting(h, 2, 0));
  EXPECT_INT("对不在队列里的人调用完成后返回 QUEUE_NOT_FOUND", QUEUE_NOT_FOUND, (Queue::finish(h, 2, 0)));

  // 另一位医生的队列互不干扰
  EXPECT_INT("P2（下标 1）找 D5（下标 4）签到成功", QUEUE_OK, (Queue::checkIn(h, 1, 4)));
  EXPECT_INT("D5 的队列里有 1 人", 1, Queue::countOf(h, 4));
  EXPECT_INT("D1 的队列里仍是 1 人", 1, Queue::countOf(h, 0));
  EXPECT_INT("P2 在 D5 队伍里排第 1 位", 1, Queue::positionOf(h, 1, 4));

  // 队伍空着时叫号
  EXPECT_INT("空队列叫号返回 QUEUE_EMPTY", QUEUE_EMPTY, (Queue::callNext(h, 20, called)));

  // 队列清空后全部复位
  h.waiting.clear();
  EXPECT_TRUE("清空后队列为空", h.waiting.empty());
  EXPECT_INT("清空后 D1 队列人数为 0", 0, Queue::countOf(h, 0));

  check::groupDone();
}

// =============================================================================
//  主程序：按参数选择要跑的组
// =============================================================================
int main(int argc, char** argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
#ifdef _WIN32
  // 让控制台按 UTF-8 显示中文
  std::system("chcp 65001 > nul");
#endif

  const char* dataDir = "data";
  bool run[11] = {false};
  int requestCount = 0;

  for (int i = 1; i < argc; ++i) {
    int no = 0;
    if (std::strcmp(argv[i], "all") == 0) {
      for (int k = 1; k <= 10; ++k) run[k] = true;
      ++requestCount;
      continue;
    }
    // 纯数字参数视为组号
    bool numeric = true;
    for (const char* p = argv[i]; *p; ++p)
      if (*p < '0' || *p > '9') numeric = false;
    if (numeric) {
      no = std::atoi(argv[i]);
      if (no >= 1 && no <= 10) {
        run[no] = true;
        ++requestCount;
      } else {
        std::printf("[警告] 没有第 %d 组（只有 1~10）\n", no);
      }
      continue;
    }
    dataDir = argv[i];  // 不是数字也不是 all，当作数据目录
  }
  if (requestCount == 0) {
    for (int k = 1; k <= 10; ++k) run[k] = true;
  }

  std::printf("============================================================\n");
  std::printf("  数据结构与算法  自检程序\n");
  std::printf("============================================================\n");
  std::printf("  数据目录：%s\n", dataDir);
  std::printf("  运行内容：");
  for (int k = 1; k <= 10; ++k)
    if (run[k]) std::printf(" %d", k);
  std::printf("\n");
  std::printf("\n  说明：每组把结构/算法单独拿出来，用可预期的输入调用，\n");
  std::printf("        把实际结果与预期结果比对。只打印失败的条目。\n");

  if (run[1]) group1SeqList();
  if (run[2]) group2RingQueue();
  if (run[3]) group3HashMap();
  if (run[4]) group4ScheduleTable();
  if (run[5]) group5WeightedGraph();
  if (run[6]) group6BipartiteGraph();
  if (run[7]) group7InsertSort();
  if (run[8]) group8Triage(dataDir);
  if (run[9]) group9Route(dataDir);
  if (run[10]) group10BookingQueue(dataDir);

  const int total = check::gPassed + check::gFailed;
  std::printf("\n============================================================\n");
  std::printf("  自检结果\n");
  std::printf("============================================================\n");
  std::printf("  通过 %d 项，失败 %d 项，共 %d 项\n", check::gPassed, check::gFailed, total);
  if (check::gFailed == 0) {
    std::printf("\n  全部通过。\n");
    return 0;
  }
  std::printf("\n  有 %d 项未通过，请按上面的「位置」逐条排查。\n", check::gFailed);
  return 1;
}
