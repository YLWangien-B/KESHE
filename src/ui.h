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

// ---------------------------- 整数解析 ----------------------------
// 不用 atoi / sscanf("%d")，因为它们对超范围的输入行为未定义：
// 一串很长的数字会溢出成任意值，后面的范围检查就形同虚设。
// 这里逐位检查，一旦超出 [lo, hi] 立刻停下并返回 false，
// 因此「日期填 1-7」这类提示能真正挡住 8、0 和一大串数字。
inline bool parseIntInRange(const char* s, int lo, int hi, int& out) {
  const char* p = s;
  while (*p == ' ' || *p == '\t') ++p;

  bool negative = false;
  if (*p == '-' || *p == '+') {
    negative = (*p == '-');
    ++p;
  }
  if (*p < '0' || *p > '9') return false;  // 没读到数字

  int value = 0;
  for (; *p >= '0' && *p <= '9'; ++p) {
    value = value * 10 + (*p - '0');
    if (negative) {
      if (-value < lo) return false;  // 负数溢出，或已经小于下限
    } else if (value > hi) {
      return false;  // 已经超过上限，再算下去会溢出
    }
  }
  // 数字后面只允许有空白，避免 "1abc" 这种被当成 1
  while (*p == ' ' || *p == '\t') ++p;
  if (*p != '\0') return false;

  out = negative ? -value : value;
  return out >= lo && out <= hi;
}

// 读取一个整数，要求落在 [lo, hi] 内；不满足返回 false
inline bool readIntInRange(int lo, int hi, int& out) {
  char buf[64];
  if (!readLine(buf, sizeof(buf))) return false;
  return parseIntInRange(buf, lo, hi, out);
}

// 从一行里解析两个整数，各自要求落在给定范围内（用于「日期 时段」这类输入）
inline bool parseTwoIntsInRange(const char* s, int lo1, int hi1, int& a, int lo2, int hi2, int& b) {
  char first[32];
  int n = 0;
  const char* p = s;
  while (*p == ' ' || *p == '\t') ++p;
  while (*p && *p != ' ' && *p != '\t' && n < 31) first[n++] = *p++;
  first[n] = '\0';
  if (n == 0) return false;
  if (!parseIntInRange(first, lo1, hi1, a)) return false;
  return parseIntInRange(p, lo2, hi2, b);
}

// 等待用户按键，避免结果一闪而过
inline void pause() {
  std::printf("\n按回车键继续...");
  char buf[8];
  readLine(buf, sizeof(buf));
}
