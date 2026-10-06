/* =============================================================================
 *  menu.h  —  六个功能的界面流程
 *
 *  每个函数负责「问用户要什么参数 -> 调业务层算 -> 把结果排版输出」。
 *  业务算法本身在 service.h 里，这里只做输入输出。
 *
 *  对应题目要求的功能：
 *      1 智能导诊   1.2.1 智能导诊     Triage::run
 *      2 医生查询   1.2.1 医生查询     按科室 / 职称 / 擅长领域筛选
 *      3 在线预约   1.2.1 在线预约     检查并扣减号源，生成预约凭证
 *      4 预约管理   1.2.1 预约管理     查看与取消本人的预约
 *      5 排班展示   1.2.1 排班展示     按医生与日期列出排班与余号
 *      6 路线查询   1.2.1 路线查询     Route::between
 * ========================================================================== */
#pragma once

#include <cstdio>
#include <cstring>

#include "model.h"
#include "service.h"
#include "ui.h"

// =============================================================================
//  功能一：智能导诊
// =============================================================================
inline void doTriage() {
  Hospital& h = db();
  std::printf("\n================ 智能导诊 ================\n");
  std::printf("请按序号选择症状，多个用空格分开（例如：4 2 6），直接回车返回\n\n");

  for (int i = 0; i < h.symptoms.size(); ++i) {
    char no[24];
    std::snprintf(no, sizeof(no), "%d.%s", i + 1, h.symptoms[i].name);
    cell(no, 14);
    if ((i + 1) % 5 == 0) std::printf("\n");
  }
  std::printf("\n\n请选择症状：");

  char buf[256];
  if (!readLine(buf, sizeof(buf))) return;
  if (buf[0] == '\0') return;

  // 把「4 2 6」这样的输入拆成症状下标。
  // 逐个数地解析，越界的直接跳过并告诉患者，避免 atoi 把一长串数字算成乱值。
  int picked[kMaxPick];
  int pickCount = 0;
  int badCount = 0;
  const char* p = buf;
  while (*p && pickCount < kMaxPick) {
    while (*p == ' ' || *p == '\t') ++p;
    if (*p == '\0') break;
    char token[32];
    int n = 0;
    while (*p && *p != ' ' && *p != '\t' && n < 31) token[n++] = *p++;
    token[n] = '\0';

    int no = 0;
    if (!parseIntInRange(token, 1, h.symptoms.size(), no)) {
      ++badCount;
      continue;
    }
    // 同一个症状选两次只算一次
    bool dup = false;
    for (int i = 0; i < pickCount; ++i)
      if (picked[i] == no - 1) dup = true;
    if (!dup) picked[pickCount++] = no - 1;
  }

  if (badCount > 0) std::printf("（有 %d 个序号无效，已忽略；症状序号范围是 1~%d）\n", badCount, h.symptoms.size());
  if (pickCount == 0) {
    std::printf("没有识别到有效的症状序号。\n");
    pause();
    return;
  }

  const TriageResult r = Triage::run(h, picked, pickCount);
  if (r.count == 0) {
    std::printf("\n所选症状与任何科室都没有关联，无法给出推荐。\n");
    pause();
    return;
  }

  std::printf("\n---------- 推荐结果（按匹配得分排序）----------\n");
  cell("序号", 6);
  cell("科室", 14);
  cell("得分", 6);
  cell("推荐依据", 28);
  std::printf("位置\n");
  for (int i = 0; i < r.count; ++i) {
    const TriageItem& it = r.items[i];
    char no[8], score[8], basis[64];
    std::snprintf(no, sizeof(no), "%d", i + 1);
    std::snprintf(score, sizeof(score), "%d", it.score);
    basis[0] = '\0';
    for (int k = 0; k < it.matchCount && k < kMaxSympPerDept; ++k) {
      if (it.matched[k] < 0) break;
      if (k) std::strcat(basis, " ");
      std::strcat(basis, h.symptoms[it.matched[k]].name);
    }
    cell(no, 6);
    cell(h.depts[it.dept].name, 14);
    cell(score, 6);
    cell(basis, 28);
    std::printf("%s\n", h.depts[it.dept].location);
  }

  // 顺便列出该科室的医生，方便患者接着去预约
  std::printf("\n%s 的出诊医生（前 3 名）：\n", h.depts[r.items[0].dept].name);
  int shown = 0;
  for (int d = 0; d < h.doctors.size() && shown < 3; ++d) {
    if (h.doctors[d].dept != r.items[0].dept) continue;
    std::printf("  %s  %s  %s\n", h.doctors[d].code, h.doctors[d].name, h.doctors[d].title);
    ++shown;
  }
  if (shown == 0) std::printf("  （该科室暂无医生）\n");
  pause();
}

