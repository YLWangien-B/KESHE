/* =============================================================================
 *  gui_font.cpp  —  中文字体装载
 *
 *  两件事：
 *    1) 按中文系统的常见顺序找一个字体文件（微软雅黑 -> 黑体 -> 宋体 -> 等线）；
 *    2) 把字体装进 ImGui 的字形图集。
 *
 *  ---------------------------------------------------------------------------
 *  字形范围为什么传 NULL（也就是「整个字体文件」）
 *
 *  最初的做法是「按需取字」：把数据文件里读到的中文，加上界面里手写的一份
 *  文案清单，塞进 ImFontGlyphRangesBuilder 生成一个刚好够用的范围，想法是省空间。
 *  实际却埋了个坑 —— 那份界面文案清单是手工维护的，**每加一句界面文字都可能漏字**，
 *  漏掉的字会显示成方块，而编译期完全看不出来。
 *  实测下来光界面文案就漏了 190 多个字。
 *
 *  现在改成给 AddFontFromFileTTF 传 NULL：ImGui 1.92 的字形图集是
 *  **按需光栅化、动态增长**的（后端声明了 ImGuiBackendFlags_RendererHasTextures，
 *  DX11 后端也实现了纹理更新），所以传 NULL 不会把两万多个汉字全部烘进贴图，
 *  只是「字体里有的字都能用」，用到哪个烘哪个。
 *
 *  结论：不用再维护任何清单，也不会再缺字。代价只是首次用到某字时多一次光栅化
 *  （微秒级），对本地 GUI 完全无感。
 *  ---------------------------------------------------------------------------
 * ========================================================================== */
#include "gui_font.h"

#include <cstdio>

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

}  // namespace

void loadChineseFont(ImGuiIO& io, float scale) {
  const char* fontPath = findChineseFont();
  if (!fontPath) {
    std::printf("[GUI] 警告：没有找到中文字体（试过 msyh/simhei/simsun/Deng），\n");
    std::printf("      界面上的汉字会显示为方块。请把一份中文字体放到 C:\\Windows\\Fonts 下。\n");
    io.Fonts->AddFontDefault();
    return;
  }

  ImFontConfig cfg;
  cfg.OversampleH = 2;
  cfg.OversampleV = 1;
  cfg.PixelSnapH = true;

  // 界面基准字号 18px，再按 DPI 缩放
  const float sizePixels = 18.0f * scale;

  // 第四个参数传 nullptr：字形范围 = 整个字体文件。
  // 字形图集按需动态增长，所以不会真的把所有汉字都烘进去，见文件头说明。
  ImFont* font = io.Fonts->AddFontFromFileTTF(fontPath, sizePixels, &cfg, nullptr);
  if (!font) {
    std::printf("[GUI] 警告：字体 %s 装载失败，退回内置字体（汉字会显示为方块）\n", fontPath);
    io.Fonts->AddFontDefault();
    return;
  }
  io.FontDefault = font;

  std::printf("[GUI] 中文字体：%s，字号 %.0fpx，字形范围 = 整个字体文件（按需光栅化）\n",
              fontPath, sizePixels);
}

}  // namespace gui
