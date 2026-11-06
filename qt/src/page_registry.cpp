/* =============================================================================
 *  page_registry.cpp  —  人员管理
 * ========================================================================== */
#include "page_registry.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "qt_common.h"
#include "service_extra.h"

namespace qt {

RegistryPage::RegistryPage(QWidget* parent) : PageBase(parent) {
  auto* root = new QVBoxLayout(this);

  root->addWidget(makePageTitle(QStringLiteral("人员管理")));
  root->addWidget(makeHint(QStringLiteral(
      "登记新患者与新医生。登记会同时写进内存表与 data\\*.txt，"
      "所以删掉 state.bin 重新装载时人还在。新医生默认不排班，排班请到「排班展示」处理。")));

  auto* body = new QHBoxLayout();

  // ---------------- 左：患者 ----------------
  auto* patBox = new QGroupBox(QStringLiteral("患者"), this);
  auto* patLayout = new QVBoxLayout(patBox);
  patientTable_ = new QTableWidget(patBox);
  setupReadOnlyTable(patientTable_, 0, 3,
                     {QStringLiteral("编号"), QStringLiteral("姓名"), QStringLiteral("联系电话")},
                     false);
  patientTable_->setColumnWidth(0, 90);
  patientTable_->setColumnWidth(1, 120);
  patientTable_->setColumnWidth(2, 150);
  patLayout->addWidget(patientTable_, 1);
  auto* patBtn = new QPushButton(QStringLiteral("新增患者"), patBox);
  patLayout->addWidget(patBtn);
  body->addWidget(patBox, 1);

  // ---------------- 右：医生 ----------------
  auto* docBox = new QGroupBox(QStringLiteral("医生"), this);
  auto* docLayout = new QVBoxLayout(docBox);
  doctorTable_ = new QTableWidget(docBox);
  setupReadOnlyTable(doctorTable_, 0, 6,
                     {QStringLiteral("编号"), QStringLiteral("姓名"), QStringLiteral("科室"),
                      QStringLiteral("职称"), QStringLiteral("擅长领域"), QStringLiteral("诊室")},
                     false);
  doctorTable_->setColumnWidth(0, 70);
  doctorTable_->setColumnWidth(1, 90);
  doctorTable_->setColumnWidth(2, 110);
  doctorTable_->setColumnWidth(3, 110);
  docLayout->addWidget(doctorTable_, 1);
  auto* docBtn = new QPushButton(QStringLiteral("新增医生"), docBox);
  docLayout->addWidget(docBtn);
  body->addWidget(docBox, 2);

  root->addLayout(body, 1);

  connect(patBtn, &QPushButton::clicked, this, &RegistryPage::onAddPatient);
  connect(docBtn, &QPushButton::clicked, this, &RegistryPage::onAddDoctor);
}

void RegistryPage::refreshPage() {
  fillPatients();
  fillDoctors();
}

void RegistryPage::fillPatients() {
  Hospital& h = db();
  patientTable_->setRowCount(h.patients.size());
  for (int i = 0; i < h.patients.size(); ++i) {
    const Patient& p = h.patients[i];
    const QStringList cells = {QString::fromUtf8(p.code), QString::fromUtf8(p.name),
                               QString::fromUtf8(p.phone)};
    for (int c = 0; c < cells.size(); ++c) patientTable_->setItem(i, c, new QTableWidgetItem(cells[c]));
  }
}

void RegistryPage::fillDoctors() {
  Hospital& h = db();
  doctorTable_->setRowCount(h.doctors.size());
  for (int i = 0; i < h.doctors.size(); ++i) {
    const Doctor& d = h.doctors[i];
    const QStringList cells = {
        QString::fromUtf8(d.code), QString::fromUtf8(d.name), deptName(h, d.dept),
        QString::fromUtf8(d.title), QString::fromUtf8(d.skill), roomName(h, d.room)};
    for (int c = 0; c < cells.size(); ++c) doctorTable_->setItem(i, c, new QTableWidgetItem(cells[c]));
  }
}

// =============================================================================
//  新增患者
// =============================================================================
void RegistryPage::onAddPatient() {
  Hospital& h = db();

  QDialog dlg(this);
  dlg.setWindowTitle(QStringLiteral("新增患者"));
  auto* form = new QFormLayout(&dlg);
  auto* codeEdit = new QLineEdit(&dlg);
  auto* nameEdit = new QLineEdit(&dlg);
  auto* phoneEdit = new QLineEdit(&dlg);
  // 给一个建议编号，省得每次都去想
  codeEdit->setText(QStringLiteral("P%1").arg(h.patients.size() + 1));
  codeEdit->setPlaceholderText(QStringLiteral("如 P13"));
  nameEdit->setPlaceholderText(QStringLiteral("如 赵强"));
  phoneEdit->setPlaceholderText(QStringLiteral("可留空"));
  form->addRow(QStringLiteral("编号："), codeEdit);
  form->addRow(QStringLiteral("姓名："), nameEdit);
  form->addRow(QStringLiteral("联系电话："), phoneEdit);
  auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  form->addRow(box);
  connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted) return;

