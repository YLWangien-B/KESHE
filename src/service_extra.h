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
//  扩展 2：停诊 / 换班通知
//
//  「通知」在本系统里的落地方式：把该时段已预约的记录状态改成 STOPPED_CANCEL，
//  患者端在「预约管理」里就能看到「因停诊取消，请改约」。
//  没有做真正的消息推送（那需要服务端与公网），这一点在文档里如实说明。
// =============================================================================
namespace StopClinic {

// 把某医生某时段设为停诊，并把该时段所有「已预约」的记录标记为「因停诊取消」。
// 返回受影响的预约条数；-1 表示该时段本来就没有排班。
//
// 整个「置停诊 + 遍历预约 + 改状态」在同一个临界区里完成，
// 否则患者端可能正好在中间插进来抢到一个本该停掉的号。
inline int apply(Hospital& h, int doctor, int day, int slot) {
  if (!h.hasSchedule(doctor, day, slot)) return -1;

  shared::Lock lock;
  ScheduleSlot& s = h.slot(doctor, day, slot);
  if (s.stopped) return 0;  // 已经停诊了

  s.stopped = true;
  s.version = ++h.versionClock;

  int affected = 0;
  for (int i = 0; i < h.bookings.size(); ++i) {
    Booking& b = h.bookings[i];
    if (b.doctor != doctor || b.day != day || b.slot != slot) continue;
    if (b.status != BOOKED) continue;

    b.status = STOPPED_CANCEL;  // 通知已送达：这条预约因停诊而取消
    if (s.booked > 0) --s.booked;
    ++affected;
    shared::logOp(shared::OP_STOP, b.patient, doctor, i);
  }

  shared::logOp(shared::OP_STOP, -1, doctor, affected);
  shared::bump();
  return affected;
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
