/* =============================================================================
 *  ds_list.h  —  实体定义 + 五个顺序表 + 排班二维数组（全部不使用模板）
 *
 *  本文件含两部分：
 *    第一部分  6 个实体的定义（科室、医生、房间、患者、预约、候诊元素）
 *    第二部分  6 个顺序表类，每个装一种实体
 *
 *  ---------------------------------------------------------------------------
 *  一、为什么实体定义放在这里，而不是 model.h
 *
 *  因为顺序表要按元素类型分开写，就必须先知道元素类型是什么。
 *  如果实体留在 model.h，而 model.h 又要 include 本文件，就成环了。
 *
 *  摆成一条无环的链：
 *      ds_base.h  （常量上限、散列表 HashNode / HashMap）
 *          ↑
 *      ds_list.h  （实体 + 顺序表）      ← 本文件
 *          ↑
 *      ds.h       （队列、二维数组、带权图、二部图）
 *          ↑
 *      model.h    （数据总库 Hospital + db() 声明）
 *
 *  这样任何一层单独看都不会缺定义。
 *  ---------------------------------------------------------------------------
 *
 *  二、为什么这里有五个几乎一样的类，而不是一个模板
 *
 *  课程要求「不能使用类库、模板等（用户界面除外）」，考查的是数据结构与算法本身。
 *  最开始的写法是 `template <typename T, int N> class SeqList`，
 *  展开之后 `SeqList<Doctor, 64>` 就是一个「Doctor 的定长数组 + 长度」，
 *  和手写一个 `DoctorList` 在生成的机器码上完全等价 —— 但「模板」这两个字
 *  确实在禁用的名单里。为了不留下任何可指摘的地方，这里改成具体类型。
 *
 *  只有五种实体需要「顺序表」这种容器，所以是五个类；排班表（ScheduleTable）
 *  是二维数组，不是顺序表，见本文件前面的定义。
 *
 *  五个类长得一模一样是有意的：
 *    · 每个类都能独立看懂，不需要先理解模板怎么实例化；
 *    · 调试时栈上的类型名是 `DoctorList` 而不是 `SeqList<Doctor,64>`；
 *    · 万一将来只有某个表需要加功能（比如按某字段排序），
 *      改那一个类就行，不会牵动其它五个。
 *
 *  代价是约 150 行重复代码。这是为了满足课程约束而明确选择的代价，
 *  不是疏忽 —— 这一点应当写进报告。
 *  ---------------------------------------------------------------------------
 *
 *  三、五个类的统一接口（只是元素类型不同）
 *
 *      int  size() const           当前元素个数
 *      bool empty() const          是否为空
 *      bool full() const           是否已满
 *      static constexpr int capacity()   容量上限（编译期常量，可静态调用）
 *      T&   operator[](int i)      按下标读写（不做越界检查，与数组一致）
 *      int  push(const T& v)       尾部追加，返回新元素下标；满了返回 -1
 *      bool removeAt(int i)        按下标删除，后续元素前移；下标非法返回 false
 *      void clear()                只把长度清零（不擦内存，O(1)）
 *
 *  共用说明（设计理由）见本文件末尾。
 * ========================================================================== */
#pragma once

#include <cstring>

#include "ds_base.h"

// =============================================================================
//  容量上限
//
//  全部表的容量在编译期确定，运行期不申请内存（见文件末尾第 4 条说明）。
//  如果数据文件比容量还大，记录会被丢弃 —— 这种失败必须是响亮的，
//  load.h 把「容量溢出」单独计数，程序会提示「请调大对应的 kMax* 常量」。
// =============================================================================
constexpr int kMaxDept = 16;      // 科室数
constexpr int kMaxDoctor = 64;    // 医生数
constexpr int kMaxPatient = 128;  // 患者数
constexpr int kMaxBook = 256;     // 预约记录数
constexpr int kMaxDay = 7;        // 排班天数（一周）
constexpr int kMaxRoom = 64;      // 房间数（含楼梯口、电梯口）
constexpr int kMaxFloor = 4;      // 楼层数

constexpr int kMaxWaiting = 64;  // 候诊队列长度（全院共用一条队列）
constexpr int kMaxPass = 2;      // 叫号后没人来，记满这个次数就重新排到队尾

// 每天的时段数：0上午 1下午 2前夜 3后夜。
// 当前数据只用前两个（门诊不排夜班），容量留到 4 是为了将来加时段时不必改结构。
constexpr int kMaxSlot = 4;

