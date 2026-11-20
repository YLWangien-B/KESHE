/* =============================================================================
 *  service_extra.h  —  题目【扩展要求】四条的业务算法
 *
 *  扩展要求原文（《第1题 医院门诊预约与智能导诊系统.docx》）：
 *      1. 增加候诊队列实时管理功能，患者到达医院后可签到进入候诊队列，
 *         实时查看排队进度；                      → 见 service_queue.h（早已实现）
 *      2. 支持医生临时停诊或换班时自动通知已预约患者；→ 本文件的 StopClinic
 *      3. 增加患者评价功能，患者就诊后可对医生进行评价，
 *         在科室预约推荐时参考评分；              → 本文件的 Rating
 *      4. 实现科室之间的转诊推荐，当某科室号源紧张时推荐相近科室。
 *                                                → 本文件的 Referral
 *
 *  和 service.h / service_queue.h 一样，这里**只算不打印**：
 *  函数把结果用返回值或传出参数交出去，怎么显示由界面层决定。
 *  这样患者端与管理端可以各自用不同的方式呈现同一份计算结果。
 * ============================================================================= */
#pragma once

#include <cstdio>
#include <cstring>

#include "model.h"
#include "shared_db.h"

// =============================================================================
//  扩展 3：患者评价
//
//  数据结构上不需要新表：Doctor 里本来就有 rankSum（评分累计）与 rankCount
//  （评分人数），平均分 = rankSum / rankCount 是 O(1) 算出来的。
//  只需要在 Booking 上加一个 rated 标记，保证同一张预约只能评一次。
// =============================================================================
namespace Rating {

constexpr int kMinStars = 1;
constexpr int kMaxStars = 5;

// 提交一次评价。bookingIndex 是 h.bookings 里的下标。
// 成功返回 true；预约不存在、状态不是「已完成」、已经评过，都返回 false。
//
// 为什么要限定「已完成」：只有真的看过病才有资格评价，否则患者可以
// 随便刷分。就诊完成由管理端的「就诊完成」按钮触发（见 Queue::finish）。
inline bool submit(Hospital& h, int bookingIndex, int stars) {
  if (bookingIndex < 0 || bookingIndex >= h.bookings.size()) return false;
  if (stars < kMinStars || stars > kMaxStars) return false;

  Booking& b = h.bookings[bookingIndex];
  if (b.status != FINISHED) return false;  // 没看完病不能评
  if (b.rated != 0) return false;          // 同一张预约只能评一次
  if (b.doctor < 0 || b.doctor >= h.doctors.size()) return false;

  Doctor& d = h.doctors[b.doctor];
  d.rankSum += stars;
  d.rankCount += 1;
  b.rated = 1;

  shared::logOp(shared::OP_RATE, b.patient, b.doctor, stars);
  shared::bump();
  return true;
}

// 平均分 ×10（整数）。无人评价返回 -1。
// 用整数是为了避免浮点比较：4.7 分存成 47，排序和显示都不会有精度问题。
inline int average10(const Doctor& d) {
  if (d.rankCount <= 0) return -1;
  return (d.rankSum * 10 + d.rankCount / 2) / d.rankCount;  // 四舍五入
}

// 把平均分写成 "4.7" 这种文本；无人评价写 "暂无评价"
inline void text(const Doctor& d, char* buf, int cap) {
  const int a = average10(d);
  if (a < 0)
    std::snprintf(buf, cap, "暂无评价");
  else
    std::snprintf(buf, cap, "%d.%d 分（%d 人）", a / 10, a % 10, d.rankCount);
}

}  // namespace Rating

