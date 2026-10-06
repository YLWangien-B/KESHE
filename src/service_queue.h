// =============================================================================
//  候诊队列
//
//  患者到院后签到排队，医生依次叫号。队列用环形队列实现（ds.h 的 RingQueue）：
//  入队、出队都是 O(1)，出队腾出的位置会被后来的患者循环使用 —— 这正是
//  「叫一个、进一个」的候诊场景。
//
//  队列元素里只存「谁在排、哪个诊室、排到几号、叫过几次」；
//  患者的姓名、医生的诊室都通过下标去查，不重复保存，
//  因此不会出现两处数据对不上的情况。
//
//  「先来先看」这条规则是怎么守住的：
//    · 队首的人一旦被叫号，他就挡在前面 —— callNext 不会越过他去叫后面的人，
//      而是返回 QUEUE_FIRST_CALLED，让医生决定是继续等还是记为过号；
//    · 记为过号的次数达到 kMaxPass 次后，他重新排到队尾，队列继续往前走；
//    · 只有医生点「就诊完成」，他才真正离开队列。
//  这样既不会让不守时的人一直堵着队伍，也不会让后来的人插队。
// =============================================================================
#pragma once

#include "model.h"

// 候诊操作的结果，供界面给出准确的提示
enum QueueResult {
  QUEUE_OK = 0,            // 成功
  QUEUE_FULL = 1,          // 队列已满
  QUEUE_EMPTY = 2,         // 该医生没有候诊患者
  QUEUE_DUP = 3,           // 该患者已经在队列里了
  QUEUE_NO_DOCTOR = 4,     // 医生或患者不存在
  QUEUE_NOT_FOUND = 5,     // 队列里找不到这个人
  QUEUE_FIRST_CALLED = 6,  // 队首已被叫号还没处理完，不能越过去叫后面的人
};

class Queue {
 public:
  // ---------------------------- 签到入队 ----------------------------
  // 排队号在同一个医生内从 1 开始递增；同一患者不能重复排队。
  static QueueResult checkIn(Hospital& h, int patient, int doctor) {
    if (patient < 0 || patient >= h.patients.size()) return QUEUE_NO_DOCTOR;
    if (doctor < 0 || doctor >= h.doctors.size()) return QUEUE_NO_DOCTOR;
    if (h.waiting.full()) return QUEUE_FULL;
    if (indexOf(h, patient, doctor) >= 0) return QUEUE_DUP;

    Waiting w;
    w.patient = patient;
    w.doctor = doctor;
    w.queueNo = nextQueueNo(h, doctor);
    w.calledCount = 0;
    h.waiting.push(w);
    return QUEUE_OK;
  }

  // ---------------------------- 叫号 ----------------------------
  // 叫该医生队伍里第一个还没被叫过的人，patient 传出他的下标。
  //
  // 如果队首的人已经被叫过（正在等他进诊室，或者他已经过号一次），
  // 就不能越过他去叫后面的人，否则「先来先看」的队列就失去意义了。
  // 这时返回 QUEUE_FIRST_CALLED，由医生决定继续等还是用 pass() 记为过号。
  //
  // 被叫到的人仍然留在队列里（只把 calledCount 加一），这样患者能看到
  // 「已经叫到我了」；真正离开队列要等医生点「就诊完成」，符合实际流程。
  static QueueResult callNext(Hospital& h, int doctor, int& patient) {
    patient = -1;
    const int n = h.waiting.size();
    for (int i = 0; i < n; ++i) {
      Waiting& w = h.waiting.at(i);
      if (w.doctor != doctor) continue;
      if (w.calledCount > 0) return QUEUE_FIRST_CALLED;  // 前面还有人没处理完
      ++w.calledCount;
      patient = w.patient;
      return QUEUE_OK;
    }
    return QUEUE_EMPTY;
  }

  // ---------------------------- 就诊完成，离开队列 ----------------------------
  static QueueResult finish(Hospital& h, int patient, int doctor) {
    const int i = indexOf(h, patient, doctor);
    if (i < 0) return QUEUE_NOT_FOUND;
    h.waiting.removeAt(i);
    return QUEUE_OK;
  }

  // ---------------------------- 过号 ----------------------------
  // 叫了却没来，记一次；记满 kMaxPass 次就重新排到队尾，不再堵着后面的人。
  // 返回 true 表示已经重新排队。
  static bool pass(Hospital& h, int patient, int doctor) {
    const int i = indexOf(h, patient, doctor);
    if (i < 0) return false;

    if (h.waiting.at(i).calledCount >= kMaxPass) {
      // 叫了这么多次都没来，重新排队：先离开队列，再以新的排队号入队
      h.waiting.removeAt(i);
      return checkIn(h, patient, doctor) == QUEUE_OK;
    }
    ++h.waiting.at(i).calledCount;
    return false;
  }

  // ---------------------------- 查询 ----------------------------
  // 该患者在某医生队伍中的位置：1 表示队首，0 表示不在队列里
  static int positionOf(const Hospital& h, int patient, int doctor) {
    int pos = 0;
    const int n = h.waiting.size();
    for (int i = 0; i < n; ++i) {
      const Waiting& w = h.waiting.at(i);
      if (w.doctor != doctor) continue;
      ++pos;
      if (w.patient == patient) return pos;
    }
    return 0;
  }

  // 该患者前面还有几个人在等。
  // 注意「已经被叫号的」不算 —— 他们已经在诊室门口甚至正在就诊，
  // 把他们算进「前面等的人」会让患者以为还要等很久。
  static int waitingAhead(const Hospital& h, int patient, int doctor) {
    int ahead = 0;
    const int n = h.waiting.size();
    for (int i = 0; i < n; ++i) {
      const Waiting& w = h.waiting.at(i);
      if (w.doctor != doctor) continue;
      if (w.patient == patient) return ahead;
      if (w.calledCount == 0) ++ahead;
    }
    return ahead;
  }

  // 某医生当前的候诊人数（含已被叫号的）
  static int countOf(const Hospital& h, int doctor) {
    int n = 0;
    for (int i = 0; i < h.waiting.size(); ++i)
      if (h.waiting.at(i).doctor == doctor) ++n;
    return n;
  }

  // 该患者是否已经签到（在队列里）
  static bool isWaiting(const Hospital& h, int patient, int doctor) { return indexOf(h, patient, doctor) >= 0; }

  // 该患者的排队号；不在队列里返回 0
  static int queueNoOf(const Hospital& h, int patient, int doctor) {
    const int i = indexOf(h, patient, doctor);
    return i >= 0 ? h.waiting.at(i).queueNo : 0;
  }

 private:
  // 在队列里找该患者，返回他在队列中的次序；找不到返回 -1
  static int indexOf(const Hospital& h, int patient, int doctor) {
    for (int i = 0; i < h.waiting.size(); ++i) {
      const Waiting& w = h.waiting.at(i);
      if (w.patient == patient && w.doctor == doctor) return i;
    }
    return -1;
  }

  // 该医生下一个该发的排队号 = 已发出的最大号 + 1
  static int nextQueueNo(const Hospital& h, int doctor) {
    int maxNo = 0;
    for (int i = 0; i < h.waiting.size(); ++i) {
      const Waiting& w = h.waiting.at(i);
      if (w.doctor == doctor && w.queueNo > maxNo) maxNo = w.queueNo;
    }
    return maxNo + 1;
  }
};