// =============================================================================
//  实体定义
//
//  四条建模原则：
//    1) 全部实体都是定长记录，只存数据不存指针，可以整体拷贝；
//    2) 实体之间用「下标」互相引用，不用指针；
//    3) 字段容量在编译期确定，运行期不申请内存；
//    4) 路网扁平化：把各楼各层的房间统一编成一张顶点表，不做「楼->层->房间」
//       的多级嵌套 —— 多级结构会让最短路算法要跨层查找，代码显著变长而收益很小。
//
//  注意：这类字段的长度要按 UTF-8 算 —— 一个汉字占 3 字节。
//        例如「门诊楼1层心血管内科诊室」是 11×3 = 33 字节，短的数组会把它截断，
//        显示出来就成了半个汉字。凡是装中文名称的字段都留足余量。
// =============================================================================

// ------------------------------- 科室 -------------------------------
// 症状与科室的关联不在这里存：题目提示第 1 条要求用加权二部图表示，
// 因此关联关系集中在 Hospital::symptomGraph 里，科室只保留一句描述文本。
struct Department {
  char code[8];           // 编号，如 K01
  char name[24];          // 名称，如 心血管内科
  char location[40];      // 位置，如 门诊楼1层东侧
  char description[120];  // 科室描述，里面写出常见症状，供导诊时与患者症状匹配
};

// ------------------------------- 排班槽 -------------------------------
// 排班表的一个格子，两个下标是「日期」和「时段」。
//
// assigned 这个布尔量是不可省的：「没排班」和「排班了但号源是 0」是两种
// 完全不同的状态，界面上一个显示「不出诊」一个显示「已约满」。
// 如果只用 quota == 0 表示没排班，这两种情况就分不出来了。
struct ScheduleSlot {
  int quota;       // 最大接诊数
  int booked;      // 已预约数
  int version;     // 版本号，用于预约时的并发校验
  bool assigned;   // 该时段是否有排班
  bool stopped;    // 是否停诊
};

// =============================================================================
//  二维数组：医生排班表，按「日期 × 时段」索引（题目实现提示第 3 条）
//
//  排班信息的主键是「日期 + 时段」，所以二维数组的下标本身就是主键：
//  查某个时段有没有号、还剩几个号，都是一次下标运算，O(1)。
//
//  尺寸固定 7 天 × 4 时段，所以不用模板参数。
//  曾经把这两个维度做成模板参数，好处是「将来改维度不用改类型名」；
//  但天数和时段数在本系统里是固定的（一周七天上下午），
//  直接写常量更简单，也少一层需要理解的东西。
//
//  放在 ds_list.h 而不是 ds.h：因为医生记录里要直接嵌一个 ScheduleTable
//  对象，它必须先于 Doctor 完整定义。摆在这里，ds.h 才不用反过来 include
//  「还没定义完的自己」。
// =============================================================================
class ScheduleTable {
 public:
  ScheduleTable() { clear(); }

  void clear() {
    for (int d = 0; d < kMaxDay; ++d)
      for (int s = 0; s < kMaxSlot; ++s) slot_[d][s].assigned = false;
  }

  static int days() { return kMaxDay; }
  static int slots() { return kMaxSlot; }
  static bool validDay(int day) { return day >= 0 && day < kMaxDay; }
  static bool validSlot(int s) { return s >= 0 && s < kMaxSlot; }

  // 按下标取到排班槽。参数合法时就是一次下标运算；不合法时返回 (0,0) 这个
  // 安全位置。注意这只是安全网 —— 调用方应当先用 validDay/validSlot 判断，
  // 因为返回 (0,0) 会静默改错格子，比崩溃更难查。
  ScheduleSlot& slot(int day, int s) {
    return slot_[validDay(day) ? day : 0][validSlot(s) ? s : 0];
  }
  const ScheduleSlot& slot(int day, int s) const {
    return slot_[validDay(day) ? day : 0][validSlot(s) ? s : 0];
  }

  bool assigned(int day, int s) const {
    return validDay(day) && validSlot(s) && slot_[day][s].assigned;
  }

  // 剩余号源；没排班或已停诊都返回 0。
  // 最后那个 left > 0 ? left : 0 是兜底不变式：正常情况下 booked <= quota 不会负，
  // 但如果数据或逻辑曾出过问题留下了 booked > quota，这里返回 0 而不是负数 ——
  // 否则界面上会出现「剩余 -2 个号」这种荒唐显示，用户完全不知道该信哪个数。
  int remaining(int day, int s) const {
    if (!assigned(day, s)) return 0;
    const ScheduleSlot& v = slot_[day][s];
    if (v.stopped) return 0;
    const int left = v.quota - v.booked;
    return left > 0 ? left : 0;
  }