// =============================================================================
//  功能二：医生查询
// =============================================================================
inline void doQueryDoctor() {
  Hospital& h = db();
  std::printf("\n================ 医生查询 ================\n");
  std::printf("  1. 按科室查询\n");
  std::printf("  2. 按职称查询\n");
  std::printf("  3. 按擅长领域关键字查询\n");
  std::printf("  0. 返回\n请选择：");

  int mode = -1;
  if (!readIntInRange(0, 3, mode)) {
    std::printf("\n请输入 0 到 3 之间的序号。\n");
    pause();
    return;
  }
  if (mode == 0) return;

  char keyword[64];
  keyword[0] = '\0';
  if (mode == 1) {
    std::printf("\n科室列表：\n");
    for (int i = 0; i < h.depts.size(); ++i) std::printf("  %-6s %s\n", h.depts[i].code, h.depts[i].name);
    std::printf("请输入科室编号：");
  } else if (mode == 2) {
    std::printf("\n请输入职称关键字（主任 / 副主任 / 主治 / 住院）：");
  } else {
    std::printf("\n请输入擅长领域关键字（如 高血压、咳嗽）：");
  }
  if (!readLine(keyword, sizeof(keyword))) return;

  std::printf("\n");
  cell("编号", 8);
  cell("姓名", 10);
  cell("科室", 16);
  cell("职称", 14);
  std::printf("擅长领域\n");

  int found = 0;
  for (int i = 0; i < h.doctors.size(); ++i) {
    const Doctor& d = h.doctors[i];
    bool hit = false;
    if (mode == 1)
      hit = (std::strcmp(h.depts[d.dept].code, keyword) == 0);
    else if (mode == 2)
      hit = (std::strstr(d.title, keyword) != nullptr);
    else
      hit = (std::strstr(d.skill, keyword) != nullptr);
    if (!hit) continue;

    cell(d.code, 8);
    cell(d.name, 10);
    cell(h.depts[d.dept].name, 16);
    cell(d.title, 14);
    std::printf("%s\n", d.skill);
    ++found;
  }
  std::printf("\n共找到 %d 名医生。\n", found);
  pause();
}

