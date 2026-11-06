/* =============================================================================
 *  shared_db.cpp  —  共享数据区的实现（Win32 内存映射 + 命名互斥体）
 *
 *  这里只用一个平台 API：Win32。它是操作系统自带的接口，性质与 <cstdio> 的
 *  fopen 相同，不属于「数据结构类库」，课程约束不涉及它。
 *  数据结构层（ds.h）、业务层（service*.h）一行都没改。
 * ========================================================================== */
#include "shared_db.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <new>  // placement new

#include <windows.h>

#include "load.h"

namespace shared {

namespace {

// 共享内核对象的名字。Local\ 前缀表示「同一个登录会话内可见」，
// 比 Global\ 更安全（后者跨会话，且某些情况下需要额外权限）。
constexpr wchar_t kMapName[] = L"Local\\HospitalSharedState";
constexpr wchar_t kMutexName[] = L"Local\\HospitalSharedMutex";

HANDLE g_file = INVALID_HANDLE_VALUE;
HANDLE g_map = nullptr;
HANDLE g_mutex = nullptr;
Block* g_block = nullptr;

// 单进程模式下的数据总库（与拆分之前的 db.cpp 行为一致）
Hospital& localInstance() {
  static Hospital instance;
  return instance;
}

// 把布局信息写进文件头
void fillHeader(Block* b) {
  b->header.magic = kMagic;
  b->header.layoutVersion = kLayoutVersion;
  b->header.sizeofHospital = static_cast<uint32_t>(sizeof(Hospital));
  b->header.initialized = 1;
  b->header.generation = 0;
  b->header.reserved = 0;
}

// 这个共享区里的数据还能不能用？
// 三种情况下都当作「不能用，要重新装载」：
//   1) 魔数不对         —— 不是一个有效的共享区（或者是别的程序留下的文件）
//   2) 布局版本不同     —— 数据模型改过
//   3) sizeof 不同      —— 两端用了不同的编译器/编译选项，布局对不上，最危险
bool headerUsable(const Block* b) {
  return b->header.magic == kMagic && b->header.layoutVersion == kLayoutVersion &&
         b->header.sizeofHospital == static_cast<uint32_t>(sizeof(Hospital)) &&
         b->header.initialized == 1;
}

// 把共享区初始化成「刚构造完、还没装数据」的状态
void resetBlock(Block* b) {
  std::memset(b, 0, sizeof(Block));
  // Hospital 有默认成员初始值（queueSeq = 0 这类），不是 trivially default
  // constructible，所以不能只用 memset —— 用 placement new 跑一遍构造函数。
  // 对象本身在共享内存里，不占栈。
  new (&b->hospital) Hospital;
}

}  // namespace

// =============================================================================
//  打开 / 关闭
// =============================================================================
bool open(const char* dataDir) {
  if (g_block) return true;  // 已经打开了

  // ---- 命名互斥体：先建出来，后面的初始化要串行化 ----
  g_mutex = CreateMutexW(nullptr, FALSE, kMutexName);
  if (g_mutex == nullptr) {
    std::printf("[共享] 创建互斥体失败（错误码 %lu）\n", GetLastError());
    return false;
  }

  // 初始化期间的互斥：两个进程同时首次启动时，只让一个去装载数据
  {
    const DWORD wait = WaitForSingleObject(g_mutex, 10000);
    if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED) {
      std::printf("[共享] 等待初始化锁超时\n");
      return false;
    }
  }

  // ---- 打开/创建承载共享内存的文件 ----
  char path[MAX_PATH];
  std::snprintf(path, sizeof(path), "%s/state.bin", dataDir);

  // 用 ANSI 版本：dataDir 本身就是系统代码页的字节（与 load.h 里的 fopen 一致），
  // 中文路径不会出问题。
  g_file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
                       FILE_SHARE_READ | FILE_SHARE_WRITE,  // 必须允许别人也打开
                       nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (g_file == INVALID_HANDLE_VALUE) {
    std::printf("[共享] 打不开共享数据文件：%s（错误码 %lu）\n", path, GetLastError());
    ReleaseMutex(g_mutex);
    return false;
  }

  // ---- 保证文件至少有 sizeof(Block) 那么大 ----
  // CreateFileMapping 会按请求的大小映射，但文件只有 0 字节时行为不直观，
  // 所以显式扩展一次，长度不对就重设。
  LARGE_INTEGER want;
  want.QuadPart = static_cast<LONGLONG>(sizeof(Block));
  LARGE_INTEGER have;
  if (!GetFileSizeEx(g_file, &have) || have.QuadPart != want.QuadPart) {
    if (!SetFilePointerEx(g_file, want, nullptr, FILE_BEGIN) || !SetEndOfFile(g_file)) {
      std::printf("[共享] 扩展共享数据文件失败（错误码 %lu）\n", GetLastError());
      CloseHandle(g_file);
      g_file = INVALID_HANDLE_VALUE;
      ReleaseMutex(g_mutex);
      return false;
    }
  }

