/* =============================================================================
 *  gui_app.cpp  —  窗口 + D3D11 + ImGui 的初始化与主循环
 *
 *  骨架取自 Dear ImGui 官方示例 examples/example_win32_directx11/main.cpp，
 *  只做三处改动：
 *    1) 字体换成「微软雅黑」，否则中文全是方块；
 *    2) 主循环里不画示例窗口，改调 renderFrame()（七项功能的界面）；
 *    3) 关窗后把数据目录等状态打印到控制台，便于命令行排查。
 * ========================================================================== */
#include "gui_app.h"

#include <cstdio>
#include <cstring>
#include <cwchar>

// Windows / D3D11
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <tchar.h>

// Dear ImGui
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include "gui_font.h"

// -----------------------------------------------------------------------------
//  这两个声明必须放在 **gui 命名空间之外**：
//    renderFrame                 由 gui_main.cpp 定义（画七项功能的界面）
//    ImGui_ImplWin32_WndProcHandler  由 imgui_impl_win32.cpp 定义（后端自带的窗口消息处理）
//  放在命名空间里的话，编译器会认为它们有内部链接，链接时就找不到定义了。
// -----------------------------------------------------------------------------
void renderFrame();
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                             LPARAM lParam);

namespace gui {

namespace {

// ------------------------------ D3D11 全局状态 ------------------------------
ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain* g_swapChain = nullptr;
bool g_swapChainOccluded = false;
UINT g_resizeWidth = 0, g_resizeHeight = 0;
ID3D11RenderTargetView* g_renderTargetView = nullptr;

HWND g_hwnd = nullptr;
WNDCLASSEXW g_wc = {};
wchar_t g_title[256] = L"医院门诊预约与智能导诊系统";
float g_dpiScale = 1.0f;        // DPI 缩放系数，init() 里定
float g_logicalWorkW = 1280.0f; // 屏幕工作区的逻辑宽度，renderFrame 用它夹尺寸
float g_logicalWorkH = 800.0f;  // 同上，逻辑高度

// ------------------------------ D3D11 辅助函数 ------------------------------
void createRenderTarget() {
  ID3D11Texture2D* backBuffer = nullptr;
  g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
  if (backBuffer) {
    g_device->CreateRenderTargetView(backBuffer, nullptr, &g_renderTargetView);
    backBuffer->Release();
  }
}

void cleanupRenderTarget() {
  if (g_renderTargetView) {
    g_renderTargetView->Release();
    g_renderTargetView = nullptr;
  }
}

bool createDeviceD3D(HWND hwnd) {
  DXGI_SWAP_CHAIN_DESC sd;
  ZeroMemory(&sd, sizeof(sd));
  sd.BufferCount = 2;
  sd.BufferDesc.Width = 0;
  sd.BufferDesc.Height = 0;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferDesc.RefreshRate.Numerator = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hwnd;
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  const D3D_FEATURE_LEVEL levels[2] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
  D3D_FEATURE_LEVEL obtained = D3D_FEATURE_LEVEL_11_0;

  HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2,
                                             D3D11_SDK_VERSION, &sd, &g_swapChain, &g_device, &obtained,
                                             &g_context);
  if (hr == DXGI_ERROR_UNSUPPORTED) {
    // 没有独立显卡（远程桌面、纯虚拟机）时退回软件渲染 WARP，保证程序仍能跑起来
    hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2,
                                       D3D11_SDK_VERSION, &sd, &g_swapChain, &g_device, &obtained,
                                       &g_context);
  }
  if (hr != S_OK) {
    std::printf("[GUI] D3D11 设备创建失败，HRESULT=0x%08lX\n", static_cast<unsigned long>(hr));
    return false;
  }
  createRenderTarget();
  return true;
}

void cleanupDeviceD3D() {
  cleanupRenderTarget();
  if (g_swapChain) { g_swapChain->Release(); g_swapChain = nullptr; }
  if (g_context) { g_context->Release(); g_context = nullptr; }
  if (g_device) { g_device->Release(); g_device = nullptr; }
}

}  // namespace