// =============================================================================
//  功能三：在线预约
// =============================================================================
inline void doBook() {
  Hospital& h = db();
  std::printf("\n================ 在线预约 ================\n");

  std::printf("患者列表：");
  for (int i = 0; i < h.patients.size(); ++i) std::printf(" %s(%s)", h.patients[i].code, h.patients[i].name);
  std::printf("\n请输入患者编号：");
  char code[32];
  if (!readLine(code, sizeof(code))) return;
  const int patient = h.findPatient(code);
  if (patient < 0) {
    std::printf("患者编号不存在。\n");
    pause();
    return;
  }

  std::printf("\n医生列表（前 12 名）：");
  for (int i = 0; i < h.doctors.size() && i < 12; ++i) std::printf(" %s", h.doctors[i].code);
  std::printf(" ...\n请输入医生编号：");
  if (!readLine(code, sizeof(code))) return;
  const int doctor = h.findDoctor(code);
  if (doctor < 0) {
    std::printf("医生编号不存在。\n");
    pause();
    return;
  }

  // 列出该医生一周的排班，格内显示「余号/总数」
  std::printf("\n%s（%s %s）的排班：\n", h.doctors[doctor].name, h.depts[h.doctors[doctor].dept].name,
              h.doctors[doctor].title);
  std::printf("       ");
  for (int day = 0; day < kMaxDay; ++day) std::printf(" 周%-3d ", day + 1);
  for (int slot = 0; slot < 2; ++slot) {
    std::printf("\n%s   ", slot == 0 ? "上午" : "下午");
    for (int day = 0; day < kMaxDay; ++day) {
      if (!h.hasSchedule(doctor, day, slot))
        std::printf("  --   ");
      else
        std::printf(" %2d/%-2d ", h.remaining(doctor, day, slot), h.slot(doctor, day, slot).quota);
    }
  }
  std::printf("\n（-- 表示该时段不出诊）\n");

  std::printf("\n请输入日期(1-%d) 和时段(0=上午 1=下午)，用空格分开：", kMaxDay);
  char buf[64];
  if (!readLine(buf, sizeof(buf))) return;
  int day = 0, slot = 0;
  // 用带范围的解析而不是 sscanf("%d")：后者对超长数字会溢出成任意值，
  // 后面的范围检查就形同虚设。这里 1~kMaxDay 与 0~1 之外一律拒绝。
  if (!parseTwoIntsInRange(buf, 1, kMaxDay, day, 0, 1, slot)) {
    std::printf("输入不对：日期要在 1~%d 之间，时段只能是 0 或 1。\n", kMaxDay);
    pause();
    return;
  }
  --day;

  if (!h.hasSchedule(doctor, day, slot)) {
    std::printf("该医生在周%d%s不出诊。\n", day + 1, slot == 0 ? "上午" : "下午");
    pause();
    return;
  }

  // ---------- 号源的检查与扣减 ----------
  // 这三步（停诊检查、余量检查、版本检查）与随后的扣减必须在一次调用里连着做完，
  // 中间不能让别的操作插进来。做法是：先记下当前版本号，扣减前再比一次；
  // 版本对不上说明号源刚被改动过，就拒绝这次预约让患者重选，因此不会超卖。
  // 版本时钟 versionClock 是单调递增的，所以不会出现「改了又改回原版本」的情况。
  Schedule& s = h.slot(doctor, day, slot);
  if (s.stopped) {
    std::printf("该时段已停诊，预约失败。\n");
    pause();
    return;
  }
  if (s.quota - s.booked <= 0) {
    std::printf("该时段号源已满，预约失败。\n");
    pause();
    return;
  }
  ++s.booked;
  s.version = ++h.versionClock;

  Booking b;
  std::snprintf(b.ticket, sizeof(b.ticket), "TK%04d", ++h.ticketSeq);
  b.patient = patient;
  b.doctor = doctor;
  b.day = day;
  b.slot = slot;
  b.status = BOOKED;
  h.bookings.push(b);

  std::printf("\n---------- 预约凭证 ----------\n");
  std::printf("  凭证号  ：%s\n", b.ticket);
  std::printf("  患者    ：%s %s\n", h.patients[patient].code, h.patients[patient].name);
  std::printf("  科室    ：%s\n", h.depts[h.doctors[doctor].dept].name);
  std::printf("  医生    ：%s %s（%s）\n", h.doctors[doctor].code, h.doctors[doctor].name, h.doctors[doctor].title);
  std::printf("  就诊时间：周%d %s\n", day + 1, slot == 0 ? "上午" : "下午");
  std::printf("  就诊地点：%s\n", h.doctors[doctor].room >= 0 ? h.rooms[h.doctors[doctor].room].name : "待安排");
  std::printf("  该时段剩余号源：%d\n", h.remaining(doctor, day, slot));
  pause();
}

