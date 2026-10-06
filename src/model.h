/* =============================================================================
 *  model.h  —  数据模型与数据总库
 *
 *  设计原则（课设够用即可，不追求与现实完全一致）：
 *    1) 全部实体都是「定长记录」，只存数据不存指针，可以整体拷贝；
 *    2) 实体之间用「下标」互相引用，不用指针；
 *    3) 全部表的容量在编译期由常量确定，运行期不申请内存；
 *    4) 路网扁平化：把各楼各层的房间统一编成一张节点表，不做「楼->层->房间」的
 *       多级嵌套——多级结构会让最短路算法要跨层查找，代码显著变长而收益很小。
 * ========================================================================== */
#pragma once

#include "ds.h"

// ------------------------------- 容量上限 -------------------------------
constexpr int kMaxDept     = 16;   // 科室数
constexpr int kMaxDoctor   = 64;   // 医生数
constexpr int kMaxPatient  = 128;  // 患者数
constexpr int kMaxBook     = 256;  // 预约记录数
constexpr int kMaxSymptom  = 64;   // 症状总数
constexpr int kMaxSympPerDept = 8; // 每个科室最多关联多少个症状
constexpr int kMaxSlot     = 4;    // 每日时段数：上午/下午/前夜/后夜
constexpr int kMaxDay      = 7;    // 排班天数（一周）
constexpr int kMaxRoom     = 64;   // 房间数（含楼梯口、电梯口）
constexpr int kMaxFloor    = 4;    // 楼层数

constexpr int kMaxWaiting = 64;    // 候诊队列长度（全院共用一条队列）

// ================================ 科室 ================================
// 注意：这类字段的长度要按 UTF-8 算——一个汉字占 3 字节。
//       例如「门诊楼1层心血管内科诊室」是 11×3 = 33 字节，short 的数组会把它截断，
//       显示出来就成了半个汉字。凡是装中文名称的字段都留足余量。
struct Department {
  char code[8];                              // 编号，如 K01
  char name[24];                             // 名称，如 心血管内科
  char location[40];                         // 位置，如 门诊楼1层东侧
  int  symptoms[kMaxSympPerDept];            // 关联症状的下标（导诊用）
  int  weights[kMaxSympPerDept];             // 各症状的关联强度 1..3
  int  symptomCount;
};

// ================================ 医生 ================================
struct Doctor {
  char code[8];
  char name[16];
  int  dept;          // 所属科室下标
  char title[16];     // 职称：主任医师 / 副主任医师 / 主治医师 / 住院医师
  char skill[48];     // 擅长领域
  int  room;          // 出诊房间下标
  int  rankSum;       // 患者评分累计（扩展功能：评价）
  int  rankCount;     // 评分人数
};

// ================================ 房间 ================================
// 房间是路网的一个节点。楼梯口、电梯口也登记为房间，因此它们天然参与寻路。
struct Room {
  char code[8];       // 编号，全局唯一，如 RA101
  char name[64];      // 名称，如 门诊楼1层心血管内科诊室（33 字节 + 余量）
  int  floor;         // 所在楼层，0 起
  int  dept;          // 所属科室下标，-1 表示公共设施（楼梯、电梯）
  int  node;          // 在路网中的节点号
};

// ================================ 症状 ================================
struct Symptom {
  char name[24];
};

// ================================ 排班 ================================
// 排班槽 = 医生 × 日期 × 时段。号源用「总量 + 已约」表示，剩余量现算。
//
// 存储方式的选择：排班槽按「医生 × 日期 × 时段」三维索引，因此直接用三维数组存。
// 数组的下标就是主键，查找是 O(1) 的下标运算，不需要再建索引，也不会出现
// 「顺序表里按顺序追加、查找时却按下标公式定位」这种下标对不上的问题。
// 医生没排班的时段用 assigned 标记为「未使用」。
struct Schedule {
  int  quota;         // 最大接诊数
  int  booked;        // 已预约数
  int  version;       // 版本号，用于预约时的并发校验
  bool assigned;      // 该时段是否有排班
  bool stopped;       // 是否停诊（扩展功能：停诊通知）
};

// ================================ 患者与预约 ================================
struct Patient {
  char code[8];
  char name[16];
  char phone[16];
};

// 预约状态
enum BookStatus { BOOKED = 0, CANCELLED = 1, FINISHED = 2 };

struct Booking {
  char ticket[16];    // 预约凭证号，如 TK0001
  int  patient;
  int  doctor;
  int  day;
  int  slot;
  int  status;
};

// ================================ 候诊队列 ================================
// 患者到院签到后按顺序排队，医生依次叫号。
// 队列里只记「谁在排、排到几号」，其余信息通过预约记录去查，不重复保存。
// 叫号后没人来，记满这个次数就重新排到队尾（不再挡着后面的人）
constexpr int kMaxPass = 2;

struct Waiting {
  int patient;      // 患者下标（姓名从这里查）
  int doctor;       // 接诊医生下标（对应诊室从这里查）
  int queueNo;      // 排队号，从 1 开始，同一医生内递增
  int calledCount;  // 已叫号次数：0 未叫，1 已叫，≥2 说明过号了
};

