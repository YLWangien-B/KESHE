/* =============================================================================
 *  tools/gen_data.cpp  —  测试数据生成器
 *
 *  程序只负责读数据，测试数据全部由本工具生成，因此数据规模、科室与症状的
 *  关联关系、就诊楼的路线拓扑都在这里一处可查、可重现。
 *
 *  生成物（写入 data/ 目录）：
 *    symptoms.txt   25 个症状词（导诊时按序号选择）
 *    depts.txt      12 个科室，每个科室带若干关联症状及权重（导诊用）
 *    doctors.txt    36 名医生（12 科室 × 3 人）
 *    schedules.txt  一周排班（36 名医生 × 7 天 × 2 个时段）
 *    patients.txt   12 名示例患者
 *    layout.txt     2 栋楼 × 2 层 × 9 个房间的路网
 *
 *  用法：gen_data.exe <输出目录>
 * ========================================================================== */
#include <cstdio>
#include <cstdlib>

// ------------------------------- 症状表 -------------------------------
// 每个症状带几个同义词（患者的口语说法）。导诊时患者在描述里写的往往是
// 这些口语词，系统要能认出来并归结到同一个症状顶点上。
struct SymptomSpec {
  const char* name;   // 规范词，也就是二部图里症状侧的顶点名
  const char* alias;  // 同义词，逗号分隔，可为空
};

const SymptomSpec kSymptoms[] = {
    {"发热", "发烧,体温高"},
    {"咳嗽", "干咳,老咳嗽"},
    {"咳痰", "痰多"},
    {"胸闷", "胸口发闷,胸口闷,胸部发闷"},
    {"心悸", "心慌,心跳快"},
    {"气短", "喘不上气,气促"},
    {"头晕", "晕,天旋地转"},
    {"头痛", "头疼"},
    {"腹痛", "肚子疼,肚子痛"},
    {"腹泻", "拉肚子"},
    {"呕吐", "想吐,吐了"},
    {"反酸", "烧心,胃酸"},
    {"腰痛", "腰疼"},
    {"关节痛", "关节疼,膝盖疼,膝盖痛,腿疼"},
    {"皮疹", "起疹子,红疹"},
    {"瘙痒", "痒,皮肤痒"},
    {"尿频", "总想小便,老想上厕所"},
    {"尿痛", "小便疼"},
    {"耳鸣", "耳朵响,耳朵嗡嗡"},
    {"鼻塞", "鼻子不通,流鼻涕,鼻塞流涕"},
    {"咽痛", "嗓子疼,喉咙痛"},
    {"视力下降", "看不清,眼花"},
    {"眼红", "眼睛红,眼睛痛,眼痛"},
    {"乏力", "没劲,浑身没劲,没力气"},
    {"失眠", "睡不着,睡眠不好"},
};
constexpr int kSymptomCount = sizeof(kSymptoms) / sizeof(kSymptoms[0]);

// ------------------------------- 科室表 -------------------------------
// 每个科室一句话描述，作为「患者症状与科室描述匹配」的文本依据。
// 描述里同时写出规范症状词和患者常用的口语说法：导诊时患者说的是口语词，
// 而某个科室的描述里也写到了这个词，就说明它确实在这个科室的主诉范围内，
// 按原权重计分；描述里没写的，说明只是沾边，要打折。
// 关联症状的权重取 1..3：权重越大，越说明该症状指向这个科室。
struct DeptSpec {
  const char* code;
  const char* name;
  const char* location;
  const char* description;
  const char* symptoms;  // 形如 "胸闷:3,心悸:3,气短:2"
};

