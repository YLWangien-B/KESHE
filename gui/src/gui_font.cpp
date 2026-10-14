/* =============================================================================
 *  gui_font.cpp  —  中文字体装载
 *
 *  一共做三件事：
 *    1) 按中文系统的常见顺序找一个字体文件（微软雅黑 -> 黑体 -> 宋体 -> 等线）；
 *    2) 收集「本程序真正会显示的全部汉字」当作字形范围；
 *    3) 把字体装进 ImGui 的字形贴图。
 *
 *  第 2 步是这里最讲究的地方：
 *    · 如果只给 io.Fonts->GetGlyphRangesChineseSimplifiedCommon()，那是「按字频取的前
 *      2500 个常用字」，人名、少见的症状词有概率落在范围外，显示成空白；
 *    · 如果直接给 GetGlyphRangesChineseFull()（约 2 万个字形），一定不会缺字，
 *      但字形贴图会大很多、启动也慢一些；
 *    · 这里的做法是「按需取字」：把数据文件里读进来的科室名、症状名、医生姓名、
 *      患者姓名、房间名，加上界面自己的固定文案，全部塞进一个 ImFontGlyphRangesBuilder，
 *      最后生成一个刚好够用的范围。用到的字一个不少，用不到的一个不装。
 * ========================================================================== */
#include "gui_font.h"

#include <cstdio>
#include <cstring>

#include "model.h"

