/* =============================================================================
 *  shared_db.h  —  单机双进程共享数据总库
 *
 *  ───────────────────────────────────────────────────────────────────────────
 *  要解决的问题
 *
 *  把系统拆成「患者客户端」与「管理端」两个程序，各自独立运行，不联网，
 *  但**可能同时读写同一份数据**。所以需要两样东西：
 *      ① 共享：两个进程看到同一份数据
 *      ② 互斥：同时写的时候不互相踩
 *
 *  ───────────────────────────────────────────────────────────────────────────
 *  做法：内存映射文件 + 命名互斥体
 *
 *  把整个 Hospital 映射到一块**由文件承载的共享内存**里
 *  （data\state.bin），两端 hospital() 拿到的是同一块物理内存；
 *  写操作外面套一把命名互斥体（见 Lock）。
 *
 *  为什么能这么做 —— 这是 model.h 那四条设计原则带来的红利：
 *      · 全部实体是定长记录，只存数据不存指针      -> 可以整体搬移
 *      · 实体之间用下标互相引用，不用指针           -> 跨进程后下标依然有效
 *      · 容量在编译期确定，运行期不申请内存         -> 布局固定，两端一致
 *  实测：sizeof(Hospital) = 370444，trivially copyable = 1，
 *        standard_layout = 1，整库 memcpy 后逐字节相同。
 *
 *  这个模型不是凭空想的：PostgreSQL 的 shared_buffers、Oracle 的 SGA
 *  就是「多进程共享一块内存 + Latch 协调」，本文件是它的缩小版，
 *  区别只是把锁粒度从「数据页」简化成「整个库」—— 对 370 KB 的数据量、
 *  每次只改一两个字段的访问模式，粗粒度反而更简单、也不会死锁。
 *
 *  ───────────────────────────────────────────────────────────────────────────
 *  两种运行模式
 *
 *      单进程模式（默认）  不调用 open()，hospital() 返回进程内的静态实例。
 *                          行为与拆分之前完全一致，回归测试仍然有效。
 *      共享模式            main 里调用 open(dir) 之后，hospital() 返回共享区。
 *
 *  共享区的文件是 data\state.bin。删掉它，下次启动会重新从文本数据装载。
 * ========================================================================== */
#pragma once

#include <cstdint>

#include "model.h"

namespace shared {

// 共享区文件头的魔数与布局版本。两端编译产物不一致时靠它们立刻发现，
// 而不是把内存按错误的布局解释成垃圾数据。
constexpr uint32_t kMagic = 0x44505348u;  // 'H' 'S' 'P' 'D'（小端）
constexpr uint32_t kLayoutVersion = 2u;  // 2：Booking 加了 rated 字段

struct Header {
  uint32_t magic;           // 必须等于 kMagic
  uint32_t layoutVersion;   // 必须等于 kLayoutVersion
  uint32_t sizeofHospital;  // 必须等于 sizeof(Hospital)：编译器/选项不一致会变
  uint32_t initialized;     // 1 = 已经装载过数据
  uint32_t generation;      // 写操作计数：一端每写一次 +1，另一端据此决定要不要刷新
  uint32_t reserved;
};

// 操作类别（写进审计日志）
enum OpKind {
  OP_NONE = 0,
  OP_BOOK = 1,     // 预约
  OP_CANCEL = 2,   // 取消预约
  OP_CHECKIN = 3,  // 签到排队
  OP_CALL = 4,     // 叫号
  OP_PASS = 5,     // 过号
  OP_FINISH = 6,   // 就诊完成
  OP_STOP = 7,     // 停诊
  OP_STOP_CANCEL = 8,  // 停诊时自动取消了预约
  OP_RATE = 9,     // 患者评价
  OP_RESUME = 10,  // 恢复出诊
};

// 一条审计记录。对应真实医院信息系统里「号源变动可追溯」的要求。
// 用定长结构 + 环形覆盖，不申请内存。
struct OpEntry {
  uint32_t seq;      // 递增序号（0 表示空位）
  uint32_t time;     // time_t
  int16_t op;
  int16_t patient;   // -1 表示不涉及
  int16_t doctor;    // -1 表示不涉及
  int16_t result;    // 0 = 成功，负数 = 失败原因（见各调用方）
};

constexpr int kOpLogSize = 256;

// 共享区的完整布局
struct Block {
  Header header;
  Hospital hospital;
  OpEntry ops[kOpLogSize];
};

// ---------------------------------------------------------------------------
//  生命周期
// ---------------------------------------------------------------------------

// 打开（不存在则创建）共享数据区并映射。dataDir 形如 "data"。
// 第一个打开的进程负责从文本数据装载；后续进程直接复用已有内容。
// 失败时返回 false 且不改变模式（仍可当单进程程序用）。
bool open(const char* dataDir);

// 解除映射（进程退出时调用；不调用也会随进程结束释放）
void close();

// 是否已处于共享模式
bool isOpen();

// 当前数据总库。共享模式下就是共享区里那一份；否则是进程内的静态实例。
Hospital& hospital();

// 共享模式下返回写操作计数，否则返回 0。界面轮询它来决定要不要刷新。
uint32_t generation();

// 写操作完成后调用：generation + 1，让另一端的界面知道该刷新了。
// 必须在持有 Lock 时调用（否则可能与另一端交错）。
void bump();

// 追加一条审计记录。同样必须在持有 Lock 时调用。
void logOp(int op, int patient, int doctor, int result);

// ---------------------------------------------------------------------------
//  跨进程互斥（RAII）
//
//  用法：把「检查 + 改数据」这一小段放进一个作用域，不要包住等用户输入的语句，
//  否则一端在等人按键时会把另一端卡住。
//
//      {
//        shared::Lock lock;
//        if (余量为 0) 失败;
//        ++booked;
//      }
//      打印结果（锁已经释放）
// ---------------------------------------------------------------------------
class Lock {
 public:
  Lock();
  ~Lock();
  Lock(const Lock&) = delete;
  Lock& operator=(const Lock&) = delete;

 private:
  bool held_ = false;
};

}  // namespace shared