const DeptSpec kDepts[] = {
    {"K01", "心血管内科", "门诊楼1层东侧", "诊治胸闷、胸口发闷、心悸、心慌、气短、高血压、冠心病等心血管疾病", "胸闷:3,心悸:3,气短:2"},
    {"K02", "呼吸内科", "门诊楼1层西侧", "诊治咳嗽、干咳、咳痰、痰多、气短、哮喘、肺部感染等呼吸系统疾病", "咳嗽:3,咳痰:3,气短:2"},
    {"K03", "消化内科", "门诊楼1层北侧", "诊治腹痛、肚子疼、腹泻、拉肚子、呕吐、反酸、烧心等消化系统疾病", "腹痛:3,腹泻:3,呕吐:2,反酸:2"},
    {"K04", "神经内科", "门诊楼2层东侧", "诊治头晕、头痛、头疼、失眠、睡不着、脑血管疾病等神经系统疾病", "头晕:3,头痛:3,失眠:2"},
    {"K05", "普通外科", "门诊楼2层西侧", "诊治腹痛、肚子疼、乏力、没劲、甲状腺与乳腺疾病等需要手术的疾病", "腹痛:2,乏力:1"},
    {"K06", "骨科", "门诊楼2层北侧", "诊治腰痛、腰疼、关节痛、关节疼、膝盖疼、骨折、颈肩痛等骨与关节疾病", "腰痛:3,关节痛:3"},
    {"K07", "泌尿外科", "内科楼1层东侧", "诊治尿频、总想小便、尿痛、结石、前列腺疾病等泌尿系统疾病", "尿频:3,尿痛:3"},
    {"K08", "皮肤科", "内科楼1层西侧", "诊治皮疹、起疹子、瘙痒、痒、湿疹、痤疮等皮肤疾病", "皮疹:3,瘙痒:3"},
    {"K09", "眼科", "内科楼1层北侧", "诊治视力下降、看不清、眼红、眼睛红、眼痛、流泪等眼部疾病", "视力下降:3,眼红:3"},
    {"K10", "耳鼻喉科", "内科楼2层东侧", "诊治耳鸣、耳朵响、鼻塞、鼻子不通、流鼻涕、咽痛、嗓子疼等耳鼻喉疾病", "耳鸣:3,鼻塞:2,咽痛:2"},
    {"K11", "儿科", "内科楼2层西侧", "诊治儿童发热、发烧、咳嗽、腹泻、拉肚子等儿童常见病", "发热:3,咳嗽:2,腹泻:2"},
    {"K12", "中医科", "内科楼2层北侧", "以中医辨证论治调理乏力、没劲、浑身没劲、失眠、睡不着等体质问题", "乏力:3,失眠:2"},
};
constexpr int kDeptCount = sizeof(kDepts) / sizeof(kDepts[0]);

// ------------------------------- 就诊楼拓扑 -------------------------------
// 简化模型：2 栋楼 × 2 层 = 4 个楼层，每层 10 个房间。楼层号 0..3 依次是
// 门诊楼1层、门诊楼2层、内科楼1层、内科楼2层。
//   房间 0       电梯口   —— 同一部电梯在各层的出入口互相连通
//   房间 1..6    六个诊室 —— 沿走廊一字排开，相邻诊室相连
//   房间 7       楼梯口   —— 同一道楼梯在相邻层的出入口相连
//   房间 8..11   四个预留诊室
// 每层的路网节点号 = 楼层号 * 20 + 房间号，节点号与「楼层-房间」一一对应。
constexpr int kRoomsPerFloor = 12;
constexpr int kFloorCount = 4;
constexpr int kNodePerFloor = 20;  // 每层占 20 个节点号，留出余量

// 科室在楼层上的分布：每个科室占同一层的连续 3 个诊室，对应 3 名医生。
// 这样一个科室集中在一处，「导诊推荐科室 -> 查它的房间 -> 查路线」才连得起来。
struct DeptPlacement {
  int dept;       // 科室下标
  int floor;      // 楼层号 0..3
  int firstRoom;  // 本层起始诊室号（取 1 / 4 / 7 三组）
};

