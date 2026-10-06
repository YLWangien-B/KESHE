/* =============================================================================
 *  menu_queue.h  —  功能七：候诊队列
 *
 *  患者到院签到排队，医生依次叫号。四个操作：
 *      签到      患者进入其接诊医生的队伍，拿到排队号
 *      叫号      医生叫队伍里第一个还没被叫过的人
 *      过号      叫了没来，记一次；两次没来就重新排到队尾
 *      就诊完成  看完病离开队列
 *  另有「查看队列」随时看到前面还有几个人。
 *
 *  队列用环形队列实现，见 ds.h 的 RingQueue；队列操作在 service_queue.h 的 Queue 类。
 * ========================================================================== */
#pragma once

#include <cstdio>
#include <cstring>

#include "model.h"
#include "service_queue.h"
#include "ui.h"

// 打印某位医生当前的候诊队列
inline void showDoctorQueue(Hospital& h, int doctor) {
  const int count = Queue::countOf(h, doctor);
  const char* roomName = (h.doctors[doctor].room >= 0) ? h.rooms[h.doctors[doctor].room].name : "未安排诊室";
  std::printf("\n%s %s（%s）的候诊队列，共 %d 人：\n", h.doctors[doctor].code, h.doctors[doctor].name,
              h.doctors[doctor].title, count);
  std::printf("诊室：%s\n\n", roomName);
  if (count == 0) {
    std::printf("  （当前没有患者候诊）\n");
    return;
  }

  cell("排队号", 10);
  cell("患者编号", 12);
  cell("姓名", 12);
  std::printf("状态\n");

  int shown = 0;
  for (int i = 0; i < h.waiting.size() && shown < count; ++i) {
    const Waiting& w = h.waiting.at(i);
    if (w.doctor != doctor) continue;
    char no[16], code[16];
    std::snprintf(no, sizeof(no), "%d", w.queueNo);
    std::snprintf(code, sizeof(code), "%s", h.patients[w.patient].code);
    cell(no, 10);
    cell(code, 12);
    cell(h.patients[w.patient].name, 12);
    if (w.calledCount == 0)
      std::printf("等待中（前面 %d 人）\n", shown);
    else if (w.calledCount == 1)
      std::printf("已叫号，请进诊室\n");
    else
      std::printf("已过号 %d 次\n", w.calledCount - 1);
    ++shown;
  }
}

// ---------------------------- 签到 ----------------------------
inline void doCheckIn() {
  Hospital& h = db();
  std::printf("\n============ 候诊签到 ============\n");

  std::printf("患者列表：");
  for (int i = 0; i < h.patients.size(); ++i) std::printf(" %s(%s)", h.patients[i].code, h.patients[i].name);
  std::printf("\n请输入患者编号：");
  char buf[32];
  if (!readLine(buf, sizeof(buf))) return;
  const int patient = h.findPatient(buf);
  if (patient < 0) {
    std::printf("患者编号不存在。\n");
    pause();
    return;
  }

  // 如果这位患者有预约，直接告诉他去哪个诊室排队，省得再查一遍
  int suggested = -1;
  for (int i = 0; i < h.bookings.size(); ++i) {
    const Booking& b = h.bookings[i];
    if (b.patient == patient && b.status == BOOKED) {
      suggested = b.doctor;
      std::printf("\n提示：%s 在 %s %s 有一条预约（凭证号 %s），就诊时间 周%d%s。\n", h.patients[patient].name,
                  h.depts[h.doctors[b.doctor].dept].name, h.doctors[b.doctor].name, b.ticket, b.day + 1,
                  b.slot == 0 ? "上午" : "下午");
      break;
    }
  }

  std::printf("\n请输入接诊医生编号%s：", suggested >= 0 ? "（直接回车用上面那位）" : "");
  if (!readLine(buf, sizeof(buf))) return;
  int doctor = -1;
  if (buf[0] == '\0' && suggested >= 0)
    doctor = suggested;
  else
    doctor = h.findDoctor(buf);
  if (doctor < 0) {
    std::printf("医生编号不存在。\n");
    pause();
    return;
  }

  const QueueResult r = Queue::checkIn(h, patient, doctor);
  switch (r) {
    case QUEUE_OK: {
      const int pos = Queue::positionOf(h, patient, doctor);
      std::printf("\n---------- 签到成功 ----------\n");
      std::printf("  患者  ：%s %s\n", h.patients[patient].code, h.patients[patient].name);
      std::printf("  诊室  ：%s\n", h.doctors[doctor].room >= 0 ? h.rooms[h.doctors[doctor].room].name : "未安排");
      std::printf("  医生  ：%s %s\n", h.doctors[doctor].code, h.doctors[doctor].name);
      std::printf("  排队位置：第 %d 位（前面还有 %d 人）\n", pos, pos - 1);
      break;
    }
    case QUEUE_DUP:
      std::printf("\n%s 已经在 %s 的候诊队列里了，当前位置第 %d 位。\n", h.patients[patient].name,
                  h.doctors[doctor].name, Queue::positionOf(h, patient, doctor));
      break;
    case QUEUE_FULL:
      std::printf("\n候诊队列已满，请稍后再签到。\n");
      break;
    default:
      std::printf("\n签到失败。\n");
      break;
  }
  pause();
}

