# =============================================================================
#  HospitalQt —— 医院门诊预约与智能导诊系统（Qt 6 图形界面）
#
#  这一份代码编译出两个程序：
#      patient.exe   患者端：导诊、查医生、预约、取消预约、看路线、签到候诊
#      admin.exe     管理端：查医生、预约总览、排班与停诊、叫号/过号/完成、看路线
#
#  两者的界面代码完全共用，靠宏 APP_ROLE_ADMIN 区分角色（见 qt_role.h）。
#
#  ---------------------------------------------------------------------------
#  设计要点
#
#  1) 业务层与数据结构层完全复用。它们全部是头文件（src/*.h），
#     所以这个工程只需要额外编译 src/db.cpp 与 src/shared_db.cpp ——
#     前者是数据总库的唯一实例，后者是两端共享同一份数据的机制。
#
#  2) Qt 6.8.3 + MSVC 2022 64 位（与命令行版本同一套工具链，不需要第二个编译器）。
#     构建目录由 tools/build_qt.bat 统一管理。
#
#  3) /utf-8：源文件里大量中文字面量（界面文案），必须让 MSVC
#     按 UTF-8 读源文件、并按 UTF-8 生成窄字符串。少这个开关，
#     中文字面量会被按系统代码页（GBK）编码，与数据文件的 UTF-8 对不上。
#
#  4) QT_NO_KEYWORDS：Qt 默认把 slots / signals 定义成宏，而业务层 ds.h 里
#     有个方法就叫 slots()（排班表的时段数），会被展开成 int () 从而语法错误。
#     关掉关键字污染，自己的代码用 Q_SLOTS / Q_SIGNALS / Q_EMIT。
#
#  5) 两个程序的共享区布局必须一致，所以它们用**完全相同**的编译选项，
#     且都来自同一份源码。shared_db.cpp 会用 sizeof(Hospital) 做运行期校验，
#     不一致时会明确报错而不是把内存解释成垃圾数据。
# =============================================================================
cmake_minimum_required(VERSION 3.16)
project(HospitalQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# 复用现有的数据结构层 / 数据层 / 业务层
set(REUSE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../src")

find_package(Qt6 REQUIRED COMPONENTS Widgets)

# ---------------------------------------------------------------------------
#  界面层源码（两个程序共用）
# ---------------------------------------------------------------------------
set(APP_SOURCES
    src/main.cpp
    src/qt_app.cpp
    src/qt_app.h
    src/qt_common.cpp
    src/qt_common.h
    src/qt_state.cpp
    src/qt_state.h
    src/qt_role.cpp
    src/qt_role.h
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
    src/page_registry.cpp
    src/page_registry.h
    src/page_about.cpp
    src/page_about.h
)

# 复用的底层源码
set(REUSE_SOURCES
    "${REUSE_SRC}/db.cpp"
    "${REUSE_SRC}/shared_db.cpp"
)

set(SHARED_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${REUSE_SRC}"
)

# /utf-8  见文件头第 3 条
# /W4     与命令行版本同一标准
set(SHARED_OPTIONS /utf-8 /W4 /EHsc /D_CRT_SECURE_NO_WARNINGS)

# ---------------------------------------------------------------------------
#  患者端
# ---------------------------------------------------------------------------
add_executable(patient ${APP_SOURCES} ${REUSE_SOURCES})
target_include_directories(patient PRIVATE ${SHARED_INCLUDES})
target_compile_options(patient PRIVATE ${SHARED_OPTIONS})
# QT_NO_KEYWORDS 见文件头第 4 条
target_compile_definitions(patient PRIVATE QT_NO_KEYWORDS)
target_link_libraries(patient PRIVATE Qt6::Widgets)

# ---------------------------------------------------------------------------
#  管理端（同一份源码，只多一个宏）
# ---------------------------------------------------------------------------
add_executable(admin ${APP_SOURCES} ${REUSE_SOURCES})
target_include_directories(admin PRIVATE ${SHARED_INCLUDES})
target_compile_options(admin PRIVATE ${SHARED_OPTIONS})
target_compile_definitions(admin PRIVATE QT_NO_KEYWORDS APP_ROLE_ADMIN)
target_link_libraries(admin PRIVATE Qt6::Widgets)

# 产物放到工程根的 build\，与其它程序放在一起
set_target_properties(patient admin PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../build"
)