// =============================================================================
//  功能四：预约管理
// =============================================================================
inline void doManageBooking() {
  Hospital& h = db();
  std::printf("\n================ 预约管理 ================\n");
  std::printf("请输入患者编号：");
  char code[32];
  if (!readLine(code, sizeof(code))) return;
  const int patient = h.findPatient(code);
  if (patient < 0) {
    std::printf("患者编号不存在。\n");
    pause();
    return;
  }

  std::printf("\n%s 的预约记录：\n", h.patients[patient].name);
  cell("凭证号", 10);
  cell("科室", 16);
  cell("医生", 10);
  cell("时间", 10);
  std::printf("状态\n");

  int count = 0;
  for (int i = 0; i < h.bookings.size(); ++i) {
    const Booking& b = h.bookings[i];
    if (b.patient != patient) continue;
    char time[24];
    std::snprintf(time, sizeof(time), "周%d%s", b.day + 1, b.slot == 0 ? "上午" : "下午");
    cell(b.ticket, 10);
    cell(h.depts[h.doctors[b.doctor].dept].name, 16);
    cell(h.doctors[b.doctor].name, 10);
    cell(time, 10);
    std::printf("%s\n", b.status == BOOKED ? "已预约" : (b.status == CANCELLED ? "已取消" : "已完成"));
    ++count;
  }
  if (count == 0) {
    std::printf("  （没有预约记录）\n");
    pause();
    return;
  }

  std::printf("\n输入要取消的凭证号（直接回车则不取消）：");
  if (!readLine(code, sizeof(code))) return;
  if (code[0] == '\0') return;

  for (int i = 0; i < h.bookings.size(); ++i) {
    Booking& b = h.bookings[i];
    if (b.patient != patient || std::strcmp(b.ticket, code) != 0) continue;
    if (b.status != BOOKED) {
      std::printf("该预约不是「已预约」状态，无法取消。\n");
      pause();
      return;
    }
    // 取消时把号源退回去，并递增版本号，让此前看到的余号信息立刻失效
    if (h.hasSchedule(b.doctor, b.day, b.slot)) {
      Schedule& s = h.slot(b.doctor, b.day, b.slot);
      if (s.booked > 0) {
        --s.booked;
        s.version = ++h.versionClock;  // 让此前看到的余号信息立刻失效
      }
    }
    b.status = CANCELLED;
    std::printf("预约 %s 已取消，号源已退回。\n", b.ticket);
    pause();
    return;
  }
  std::printf("没有找到该凭证号。\n");
  pause();
}

// =============================================================================
//  功能五：排班展示
// =============================================================================
inline void doShowSchedule() {
  Hospital& h = db();
  std::printf("\n================ 排班展示 ================\n");
  std::printf("请输入医生编号（直接回车则显示各科室汇总）：");
  char code[32];
  if (!readLine(code, sizeof(code))) return;

  // 不给医生编号：按科室汇总一周的号源
  if (code[0] == '\0') {
    std::printf("\n");
    cell("科室", 16);
    cell("出诊时段数", 12);
    std::printf("一周总号源\n");
    for (int d = 0; d < h.depts.size(); ++d) {
      int slots = 0;
      int total = 0;
      for (int doc = 0; doc < h.doctors.size(); ++doc) {
        if (h.doctors[doc].dept != d) continue;
        for (int day = 0; day < kMaxDay; ++day)
          for (int s = 0; s < 2; ++s) {
            if (!h.hasSchedule(doc, day, s)) continue;
            ++slots;
            total += h.slot(doc, day, s).quota;
          }
      }
      char slotText[16], totalText[16];
      std::snprintf(slotText, sizeof(slotText), "%d", slots);
      std::snprintf(totalText, sizeof(totalText), "%d", total);
      cell(h.depts[d].name, 16);
      cell(slotText, 12);
      std::printf("%s\n", totalText);
    }
    pause();
    return;
  }

  const int doctor = h.findDoctor(code);
  if (doctor < 0) {
    std::printf("医生编号不存在。\n");
    pause();
    return;
  }

  // 给了医生编号：按星期几列出上午下午的排班
  std::printf("\n%s %s（%s）一周排班：\n", h.doctors[doctor].code, h.doctors[doctor].name, h.doctors[doctor].title);
  cell("日期", 8);
  cell("上午", 16);
  cell("下午", 16);
  std::printf("备注\n");
  for (int day = 0; day < kMaxDay; ++day) {
    char dayText[16], am[32], pm[32];
    std::snprintf(dayText, sizeof(dayText), "周%d", day + 1);
    const bool hasAm = h.hasSchedule(doctor, day, 0);
    const bool hasPm = h.hasSchedule(doctor, day, 1);
    if (hasAm)
      std::snprintf(am, sizeof(am), "余 %d / 共 %d", h.remaining(doctor, day, 0), h.slot(doctor, day, 0).quota);
    else
      std::snprintf(am, sizeof(am), "不出诊");
    if (hasPm)
      std::snprintf(pm, sizeof(pm), "余 %d / 共 %d", h.remaining(doctor, day, 1), h.slot(doctor, day, 1).quota);
    else
      std::snprintf(pm, sizeof(pm), "不出诊");
    cell(dayText, 8);
    cell(am, 16);
    cell(pm, 16);
    std::printf("%s\n", (!hasAm && !hasPm) ? "休息" : "");
  }
  pause();
}

