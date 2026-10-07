/* =============================================================================
 *  model.h  —  数据模型与数据总库
 *
 *  设计原则（课设够用即可，不追求与现实完全一致，可为了简化代码对模型做简化）：
 *    1) 全部实体都是「定长记录」，只存数据不存指针，可以整体拷贝；
 *    2) 实体之间用「下标」互相引用，不用指针；
 *    3) 全部表的容量在编译期由常量确定，运行期不申请内存；
 *    4) 路网扁平化：把各楼各层的房间统一编成一张顶点表，不做「楼->层->房间」的
 *       多级嵌套——多级结构会让最短路算法要跨层查找，代码显著变长而收益很小。
 *
 *  排班、症状与科室的关联这两处按题目实现提示来组织：
 *    · 排班用二维数组，按日期和时段索引（提示第 3 条）——见 Doctor::schedule
 *    · 症状与科室的关联用带权二部图（提示第 1 条）——见 Hospital::symptomGraph
 * ========================================================================== */
#pragma once

#include "ds.h"

// ------------------------------- 容量上限 -------------------------------
constexpr int kMaxDept = 16;     // 科室数
constexpr int kMaxDoctor = 64;   // 医生数
constexpr int kMaxPatient = 128; // 患者数
constexpr int kMaxBook = 256;    // 预约记录数
constexpr int kMaxDay = 7;       // 排班天数（一周）
constexpr int kMaxRoom = 64;     // 房间数（含楼梯口、电梯口）
constexpr int kMaxFloor = 4;     // 楼层数

constexpr int kMaxWaiting = 64;  // 候诊队列长度（全院共用一条队列）
constexpr int kMaxPass = 2;      // 叫号后没人来，记满这个次数就重新排到队尾

// ================================ 科室 ================================
// 注意：这类字段的长度要按 UTF-8 算——一个汉字占 3 字节。
//       例如「门诊楼1层心血管内科诊室」是 11×3 = 33 字节，短的数组会把它截断，
//       显示出来就成了半个汉字。凡是装中文名称的字段都留足余量。
//
// 症状与科室的关联不在这里存：题目提示第 1 条要求用加权二部图表示，
// 因此关联关系集中在 Hospital::symptomGraph 里，科室只保留一句描述文本。
struct Department {
  char code[8];           // 编号，如 K01
  char name[24];          // 名称，如 心血管内科
  char location[40];      // 位置，如 门诊楼1层东侧
  char description[120];  // 科室描述，里面写出常见症状，供导诊时与患者症状匹配
};

// ================================ 医生 ================================
// 排班按题目提示第 3 条用二维数组存，按「日期 × 时段」索引。
// 放在医生记录里，就是「每位医生一张排班表」，查询时下标直接就是主键。
struct Doctor {
  char code[8];
  char name[16];
  int dept;        // 所属科室下标
  char title[16];  // 职称：主任医师 / 副主任医师 / 主治医师 / 住院医师
  char skill[48];  // 擅长领域
  int room;        // 出诊房间下标
  int rankSum;     // 患者评分累计（扩展功能：评价）
  int rankCount;   // 评分人数

  ScheduleTable<kMaxDay, kMaxSlot> schedule;  // 排班二维数组：日期 × 时段
};

// ================================ 房间 ================================
// 房间是路网的一个顶点。楼梯口、电梯口也登记为房间，因此它们天然参与寻路。
struct Room {
  char code[8];   // 编号，全局唯一，如 RA101
  char name[64];  // 名称，如 门诊楼1层心血管内科诊室（33 字节 + 余量）
  int floor;      // 所在楼层，0 起
  int dept;       // 所属科室下标，-1 表示公共设施（楼梯、电梯）
  int node;       // 在路网中的顶点号
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
  char ticket[16];  // 预约凭证号，如 TK0001
  int patient;
  int doctor;
  int day;
  int slot;
  int status;
};

// ================================ 候诊队列 ================================
// 患者到院签到后按顺序排队，医生依次叫号。
// 队列里只记「谁在排、排到几号、叫过几次」，其余信息通过下标去查，不重复保存。
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
  SeqList<Department, kMaxDept> depts;
  SeqList<Doctor, kMaxDoctor> doctors;
  SeqList<Patient, kMaxPatient> patients;
  SeqList<Booking, kMaxBook> bookings;
  SeqList<Room, kMaxRoom> rooms;

  // 症状与科室的加权二部图：一侧是症状顶点，一侧是科室顶点，边权是关联强度
  BipartiteGraph symptomGraph;

  // 候诊队列：用环形队列实现，先进先出，出队的位置可以被后来的患者循环使用。
  // 全院共用一条队列，靠 doctor 字段区分是哪个诊室的队伍。
  // 不采用「每位医生一条队列」是因为医生有 64 位、每位队列要 32 个位置，
  // 那样光队列就要占 32 KB 以上；共用一条队列既省空间，又能直接按
  // 就诊顺序查看全院候诊情况。
  RingQueue<Waiting, kMaxWaiting> waiting;
  int queueSeq = 0;  // 排队号自增序号

  WeightedGraph road;  // 就诊路网（邻接矩阵 + Floyd 预计算的最短路）

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
    symptomGraph.clear();
    waiting.clear();
    queueSeq = 0;
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
  // 排班是「每位医生一张 日期×时段 二维表」，所以先定位医生，再按下标取槽。
  // 医生不存在或该时段没排班时，hasSchedule 返回 false。
  bool hasSchedule(int doctor, int day, int slot) const {
    if (doctor < 0 || doctor >= doctors.size()) return false;
    return doctors[doctor].schedule.assigned(day, slot);
  }

  ScheduleSlot& slot(int doctor, int day, int slot) { return doctors[doctor].schedule.slot(day, slot); }

  // 剩余号源；没排班或已停诊都返回 0
  int remaining(int doctor, int day, int slot) const {
    if (doctor < 0 || doctor >= doctors.size()) return 0;
    return doctors[doctor].schedule.remaining(day, slot);
  }

  // ---------------------------- 按名称查找科室 ----------------------------
  int findDeptByName(const char* name) const {
    for (int i = 0; i < depts.size(); ++i)
      if (std::strcmp(depts[i].name, name) == 0) return i;
    return -1;
  }

  // 路网顶点号 -> 房间下标（还原路线时用）
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
