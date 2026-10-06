/* =============================================================================
 *  load.h  —  数据装载
 *
 *  题目要求「测试数据通过文本文件读入」。装载按下面的顺序进行，顺序不能颠倒，
 *  因为后面的表要用前面的表做外键校验：
 *      symptoms.txt -> depts.txt -> doctors.txt -> layout.txt
 *      schedules.txt / patients.txt 与其它表无关
 *
 *  文件格式：UTF-8 文本，字段用 '|' 分隔，'#' 开头是注释行。
 *  某一行字段不对或外键找不到时，跳过该行并计入错误数，不中断整个装载，
 *  这样数据文件有小毛病时程序仍能起来，便于排查。
 * ========================================================================== */
#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "model.h"

class Loader {
 public:
  // 装载全部数据。dataDir 形如 "data"。返回是否成功。
  static bool loadAll(Hospital& h, const char* dataDir) {
    h.clear();
    db_ = &h;  // 各 load 函数通过它访问总库，省去逐个传参
    dir_ = dataDir;
    errors_ = 0;

    if (!loadSymptoms() || !loadDepts() || !loadDoctors() || !loadSchedules() || !loadPatients() || !loadLayout()) {
      return false;
    }
    // 全部房间与边就位后，用 Floyd 算法一次算出任意两点之间的最短路线
    h.road.floyd();
    return true;
  }

  static int errorCount() { return errors_; }
  static int overflowCount() { return overflows_; }

 private:
  // 类内静态成员：整个装载过程共用的总库地址、文件句柄、目录与错误计数
  inline static Hospital* db_ = nullptr;
  inline static const char* dir_ = "data";
  inline static std::FILE* fp_ = nullptr;
  inline static int errors_ = 0;
  inline static int overflows_ = 0;

  static Hospital& hospital() { return *db_; }

  // ------------------------------ 容量检查 ------------------------------
  // 全部数据表的容量都是编译期固定的常量。如果数据文件比容量还大，
  // 记录会被丢弃 —— 这种失败必须是响亮的：静默丢掉几条数据，
  // 后面查到「某个医生不见了」，很难定位到是容量的问题。
  static bool ensureRoom(const char* file, const char* what, bool hasRoom) {
    if (hasRoom) return true;
    ++errors_;
    ++overflows_;
    std::printf("[数据] %s：%s 已超出容量上限，后面的记录被丢弃。\n", file, what);
    std::printf("       请调大 src/model.h 里对应的 kMax* 常量后重新编译。\n");
    return false;
  }

  // ------------------------------ 文件与行的读取 ------------------------------
  static bool openFile(const char* name) {
    char path[512];
    std::snprintf(path, sizeof(path), "%s/%s", dir_, name);
    fp_ = std::fopen(path, "rb");
    if (!fp_) std::printf("[错误] 打不开数据文件 %s\n", path);
    return fp_ != nullptr;
  }

  static void closeFile() {
    if (fp_) std::fclose(fp_);
    fp_ = nullptr;
  }

  // 记录一行数据的问题，便于定位是哪个文件的哪一行出了问题
  static void warn(const char* file, const char* what, const char* detail = "") {
    ++errors_;
    std::printf("[数据] %s：%s %s\n", file, what, detail);
  }

  // 读取下一个有效行（跳过空行与注释行），去掉行尾换行与行首空白
  static bool nextLine(char* buf, int cap) {
    while (std::fgets(buf, cap, fp_)) {
      int n = static_cast<int>(std::strlen(buf));
      while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = '\0';
      char* s = buf;
      while (*s == ' ' || *s == '\t') ++s;
      if (*s == '\0' || *s == '#') continue;
      if (s != buf) std::memmove(buf, s, std::strlen(s) + 1);
      return true;
    }
    return false;
  }

  // 按 '|' 切分一行，最多切出 maxFields 个字段；返回实际字段数
  static int split(char* line, char fields[][64], int maxFields) {
    int n = 0;
    char* p = line;
    while (n < maxFields) {
      char* bar = std::strchr(p, '|');
      if (bar) *bar = '\0';
      std::snprintf(fields[n], 64, "%s", p);
      ++n;
      if (!bar) break;
      p = bar + 1;
    }
    return n;
  }

  // 从一个逗号分隔的整数串里取下 n 个整数
  static int parseInts(const char* s, int* out, int cap) {
    int n = 0;
    const char* p = s;
    while (*p && n < cap) {
      out[n++] = std::atoi(p);
      while (*p && *p != ',') ++p;
      if (*p == ',') ++p;
    }
    return n;
  }

  // ------------------------------ 症状表 ------------------------------
  static bool loadSymptoms() {
    Hospital& h = hospital();
    if (!openFile("symptoms.txt")) return false;
    char line[256];
    while (nextLine(line, sizeof(line))) {
      if (!ensureRoom("symptoms.txt", "症状表", !h.symptoms.full())) break;
      Symptom s;
      std::snprintf(s.name, sizeof(s.name), "%s", line);
      h.symptoms.push(s);
    }
    closeFile();
    return true;
  }