  const QByteArray code = codeEdit->text().trimmed().toUtf8();
  const QByteArray name = nameEdit->text().trimmed().toUtf8();
  const QByteArray phone = phoneEdit->text().trimmed().toUtf8();

  const int r = Registry::addPatient(h, state().dataDir, code.constData(), name.constData(),
                                     phone.constData());
  if (r == Registry::REG_OK) {
    notifier().ok(QStringLiteral("已登记患者 %1 %2")
                      .arg(codeEdit->text().trimmed(), nameEdit->text().trimmed()));
    fillPatients();
  } else {
    notifier().error(QStringLiteral("登记失败：%1").arg(QString::fromUtf8(Registry::resultText(r))));
  }
}

// =============================================================================
//  新增医生
// =============================================================================
void RegistryPage::onAddDoctor() {
  Hospital& h = db();

  QDialog dlg(this);
  dlg.setWindowTitle(QStringLiteral("新增医生"));
  dlg.resize(420, 0);
  auto* form = new QFormLayout(&dlg);

  auto* codeEdit = new QLineEdit(&dlg);
  codeEdit->setText(QStringLiteral("D%1").arg(h.doctors.size() + 1));
  codeEdit->setPlaceholderText(QStringLiteral("如 D37"));
  auto* nameEdit = new QLineEdit(&dlg);
  nameEdit->setPlaceholderText(QStringLiteral("如 周敏"));

  auto* deptCombo = new QComboBox(&dlg);
  for (int i = 0; i < h.depts.size(); ++i) deptCombo->addItem(deptName(h, i));

  auto* titleCombo = new QComboBox(&dlg);
  titleCombo->addItems(titleList());

  auto* skillEdit = new QLineEdit(&dlg);
  skillEdit->setPlaceholderText(QStringLiteral("如 心律失常"));

  auto* roomCombo = new QComboBox(&dlg);
  roomCombo->addItem(QStringLiteral("（暂不安排）"), -1);
  for (int i = 0; i < h.rooms.size(); ++i)
    roomCombo->addItem(roomLabel(h, i), i);

  form->addRow(QStringLiteral("编号："), codeEdit);
  form->addRow(QStringLiteral("姓名："), nameEdit);
  form->addRow(QStringLiteral("科室："), deptCombo);
  form->addRow(QStringLiteral("职称："), titleCombo);
  form->addRow(QStringLiteral("擅长领域："), skillEdit);
  form->addRow(QStringLiteral("诊室："), roomCombo);

  auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  form->addRow(box);
  connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted) return;

  const QByteArray code = codeEdit->text().trimmed().toUtf8();
  const QByteArray name = nameEdit->text().trimmed().toUtf8();
  const QByteArray dept = deptCombo->currentText().toUtf8();
  const QByteArray title = titleCombo->currentText().toUtf8();
  const QByteArray skill = skillEdit->text().trimmed().toUtf8();
  // 房间传的是编号（"RA101"），不是下标 —— 业务层按编号查，跨进程也稳定
  const int roomIdx = roomCombo->currentData().toInt();
  QByteArray roomCode;
  if (roomIdx >= 0 && roomIdx < h.rooms.size()) roomCode = QByteArray(h.rooms[roomIdx].code);

  const int r = Registry::addDoctor(h, state().dataDir, code.constData(), name.constData(),
                                    dept.constData(), title.constData(), skill.constData(),
                                    roomCode.constData());
  if (r == Registry::REG_OK) {
    notifier().ok(QStringLiteral("已登记医生 %1 %2（%3 · %4）")
                      .arg(codeEdit->text().trimmed(), nameEdit->text().trimmed(),
                           deptCombo->currentText(), titleCombo->currentText()));
    fillDoctors();
  } else {
    notifier().error(QStringLiteral("登记失败：%1").arg(QString::fromUtf8(Registry::resultText(r))));
  }
}

}  // namespace qt