// -----------------------------------------------------------------------------
//  窗口过程
// -----------------------------------------------------------------------------
//  窗口过程（唯一的实现，前面已经有 ImGui_ImplWin32_WndProcHandler 的声明）
// -----------------------------------------------------------------------------
namespace {

LRESULT WINAPI wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;

  switch (msg) {
    case WM_SIZE:
      if (wParam == SIZE_MINIMIZED) return 0;
      g_resizeWidth = static_cast<UINT>(LOWORD(lParam));
      g_resizeHeight = static_cast<UINT>(HIWORD(lParam));
      return 0;
    case WM_SYSCOMMAND:
      if ((wParam & 0xfff0) == SC_KEYMENU) return 0;  // 屏蔽 Alt 唤出系统菜单
      break;
    case WM_DESTROY:
      ::PostQuitMessage(0);
      return 0;
    default:
      break;
  }
  return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

}  // namespace

// -----------------------------------------------------------------------------
//  初始化
// -----------------------------------------------------------------------------
bool init(const wchar_t* title, int width, int height) {
  ImGui_ImplWin32_EnableDpiAwareness();
  g_dpiScale = ImGui_ImplWin32_GetDpiScaleForMonitor(
      ::MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY));
  if (g_dpiScale <= 0.0f) g_dpiScale = 1.0f;

  // 窗口标题存一份，后面 setTitleSuffix 要在它后面追加装载状态
  const wchar_t* baseTitle = title ? title : L"医院门诊预约与智能导诊系统";
  ::wcsncpy_s(g_title, 256, baseTitle, _TRUNCATE);

  // 窗口尺寸按屏幕工作区算，而不是硬编码像素：
  // CreateWindowW 收的是物理像素，而 ImGui 的界面按逻辑像素排版，
  // 两者差一个 DPI 缩放系数。用「工作区的 84%，但不小于请求值」就不会超出屏幕 ——
  // 否则 125%/150% 缩放的笔记本上，窗口右半边会跑到屏幕外面被裁掉。
  RECT wa = {0, 0, 0, 0};
  if (!::SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0)) {
    wa.right = ::GetSystemMetrics(SM_CXSCREEN);
    wa.bottom = ::GetSystemMetrics(SM_CYSCREEN);
  }
  int waW = (wa.right - wa.left) > 0 ? (wa.right - wa.left) : 1280;
  int waH = (wa.bottom - wa.top) > 0 ? (wa.bottom - wa.top) : 800;

  // 关键一步：SPI_GETWORKAREA 在某些机器上会返回「虚拟化」的尺寸
  // （比如实际 1920x1200，它却报 2880x1728），照它算窗口就会超出屏幕。
  // 这里再向 GetSystemMetrics 要一次真实的屏幕尺寸，两者取小。
  const int screenW = ::GetSystemMetrics(SM_CXSCREEN);
  const int screenH = ::GetSystemMetrics(SM_CYSCREEN);
  const int physW = ::GetDeviceCaps(::GetDC(nullptr), DESKTOPHORZRES);
  const int physH = ::GetDeviceCaps(::GetDC(nullptr), DESKTOPVERTRES);
  if (screenW > 0 && screenW < waW) waW = screenW;
  if (screenH > 0 && screenH < waH) waH = screenH;
  if (physW > 0 && physW < waW) waW = physW;
  if (physH > 0 && physH < waH) waH = physH;

  int winW = static_cast<int>(waW * 0.84f);
  int winH = static_cast<int>(waH * 0.88f);
  const int minW = static_cast<int>(width * g_dpiScale);
  const int minH = static_cast<int>(height * g_dpiScale);
  if (winW < minW) winW = minW;
  if (winH < minH) winH = minH;
  if (winW > waW) winW = waW;
  if (winH > waH) winH = waH;
  g_logicalWorkW = waW / g_dpiScale;
  g_logicalWorkH = waH / g_dpiScale;

  g_wc = {sizeof(g_wc), CS_CLASSDC, wndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr,
          nullptr, nullptr, L"ImGuiHospitalWnd", nullptr};
  ::RegisterClassExW(&g_wc);
  g_hwnd = ::CreateWindowW(g_wc.lpszClassName, baseTitle, WS_OVERLAPPEDWINDOW, 40, 20, winW, winH,
                           nullptr, nullptr, g_wc.hInstance, nullptr);
  if (!g_hwnd) {
    std::printf("[GUI] 创建窗口失败\n");
    return false;
  }

  if (!createDeviceD3D(g_hwnd)) {
    cleanupDeviceD3D();
    ::UnregisterClassW(g_wc.lpszClassName, g_wc.hInstance);
    return false;
  }

  ::ShowWindow(g_hwnd, SW_SHOWDEFAULT);
  ::UpdateWindow(g_hwnd);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.IniFilename = nullptr;  // 不写 imgui.ini，保持工程目录干净

  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  style.ScaleAllSizes(g_dpiScale);
  style.FontScaleDpi = g_dpiScale;
  style.WindowRounding = 4.0f;
  style.FrameRounding = 3.0f;

  ImGui_ImplWin32_Init(g_hwnd);
  ImGui_ImplDX11_Init(g_device, g_context);

  loadChineseFont(io, g_dpiScale);

  std::printf("[GUI] 显示：DPI 缩放 %.0f%%，可用 %dx%d 逻辑像素，窗口 %dx%d 逻辑像素\n", g_dpiScale * 100.0f,
              static_cast<int>(g_logicalWorkW), static_cast<int>(g_logicalWorkH),
              static_cast<int>(winW / g_dpiScale), static_cast<int>(winH / g_dpiScale));
  return true;
}