// =============================================================================
//  扩展 3 的后半句：让评分**真正参与**医生推荐
//
//  题目原文：「患者就诊后可对医生进行评价，**在科室预约推荐时参考评分**」。
//  之前只做到"能评价、能显示"，排序完全没用它 —— 这一块补上。
//
//  关键的取舍（上一轮就说过）：**不能把评分混进科室得分**。
//  科室得分衡量"症状匹配度"（二部图边权和，1~5），评分衡量"医生好不好"（1~5），
//  两者数值范围一样但含义完全不同，直接相加会让"医生态度好"盖过"症状根本不匹配"，
//  推荐出无关科室。
//
//  正确做法是**分层**：
//      科室排序：仍然只看症状匹配（不变）
//      医生排序：评分 50% + 职称 30% + 余号 20%   ← 评分在这里起作用
//  这样两个维度各司其职，也符合真实挂号的逻辑：先定科室，再在科室内挑医生。
// =============================================================================
namespace DoctorRank {

// 职称的含金量（0..100）
inline int titleScore(const char* title) {
  if (title == nullptr) return 0;
  if (std::strcmp(title, "主任医师") == 0) return 100;
  if (std::strcmp(title, "副主任医师") == 0) return 75;
  if (std::strcmp(title, "主治医师") == 0) return 50;
  if (std::strcmp(title, "住院医师") == 0) return 25;
  return 0;
}

// rankQuality 内部用的分数（与上面同一套公式，只是不含号源项）
inline int rankQualityScore(const Hospital& h, int doctor) {
  if (doctor < 0 || doctor >= h.doctors.size()) return 0;
  const Doctor& d = h.doctors[doctor];
  const int avg10 = Rating::average10(d);
  const int rating = (avg10 < 0) ? 60 : (avg10 * 2);
  return (rating * 50 + titleScore(d.title) * 30) / 80;
}

// 综合推荐分 0..100。三项权重 50 / 30 / 20。
// 无人评价的医生评分项按 60 分算（中性，既不惩罚新医生，也不让他占便宜）。
inline int score(const Hospital& h, int doctor, int day, int slot) {
  if (doctor < 0 || doctor >= h.doctors.size()) return 0;
  const Doctor& d = h.doctors[doctor];

  const int avg10 = Rating::average10(d);          // 0..50（即 0.0~5.0 分）
  const int rating = (avg10 < 0) ? 60 : (avg10 * 2);  // 折算成 0..100

  const int title = titleScore(d.title);

  int freeScore = 0;
  if (h.hasSchedule(doctor, day, slot)) {
    const ScheduleSlot& s = h.doctors[doctor].schedule.slot(day, slot);
    if (!s.stopped && s.quota > 0) {
      const int left = s.quota - s.booked;
      freeScore = (left <= 0) ? 0 : (left * 100 / s.quota);
    }
  }

  return (rating * 50 + title * 30 + freeScore * 20) / 100;
}

// 把某科室在该时段的医生按综合推荐分降序写进 out（最多 cap 个），返回条数。
// **只列出该时段出诊且有余号的医生** —— 推荐一个约不上的医生等于没推荐。
// 只按「评分 + 职称」排序，不看号源 —— 用在还没选定时段的地方（比如导诊结果）。
// 权重仍按 50/30，归一化到这两项之和 80%，保证分数可比。
inline int rankQuality(const Hospital& h, int dept, int* out, int cap) {
  if (cap <= 0) return 0;
  int cnt = 0;
  for (int i = 0; i < h.doctors.size(); ++i) {
    if (h.doctors[i].dept != dept) continue;
    const Doctor& d = h.doctors[i];
    const int avg10 = Rating::average10(d);
    const int rating = (avg10 < 0) ? 60 : (avg10 * 2);
    const int sc = (rating * 50 + titleScore(d.title) * 30) / 80;  // 0..100

    int k = cnt;
    while (k > 0 && rankQualityScore(h, out[k - 1]) < sc) {
      out[k] = out[k - 1];
      --k;
    }
    out[k] = i;
    if (cnt < cap) ++cnt;
  }
  return cnt;
}

inline int rank(const Hospital& h, int dept, int day, int slot, int* out, int cap) {
  if (cap <= 0) return 0;
  int cnt = 0;
  for (int i = 0; i < h.doctors.size(); ++i) {
    if (h.doctors[i].dept != dept) continue;
    if (!h.hasSchedule(i, day, slot)) continue;
    const ScheduleSlot& s = h.doctors[i].schedule.slot(day, slot);
    if (s.stopped || s.quota - s.booked <= 0) continue;  // 停诊或已满
    const int sc = score(h, i, day, slot);

    // 插入排序：按推荐分从大到小，同分按下标（保证次序稳定）
    int k = cnt;
    while (k > 0 && score(h, out[k - 1], day, slot) < sc) {
      out[k] = out[k - 1];
      --k;
    }
    out[k] = i;
    if (cnt < cap) ++cnt;
  }
  return cnt;
}

}  // namespace DoctorRank

