# =============================================================================
#  HospitalQt —— 医院门诊预约与智能导诊系统（Qt 6 图形界面）
#
#  设计要点：
#
#  1) 业务层与数据结构层完全复用。它们全部是头文件（src/*.h），
#     所以这个工程只需要额外编译 src/db.cpp —— 数据总库的唯一实例。
#     界面从 Dear ImGui 换成 Qt，业务层与数据结构层一行都没改，
#     这正是「业务层只返回 struct、不打印」这个分层的价值。
#
#  2) Qt 6.8.3 + MSVC 2022 64 位（与命令行版本同一套工具链，不需要第二个编译器）。
#     构建目录由 tools/build_qt.bat 统一管理。
#
#  3) /utf-8：源文件里大量中文字面量（界面文案），必须让 MSVC
#     按 UTF-8 读源文件、并按 UTF-8 生成窄字符串。少这个开关，
#     中文字面量会被按系统代码页（GBK）编码，与数据文件的 UTF-8 对不上。
# =============================================================================
cmake_minimum_required(VERSION 3.16)
project(HospitalQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# 复用现有的数据结构层 / 数据层 / 业务层
set(REUSE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../src")

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(hospital_qt
    src/main.cpp
    src/qt_app.cpp
    src/qt_app.h
    src/qt_common.cpp
    src/qt_common.h
    src/qt_state.cpp
    src/qt_state.h
    src/qt_widgets.cpp
    src/qt_widgets.h
    src/floor_plan.cpp
    src/floor_plan.h
    src/page_triage.cpp
    src/page_triage.h
    src/page_doctor.cpp
    src/page_doctor.h
    src/page_booking.cpp
    src/page_booking.h
    src/page_manage.cpp
    src/page_manage.h
    src/page_schedule.cpp
    src/page_schedule.h
    src/page_route.cpp
    src/page_route.h
    src/page_queue.cpp
    src/page_queue.h
    src/page_about.cpp
    src/page_about.h
    "${REUSE_SRC}/db.cpp"
)

target_include_directories(hospital_qt PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${REUSE_SRC}"
)

# QT_NO_KEYWORDS 见文件头第 4 条
# /utf-8  见文件头第 3 条
# /W4     与命令行版本同一标准
target_compile_options(hospital_qt PRIVATE /utf-8 /W4 /EHsc /D_CRT_SECURE_NO_WARNINGS)

target_compile_definitions(hospital_qt PRIVATE QT_NO_KEYWORDS)

target_link_libraries(hospital_qt PRIVATE Qt6::Widgets)

# 产物放到工程根的 build\，与其它三个程序放在一起
set_target_properties(hospital_qt PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../build"
)