namespace gui {

namespace {

// 找一个可用的中文字体文件。按「字形完整度 + 常见度」排的优先级。
const char* findChineseFont() {
  static const char* kCandidates[] = {
      "C:/Windows/Fonts/msyh.ttc",    // 微软雅黑（Win7+ 简体中文默认界面字体）
      "C:/Windows/Fonts/msyh.ttf",    // 少数精简系统上是 ttf
      "C:/Windows/Fonts/simhei.ttf",  // 黑体
      "C:/Windows/Fonts/simsun.ttc",  // 宋体（老系统保底）
      "C:/Windows/Fonts/Deng.ttf",    // 等线（Win10+）
  };
  for (const char* p : kCandidates) {
    if (std::FILE* f = std::fopen(p, "rb")) {
      std::fclose(f);
      return p;
    }
  }
  return nullptr;
}

// 把界面里硬编码的中文文案也加进字形范围。
// 这些字不一定出现在数据文件里（例如「预约凭证」「候诊队列」），但一定会显示，
// 所以必须显式登记，否则会出现「数据里的字正常、菜单里的字却是方块」这种怪现象。
void addUiText(ImFontGlyphRangesBuilder& builder) {
  static const char* kUiTexts[] = {
      // 主菜单与页面标题
      "医院门诊预约与智能导诊系统",
      "智能导诊", "医生查询", "在线预约", "预约管理", "排班展示", "就诊路线", "候诊队列",
      "楼层平面图", "数据总览",
      // 通用
      "序号", "编号", "姓名", "科室", "医生", "职称", "擅长领域", "位置", "得分", "推荐依据",
      "状态", "操作", "时间", "日期", "时段", "上午", "下午", "余号", "总数", "剩余号源",
      "共", "个", "名", "间", "人", "前", "后", "第", "位", "条", "张", "次",
      "查询", "返回", "取消", "确认", "关闭", "刷新", "重置", "全部", "无", "没有",
      "不清楚", "请选择", "请输入", "提示", "错误", "成功", "失败", "注意",
      // 导诊
      "请选择您的症状（可多选）", "已选症状", "开始导诊", "清空选择", "推荐科室",
      "该科室主诉范围", "出诊医生", "匹配得分", "命中症状", "症状越多结果越准",
      "所选症状与任何科室都没有关联",
      // 医生查询
      "按科室查询", "按职称查询", "按擅长领域查询", "全部科室", "全部职称",
      "主任医师", "副主任医师", "主治医师", "住院医师", "关键字", "共找到",
      // 预约
      "选择患者", "选择医生", "选择日期与时段", "生成预约凭证", "凭证号", "就诊时间",
      "就诊地点", "该时段剩余号源", "该时段不出诊", "该时段已停诊", "该时段号源已满",
      "号源紧张，请尽快预约", "可以预约", "预约成功", "预约失败", "号源已退回",
      "已预约", "已取消", "已完成", "我的预约", "取消该预约", "没有预约记录",
      "请选择患者后查看预约记录", "预约记录",
      // 排班
      "按科室汇总", "按医生查看", "一周排班", "出诊时段数", "一周总号源", "休息",
      "不出诊", "号源日历", "点击格子可预约",
      // 路线
      "起点", "终点", "查询路线", "全程约", "米", "途经", "节点", "换层",
      "全程在同一层，无需上下楼", "两点之间不连通", "换层指引", "乘电梯", "走楼梯",
      "累计", "步行路线", "在平面图上高亮", "起点与终点是同一个房间",
      // 候诊队列
      "患者签到", "医生叫号", "过号处理", "就诊完成", "查看队列", "排队号", "诊室",
      "等待中", "已叫号", "请进诊室", "已过号", "重新排队", "前面还有", "候诊人数",
      "全院候诊情况", "出诊医生数", "当前没有患者候诊", "签到成功", "叫下一位",
      "队列已满", "已经在队列里", "候诊队列已满", "请稍后再签到",
      "队首的患者还没处理完", "不能越过他叫后面的人", "该医生没有需要叫号的候诊患者",
      // 楼层平面图
      "电梯口", "楼梯口", "诊室", "备用", "公共设施", "高亮路径", "显示房间号",
      "门诊楼1层", "门诊楼2层", "内科楼1层", "内科楼2层", "点击房间可设为起点或终点",
      // 数据总览
      "科室数", "症状数", "医生数", "患者数", "房间数", "排班槽数", "二部图关联数",
      "数据目录", "装载统计", "容量上限", "内存占用", "本程序的全部数据来自文本文件",
      // 单位与标点
      "：", "（", "）", "、", "，", "。", "／", "—", "?", "！",
  };
  for (const char* t : kUiTexts) builder.AddText(t);
}

}  // namespace

void loadChineseFont(ImGuiIO& io, float scale) {
  const char* fontPath = findChineseFont();
  if (!fontPath) {
    std::printf("[GUI] 警告：没有找到中文字体（试过 msyh/simhei/simsun/Deng），\n");
    std::printf("      界面上的汉字会显示为方块。请把一份中文字体放到 C:\\Windows\\Fonts 下。\n");
    io.Fonts->AddFontDefault();
    return;
  }

  // ---- 收集字形范围：数据里的全部中文 + 界面固定文案 ----
  ImFontGlyphRangesBuilder builder;
  const Hospital& h = db();
  for (int i = 0; i < h.depts.size(); ++i) {
    builder.AddText(h.depts[i].code);
    builder.AddText(h.depts[i].name);
    builder.AddText(h.depts[i].location);
    builder.AddText(h.depts[i].description);
  }
  for (int i = 0; i < h.symptomGraph.symptomCount(); ++i)
    builder.AddText(h.symptomGraph.symptomName(i));
  for (int i = 0; i < h.doctors.size(); ++i) {
    builder.AddText(h.doctors[i].code);
    builder.AddText(h.doctors[i].name);
    builder.AddText(h.doctors[i].title);
    builder.AddText(h.doctors[i].skill);
  }
  for (int i = 0; i < h.patients.size(); ++i) {
    builder.AddText(h.patients[i].code);
    builder.AddText(h.patients[i].name);
    builder.AddText(h.patients[i].phone);
  }
  for (int i = 0; i < h.rooms.size(); ++i) {
    builder.AddText(h.rooms[i].code);
    builder.AddText(h.rooms[i].name);
  }
  addUiText(builder);

  static ImVector<ImWchar> ranges;  // 必须活到字形贴图建好之后，所以用静态
  ranges.clear();
  builder.BuildRanges(&ranges);

  ImFontConfig cfg;
  cfg.OversampleH = 2;
  cfg.OversampleV = 1;
  cfg.PixelSnapH = true;

  // 界面基准字号 18px，再按 DPI 缩放；行高留一点余量便于排版
  const float sizePixels = 18.0f * scale;
  ImFont* font = io.Fonts->AddFontFromFileTTF(fontPath, sizePixels, &cfg, ranges.Data);
  if (!font) {
    std::printf("[GUI] 警告：字体 %s 装载失败，退回内置字体（汉字会显示为方块）\n", fontPath);
    io.Fonts->AddFontDefault();
    return;
  }
  io.FontDefault = font;

  std::printf("[GUI] 中文字体：%s，字号 %.0fpx，登记字形范围 %d 段\n", fontPath, sizePixels,
              (ranges.Size / 2) - 1);
}

}  // namespace gui