// ---------------------------- 叫号 ----------------------------
inline void doCallNext() {
  Hospital& h = db();
  std::printf("\n============ 医生叫号 ============\n");
  std::printf("请输入医生编号：");
  char buf[32];
  if (!readLine(buf, sizeof(buf))) return;
  const int doctor = h.findDoctor(buf);
  if (doctor < 0) {
    std::printf("医生编号不存在。\n");
    pause();
    return;
  }

  showDoctorQueue(h, doctor);

  std::printf("\n按回车键叫下一位...");
  readLine(buf, sizeof(buf));

  int patient = -1;
  const QueueResult r = Queue::callNext(h, doctor, patient);
  if (r == QUEUE_EMPTY) {
    std::printf("没有需要叫号的患者了。\n");
    pause();
    return;
  }

  std::printf("\n---------- 叫号 ----------\n");
  std::printf("  请 %s %s（排队号 %d）到 %s 就诊。\n", h.patients[patient].code, h.patients[patient].name,
              h.waiting.at(Queue::positionOf(h, patient, doctor) - 1).queueNo,
              h.doctors[doctor].room >= 0 ? h.rooms[h.doctors[doctor].room].name : "诊室");
  std::printf("\n剩余候诊人数：%d\n", Queue::countOf(h, doctor));
  pause();
}

// ---------------------------- 过号 ----------------------------
inline void doPass() {
  Hospital& h = db();
  std::printf("\n============ 过号处理 ============\n");
  std::printf("请输入医生编号：");
  char buf[32];
  if (!readLine(buf, sizeof(buf))) return;
  const int doctor = h.findDoctor(buf);
  if (doctor < 0) {
    std::printf("医生编号不存在。\n");
    pause();
    return;
  }
  showDoctorQueue(h, doctor);

  std::printf("\n请输入患者编号：");
  if (!readLine(buf, sizeof(buf))) return;
  const int patient = h.findPatient(buf);
  if (patient < 0 || !Queue::isWaiting(h, patient, doctor)) {
    std::printf("该患者不在这个医生的候诊队列里。\n");
    pause();
    return;
  }

  const int before = Queue::positionOf(h, patient, doctor);
  const bool requeued = Queue::pass(h, patient, doctor);
  if (requeued) {
    std::printf("\n%s 已过号两次，重新排到队尾，新位置第 %d 位。\n", h.patients[patient].name,
                Queue::positionOf(h, patient, doctor));
  } else {
    std::printf("\n%s 记过一次过号（原位置第 %d 位）。再叫一次不到就重新排队。\n", h.patients[patient].name, before);
  }
  pause();
}

// ---------------------------- 就诊完成 ----------------------------
inline void doFinish() {
  Hospital& h = db();
  std::printf("\n============ 就诊完成 ============\n");
  std::printf("请输入医生编号：");
  char buf[32];
  if (!readLine(buf, sizeof(buf))) return;
  const int doctor = h.findDoctor(buf);
  if (doctor < 0) {
    std::printf("医生编号不存在。\n");
    pause();
    return;
  }
  showDoctorQueue(h, doctor);

  std::printf("\n请输入已完成就诊的患者编号：");
  if (!readLine(buf, sizeof(buf))) return;
  const int patient = h.findPatient(buf);
  const QueueResult r = Queue::finish(h, patient, doctor);
  if (r == QUEUE_NOT_FOUND) {
    std::printf("该患者不在这个医生的候诊队列里。\n");
    pause();
    return;
  }
  std::printf("\n%s 已完成就诊，离开候诊队列。当前还有 %d 人候诊。\n", h.patients[patient].name,
              Queue::countOf(h, doctor));
  pause();
}

// ---------------------------- 查看队列 ----------------------------
inline void doViewQueue() {
  Hospital& h = db();
  std::printf("\n============ 查看候诊队列 ============\n");
  std::printf("输入医生编号查看某位医生的队列，直接回车看全院汇总：");
  char buf[32];
  if (!readLine(buf, sizeof(buf))) return;

  if (buf[0] != '\0') {
    const int doctor = h.findDoctor(buf);
    if (doctor < 0) {
      std::printf("医生编号不存在。\n");
      pause();
      return;
    }
    showDoctorQueue(h, doctor);
    pause();
    return;
  }

  // 全院汇总：按科室列出还有多少人在候诊
  std::printf("\n全院候诊情况：\n");
  cell("科室", 16);
  cell("医生数", 10);
  std::printf("候诊人数\n");
  int total = 0;
  for (int d = 0; d < h.depts.size(); ++d) {
    int docs = 0, waiting = 0;
    for (int i = 0; i < h.doctors.size(); ++i) {
      if (h.doctors[i].dept != d) continue;
      ++docs;
      waiting += Queue::countOf(h, i);
    }
    if (waiting == 0) continue;
    char docText[8], waitText[8];
    std::snprintf(docText, sizeof(docText), "%d", docs);
    std::snprintf(waitText, sizeof(waitText), "%d", waiting);
    cell(h.depts[d].name, 16);
    cell(docText, 10);
    std::printf("%s\n", waitText);
    total += waiting;
  }
  if (total == 0)
    std::printf("  （全院当前没有患者候诊）\n");
  else
    std::printf("\n全院共 %d 人候诊。\n", total);
  pause();
}

// ---------------------------- 候诊队列子菜单 ----------------------------
inline void doQueue() {
  for (;;) {
    std::printf("\n------------ 候诊队列 ------------\n");
    std::printf("  1. 患者签到排队\n");
    std::printf("  2. 医生叫号\n");
    std::printf("  3. 过号处理\n");
    std::printf("  4. 就诊完成，离开队列\n");
    std::printf("  5. 查看候诊队列\n");
    std::printf("  0. 返回主菜单\n");
    std::printf("-----------------------------------\n请选择：");

    char buf[32];
    if (!readLine(buf, sizeof(buf))) return;
    const int c = std::atoi(buf);
    if (c == 0) return;
    switch (c) {
      case 1: doCheckIn(); break;
      case 2: doCallNext(); break;
      case 3: doPass(); break;
      case 4: doFinish(); break;
      case 5: doViewQueue(); break;
      default: std::printf("\n请输入 0 到 5 之间的序号。\n"); break;
    }
  }
}