 private:
  ScheduleSlot slot_[kMaxDay][kMaxSlot];
};

// ------------------------------- 医生 -------------------------------
// 排班按题目提示第 3 条用二维数组存，按「日期 × 时段」索引。
// 放在医生记录里，就是「每位医生一张排班表」，查询时下标直接就是主键。
struct Doctor {
  char code[8];
  char name[16];
  int dept;        // 所属科室下标
  char title[16];  // 职称：主任医师 / 副主任医师 / 主治医师 / 住院医师
  char skill[48];  // 擅长领域
  int room;        // 出诊房间下标
  int rankSum;     // 患者评分累计
  int rankCount;   // 评分人数

  // 日期 × 时段的排班二维数组（kMaxDay × kMaxSlot = 7 × 4 = 28 个槽）
  ScheduleTable schedule;
};

// ------------------------------- 房间 -------------------------------
// 房间是路网的一个顶点。楼梯口、电梯口也登记为房间，因此它们天然参与寻路。
struct Room {
  char code[8];   // 编号，全局唯一，如 RA101
  char name[64];  // 名称，如 门诊楼1层心血管内科诊室（33 字节 + 余量）
  int floor;      // 所在楼层，0 起
  int dept;       // 所属科室下标，-1 表示公共设施（楼梯、电梯）
  int node;       // 在路网中的顶点号
};

// ------------------------------- 患者与预约 -------------------------------
struct Patient {
  char code[8];
  char name[16];
  char phone[16];
};

// 预约状态
enum BookStatus { BOOKED = 0, CANCELLED = 1, FINISHED = 2 };

struct Booking {
  char ticket[16];  // 预约凭证号，如 TK0001
  int patient;
  int doctor;
  int day;
  int slot;
  int status;
};

// ------------------------------- 候诊队列元素 -------------------------------
// 患者到院签到后按顺序排队，医生依次叫号。
// 队列里只记「谁在排、排到几号、叫过几次」，其余信息通过下标去查，不重复保存 ——
// 这样不会出现「队列里的姓名和患者表不一致」这种两份数据对不上的问题。
struct Waiting {
  int patient;      // 患者下标（姓名从这里查）
  int doctor;       // 接诊医生下标（对应诊室从这里查）
  int queueNo;      // 排队号，从 1 开始，同一医生内递增
  int calledCount;  // 已叫号次数：0 未叫，1 已叫，≥2 说明过号了
};

// =============================================================================
//  顺序表一：科室表（最多 16 个）
// =============================================================================
class DepartmentList {
 public:
  int size() const { return n_; }
  bool empty() const { return n_ == 0; }
  bool full() const { return n_ >= kMaxDept; }
  static constexpr int capacity() { return kMaxDept; }

  Department& operator[](int i) { return data_[i]; }
  const Department& operator[](int i) const { return data_[i]; }

  // 尾部追加；容量已满返回 -1，否则返回新元素下标
  int push(const Department& v) {
    if (n_ >= kMaxDept) return -1;
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
  Department data_[kMaxDept];
  int n_ = 0;
};

// =============================================================================
//  顺序表二：医生表（最多 64 名，每人内含 28 个排班槽，所以这张表最大）
// =============================================================================
class DoctorList {
 public:
  int size() const { return n_; }
  bool empty() const { return n_ == 0; }
  bool full() const { return n_ >= kMaxDoctor; }
  static constexpr int capacity() { return kMaxDoctor; }

  Doctor& operator[](int i) { return data_[i]; }
  const Doctor& operator[](int i) const { return data_[i]; }

  int push(const Doctor& v) {
    if (n_ >= kMaxDoctor) return -1;
    data_[n_] = v;
    return n_++;
  }

  bool removeAt(int i) {
    if (i < 0 || i >= n_) return false;
    for (int k = i; k + 1 < n_; ++k) data_[k] = data_[k + 1];
    --n_;
    return true;
  }

  void clear() { n_ = 0; }

 private:
  Doctor data_[kMaxDoctor];
  int n_ = 0;
};

// =============================================================================
//  顺序表三：患者表（最多 128 名）
// =============================================================================
class PatientList {
 public:
  int size() const { return n_; }
  bool empty() const { return n_ == 0; }
  bool full() const { return n_ >= kMaxPatient; }
  static constexpr int capacity() { return kMaxPatient; }

  Patient& operator[](int i) { return data_[i]; }
  const Patient& operator[](int i) const { return data_[i]; }

  int push(const Patient& v) {
    if (n_ >= kMaxPatient) return -1;
    data_[n_] = v;
    return n_++;
  }