// ================================ 数据总库 ================================
// 全部数据表集中在一个对象里，各功能模块共享同一份数据，不会出现状态不一致。
// 由于总库体积超过默认的 1MB 线程栈，程序中以「函数内静态对象」方式持有。
class Hospital {
 public:
  // 排班三维数组：下标就是主键（医生号、日期、时段），查找是 O(1) 的下标运算。
  // 容量固定为 kMaxDoctor × kMaxDay × kMaxSlot，约 56 KB。
  Schedule schedule[kMaxDoctor][kMaxDay][kMaxSlot];

  SeqList<Department, kMaxDept> depts;
  SeqList<Doctor, kMaxDoctor>   doctors;
  SeqList<Patient, kMaxPatient> patients;
  SeqList<Booking, kMaxBook>    bookings;
  SeqList<Room, kMaxRoom>       rooms;
  SeqList<Symptom, kMaxSymptom> symptoms;

  // 候诊队列：用环形队列实现，先进先出，出队的位置可以被后来的患者循环使用。
  // 全院共用一条队列，靠 doctor 字段区分是哪个诊室的队伍。
  // 不采用「每位医生一条队列」是因为医生有 64 位、每位队列要 32 个位置，
  // 那样光队列就要占 150 KB 以上；共用一条队列既省空间，又能直接按
  // 就诊顺序查看全院候诊情况。
  RingQueue<Waiting, kMaxWaiting> waiting;
  int queueSeq = 0;   // 排队号自增序号

  RoadNet road;          // 就诊路网（邻接矩阵 + Floyd 预计算的最短路）

  // 索引：编号 -> 表内下标
  HashMap deptIndex;
  HashMap doctorIndex;
  HashMap patientIndex;
  HashMap roomIndex;

  int ticketSeq = 0;     // 凭证号自增序号
  int versionClock = 0;  // 版本时钟：任何号源变动都取一个新版本号

  void clear() {
    depts.clear();
    doctors.clear();
    patients.clear();
    bookings.clear();
    rooms.clear();
    symptoms.clear();
    waiting.clear();
    queueSeq = 0;
    for (int d = 0; d < kMaxDoctor; ++d)
      for (int day = 0; day < kMaxDay; ++day)
        for (int s = 0; s < kMaxSlot; ++s) schedule[d][day][s].assigned = false;
    road.init(0);
    deptIndex.clear();
    doctorIndex.clear();
    patientIndex.clear();
    roomIndex.clear();
    ticketSeq = 0;
    versionClock = 0;
  }

  // ---------------------------- 按编号查找 ----------------------------
  int findDept(const char* code) const { return lookup(deptIndex, code); }
  int findDoctor(const char* code) const { return lookup(doctorIndex, code); }
  int findPatient(const char* code) const { return lookup(patientIndex, code); }
  int findRoom(const char* code) const { return lookup(roomIndex, code); }

  // ---------------------------- 排班槽访问 ----------------------------
  // 排班槽就是「医生 × 日期 × 时段」这个格子；没排班的格子 assigned == false。
  Schedule& slot(int doctor, int day, int s) { return schedule[doctor][day][s]; }
  const Schedule& slot(int doctor, int day, int s) const { return schedule[doctor][day][s]; }

  // 该时段是否有排班
  bool hasSchedule(int doctor, int day, int s) const {
    if (doctor < 0 || doctor >= kMaxDoctor || day < 0 || day >= kMaxDay || s < 0 || s >= kMaxSlot) return false;
    return schedule[doctor][day][s].assigned;
  }

  // 剩余号源；没排班或已停诊都返回 0
  int remaining(int doctor, int day, int s) const {
    if (!hasSchedule(doctor, day, s)) return 0;
    const Schedule& sc = schedule[doctor][day][s];
    if (sc.stopped) return 0;
    const int left = sc.quota - sc.booked;
    return left > 0 ? left : 0;
  }

  // ---------------------------- 按名称查找科室 ----------------------------
  int findDeptByName(const char* name) const {
    for (int i = 0; i < depts.size(); ++i)
      if (std::strcmp(depts[i].name, name) == 0) return i;
    return -1;
  }

  int findSymptomByName(const char* name) const {
    for (int i = 0; i < symptoms.size(); ++i)
      if (std::strcmp(symptoms[i].name, name) == 0) return i;
    return -1;
  }

  // 路网节点号 -> 房间下标（还原路线时用）
  int findRoomByNode(int node) const {
    for (int i = 0; i < rooms.size(); ++i)
      if (rooms[i].node == node) return i;
    return -1;
  }

  // 某科室的房间（一个科室通常占同一层的若干连续诊室）
  int roomsOfDept(int deptIdx, int* out, int cap) const {
    int n = 0;
    for (int i = 0; i < rooms.size() && n < cap; ++i)
      if (rooms[i].dept == deptIdx) out[n++] = i;
    return n;
  }

 private:
  static int lookup(const HashMap& map, const char* code) {
    int idx = -1;
    return map.get(code, idx) ? idx : -1;
  }
};

// ------------------------------- 内存规模约束 -------------------------------
static_assert(sizeof(Hospital) < 4 * 1024 * 1024, "数据总库规模超出预期，请检查容量配置");

// ------------------------------- 数据总库的唯一实例 -------------------------------
// 总库体积超过默认的 1MB 线程栈，不能作为局部变量，因此由 main.cpp 提供一个
// 「函数内静态对象」，各模块统一通过 db() 访问同一份数据。
// 这里只做声明，避免每个 .cpp 各建一份而出现数据不一致。
extern Hospital& db();