// =============================================================================
//  扩展 2 的后半句：换班
//
//  停诊是「这个时段没号了」，换班是「这个时段还是这些号，但换成另一位医生看」。
//  所以换班不能像停诊那样取消预约 —— 号源要**带着预约一起转移**：
//      · 原时段的 booked 清零，预约记录的 doctor 改成新医生
//      · 新时段的 booked 增加相应数量
//  患者那边什么都不用改，到院还是那个时间、那个诊室，只是医生换了。
// =============================================================================
namespace Shift {

enum Result {
  SHIFT_OK = 0,
  SHIFT_NO_SLOT = -1,     // 原时段没排班
  SHIFT_BAD_TARGET = -2,  // 目标时段没排班/停诊/不存在
  SHIFT_SAME = -3,        // 换给同一个人
  SHIFT_NO_ROOM = -4,     // 目标时段余号不够放下这些预约
};

inline const char* resultText(int r) {
  switch (r) {
    case SHIFT_OK: return "换班成功";
    case SHIFT_NO_SLOT: return "原时段没有排班";
    case SHIFT_BAD_TARGET: return "目标医生在该时段没有出诊（或已停诊）";
    case SHIFT_SAME: return "原医生与目标医生是同一个人";
    case SHIFT_NO_ROOM: return "目标时段余号不足以接收这些预约";
    default: return "未知错误";
  }
}

struct ShiftResult {
  int result = SHIFT_NO_SLOT;
  int moved = 0;  // 转移过去的预约条数
};

// 把 (fromDoctor, day, slot) 的坐诊医生换成 toDoctor。
inline ShiftResult transfer(Hospital& h, int fromDoctor, int day, int slot, int toDoctor) {
  ShiftResult r;
  if (!h.hasSchedule(fromDoctor, day, slot)) return r;
  if (fromDoctor == toDoctor) {
    r.result = SHIFT_SAME;
    return r;
  }
  if (!h.hasSchedule(toDoctor, day, slot)) {
    r.result = SHIFT_BAD_TARGET;
    return r;
  }

  shared::Lock lock;

  ScheduleSlot& from = h.slot(fromDoctor, day, slot);
  ScheduleSlot& to = h.slot(toDoctor, day, slot);
  if (to.stopped) {
    r.result = SHIFT_BAD_TARGET;
    return r;
  }

  // 数一数要带过去几条预约
  int count = 0;
  for (int i = 0; i < h.bookings.size(); ++i) {
    const Booking& b = h.bookings[i];
    if (b.doctor == fromDoctor && b.day == day && b.slot == slot && b.status == BOOKED) ++count;
  }
  if (to.quota - to.booked < count) {
    r.result = SHIFT_NO_ROOM;
    return r;
  }

  // 转移：预约记录的医生改掉
  for (int i = 0; i < h.bookings.size(); ++i) {
    Booking& b = h.bookings[i];
    if (b.doctor == fromDoctor && b.day == day && b.slot == slot && b.status == BOOKED) {
      b.doctor = toDoctor;
      shared::logOp(shared::OP_SHIFT, b.patient, toDoctor, 0);
    }
  }

  // 号源跟着走：原时段清空，新时段加上
  to.booked += count;
  from.booked = 0;
  to.version = ++h.versionClock;
  from.version = ++h.versionClock;

  shared::logOp(shared::OP_SHIFT, -1, toDoctor, count);
  shared::bump();

  r.result = SHIFT_OK;
  r.moved = count;
  return r;
}

}  // namespace Shift

// =============================================================================
//  扩展 2：停诊 / 换班通知
//
//  「通知」在本系统里的落地方式：把该时段已预约的记录状态改成 STOPPED_CANCEL，
//  患者端在「预约管理」里就能看到「因停诊取消，请改约」。
//  没有做真正的消息推送（那需要服务端与公网），这一点在文档里如实说明。
// =============================================================================
// =============================================================================
//  健壮性 / 实用：预约冲突检测
//
//  同一个人不可能同时出现在两个诊室。但系统里没有任何东西阻止患者
//  在「周一上午」约两个不同科室的医生 —— 到那天两份凭证都有效，两边都能
//  签到，人却只有一个。这是真实使用中一定会踩到的坑。
//
//  判据只看 day + slot：具体星期几、上午还是下午，同一时段只能有一张有效预约。
// =============================================================================
namespace BookingOps {

// 同一患者在同一时段是否已经有「已预约」的记录。
// 有则返回那条记录的凭证号（可直接拿去显示），没有返回 nullptr。
inline const char* sameSlotConflict(const Hospital& h, int patient, int day, int slot) {
  for (int i = 0; i < h.bookings.size(); ++i) {
    const Booking& b = h.bookings[i];
    if (b.patient != patient) continue;
    if (b.status != BOOKED) continue;  // 取消/完成/被改约的不算冲突
    if (b.day == day && b.slot == slot) return b.ticket;
  }
  return nullptr;
}

}  // namespace BookingOps