  // ------------------------------ 科室表 ------------------------------
  // 行格式：编号|名称|位置|关联症状(症状名:权重, 逗号分隔)
  static bool loadDepts() {
    Hospital& h = hospital();
    if (!openFile("depts.txt")) return false;
    char line[512];
    while (nextLine(line, sizeof(line))) {
      if (!ensureRoom("depts.txt", "科室表", !h.depts.full())) break;
      char fields[4][64];
      if (split(line, fields, 4) < 4) {
        warn("depts.txt", "字段不足：", line);
        continue;
      }
      Department d;
      std::snprintf(d.code, sizeof(d.code), "%s", fields[0]);
      std::snprintf(d.name, sizeof(d.name), "%s", fields[1]);
      std::snprintf(d.location, sizeof(d.location), "%s", fields[2]);
      d.symptomCount = 0;

      // 解析「症状名:权重, 症状名:权重, ...」
      char* item = fields[3];
      while (*item && d.symptomCount < kMaxSympPerDept) {
        char* comma = std::strchr(item, ',');
        if (comma) *comma = '\0';
        char* colon = std::strchr(item, ':');
        int weight = 1;
        if (colon) {
          *colon = '\0';
          weight = std::atoi(colon + 1);
        }
        const int sym = h.findSymptomByName(item);
        if (sym >= 0) {
          d.symptoms[d.symptomCount] = sym;
          d.weights[d.symptomCount] = weight;
          ++d.symptomCount;
        } else {
          warn("depts.txt", "症状词不在 symptoms.txt 中：", item);
        }
        if (!comma) break;
        item = comma + 1;
      }

      const int idx = h.depts.push(d);
      if (idx < 0) break;
      h.deptIndex.put(h.depts[idx].code, idx);
    }
    closeFile();
    return true;
  }

  // ------------------------------ 医生表 ------------------------------
  // 行格式：编号|姓名|科室编号|职称|擅长领域|房间编号
  static bool loadDoctors() {
    Hospital& h = hospital();
    if (!openFile("doctors.txt")) return false;
    char line[512];
    while (nextLine(line, sizeof(line))) {
      if (!ensureRoom("doctors.txt", "医生表", !h.doctors.full())) break;
      char fields[6][64];
      if (split(line, fields, 6) < 6) {
        warn("doctors.txt", "字段不足：", line);
        continue;
      }
      Doctor d;
      std::snprintf(d.code, sizeof(d.code), "%s", fields[0]);
      std::snprintf(d.name, sizeof(d.name), "%s", fields[1]);
      std::snprintf(d.title, sizeof(d.title), "%s", fields[3]);
      std::snprintf(d.skill, sizeof(d.skill), "%s", fields[4]);
      d.dept = h.findDept(fields[2]);  // 外键：科室编号
      d.room = -1;                     // 房间号要等 layout 装载完才能查，先记下编号
      d.rankSum = 0;
      d.rankCount = 0;
      if (d.dept < 0) {
        warn("doctors.txt", "科室编号不存在：", fields[2]);
        continue;
      }
      // 房间编号暂存在 pendingRoom_ 里，与医生下标一一对应
      const int idx = h.doctors.push(d);
      if (idx < 0) break;
      std::snprintf(pendingRoom_[idx], sizeof(pendingRoom_[0]), "%s", fields[5]);
      h.doctorIndex.put(h.doctors[idx].code, idx);
    }
    closeFile();
    return true;
  }

  // ------------------------------ 排班表 ------------------------------
  // 行格式：医生编号|日期(0..6)|时段(0上午 1下午)|最大接诊数
  // 排班槽在总库里是三维数组，下标就是主键，装载时直接往对应的格子里填。
  static bool loadSchedules() {
    Hospital& h = hospital();
    if (!openFile("schedules.txt")) return false;
    char line[256];
    while (nextLine(line, sizeof(line))) {
      char fields[4][64];
      if (split(line, fields, 4) < 4) {
        warn("schedules.txt", "字段不足：", line);
        continue;
      }
      const int doctor = h.findDoctor(fields[0]);
      const int day = std::atoi(fields[1]);
      const int slot = std::atoi(fields[2]);
      const int quota = std::atoi(fields[3]);
      if (doctor < 0 || day < 0 || day >= kMaxDay || slot < 0 || slot >= kMaxSlot) {
        warn("schedules.txt", "医生或日期时段非法：", line);
        continue;
      }
      if (h.slot(doctor, day, slot).assigned) {
        warn("schedules.txt", "同一医生同一时段重复排班：", line);
        continue;
      }
      Schedule& sc = h.slot(doctor, day, slot);
      sc.quota = quota;
      sc.booked = 0;
      sc.version = ++h.versionClock;
      sc.assigned = true;
      sc.stopped = false;
    }
    closeFile();
    return true;
  }