  bool removeAt(int i) {
    if (i < 0 || i >= n_) return false;
    for (int k = i; k + 1 < n_; ++k) data_[k] = data_[k + 1];
    --n_;
    return true;
  }

  void clear() { n_ = 0; }

 private:
  Patient data_[kMaxPatient];
  int n_ = 0;
};

// =============================================================================
//  顺序表四：预约表（最多 256 条）
//
//  这张表实际上「只增不删」：取消预约是把 status 改成 CANCELLED 而不是删记录，
//  因为患者要能看到自己的历史记录。removeAt 保留是为了接口一致。
// =============================================================================
class BookingList {
 public:
  int size() const { return n_; }
  bool empty() const { return n_ == 0; }
  bool full() const { return n_ >= kMaxBook; }
  static constexpr int capacity() { return kMaxBook; }

  Booking& operator[](int i) { return data_[i]; }
  const Booking& operator[](int i) const { return data_[i]; }

  int push(const Booking& v) {
    if (n_ >= kMaxBook) return -1;
    data_[n_] = v;
    return n_++;
  }

  bool removeAt(int i) {
    if (i < 0 || i >= n_) return false;
    for (int k = i; k + 1 < n_; ++k) data_[k] = data_[k + 1];
    --n_;
    return true;
  }

  void clear() { n_ = 0; }

 private:
  Booking data_[kMaxBook];
  int n_ = 0;
};

// =============================================================================
//  顺序表五：房间表（最多 64 个，含楼梯口、电梯口）
// =============================================================================
class RoomList {
 public:
  int size() const { return n_; }
  bool empty() const { return n_ == 0; }
  bool full() const { return n_ >= kMaxRoom; }
  static constexpr int capacity() { return kMaxRoom; }

  Room& operator[](int i) { return data_[i]; }
  const Room& operator[](int i) const { return data_[i]; }

  int push(const Room& v) {
    if (n_ >= kMaxRoom) return -1;
    data_[n_] = v;
    return n_++;
  }

  bool removeAt(int i) {
    if (i < 0 || i >= n_) return false;
    for (int k = i; k + 1 < n_; ++k) data_[k] = data_[k + 1];
    --n_;
    return true;
  }

  void clear() { n_ = 0; }

 private:
  Room data_[kMaxRoom];
  int n_ = 0;
};

/* =============================================================================
 *  共用说明（五个类都适用）
 *
 *  一、为什么是「定长数组 + 长度」而不是动态数组
 *      容量在编译期由常量确定，运行期不申请任何内存。好处有三：
 *        1) 没有内存分配就没有失败路径，不需要处理分配异常；
 *        2) 没有内存碎片，数据永远连续存放；
 *        3) 容量上限能编译期检查（见 model.h 的 static_assert）。
 *      代价是容量满了会丢数据，所以 load.h 把「容量溢出」单独计数，
 *      程序会明确提示「请调大 kMaxDept 这类常量」，而不是静默丢。
 *
 *  二、为什么 push 返回「下标」而不是「成功与否」
 *      返回下标是有意的：装载数据时可以直接拿来建索引，省一次查找。
 *          const int idx = h.doctors.push(d);
 *          h.doctorIndex.put(h.doctors[idx].code, idx);
 *      注意 push 满时返回 -1，而 WaitingQueue::pop 返回的才是成功与否 ——
 *      两个相似的函数返回语义不同，读代码时看清。
 *
 *  三、为什么 removeAt 一定要把后续元素前移
 *      因为「连续存放」是 O(1) 随机访问的前提。一旦中间留洞，
 *      第 i 个元素就不知道在哪了。代价是删除 O(n)，而本系统
 *      查询远多于删除（查医生、查排班、查房间、查预约全是随机访问），
 *      所以这个取舍是对的。
 *
 *  四、为什么 clear() 不擦内存
 *      只把 n_ 置零，旧数据还在但已不可见（所有访问都受 n_ 限制）。
 *      这样清空是 O(1)，而不是 O(容量) 的 memset。
 *      对 DoctorList（35 KB）这个差别有意义。
 *
 *  五、为什么不用链表
 *      链表要指针、要动态分配，与「运行期不申请内存」直接冲突；
 *      而且查询要 O(n) 定位，缓存也不友好。本系统读远多于写，顺序表更合适。
 *
 *  六、复杂度
 *      operator[]  O(1)   连续存放，一次地址运算
 *      push        O(1)   只在尾部追加
 *      removeAt    O(n)   后续元素前移
 *      size/empty/full/capacity   O(1)   只读 n_ 与常量
 * ========================================================================== */
