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
constexpr uint32_t kLayoutVersion = 4u;  // 4：BookStatus 增加了 RESCHEDULED

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
  OP_REPAIR = 11,  // 一致性修复
  OP_RESCHEDULE = 12,   // 停诊后自动改约
  OP_ADD_PATIENT = 13,  // 登记新患者
  OP_ADD_DOCTOR = 14,   // 登记新医生
  OP_SHIFT = 15,        // 换班（号源带预约转移）
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

// ---- 心跳槽：让每个进程能看出「另一端还活着吗」----
// 为什么需要：两个进程共享数据，如果对端崩溃（或被强行关掉），
// 本端看到的是"最后写入的状态"，看起来一切正常；但从那一刻起
// 就不会再有别人进来改数据了。界面上提示一句「对端已退出」，
// 可以避免把「另一端没反应」误判成「功能不好使」。
//
// 每个进程定期刷自己的槽：pid + 角色 + 时间戳。超过 kPeerTimeoutSec 没刷新
// 就认为它已经不在（正常退出会主动清掉自己的槽）。
struct PeerSlot {
  uint32_t pid;       // 0 表示空位
  uint32_t role;      // 1=患者端 2=管理端 3=字符界面
  uint32_t lastSeen;  // time_t，最后一次心跳
  uint32_t reserved;
};
constexpr int kMaxPeers = 4;
constexpr uint32_t kPeerTimeoutSec = 5;

// 共享区的完整布局
struct Block {
  Header header;
  Hospital hospital;
  OpEntry ops[kOpLogSize];
  PeerSlot peers[kMaxPeers];
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

// ---- 审计日志的读取（界面显示用）----
// opEntries() 返回环形数组首地址（kOpLogSize 条，可能含空位：seq == 0）；
// opFillCount() 返回已经写过的条数（最多 kOpLogSize，环形覆盖后会停在 kOpLogSize）。
// 单进程模式下两者分别返回 nullptr 与 0。
const OpEntry* opEntries();
int opFillCount();

// 操作类别的中文名（界面显示用）
const char* opName(int op);

// ---- 心跳 ----
// 登记/刷新自己（role 见 PeerRole）。程序退出时调用 leave() 清掉自己的槽。
void join(uint32_t role);
void beat();
void leave();

enum PeerRole { ROLE_NONE = 0, ROLE_PATIENT = 1, ROLE_ADMIN = 2, ROLE_CONSOLE = 3 };
const char* roleText(uint32_t role);

// 当前有几个「活着的对端」（不含自己）
int alivePeers();
// 取第 i 个活着的对端信息；返回 false 表示没有这个下标
bool peerInfo(int i, uint32_t* pid, uint32_t* role);
// 有没有对端是「不正常退出」的（槽还占着但心跳超时）
bool hasStalePeer();

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
