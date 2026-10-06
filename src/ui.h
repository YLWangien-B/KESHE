/* =============================================================================
 *  ui.h  —  界面辅助
 *
 *  菜单、输入、表格对齐这些与业务无关的杂事集中放这里，让 main.cpp 只关心
 *  「选哪个功能、显示什么结果」。以后换成图形界面时，替换的就是这一层。
 * ========================================================================== */
#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>

// 终端里一个汉字占两格、一个 ASCII 字符占一格，而 printf 的 %-12s 是按字节补齐的，
// 中文列必然参差不齐。这里先算出显示宽度，再手工补空格，中英混排才能对齐。
inline int width(const char* s) {
  int w = 0;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p;) {
    if (*p < 0x80) {  // ASCII：单字节，占一格
      ++w;
      ++p;
    } else if ((*p & 0xE0) == 0xC0) {  // 两字节的 UTF-8 字符
      w += 2;
      p += 2;
    } else if ((*p & 0xF0) == 0xE0) {  // 汉字在这里：三字节，占两格
      w += 2;
      p += 3;
    } else {  // 四字节字符
      w += 2;
      p += 4;
    }
  }
  return w;
}

// 左对齐输出一个定宽单元格（内容 + 补空格 + 一个分隔空格）
inline void cell(const char* s, int padTo) {
  std::printf("%s", s);
  for (int i = width(s); i < padTo; ++i) std::printf(" ");
  std::printf(" ");
}

// 读取一行输入；返回 false 表示输入结束
inline bool readLine(char* buf, int cap) {
  if (!std::fgets(buf, cap, stdin)) return false;
  int n = static_cast<int>(std::strlen(buf));
  while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = '\0';
  return true;
}

// 读取一个整数；输入结束返回 false
inline bool readInt(int& out) {
  char buf[64];
  if (!readLine(buf, sizeof(buf))) return false;
  out = std::atoi(buf);
  return true;
}

// 等待用户按键，避免结果一闪而过
inline void pause() {
  std::printf("\n按回车键继续...");
  char buf[8];
  readLine(buf, sizeof(buf));
}