const DeptPlacement kPlacements[] = {
    {0, 0, 1},  {1, 0, 5},  {2, 0, 9},   // 门诊楼1层：心血管内科 / 呼吸内科 / 消化内科
    {3, 1, 1},  {4, 1, 5},  {5, 1, 9},   // 门诊楼2层：神经内科 / 普通外科 / 骨科
    {6, 2, 1},  {7, 2, 5},  {8, 2, 9},   // 内科楼1层：泌尿外科 / 皮肤科 / 眼科
    {9, 3, 1},  {10, 3, 5}, {11, 3, 9},  // 内科楼2层：耳鼻喉科 / 儿科 / 中医科
};
constexpr int kPlacementCount = sizeof(kPlacements) / sizeof(kPlacements[0]);

const char* kFloorName[] = {"门诊楼1层", "门诊楼2层", "内科楼1层", "内科楼2层"};
const char* kFloorLetter[] = {"A", "A", "B", "B"};
const int kFloorLevel[] = {1, 2, 1, 2};

const char* kDoctorTitles[] = {"主任医师", "副主任医师", "主治医师"};
const char* kSurnames[] = {"王", "李", "张", "刘", "陈", "杨", "赵", "黄", "周", "吴", "徐", "孙"};
const char* kGivens[] = {"建国", "淑华", "志强", "秀英", "海涛", "丽娟", "文博", "玉兰", "思远", "晓峰", "雅琴", "立新"};
const char* kSkills[] = {"冠心病与高血压",   "咳嗽与哮喘",   "胃肠疾病",     "脑血管与眩晕", "腹腔与甲状腺手术", "骨折与关节",
                         "结石与前列腺疾病", "湿疹与痤疮",   "白内障与眼底病", "鼻炎与听力障碍", "儿童常见病",       "中医体质调理"};

// ------------------------------- 辅助函数 -------------------------------
std::FILE* gOut = nullptr;

void openFile(const char* dir, const char* name) {
  char path[512];
  std::snprintf(path, sizeof(path), "%s/%s", dir, name);
  gOut = std::fopen(path, "wb");
  if (!gOut) {
    std::printf("[错误] 无法写入 %s\n", path);
    std::exit(1);
  }
}

void closeFile() {
  std::fclose(gOut);
  gOut = nullptr;
}

// 楼层号 + 房间号 -> 该房间的路网节点号
int nodeOf(int floor, int room) { return floor * kNodePerFloor + room; }

// 查某个 (楼层, 房间号) 属于哪个科室；属于公共设施则返回 -1
int deptOfRoom(int floor, int room) {
  for (int i = 0; i < kPlacementCount; ++i) {
    const DeptPlacement& p = kPlacements[i];
    if (p.floor == floor && room >= p.firstRoom && room < p.firstRoom + 3) return p.dept;
  }
  return -1;
}

// --------------------------------- 各文件生成 ---------------------------------
void genSymptoms(const char* dir) {
  openFile(dir, "symptoms.txt");
  std::fprintf(gOut, "# 症状顶点：规范词|同义词(逗号分隔，可空)\n");
  std::fprintf(gOut, "# 规范词是「症状—科室」加权二部图里症状侧的顶点名\n");
  std::fprintf(gOut, "# 同义词是患者的口语说法，导诊时一并识别\n");
  for (int i = 0; i < kSymptomCount; ++i)
    std::fprintf(gOut, "%s|%s\n", kSymptoms[i].name, kSymptoms[i].alias);
  closeFile();
}

void genDepts(const char* dir) {
  openFile(dir, "depts.txt");
  std::fprintf(gOut, "# 科室：编号|名称|位置|科室描述|关联症状(症状名:权重, 逗号分隔)\n");
  std::fprintf(gOut, "# 「关联症状:权重」就是「症状—科室」这张加权二部图的边表，权重 1..3\n");
  std::fprintf(gOut, "# 科室描述里写出常见症状词，导诊时患者的描述就是与它做匹配\n");
  for (int i = 0; i < kDeptCount; ++i)
    std::fprintf(gOut, "%s|%s|%s|%s|%s\n", kDepts[i].code, kDepts[i].name, kDepts[i].location, kDepts[i].description,
                 kDepts[i].symptoms);
  closeFile();
}