namespace StopClinic {

// 把某医生某时段设为停诊，并把该时段所有「已预约」的记录标记为「因停诊取消」。
// 返回受影响的预约条数；-1 表示该时段本来就没有排班。
//
// 整个「置停诊 + 遍历预约 + 改状态」在同一个临界区里完成，
// 否则患者端可能正好在中间插进来抢到一个本该停掉的号。
// 停诊的结果统计（比只返回一个数更有用：管理者要知道改约成功几个、几个没着落）
struct StopResult {
  int affected = 0;     // 受影响（原本已预约）的条数
  int rescheduled = 0;  // 成功自动改约的条数
  int failed = 0;       // 找不到合适时段、只能等患者自己重约的条数
  bool valid = true;    // false = 该时段本来就没排班
};

// 给某条受影响的预约找替代时段，按优先级依次试：
//   ① 同一位医生、同一天、另一个时段
//   ② 同一位医生、其它天、同一时段
//   ③ 同科室（该科室还有余号）、同一天、同一时段
// 找到就返回 true，并通过传出参数给出新的 (医生, 日期, 时段)。
//
// 为什么是这个顺序：尽量少改动患者已经记住的信息 ——
// 先保住医生和日期、换时段；再保住医生和时段、换日期；最后才换医生。
inline bool findAlternative(const Hospital& h, int doctor, int day, int slot, int* outDoctor,
                            int* outDay, int* outSlot) {
  // ① 同医生 同一天 另一个时段
  for (int s2 = 0; s2 < 2; ++s2) {
    if (s2 == slot) continue;
    if (h.remaining(doctor, day, s2) > 0) {
      *outDoctor = doctor;
      *outDay = day;
      *outSlot = s2;
      return true;
    }
  }
  // ② 同医生 其它天 同一时段
  for (int d2 = 0; d2 < kMaxDay; ++d2) {
    if (d2 == day) continue;
    if (h.remaining(doctor, d2, slot) > 0) {
      *outDoctor = doctor;
      *outDay = d2;
      *outSlot = slot;
      return true;
    }
  }
  // ③ 同科室 其它医生 同一天 同一时段
  const int dept = h.doctors[doctor].dept;
  for (int doc2 = 0; doc2 < h.doctors.size(); ++doc2) {
    if (doc2 == doctor) continue;
    if (h.doctors[doc2].dept != dept) continue;
    if (h.remaining(doc2, day, slot) > 0) {
      *outDoctor = doc2;
      *outDay = day;
      *outSlot = slot;
      return true;
    }
  }
  return false;
}

// 停诊 + 自动改约（扩展要求第 2 条的完整闭环）
//
// 只"通知患者已停诊"是不够的 —— 患者还得自己从头重约一遍。
// 这里在停诊的同时就把受影响的人改到替代时段，生成新凭证，
// 原记录标记为「已改约」留痕。这样管理者一条操作就把事情办完了。
inline StopResult applyAndReschedule(Hospital& h, int doctor, int day, int slot) {
  StopResult r;
  if (!h.hasSchedule(doctor, day, slot)) {
    r.valid = false;
    return r;
  }

  shared::Lock lock;
  ScheduleSlot& s = h.slot(doctor, day, slot);
  if (s.stopped) return r;  // 已经停诊了

  s.stopped = true;
  s.version = ++h.versionClock;

  // 注意：循环里会往 h.bookings 里追加新记录，所以先把原条数记下来，
  // 否则新记录也会被当成"受影响的预约"再处理一遍。
  const int originalCount = h.bookings.size();

  for (int i = 0; i < originalCount; ++i) {
    Booking& b = h.bookings[i];
    if (b.doctor != doctor || b.day != day || b.slot != slot) continue;
    if (b.status != BOOKED) continue;

    ++r.affected;
    if (s.booked > 0) --s.booked;

    int nd = -1, nday = -1, nslot = -1;
    if (!findAlternative(h, doctor, day, slot, &nd, &nday, &nslot)) {
      // 没有可改约的时段：只能标记为「因停诊取消」，让患者自己重约
      b.status = STOPPED_CANCEL;
      ++r.failed;
      shared::logOp(shared::OP_STOP, b.patient, doctor, i);
      continue;
    }

    // 原记录留痕
    b.status = RESCHEDULED;
    const int patient = b.patient;

    // 新记录：新凭证 + 新时段
    Booking nb;
    std::snprintf(nb.ticket, sizeof(nb.ticket), "TK%04d", ++h.ticketSeq);
    nb.patient = patient;
    nb.doctor = nd;
    nb.day = nday;
    nb.slot = nslot;
    nb.status = BOOKED;
    nb.rated = 0;
    if (h.bookings.push(nb) >= 0) {
      // 新时段占号
      ScheduleSlot& ns = h.slot(nd, nday, nslot);
      ++ns.booked;
      ns.version = ++h.versionClock;
      ++r.rescheduled;
      shared::logOp(shared::OP_RESCHEDULE, patient, nd, nday);
    } else {
      // 预约表满了：改约失败，退回「因停诊取消」
      b.status = STOPPED_CANCEL;
      ++r.failed;
    }
  }

  shared::logOp(shared::OP_STOP, -1, doctor, r.affected);
  shared::bump();
  return r;
}

// 兼容旧接口：只要数量、不要细节时用（返回受影响的条数）
inline int apply(Hospital& h, int doctor, int day, int slot) {
  const StopResult r = applyAndReschedule(h, doctor, day, slot);
  return r.valid ? r.affected : -1;
}

// 恢复出诊。注意**不会**自动恢复那些因停诊被取消的预约 ——
// 号已经被别人约走了，强行恢复会超卖，只能让患者重新约。
inline bool resume(Hospital& h, int doctor, int day, int slot) {
  if (!h.hasSchedule(doctor, day, slot)) return false;

  shared::Lock lock;
  ScheduleSlot& s = h.slot(doctor, day, slot);
  if (!s.stopped) return false;

  s.stopped = false;
  s.version = ++h.versionClock;
  shared::logOp(shared::OP_RESUME, -1, doctor, 0);
  shared::bump();
  return true;
}

}  // namespace StopClinic