void setTitleSuffix(const char* suffix) {
  if (!g_hwnd) return;
  wchar_t wide[512];
  // 用 MultiByteToWideChar 而不是 swprintf：源文件是 UTF-8，要显式按 UTF-8 转宽字符
  const int n = ::MultiByteToWideChar(CP_UTF8, 0, suffix ? suffix : "", -1, wide, 511);
  if (n <= 0) return;
  wchar_t full[768];
  std::swprintf(full, 768, L"%ls —— %ls", g_title, wide);
  ::SetWindowTextW(g_hwnd, full);
}

void getLogicalWorkArea(float* outW, float* outH) {
  if (outW) *outW = g_logicalWorkW;
  if (outH) *outH = g_logicalWorkH;
}

// -----------------------------------------------------------------------------
//  主循环
// -----------------------------------------------------------------------------
void run() {
  bool done = false;
  while (!done) {
    MSG msg;
    while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
      if (msg.message == WM_QUIT) done = true;
    }
    if (done) break;

    // 窗口被最小化或被锁屏时不做渲染，省 CPU
    if (g_swapChainOccluded && g_swapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) {
      ::Sleep(10);
      continue;
    }
    g_swapChainOccluded = false;

    if (g_resizeWidth != 0 && g_resizeHeight != 0) {
      cleanupRenderTarget();
      g_swapChain->ResizeBuffers(0, g_resizeWidth, g_resizeHeight, DXGI_FORMAT_UNKNOWN, 0);
      g_resizeWidth = g_resizeHeight = 0;
      createRenderTarget();
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();


    renderFrame();

    ImGui::Render();
    const float clear[4] = {0.09f, 0.11f, 0.14f, 1.0f};
    g_context->OMSetRenderTargets(1, &g_renderTargetView, nullptr);
    g_context->ClearRenderTargetView(g_renderTargetView, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    const HRESULT hr = g_swapChain->Present(1, 0);  // 开垂直同步
    g_swapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
  }
}

void shutdown() {
  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();

  cleanupDeviceD3D();
  if (g_hwnd) ::DestroyWindow(g_hwnd);
  if (g_wc.lpszClassName) ::UnregisterClassW(g_wc.lpszClassName, g_wc.hInstance);
  g_hwnd = nullptr;
}

}  // namespace gui
