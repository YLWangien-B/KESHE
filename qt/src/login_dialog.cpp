/* =============================================================================
 *  login_dialog.cpp  —  患者端登录
 * ========================================================================== */
#include "login_dialog.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "model.h"
#include "qt_common.h"
#include "qt_state.h"
#include "qt_widgets.h"

namespace qt {

LoginDialog::LoginDialog(QWidget* parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("患者登录"));
  setMinimumWidth(420);

  auto* layout = new QVBoxLayout(this);

  auto* title = new QLabel(QStringLiteral("医院门诊预约与智能导诊系统"), this);
  title->setStyleSheet(QStringLiteral("font-size:18px;font-weight:bold;color:#3b6ea8;"));
  layout->addWidget(title);

  layout->addWidget(makeHint(QStringLiteral(
      "请输入您在医院的登记姓名。若还未建档，请先到医院服务台登记（由管理端录入）。")));

  auto* row = new QHBoxLayout();
  row->addWidget(new QLabel(QStringLiteral("姓名："), this));
  nameEdit_ = new QLineEdit(this);
  nameEdit_->setPlaceholderText(QStringLiteral("如 张伟"));
  nameEdit_->setMinimumWidth(200);
  row->addWidget(nameEdit_, 1);
  layout->addLayout(row);

  tipLabel_ = makeHint(QString());
  layout->addWidget(tipLabel_);

  auto* btnRow = new QHBoxLayout();
  btnRow->addStretch(1);
  auto* loginBtn = new QPushButton(QStringLiteral("登录"), this);
  loginBtn->setDefault(true);
  loginBtn->setMinimumWidth(90);
  auto* quitBtn = new QPushButton(QStringLiteral("退出"), this);
  quitBtn->setMinimumWidth(90);
  btnRow->addWidget(loginBtn);
  btnRow->addWidget(quitBtn);
  layout->addLayout(btnRow);

  connect(loginBtn, &QPushButton::clicked, this, &LoginDialog::onLogin);
  connect(nameEdit_, &QLineEdit::returnPressed, this, &LoginDialog::onLogin);
  connect(quitBtn, &QPushButton::clicked, this, &QDialog::reject);

  nameEdit_->setFocus();
}

// 按姓名找人。0 个 —— 未建档；1 个 —— 直接登录；多个 —— 让患者自己认领。
void LoginDialog::onLogin() {
  Hospital& h = db();
  const QString name = nameEdit_->text().trimmed();
  if (name.isEmpty()) {
    tipLabel_->setText(QStringLiteral("<span style='color:#b04a4a;'>请输入姓名</span>"));
    nameEdit_->setFocus();
    return;
  }

  const QByteArray nameUtf8 = name.toUtf8();
  QVector<int> hits;
  for (int i = 0; i < h.patients.size(); ++i)
    if (std::strcmp(h.patients[i].name, nameUtf8.constData()) == 0) hits.append(i);

  if (hits.isEmpty()) {
    tipLabel_->setText(QStringLiteral(
        "<span style='color:#b04a4a;'>没有找到「%1」的建档记录。</span><br>"
        "请先到医院服务台登记（管理端 →「人员管理」→ 新增患者）。")
                           .arg(name));
    return;
  }

  int patient = hits.first();
  if (hits.size() > 1) {
    // 重名：列出候选让患者认领（编号 + 电话后 4 位用来区分）
    QStringList items;
    for (int idx : hits) {
      const QString phone = QString::fromUtf8(h.patients[idx].phone);
      const QString tail = phone.size() >= 4 ? phone.right(4) : phone;
      items << QStringLiteral("%1  %2（电话 …%3）")
                   .arg(QString::fromUtf8(h.patients[idx].code),
                        QString::fromUtf8(h.patients[idx].name), tail);
    }
    bool ok = false;
    const QString chosen = QInputDialog::getItem(
        this, QStringLiteral("重名"),
        QStringLiteral("有 %1 位同名患者，请选择您的建档记录：").arg(hits.size()), items, 0,
        false, &ok);
    if (!ok) return;
    const int pos = items.indexOf(chosen);
    if (pos < 0 || pos >= hits.size()) return;
    patient = hits[pos];
  }

  state().patient = patient;
  state().loggedIn = true;
  accept();
}

bool LoginDialog::runLogin(QWidget* parent) {
  LoginDialog dlg(parent);
  return dlg.exec() == QDialog::Accepted;
}

}  // namespace qt