// =============================================================================
//  扩展 4：转诊推荐
//
//  「相近科室」的定义：两个科室在加权二部图上关联的症状重叠得多相似度就高。
//  用 Jaccard 系数：  相似度 = |A∩B| / |A∪B|
//
//  实测数据能验证这个定义是对的：
//      呼吸内科 ↔ 儿科       共同症状 咳嗽、气短   → 2/4 = 50%（最相近）
//      心血管内科 ↔ 呼吸内科  共同症状 气短        → 1/5 = 20%
//  呼吸内科和儿科确实最像（都是呼吸道疾病），符合医学常识。
//
//  为什么用 Jaccard 而不是「共同症状数」：不除以并集的话，
//  症状特别多的科室会跟谁都显得相近，这是错误的。
// =============================================================================
namespace Referral {

// 两个科室的症状相似度，返回 0..100 的百分数
inline int similarity(const Hospital& h, int a, int b) {
  if (a == b) return 100;
  const BipartiteGraph& g = h.symptomGraph;
  if (a < 0 || a >= g.departmentCount()) return 0;
  if (b < 0 || b >= g.departmentCount()) return 0;

  int intersection = 0;
  int unionSize = 0;

  // a 关联的症状：并集先全算进去，同时数交集
  for (int e = g.firstSymptomOf(a); e != -1; e = g.nextSymptomEdge(e)) {
    ++unionSize;
    if (g.weightOf(g.edgeSymptom(e), b) > 0) ++intersection;
  }
  // b 独有的症状（a 没有的）只增加并集
  for (int e = g.firstSymptomOf(b); e != -1; e = g.nextSymptomEdge(e)) {
    if (g.weightOf(g.edgeSymptom(e), a) == 0) ++unionSize;
  }

  if (unionSize == 0) return 0;
  return intersection * 100 / unionSize;
}

// 某科室在某时段的总余号（该科室所有出诊医生相加）
inline int freeSlots(const Hospital& h, int dept, int day, int slot) {
  int total = 0;
  for (int i = 0; i < h.doctors.size(); ++i) {
    if (h.doctors[i].dept != dept) continue;
    if (!h.hasSchedule(i, day, slot)) continue;
    total += h.remaining(i, day, slot);
  }
  return total;
}

// 某科室在某时段的总号源（分母，用来算紧张度）
inline int totalQuota(const Hospital& h, int dept, int day, int slot) {
  int total = 0;
  for (int i = 0; i < h.doctors.size(); ++i) {
    if (h.doctors[i].dept != dept) continue;
    if (!h.hasSchedule(i, day, slot)) continue;
    total += h.doctors[i].schedule.slot(day, slot).quota;
  }
  return total;
}

// 紧张度：0 = 充足，1 = 紧张（余号 < 20%），2 = 已满或全部停诊
inline int tightness(const Hospital& h, int dept, int day, int slot) {
  const int freeCount = freeSlots(h, dept, day, slot);
  if (freeCount <= 0) return 2;
  const int quota = totalQuota(h, dept, day, slot);
  if (quota <= 0) return 2;
  if (freeCount * 5 < quota) return 1;  // 余号不到两成
  return 0;
}

// 给 dept 找最相近的其它科室，按相似度降序写进 out（最多 cap 个）。
// **只返回在该时段还有号的科室** —— 号也满了的科室推荐了没用。
// 返回条数。
inline int recommend(const Hospital& h, int dept, int day, int slot, int* out, int cap) {
  const int n = h.depts.size();
  if (n <= 0 || cap <= 0) return 0;

  // 科室数最多 16 个，用两个定长数组做**插入排序**即可 —— 不申请内存，
  // 而且科室很少，O(n²) 完全够。相似度与科室下标同步移动，保证一一对应。
  int sim[16];   // 相似度
  int ids[16];   // 对应的科室下标
  int cnt = 0;

  for (int d = 0; d < n && d < 16; ++d) {
    if (d == dept) continue;
    if (freeSlots(h, d, day, slot) <= 0) continue;  // 没号的不推荐
    const int s = similarity(h, dept, d);

    // 从后往前找插入位置（按相似度从大到小）
    int k = cnt;
    while (k > 0 && sim[k - 1] < s) {
      sim[k] = sim[k - 1];
      ids[k] = ids[k - 1];
      --k;
    }
    sim[k] = s;
    ids[k] = d;
    if (cnt < 16) ++cnt;
  }

  const int m = (cnt < cap) ? cnt : cap;
  for (int i = 0; i < m; ++i) out[i] = ids[i];
  return m;
}

}  // namespace Referral

