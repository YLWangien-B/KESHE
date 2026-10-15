/* =============================================================================
 *  model.h  —  数据总库 Hospital
 *
 *  实体定义与各表的容量常量已经移到 ds_list.h（顺序表需要先知道元素类型，
 *  所以实体必须排在一起）。本文件只负责把它们聚合成「数据总库」。
 *
 *  分层（无环，见 ds_list.h 的说明）：
 *      ds_base.h  基础工具（散列表）
 *      ds_list.h  实体定义 + 六个顺序表
 *      ds.h       环形队列、二维数组、带权图、带权二部图
 *      model.h    本文件：数据总库
 *
 *  排班、症状与科室的关联这两处按题目实现提示来组织：
 *    · 排班用二维数组，按日期和时段索引（提示第 3 条）——见 Doctor::schedule
 *    · 症状与科室的关联用带权二部图（提示第 1 条）——见 Hospital::symptomGraph
 * ========================================================================== */
#pragma once

#include <cstring>

#include "ds.h"
#include "ds_list.h"

// =============================================================================
//  数据总库：全部数据表集中在一个对象里
//
//  各功能模块共享同一份数据，不会出现状态不一致。
//
//  内存占用约 361 KB，其中最大的是：
//      WeightedGraph road     131 KB（两个 128×128 的 int 矩阵）
//      HashMap × 4            131 KB（结点池 1024 × 32 字节）
//      DoctorList doctors      35 KB（每人内含 28 个排班槽）
//
//  怎么持有它（见文件末尾）：
//      361 KB 没有超过默认的 1 MB 线程栈，但一个大对象放在栈上不是好习惯
//      —— 将来加表就可能溢出。因此用「函数内静态对象」持有（静态存储区）。
// =============================================================================
class Hospital {
 public:
  DepartmentList depts;    // 科室表
  DoctorList doctors;      // 医生表（每人内含一张 日期 × 时段 的排班表）
  PatientList patients;    // 患者表
  BookingList bookings;    // 预约记录表
  RoomList rooms;          // 房间表（含楼梯口、电梯口，它们也是路网顶点）

  // 症状与科室的加权二部图：一侧是症状顶点，一侧是科室顶点，边权是关联强度。
  // 这是智能导诊的数学模型（题目实现提示第 1 条）。
  BipartiteGraph symptomGraph;

  // 候诊队列：用环形队列实现，先进先出，出队的位置可以被后来的患者循环使用。
  //
  // 全院共用一条队列，靠 doctor 字段区分是哪个诊室的队伍。
  // 不采用「每位医生一条队列」是因为医生有 64 位、每位队列要 32 个位置，
  // 那样光队列就要占 32 KB 以上；共用一条队列既省空间（64×16 = 1 KB），
  // 又能直接按就诊顺序查看全院候诊情况。
  WaitingQueue waiting;
  int queueSeq = 0;  // 排队号自增序号

  WeightedGraph road;  // 就诊路网（邻接矩阵 + Floyd 预计算的最短路）

  // 索引：编号 -> 表内下标。四个散列表，让「按编号查」从 O(n) 变成平均 O(1)。
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
  // 科室表只有 12 条，线性查找足够；这里没有为它建散列表，
  // 因为「按名字查科室」只在装载数据时用几次，不值得多一个索引。
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
// 容量常量如果被调得太大，在这里编译期就会失败，而不是等到运行时栈溢出。
static_assert(sizeof(Hospital) < 4 * 1024 * 1024, "数据总库规模超出预期，请检查容量配置");

// ------------------------------- 数据总库的唯一实例 -------------------------------
// 总库有 361 KB，虽然没超过默认的 1MB 线程栈，但一个大对象放在栈上不是好习惯
// （将来加表就可能溢出），因此由 db.cpp 提供一个「函数内静态对象」，
// 各模块统一通过 db() 访问同一份数据。
// 这里只做声明，避免每个 .cpp 各建一份而出现数据不一致。
extern Hospital& db();