  // ------------------------------ 患者表 ------------------------------
  static bool loadPatients() {
    Hospital& h = hospital();
    if (!openFile("patients.txt")) return false;
    char line[256];
    while (nextLine(line, sizeof(line))) {
      if (!ensureRoom("patients.txt", "患者表", !h.patients.full())) break;
      char fields[3][64];
      if (split(line, fields, 3) < 3) {
        warn("patients.txt", "字段不足：", line);
        continue;
      }
      Patient p;
      std::snprintf(p.code, sizeof(p.code), "%s", fields[0]);
      std::snprintf(p.name, sizeof(p.name), "%s", fields[1]);
      std::snprintf(p.phone, sizeof(p.phone), "%s", fields[2]);
      const int idx = h.patients.push(p);
      if (idx < 0) break;
      h.patientIndex.put(h.patients[idx].code, idx);
    }
    closeFile();
    return true;
  }

  // --------------------------- 房间与就诊路网 ---------------------------
  // 行格式：房间号|名称|楼层|科室编号(-为公共设施)|路网节点号|相连节点:距离(逗号分隔)
  // 例如：RA101|门诊楼1层心血管内科诊室|0|K01|1|0:10,2:10
  //
  // 要读两遍，顺序不能颠倒：
  //   第一遍读入全部房间，才能知道路网一共有多少个节点；
  //   第二遍才能按「相连节点」建边——节点总数没定下来之前，边无处可放。
  static bool loadLayout() {
    Hospital& h = hospital();

    // ---- 第一遍：房间 ----
    if (!openFile("layout.txt")) return false;
    int maxNode = 0;
    char line[1024];
    while (nextLine(line, sizeof(line))) {
      if (!ensureRoom("layout.txt", "房间表", !h.rooms.full())) break;
      char fields[6][64];
      if (split(line, fields, 6) < 6) {
        warn("layout.txt", "字段不足：", line);
        continue;
      }
      Room r;
      std::snprintf(r.code, sizeof(r.code), "%s", fields[0]);
      std::snprintf(r.name, sizeof(r.name), "%s", fields[1]);
      r.floor = std::atoi(fields[2]);
      // 科室字段：'-' 表示公共设施（电梯口、楼梯口），是合法的；
      // 写了别的编号却查不到，说明数据错了，不能当成「无科室」混过去 ——
      // 否则这个房间就查不出属于哪个科室，导诊推荐出科室后也找不到它的房间。
      if (fields[3][0] == '-') {
        r.dept = -1;
      } else {
        r.dept = h.findDept(fields[3]);
        if (r.dept < 0) {
          warn("layout.txt", "科室编号不存在：", fields[3]);
          continue;
        }
      }
      r.node = std::atoi(fields[4]);
      // 节点号会直接用来索引邻接矩阵，必须是非负且在上限内的。
      // 这里挡一道：否则一个负数会让 maxNode 算错，路网被整体清空，
      // 表现为「所有路线都查不到」，很难想到是数据里一个负号引起的。
      if (r.node < 0 || r.node >= RoadNet::kMaxNodes || r.floor < 0) {
        warn("layout.txt", "节点号或楼层非法：", line);
        continue;
      }
      if (r.node + 1 > maxNode) maxNode = r.node + 1;
      const int idx = h.rooms.push(r);
      if (idx < 0) break;
      h.roomIndex.put(h.rooms[idx].code, idx);
    }
    closeFile();

    // 节点总数确定后初始化路网（内部把全部距离置为不可达）
    h.road.init(maxNode);

    // ---- 第二遍：把边挂上 ----
    if (!openFile("layout.txt")) return false;
    while (nextLine(line, sizeof(line))) {
      char fields[6][64];
      if (split(line, fields, 6) < 6) continue;
      const int u = std::atoi(fields[4]);

      // 字段 5 形如 "0:10,2:10,11:20"，逐项解析出「相邻节点:距离」
      char* item = fields[5];
      while (*item) {
        char* comma = std::strchr(item, ',');
        if (comma) *comma = '\0';
        char* colon = std::strchr(item, ':');
        int weight = 10;
        if (colon) {
          *colon = '\0';
          weight = std::atoi(colon + 1);
        }
        const int v = std::atoi(item);
        h.road.addEdge(u, v, weight);  // 无向边，两端都登记
        if (!comma) break;
        item = comma + 1;
      }
    }
    closeFile();

    // ---- 把医生与房间挂起来：医生表里存的是房间编号，现在才能翻译成下标 ----
    for (int i = 0; i < h.doctors.size(); ++i) {
      const int room = h.findRoom(pendingRoom_[i]);
      h.doctors[i].room = room;
      if (room < 0) warn("doctors.txt", "房间编号在 layout.txt 中不存在：", pendingRoom_[i]);
    }
    return true;
  }

  // 医生表里暂存的房间编号（与医生下标一一对应），layout 装载完再翻译
  inline static char pendingRoom_[kMaxDoctor][10];
};