// =============================================================================
//  健壮性：数据一致性自检（自选功能）
//
//  为什么需要它：两个程序通过共享内存同时改数据，一旦某次写入撞上崩溃、
//  或者哪段代码忘了同步维护 slot.booked，数据就会"看着能跑，数字对不上"。
//  更糟的是这种不一致不会报错 —— 它只是让余号慢慢算错，直到某天
//  号被约光了却还有人能约上（超卖），或者明明有号却约不上。
//
//  这个自检把「应该成立的等式」逐条核对一遍：
//      slot.booked == 该时段状态为「已预约」的记录条数
//  另外检查所有下标引用是否有效（指向不存在的医生/患者就是脏数据）。
//
//  它只读不写，可以在任何时候安全调用。
// =============================================================================
namespace Health {

struct Report {
  int totalBookings = 0;   // 预约表总条数
  int activeBookings = 0;  // 其中状态为「已预约」的
  int slotUnder = 0;       // booked **小于**预约表条数的时段数 —— 危险，可能超卖
  int slotOver = 0;        // booked 大于预约表条数 —— 多半是数据文件自带的初始已约数
  int badBooking = 0;      // 指向不存在的医生/患者/时段的预约条数
  int badWaiting = 0;      // 候诊队列里指向不存在的患者/医生的条数
  int negativeBooked = 0;  // booked 出现负数的时段数（不该发生）
  int nearCapacity = 0;    // 已用超过 90% 容量的表数

  // 只有「会算错号」的问题才算不一致。
  // slotOver 不计入：数据文件本来就带初始已约数（医院已有的预约记录不在本系统里），
  // 那种情况 booked 大于预约表条数是正常的。
  bool ok() const {
    return slotUnder == 0 && badBooking == 0 && badWaiting == 0 && negativeBooked == 0;
  }
};

inline Report check(const Hospital& h) {
  Report r;
  r.totalBookings = h.bookings.size();

  // 实际「已预约」的条数，按 (医生, 日期, 时段) 分开计数。
  // 上限是 kMaxDoctor × kMaxDay × 2 个时段 = 64 × 7 × 2 = 896 个 int，约 3.5 KB，
  // 放在栈上没问题（数据总库本身才不在栈上）。
  int expect[kMaxDoctor][kMaxDay][2];
  for (int d = 0; d < kMaxDoctor; ++d)
    for (int day = 0; day < kMaxDay; ++day)
      for (int s = 0; s < 2; ++s) expect[d][day][s] = 0;

  for (int i = 0; i < h.bookings.size(); ++i) {
    const Booking& b = h.bookings[i];
    if (b.patient < 0 || b.patient >= h.patients.size()) {
      ++r.badBooking;
      continue;
    }
    if (b.doctor < 0 || b.doctor >= h.doctors.size()) {
      ++r.badBooking;
      continue;
    }
    if (b.day < 0 || b.day >= kMaxDay || b.slot < 0 || b.slot >= 2) {
      ++r.badBooking;
      continue;
    }
    if (b.status == BOOKED) {
      ++r.activeBookings;
      ++expect[b.doctor][b.day][b.slot];
    }
  }

  // 逐槽核对
  for (int d = 0; d < h.doctors.size(); ++d) {
    for (int day = 0; day < kMaxDay; ++day) {
      for (int s = 0; s < 2; ++s) {
        // 未排班的时段不参与核对 —— 那些格子的数据本来就没有意义，
        // 拿它们比 booked 会误报「负余号」。
        if (!h.hasSchedule(d, day, s)) continue;
        const ScheduleSlot& slot = h.doctors[d].schedule.slot(day, s);
        if (slot.booked < 0) {
          ++r.negativeBooked;
          continue;
        }
        if (slot.booked < expect[d][day][s])
          ++r.slotUnder;  // 系统以为还有号，其实已经被约走了 —— 会超卖
        else if (slot.booked > expect[d][day][s])
          ++r.slotOver;   // 数据文件自带的初始已约数，属正常
      }
    }
  }

  // 候诊队列的下标有效性
  for (int i = 0; i < h.waiting.size(); ++i) {
    const Waiting& w = h.waiting.at(i);
    if (w.patient < 0 || w.patient >= h.patients.size()) ++r.badWaiting;
    else if (w.doctor < 0 || w.doctor >= h.doctors.size()) ++r.badWaiting;
  }

  // 容量接近上限的表（装满了再往里 push 会静默丢记录，值得提前提醒）
  struct { int used; int cap; } tables[] = {
      {h.depts.size(), kMaxDept},       {h.doctors.size(), kMaxDoctor},
      {h.patients.size(), kMaxPatient}, {h.bookings.size(), kMaxBook},
      {h.rooms.size(), kMaxRoom},
      {h.symptomGraph.symptomCount(), BipartiteGraph::kMaxSymptoms},
      {h.waiting.size(), kMaxWaiting},
  };
  for (int i = 0; i < static_cast<int>(sizeof(tables) / sizeof(tables[0])); ++i) {
    if (tables[i].cap > 0 && tables[i].used * 10 >= tables[i].cap * 9) ++r.nearCapacity;
  }

  return r;
}

// 按预约表把每个时段的 booked 重新算一遍并写回。返回修正的槽数。
//
// 什么时候用：自检报告「余额对不上」的时候。把 booked 当成派生值重算，
// 是唯一能自动恢复的办法 —— 因为预约表是**事实**，booked 只是它的计数缓存。
// 修正后递增版本号，让此前读到余号的界面信息作废。
//
// 注意：这会把 booked 覆盖成「按预约表算出来的值」，所以只在确认不一致时用。
inline int repair(Hospital& h) {
  shared::Lock lock;

  int expect[kMaxDoctor][kMaxDay][2];
  for (int d = 0; d < kMaxDoctor; ++d)
    for (int day = 0; day < kMaxDay; ++day)
      for (int s = 0; s < 2; ++s) expect[d][day][s] = 0;

  for (int i = 0; i < h.bookings.size(); ++i) {
    const Booking& b = h.bookings[i];
    if (b.status != BOOKED) continue;
    if (b.doctor < 0 || b.doctor >= h.doctors.size()) continue;
    if (b.day < 0 || b.day >= kMaxDay || b.slot < 0 || b.slot >= 2) continue;
    ++expect[b.doctor][b.day][b.slot];
  }

  // 只修「少算」的方向（booked < 预约表）。
  // booked 偏大时不能往下抹 —— 那可能是数据文件里自带的初始已约数，
  // 抹掉就等于凭空多放号出来。
  int fixed = 0;
  for (int d = 0; d < h.doctors.size(); ++d) {
    for (int day = 0; day < kMaxDay; ++day) {
      for (int s = 0; s < 2; ++s) {
        ScheduleSlot& slot = h.doctors[d].schedule.slot(day, s);
        if (slot.booked < expect[d][day][s]) {
          slot.booked = expect[d][day][s];
          slot.version = ++h.versionClock;
          ++fixed;
        }
      }
    }
  }
  if (fixed > 0) {
    shared::logOp(shared::OP_REPAIR, -1, -1, fixed);
    shared::bump();
  }
  return fixed;
}

}  // namespace Health