void genDoctors(const char* dir) {
  openFile(dir, "doctors.txt");
  std::fprintf(gOut, "# 医生：编号|姓名|科室编号|职称|擅长领域|房间编号\n");
  int no = 0;
  for (int p = 0; p < kPlacementCount; ++p) {
    const DeptPlacement& pl = kPlacements[p];
    for (int k = 0; k < 3; ++k) {  // 每个科室 3 名医生
      const int room = pl.firstRoom + k;
      char code[8], roomCode[10], name[16];
      std::snprintf(code, sizeof(code), "D%d", no + 1);
      std::snprintf(roomCode, sizeof(roomCode), "R%s%d%02d", kFloorLetter[pl.floor], kFloorLevel[pl.floor], room);
      std::snprintf(name, sizeof(name), "%s%s", kSurnames[no % 12], kGivens[(no * 5 + 3) % 12]);
      std::fprintf(gOut, "%s|%s|%s|%s|%s|%s\n", code, name, kDepts[pl.dept].code, kDoctorTitles[(k + p) % 3], kSkills[pl.dept],
                   roomCode);
      ++no;
    }
  }
  closeFile();
}

void genSchedules(const char* dir) {
  openFile(dir, "schedules.txt");
  std::fprintf(gOut, "# 排班：医生编号|日期(0..6)|时段(0上午 1下午)|最大接诊数\n");
// 排班规则：每名医生隔天出诊一次，上午下午交替，这样
  //   · 任意一天的上午、下午都有医生在出诊；
  //   · 任意一名医生一周都有若干时段可约；
  //   · 号源上限按医生序号错开，便于演示「某时段已约满」。
  for (int d = 0; d < kDeptCount * 3; ++d) {
    const int quotaAm = 15 - (d % 3) * 3;
    const int quotaPm = 10 - (d % 3) * 2;
    for (int day = 0; day < 7; ++day) {
      if ((day + d) % 2 == 0)
        std::fprintf(gOut, "D%d|%d|0|%d\n", d + 1, day, quotaAm);  // 隔天上午
      else
        std::fprintf(gOut, "D%d|%d|1|%d\n", d + 1, day, quotaPm);  // 另一天下午
    }
  }
  closeFile();
}

void genPatients(const char* dir) {
  openFile(dir, "patients.txt");
  std::fprintf(gOut, "# 患者：编号|姓名|联系电话\n");
  const char* names[] = {"张伟", "王芳", "李娜", "刘洋", "陈静", "杨帆", "赵磊", "黄敏", "周涛", "吴倩", "徐鹏", "孙丽"};
  for (int i = 0; i < 12; ++i) std::fprintf(gOut, "P%d|%s|138%08d\n", i + 1, names[i], 10000000 + i * 137);
  closeFile();
}

