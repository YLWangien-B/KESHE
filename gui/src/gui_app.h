/* =============================================================================
 *  gui_app.h  —  GUI 程序的骨架（窗口 + D3D11 + ImGui 初始化）
 *
 *  这个文件只负责「把一块画布准备好」：创建窗口、初始化 D3D11 交换链、
 *  初始化 ImGui 的两个后端（Win32 平台 + DX11 渲染器）、装中文字体。
 *  七项功能怎么画，见 panels/ 目录。
 * ========================================================================== */
#pragma once

namespace gui {

// 创建窗口与 D3D11 设备、初始化 ImGui。成功返回 true。
bool init(const wchar_t* title, int width, int height);

// 主循环：每帧调用一次 renderFrame()（由 gui_main.cpp 提供），直到用户关窗
void run();

// 释放全部资源
void shutdown();

// 把窗口标题改成「标题 —— 附加信息」，用来在标题栏显示当前装载状态
void setTitleSuffix(const char* suffix);

// 主显示器工作区的「逻辑尺寸」= 物理像素 ÷ DPI 缩放。
// renderFrame() 用它把界面尺寸夹在屏幕内 —— 否则在 125%/150% 缩放的笔记本上，
// 窗口会有一部分跑到屏幕外面去，右边被裁掉。
void getLogicalWorkArea(float* outW, float* outH);

}  // namespace gui