// =============================================================================
//  新增功能：登记患者 / 医生
//
//  原来数据只能从 data\*.txt 装载，运行期没法加人 —— 医院来了新患者、
//  新医生入职，只能改文本文件再重启。这里补上运行期登记。
//
//  两条路都要走，缺一不可：
//    ① 写进内存里的表（并把编号索引补上，否则 findPatient/findDoctor 查不到）
//    ② 追加一行到 data\patients.txt / data\doctors.txt
//       否则删掉 state.bin 重新装载时，新登记的人就没了。
//
//  整个过程在跨进程锁里完成：两端同时登记不能撞号。
// =============================================================================
namespace Registry {

enum Result {
  REG_OK = 0,
  REG_DUP_CODE = -1,   // 编号已存在
  REG_FULL = -2,       // 表已满
  REG_EMPTY = -3,      // 必填项为空
  REG_BAD_DEPT = -4,   // 科室不存在
  REG_BAD_ROOM = -5,   // 房间不存在
  REG_BAD_TITLE = -6,  // 职称不在允许范围内
};

// 把结果码翻译成一句能直接显示的话
inline const char* resultText(int r) {
  switch (r) {
    case REG_OK: return "成功";
    case REG_DUP_CODE: return "编号已存在";
    case REG_FULL: return "记录数已达容量上限，请先在 model.h 里调大容量后重新编译";
    case REG_EMPTY: return "编号、姓名等必填项不能为空";
    case REG_BAD_DEPT: return "科室不存在";
    case REG_BAD_ROOM: return "房间不存在";
    case REG_BAD_TITLE: return "职称必须是：主任医师 / 副主任医师 / 主治医师 / 住院医师";
    default: return "未知错误";
  }
}

// 追加一行到数据文件。dataDir 形如 "data"。
// 返回 true 表示写成功 —— 写失败只影响「下次重新装载」，
// 当前运行期仍然可用，所以调用方给个提示即可，不必回滚。
inline bool appendLineToFile(const char* dataDir, const char* fileName, const char* line) {
  char path[512];
  std::snprintf(path, sizeof(path), "%s/%s", dataDir, fileName);
  std::FILE* f = std::fopen(path, "ab");  // 追加，二进制避免多写字
  if (f == nullptr) return false;
  std::fputs(line, f);
  std::fputc('\n', f);
  std::fclose(f);
  return true;
}

// 登记患者。phone 允许为空（医院不强制留电话）。
inline int addPatient(Hospital& h, const char* dataDir, const char* code, const char* name,
                      const char* phone) {
  if (code == nullptr || code[0] == '\0' || name == nullptr || name[0] == '\0')
    return REG_EMPTY;
  if (std::strlen(code) >= sizeof(Patient::code)) return REG_EMPTY;
  if (std::strlen(name) >= sizeof(Patient::name)) return REG_EMPTY;

  shared::Lock lock;

  if (h.findPatient(code) >= 0) return REG_DUP_CODE;
  if (h.patients.full()) return REG_FULL;

  Patient p{};
  std::snprintf(p.code, sizeof(p.code), "%s", code);
  std::snprintf(p.name, sizeof(p.name), "%s", name);
  std::snprintf(p.phone, sizeof(p.phone), "%s", phone != nullptr ? phone : "");

  if (h.patients.push(p) < 0) return REG_FULL;
  // 编号 -> 下标的索引必须一起维护，否则装完数据就查不到了
  h.patientIndex.put(h.patients[h.patients.size() - 1].code, h.patients.size() - 1);

  char line[128];
  std::snprintf(line, sizeof(line), "%s|%s|%s", p.code, p.name, p.phone);
  appendLineToFile(dataDir, "patients.txt", line);

  shared::logOp(shared::OP_ADD_PATIENT, h.patients.size() - 1, -1, 0);
  shared::bump();
  return REG_OK;
}

// 登记医生。deptName 传科室名（界面里选的是名字，比编号好认）。
// roomCode 传房间编号；传空表示暂不安排诊室。
inline int addDoctor(Hospital& h, const char* dataDir, const char* code, const char* name,
                     const char* deptName, const char* title, const char* skill,
                     const char* roomCode) {
  if (code == nullptr || code[0] == '\0' || name == nullptr || name[0] == '\0')
    return REG_EMPTY;
  if (std::strlen(code) >= sizeof(Doctor::code)) return REG_EMPTY;
  if (std::strlen(name) >= sizeof(Doctor::name)) return REG_EMPTY;
  if (skill != nullptr && std::strlen(skill) >= sizeof(Doctor::skill)) return REG_EMPTY;

  shared::Lock lock;

  if (h.findDoctor(code) >= 0) return REG_DUP_CODE;
  if (h.doctors.full()) return REG_FULL;

  const int dept = h.findDeptByName(deptName != nullptr ? deptName : "");
  if (dept < 0) return REG_BAD_DEPT;

  // 职称限定在这四种，界面上是下拉框，但业务层也要自己校验一遍
  static const char* kTitles[] = {"主任医师", "副主任医师", "主治医师", "住院医师"};
  bool titleOk = false;
  for (int i = 0; i < 4; ++i)
    if (title != nullptr && std::strcmp(title, kTitles[i]) == 0) titleOk = true;
  if (!titleOk) return REG_BAD_TITLE;

  int room = -1;
  if (roomCode != nullptr && roomCode[0] != '\0') {
    room = h.findRoom(roomCode);
    if (room < 0) return REG_BAD_ROOM;
  }

  Doctor d{};
  std::snprintf(d.code, sizeof(d.code), "%s", code);
  std::snprintf(d.name, sizeof(d.name), "%s", name);
  d.dept = dept;
  std::snprintf(d.title, sizeof(d.title), "%s", title);
  std::snprintf(d.skill, sizeof(d.skill), "%s", skill != nullptr ? skill : "");
  d.room = room;
  d.rankSum = 0;
  d.rankCount = 0;
  d.schedule.clear();  // 新医生默认不排班，由排班页/排班数据决定

  if (h.doctors.push(d) < 0) return REG_FULL;
  h.doctorIndex.put(h.doctors[h.doctors.size() - 1].code, h.doctors.size() - 1);

  char line[256];
  std::snprintf(line, sizeof(line), "%s|%s|%s|%s|%s|%s", d.code, d.name,
                h.depts[dept].code, d.title, d.skill,
                room >= 0 ? h.rooms[room].code : "");
  appendLineToFile(dataDir, "doctors.txt", line);

  shared::logOp(shared::OP_ADD_DOCTOR, -1, h.doctors.size() - 1, 0);
  shared::bump();
  return REG_OK;
}

}  // namespace Registry
