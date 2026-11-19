/* =============================================================================
 *  login_dialog.h  —  患者端登录
 *
 *  账户体系的分工（按需求）：
 *      · **注册由医院完成** —— 管理端「人员管理」页登记患者（已有），
 *        所以患者客户端不提供注册入口
 *      · **患者用姓名登录** —— 到院挂号时自报姓名即可
 *
 *  重名怎么处理：姓名不是唯一标识（数据里确实可能有同名），
 *  所以匹配到多个时列出候选（编号 + 姓名 + 电话后 4 位）让患者自己认领。
 *  一个都没匹配到就明确告诉他"未建档，请到医院服务台登记"。
 *
 *  登录成功后 state().patient 就是当前患者，各页面不再允许随便切换他人 ——
 *  这既是权限边界，也避免了"患者 A 的界面显示患者 B 的预约"这类笑话。
 * ========================================================================== */
#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;

namespace qt {

class LoginDialog : public QDialog {
  Q_OBJECT

 public:
  explicit LoginDialog(QWidget* parent = nullptr);

  // 便捷入口：弹出登录框，成功返回 true（并把当前患者写进 state()）
  static bool runLogin(QWidget* parent = nullptr);

 private Q_SLOTS:
  void onLogin();

 private:
  QLineEdit* nameEdit_ = nullptr;
  QLabel* tipLabel_ = nullptr;
};

}  // namespace qt