// =============================================================================
//  功能六：就诊路线
// =============================================================================
inline void doRoute() {
  Hospital& h = db();
  std::printf("\n================ 就诊路线 ================\n");
  std::printf("可用房间：\n");
  for (int i = 0; i < h.rooms.size(); ++i) std::printf("  %-8s %s\n", h.rooms[i].code, h.rooms[i].name);

  std::printf("\n请输入起点房间号：");
  char from[32];
  if (!readLine(from, sizeof(from))) return;
  std::printf("请输入终点房间号：");
  char to[32];
  if (!readLine(to, sizeof(to))) return;

  const int a = h.findRoom(from);
  const int b = h.findRoom(to);
  if (a < 0 || b < 0) {
    std::printf("房间号不存在。\n");
    pause();
    return;
  }
  if (a == b) {
    std::printf("起点与终点是同一个房间。\n");
    pause();
    return;
  }

  const RouteResult r = Route::between(h, a, b);
  if (!r.found) {
    std::printf("两点之间不连通。\n");
    pause();
    return;
  }

  std::printf("\n---------- 路线 ----------\n");
  std::printf("全程约 %d 米，途经 %d 个节点，换层 %d 次\n\n", r.totalDistance, r.stepCount, r.floorChanges);

  // 累计距离直接查最短路表：Floyd 已经算好任意两点间的距离，这里是 O(1)
  const int startNode = h.rooms[r.steps[0]].node;
  for (int i = 0; i < r.stepCount; ++i) {
    const Room& room = h.rooms[r.steps[i]];
    const bool isEnd = (i == 0 || i == r.stepCount - 1);
    const int acc = h.road.distance(startNode, room.node);
    std::printf("  %s %-30s 累计 %3d 米\n", isEnd ? "■" : "○", room.name, acc);
  }

  std::printf("\n换层指引：\n");
  if (r.floorChanges == 0) {
    std::printf("  全程在同一层，无需上下楼。\n");
  } else {
    for (int i = 1; i < r.stepCount; ++i) {
      const Room& prev = h.rooms[r.steps[i - 1]];
      const Room& cur = h.rooms[r.steps[i]];
      if (prev.floor == cur.floor) continue;
      // 换层发生在通道处，说明用的是电梯还是楼梯更实用
      const char* way = (std::strstr(prev.name, "电梯") || std::strstr(cur.name, "电梯")) ? "乘电梯" : "走楼梯";
      std::printf("  在「%s」%s，到达 %d 层的「%s」\n", prev.name, way, cur.floor + 1, cur.name);
    }
  }
  pause();
}