  // ---- 映射 ----
  g_map = CreateFileMappingW(g_file, nullptr, PAGE_READWRITE,
                             static_cast<DWORD>(want.QuadPart >> 32),
                             static_cast<DWORD>(want.QuadPart & 0xFFFFFFFFu), kMapName);
  if (g_map == nullptr) {
    std::printf("[共享] 创建文件映射失败（错误码 %lu）\n", GetLastError());
    CloseHandle(g_file);
    g_file = INVALID_HANDLE_VALUE;
    ReleaseMutex(g_mutex);
    return false;
  }

  void* view = MapViewOfFile(g_map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Block));
  if (view == nullptr) {
    std::printf("[共享] 映射视图失败（错误码 %lu）\n", GetLastError());
    CloseHandle(g_map);
    g_map = nullptr;
    CloseHandle(g_file);
    g_file = INVALID_HANDLE_VALUE;
    ReleaseMutex(g_mutex);
    return false;
  }

  g_block = static_cast<Block*>(view);

  // ---- 判断要不要初始化 ----
  if (headerUsable(g_block)) {
    std::printf("[共享] 复用已有的共享数据（写操作计数 %u）\n", g_block->header.generation);
  } else {
    // 第一次启动：把文本数据装进共享区
    if (g_block->header.magic == kMagic &&
        g_block->header.sizeofHospital != static_cast<uint32_t>(sizeof(Hospital))) {
      std::printf("[共享] 警告：已有的共享数据布局不一致"
                  "（文件里 %u 字节，本程序 %u 字节），将重新装载。\n",
                  g_block->header.sizeofHospital, static_cast<uint32_t>(sizeof(Hospital)));
    }
    std::printf("[共享] 首次初始化共享数据区：%s\n", path);
    resetBlock(g_block);
    if (!Loader::loadAll(g_block->hospital, dataDir)) {
      std::printf("[共享] 装载数据失败，放弃共享模式\n");
      UnmapViewOfFile(g_block);
      g_block = nullptr;
      CloseHandle(g_map);
      g_map = nullptr;
      CloseHandle(g_file);
      g_file = INVALID_HANDLE_VALUE;
      ReleaseMutex(g_mutex);
      return false;
    }
    fillHeader(g_block);
    std::printf("[共享] 初始化完成（共享区 %d 字节，其中数据总库 %d 字节）\n",
                static_cast<int>(sizeof(Block)), static_cast<int>(sizeof(Hospital)));
  }

  ReleaseMutex(g_mutex);
  return true;
}

void close() {
  if (g_block) {
    UnmapViewOfFile(g_block);
    g_block = nullptr;
  }
  if (g_map) {
    CloseHandle(g_map);
    g_map = nullptr;
  }
  if (g_file != INVALID_HANDLE_VALUE) {
    CloseHandle(g_file);
    g_file = INVALID_HANDLE_VALUE;
  }
  if (g_mutex) {
    CloseHandle(g_mutex);
    g_mutex = nullptr;
  }
}

bool isOpen() { return g_block != nullptr; }

// =============================================================================
//  数据访问
// =============================================================================
Hospital& hospital() {
  if (g_block) return g_block->hospital;
  return localInstance();
}

uint32_t generation() { return g_block ? g_block->header.generation : 0u; }

void bump() {
  if (g_block) ++g_block->header.generation;
}

void logOp(int op, int patient, int doctor, int result) {
  if (!g_block) return;
  // 环形覆盖：seq 从 1 开始，取模定位槽位。最老的记录被新记录盖掉。
  static uint32_t nextSeq = 1;  // 只在持有锁时被调用，不需要原子
  Block* b = g_block;
  OpEntry& e = b->ops[nextSeq % kOpLogSize];
  e.seq = nextSeq++;
  e.time = static_cast<uint32_t>(std::time(nullptr));
  e.op = static_cast<int16_t>(op);
  e.patient = static_cast<int16_t>(patient);
  e.doctor = static_cast<int16_t>(doctor);
  e.result = static_cast<int16_t>(result);
}

// =============================================================================
//  跨进程互斥
// =============================================================================
Lock::Lock() {
  if (!g_mutex) return;  // 单进程模式：不需要锁
  const DWORD r = WaitForSingleObject(g_mutex, INFINITE);
  // WAIT_ABANDONED 表示上一个持有者崩溃了、系统替它释放了锁。
  // 锁是拿到了，但数据可能停在不一致的状态 —— 当作「拿到锁」处理，
  // 让程序继续（界面刷新一次就能看到当前真实状态）。
  held_ = (r == WAIT_OBJECT_0 || r == WAIT_ABANDONED);
  if (r == WAIT_ABANDONED) {
    std::printf("[共享] 警告：上一个进程异常退出，数据可能不一致\n");
  }
}

Lock::~Lock() {
  if (held_ && g_mutex) ReleaseMutex(g_mutex);
}

}  // namespace shared
