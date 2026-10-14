# 第三方库

这个目录放的是本工程用到的第三方代码。**只有图形界面程序会用到它**，
字符界面主程序（`hospital.exe`）、自检程序（`selfcheck.exe`）与数据生成器
（`gen_data.exe`）都不依赖这里的任何东西。

## Dear ImGui

| 项 | 内容 |
| --- | --- |
| 版本 | **v1.92.9b** |
| 作者 | Omar Cornut 与 ImGui 贡献者 |
| 许可证 | MIT（见 `imgui/LICENSE.txt`） |
| 官网 | https://github.com/ocornut/imgui |
| 为什么选它 | 见下面「选它的理由」 |

### 为什么把它整份复制进工程，而不是用包管理器

课程设计要交源码、要能在老师的机器上直接编译。用 vcpkg 之类的话，
换一台机器就得先装包管理器再装库，答辩现场一旦装不上就演示不了。
整份复制进来，工程自带全部依赖，`tools\run_gui.bat` 一条命令就能跑起来。

### 用到了哪些文件

```
imgui/
├── imgui.h                 ImGui 的公开接口（程序里 include 的就是它）
├── imgui.cpp               核心：上下文、窗口、输入、布局
├── imgui_draw.cpp          绘制与字体栅格化
├── imgui_tables.cpp        表格（BeginTable）—— 七项功能的列表都靠它
├── imgui_widgets.cpp       按钮、下拉框、输入框等控件
├── imgui_demo.cpp          官方示例窗口（本工程没编译它，留着备查用法）
├── imgui_internal.h        内部头（核心 .cpp 之间共用）
├── imconfig.h              编译期配置（保持默认）
├── imstb_*.h               内嵌的 stb 库：字体栅格化、矩形打包、文本编辑
├── LICENSE.txt             MIT 许可证原文
└── backends/
    ├── imgui_impl_win32.*  平台后端：窗口、鼠标键盘、剪贴板、输入法
    └── imgui_impl_dx11.*   渲染后端：把 ImGui 的画图指令交给 DirectX 11
```

### 选它的理由

1. **不引入构建系统**。只要把几个 `.cpp` 加进编译就行，
   本工程原有的 `tools\build.bat` 与三个 `.vcxproj` 的结构一点没动。
2. **不依赖 STL 容器**。ImGui 自带 `ImVector` 等极简容器，界面层不会
   顺手用 `std::vector`/`std::map` 把课程要求的数据结构绕过去。
3. **表格与自绘都够用**。`BeginTable` 画医生查询、预约记录、候诊队列这些列表；
   `ImDrawList` 直接画楼层平面图的方块和走廊。
4. **许可宽松**。MIT，随工程分发没有任何限制。

### 与课程要求的关系

课程要求「不能使用类库、模板等（**用户界面除外**）」——
界面部分允许用现成的库，而这个库只用在界面层。
`src/ds.h`、`src/ds_sort.h` 里那六种数据结构全部是自己写的，
`src/service.h`、`src/service_queue.h` 里的算法也全部是自己写的，
一行都没有交给 ImGui。

### 升级方式

换版本时把上面「用到了哪些文件」列出的文件整份替换即可。
唯一要注意的是 `imconfig.h`：本工程用的是官方默认值，没有改过任何开关。