void genLayout(const char* dir) {
  openFile(dir, "layout.txt");
  std::fprintf(gOut, "# 就诊路线数据：每行一个房间\n");
  std::fprintf(gOut, "# 房间号|名称|楼层|科室编号(-为公共设施)|路网节点号|相连节点:距离(逗号分隔)\n");
  std::fprintf(gOut, "# 每层节点号 = 楼层号 * 10 + 房间号；房间 0 是电梯口，房间 8 是楼梯口\n");
  std::fprintf(gOut, "# 距离单位为米：同层相邻房间 10 米，跨层 20 米（含上下楼）\n");

  for (int fl = 0; fl < kFloorCount; ++fl) {
    for (int r = 0; r < kRoomsPerFloor; ++r) {
      char code[10], name[64], deptCode[8];  // 名称最长约 34 字节，留足余量避免截断汉字
      const int dept = deptOfRoom(fl, r);
      if (dept >= 0)
        std::snprintf(deptCode, sizeof(deptCode), "%s", kDepts[dept].code);
      else
        std::snprintf(deptCode, sizeof(deptCode), "-");

      if (r == 0) {
        std::snprintf(code, sizeof(code), "R%s%d00", kFloorLetter[fl], kFloorLevel[fl]);
        std::snprintf(name, sizeof(name), "%s电梯口", kFloorName[fl]);
      } else if (r == 7) {
        std::snprintf(code, sizeof(code), "R%s%d07", kFloorLetter[fl], kFloorLevel[fl]);
        std::snprintf(name, sizeof(name), "%s楼梯口", kFloorName[fl]);
      } else {
        std::snprintf(code, sizeof(code), "R%s%d%02d", kFloorLetter[fl], kFloorLevel[fl], r);
        if (dept >= 0)
          std::snprintf(name, sizeof(name), "%s%s诊室", kFloorName[fl], kDepts[dept].name);
        else
          std::snprintf(name, sizeof(name), "%s%d诊室(备用)", kFloorName[fl], r);
      }

      // 汇总本房间直接相连的节点与距离
      int nb[8];
      int w[8];
      int nbCount = 0;
      if (r == 0) {
        // 电梯口：接走廊两端，并与其它各层的电梯口相连
        nb[nbCount] = nodeOf(fl, 1);  w[nbCount++] = 10;
        nb[nbCount] = nodeOf(fl, 7);  w[nbCount++] = 30;
        for (int other = 0; other < kFloorCount; ++other)
          if (other != fl) { nb[nbCount] = nodeOf(other, 0); w[nbCount++] = 20; }
      } else if (r == 7) {
        // 楼梯口：接走廊另一端，并与相邻层的楼梯口相连
        nb[nbCount] = nodeOf(fl, 6);  w[nbCount++] = 10;
        nb[nbCount] = nodeOf(fl, 0);  w[nbCount++] = 30;
        if (fl > 0) { nb[nbCount] = nodeOf(fl - 1, 7); w[nbCount++] = 20; }
        if (fl + 1 < kFloorCount) { nb[nbCount] = nodeOf(fl + 1, 7); w[nbCount++] = 20; }
      } else {
        // 诊室：接左右相邻诊室；走廊两端分别接电梯口与楼梯口
        if (r > 1) { nb[nbCount] = nodeOf(fl, r - 1); w[nbCount++] = 10; }
        else { nb[nbCount] = nodeOf(fl, 0); w[nbCount++] = 10; }
        if (r < 6) { nb[nbCount] = nodeOf(fl, r + 1); w[nbCount++] = 10; }
        else { nb[nbCount] = nodeOf(fl, 7); w[nbCount++] = 10; }
      }

      std::fprintf(gOut, "%s|%s|%d|%s|%d|", code, name, fl, deptCode, nodeOf(fl, r));
      for (int i = 0; i < nbCount; ++i) std::fprintf(gOut, "%s%d:%d", i ? "," : "", nb[i], w[i]);
      std::fprintf(gOut, "\n");
    }
  }
  closeFile();
}

// --------------------------------- 主函数 ---------------------------------
int main(int argc, char** argv) {
  const char* dir = (argc > 1) ? argv[1] : "data";
  std::printf("生成测试数据到目录：%s\n", dir);

  genSymptoms(dir);
  genDepts(dir);
  genDoctors(dir);
  genSchedules(dir);
  genPatients(dir);
  genLayout(dir);

  std::printf("完成：症状 %d 个 / 科室 %d 个 / 医生 %d 名 / 楼层 %d 层 / 房间 %d 间\n", kSymptomCount, kDeptCount,
              kDeptCount * 3, kFloorCount, kFloorCount * kRoomsPerFloor);
  return 0;
}
