/* =============================================================================
 *  page_home.h  —  患者端首页
 *
 *  手机 App 的首页逻辑：先看"我接下来要干嘛"，再决定点哪里。
 *  所以这一页只放三类东西：
 *    1. 我的待就诊卡片  —— 下次什么时候来、找谁、去哪个诊室
 *    2. 快捷入口        —— 导诊 / 预约 / 我的预约 / 查路线
 *    3. 概览数字        —— 已预约几条、已完成几条（顺便当"评价"入口）
 *
 *  真正的操作都在其它页，这里只负责"导航 + 一眼看清状态"。
 * ========================================================================== */
#pragma once

#include "qt_widgets.h"

class QLabel;
class QPushButton;
class QVBoxLayout;

namespace qt {

class HomePage : public PageBase {
  Q_OBJECT

 public:
  explicit HomePage(QWidget* parent = nullptr);
  void refreshPage() override;

 private:
  void rebuildCards();

  QVBoxLayout* cardLayout_ = nullptr;  // 待就诊卡片的容器
  QLabel* greetLabel_ = nullptr;       // 你好，张伟
  QLabel* summaryLabel_ = nullptr;     // 已预约 N 条 · 已完成 M 条
};

}  // namespace qt
