/* =============================================================================
 *  page_booking.cpp  —  在线预约页
 *
 *  预约成功的处理（与字符界面同一套业务逻辑）：
 *      检查停诊 → 检查余量 → 扣减 booked → 递增版本号 → 生成凭证 → 写入预约表
 *  这几步中间不让出执行权，所以这一段是原子的（没有超卖）。
 * ========================================================================== */
#include "page_booking.h"

#include <cstdio>
#include <cstring>

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "qt_app.h"
#include "qt_common.h"
#include "shared_db.h"
#include "service_extra.h"

namespace qt {

BookingPage::BookingPage(QWidget* parent) : PageBase(parent) {
  auto* layout = new QVBoxLayout(this);

  layout->addWidget(makePageTitle(QStringLiteral("功能 3  在线预约")));
  layout->addWidget(makeHint(QStringLiteral(
      "先选患者与医生，再在下面的号源格子上点一个时段。"
      "预约时「检查停诊 → 检查余量 → 扣减 → 递增版本号」连着做完，中间不让出执行权，"
      "所以不会超卖（题目实现提示第 4 条）。")));

  // ---- 选择行 ----
  auto* row = new QHBoxLayout();
  row->addWidget(new QLabel(QStringLiteral("患者："), this));
  patientCombo_ = new QComboBox(this);
  patientCombo_->setMinimumWidth(180);
  row->addWidget(patientCombo_);

  row->addWidget(new QLabel(QStringLiteral("科室："), this));
  deptFilterCombo_ = new QComboBox(this);
  deptFilterCombo_->setMinimumWidth(150);
  row->addWidget(deptFilterCombo_);

  row->addWidget(new QLabel(QStringLiteral("医生："), this));
  doctorCombo_ = new QComboBox(this);
  doctorCombo_->setMinimumWidth(300);
  row->addWidget(doctorCombo_);
  row->addStretch(1);
  layout->addLayout(row);

  // ---- 号源格子 ----
  auto* gridRow = new QHBoxLayout();
  grid_ = new SlotGrid(this);
  gridRow->addWidget(grid_, 0, Qt::AlignTop);

  auto* rightCol = new QVBoxLayout();
  slotInfo_ = new QLabel(this);
  slotInfo_->setWordWrap(true);
  slotInfo_->setMinimumWidth(260);
  slotInfo_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  rightCol->addWidget(slotInfo_);
  rightCol->addStretch(1);

  bookBtn_ = new QPushButton(QStringLiteral("生成预约凭证"), this);
  bookBtn_->setMinimumHeight(42);
  bookBtn_->setDefault(true);
  rightCol->addWidget(bookBtn_);

  myBookingsBtn_ = new QPushButton(QStringLiteral("查看我的预约"), this);
  myBookingsBtn_->setMinimumHeight(34);
  rightCol->addWidget(myBookingsBtn_);

  gridRow->addLayout(rightCol, 1);
  layout->addLayout(gridRow);
  layout->addStretch(1);

  connect(patientCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &BookingPage::onPatientChanged);
  connect(deptFilterCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &BookingPage::onDeptFilterChanged);
  connect(doctorCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &BookingPage::onDoctorChanged);
  connect(grid_, &SlotGrid::slotClicked, this, &BookingPage::onSlotClicked);
  connect(bookBtn_, &QPushButton::clicked, this, &BookingPage::onBook);
  connect(myBookingsBtn_, &QPushButton::clicked, this, &BookingPage::onShowMyBookings);
}

void BookingPage::refreshPage() {
  Hospital& h = db();
  QtState& st = state();

  // 患者
  {
    const int keep = patientCombo_->currentIndex();
    QSignalBlocker b(patientCombo_);
    patientCombo_->clear();
    for (int i = 0; i < h.patients.size(); ++i) patientCombo_->addItem(patientLabel(h, i));
    int want = (keep >= 0 && keep < patientCombo_->count()) ? keep : st.patient;
    if (want < 0 || want >= patientCombo_->count()) want = 0;
    patientCombo_->setCurrentIndex(want);
    st.patient = want;
  }
  // 患者端已登录：身份固定，不允许改成别人
  if (st.loggedIn) {
    patientCombo_->setCurrentIndex(st.patient);
    patientCombo_->setEnabled(false);
    patientCombo_->setToolTip(QStringLiteral("当前登录患者（不可切换）"));
  }

  // 科室筛选（-1 = 全部）
  {
    const int keep = deptFilterCombo_->currentIndex();
    QSignalBlocker b(deptFilterCombo_);
    deptFilterCombo_->clear();
    deptFilterCombo_->addItem(QStringLiteral("全部科室"));
    for (int i = 0; i < h.depts.size(); ++i)
      deptFilterCombo_->addItem(QStringLiteral("%1  %2").arg(QString::fromUtf8(h.depts[i].code),
                                                             deptName(h, i)));
    if (keep >= 0 && keep < deptFilterCombo_->count())
      deptFilterCombo_->setCurrentIndex(keep);
    else
      deptFilterCombo_->setCurrentIndex(0);
  }

  // 排班页点格子跳过来时，预填医生与时段
  if (st.jumpToBooking) {
    st.jumpToBooking = false;
    st.doctor = st.jumpDoctor;
    st.day = st.jumpDay;
    st.slot = st.jumpSlot;
    // 把科室筛选对上这位医生所属科室
    const int dept = h.doctors[st.doctor].dept;
    deptFilterCombo_->setCurrentIndex(dept + 1);
  }

  rebuildDoctorCombo();

  // 若当前医生不在筛选后的列表里，选第一个
  if (doctorCombo_->count() == 0) {
    grid_->setDoctor(-1);
  } else {
    grid_->setDoctor(doctorCombo_->currentIndex());
  }
  grid_->setSelection(st.day, st.slot);
  updateSlotInfo();
}

void BookingPage::rebuildDoctorCombo() {
  Hospital& h = db();
  QtState& st = state();
  const int filter = deptFilterCombo_->currentIndex() - 1;  // -1 = 全部

  const int keep = st.doctor;
  QSignalBlocker b(doctorCombo_);
  doctorCombo_->clear();
  for (int i = 0; i < h.doctors.size(); ++i) {
    if (filter >= 0 && h.doctors[i].dept != filter) continue;
    doctorCombo_->addItem(QStringLiteral("%1  %2（%3 · %4）")
                              .arg(QString::fromUtf8(h.doctors[i].code),
                                   QString::fromUtf8(h.doctors[i].name), deptName(h, h.doctors[i].dept),
                                   QString::fromUtf8(h.doctors[i].title)),
                          i);  // 用 Qt::UserRole 存真实医生下标
  }
  // 恢复选中：按真实下标找
  int target = 0;
  for (int k = 0; k < doctorCombo_->count(); ++k)
    if (doctorCombo_->itemData(k).toInt() == keep) target = k;
  if (doctorCombo_->count() > 0) doctorCombo_->setCurrentIndex(target);
}

void BookingPage::onPatientChanged(int index) {
  if (index < 0) return;
  state().patient = index;
  updateSlotInfo();
}

void BookingPage::onDeptFilterChanged(int) {
  rebuildDoctorCombo();
  // 换了科室筛选后医生列表变了，号源格子要跟着换人
  if (doctorCombo_->count() > 0)
    grid_->setDoctor(doctorCombo_->currentData().toInt());
  else
    grid_->setDoctor(-1);
  updateSlotInfo();
}

void BookingPage::onDoctorChanged(int) {
  if (doctorCombo_->count() > 0)
    grid_->setDoctor(doctorCombo_->currentData().toInt());
  else
    grid_->setDoctor(-1);
  updateSlotInfo();
}

void BookingPage::onSlotClicked(int day, int slot) {
  state().day = day;
  state().slot = slot;
  updateSlotInfo();
}

void BookingPage::onBook() {
  Hospital& h = db();
  QtState& st = state();

  const int patient = patientCombo_->currentIndex();
  if (patient < 0) {
    notifier().warn(QStringLiteral("请先选择患者"));
    return;
  }
  if (doctorCombo_->count() == 0) {
    notifier().warn(QStringLiteral("该科室下没有出诊医生"));
    return;
  }
  const int doctor = doctorCombo_->currentData().toInt();
  if (doctor < 0 || doctor >= h.doctors.size()) return;

  // ---- 检查 + 扣减（临界区）----
  // 拆成两个程序之后，「检查与扣减之间不让出执行权」这个前提不再成立 ——
  // 另一端的程序可能正好也在改同一份数据。所以外面套一把跨进程锁。
  // 注意：锁里**不能弹对话框**（那是模态的，会把另一端卡住），
  // 所以失败原因先记下来，出了作用域再提示。
  const char* failText = nullptr;
  char dupBuf[160] = {0};
  Booking b;
  {
    shared::Lock lock;  // 跨进程互斥（单进程模式下是空操作）

    // ① 冲突检测：同一时段只能有一张有效预约
    const char* dup = BookingOps::sameSlotConflict(h, patient, st.day, st.slot);
    ScheduleSlot& s = h.slot(doctor, st.day, st.slot);
    if (dup != nullptr) {
      std::snprintf(dupBuf, sizeof(dupBuf),
                    "你在该时段已有预约 %s，不能重复预约。请先到「预约管理」取消原预约。",
                    dup);
      failText = dupBuf;
    } else if (!h.hasSchedule(doctor, st.day, st.slot)) {
      failText = "该时段不出诊，预约失败";
    } else if (s.stopped) {
      failText = "该时段已停诊，预约失败";
    } else if (s.quota - s.booked <= 0) {
      failText = "该时段号源已满，预约失败";
    } else {
      // 扣减 + 递增版本号（让此前看到的余号信息作废）
      ++s.booked;
      s.version = ++h.versionClock;

      std::snprintf(b.ticket, sizeof(b.ticket), "TK%04d", ++h.ticketSeq);
      b.patient = patient;
      b.doctor = doctor;
      b.day = st.day;
      b.slot = st.slot;
      b.status = BOOKED;
      b.rated = 0;  // 还没评价过
      if (h.bookings.push(b) < 0) {
        // 记满之后回滚号源，否则这个号就永远被吃掉了
        --s.booked;
        failText = "预约记录表已满，预约失败";
      } else {
        shared::logOp(shared::OP_BOOK, patient, doctor, 0);
        shared::bump();
      }
    }
  }
  if (failText != nullptr) {
    notifier().error(QString::fromUtf8(failText));
    return;
  }

  const Doctor& doc = h.doctors[doctor];
  const int left = h.remaining(doctor, st.day, st.slot);

  // ---- 凭证弹窗 ----
  QMessageBox box(this);
  box.setWindowTitle(QStringLiteral("预约成功"));
  box.setIcon(QMessageBox::Information);
  box.setTextFormat(Qt::RichText);
  box.setText(QStringLiteral("<b>预约凭证 %1</b>").arg(QString::fromUtf8(b.ticket)));
  box.setInformativeText(
      QStringLiteral("<table cellpadding='4'>"
                     "<tr><td>患者</td><td><b>%1</b></td></tr>"
                     "<tr><td>科室</td><td>%2</td></tr>"
                     "<tr><td>医生</td><td>%3 %4（%5）</td></tr>"
                     "<tr><td>时间</td><td>%6</td></tr>"
                     "<tr><td>地点</td><td>%7</td></tr>"
                     "<tr><td>该时段剩余号源</td><td><b>%8</b></td></tr>"
                     "</table>")
          .arg(patientLabel(h, patient), deptName(h, doc.dept), QString::fromUtf8(doc.code),
               QString::fromUtf8(doc.name), QString::fromUtf8(doc.title), timeText(st.day, st.slot),
               roomName(h, doc.room))
          .arg(left));
  QPushButton* goManage = box.addButton(QStringLiteral("查看我的预约"), QMessageBox::ActionRole);
  box.addButton(QStringLiteral("知道了"), QMessageBox::AcceptRole);
  box.exec();

  notifier().ok(QStringLiteral("预约成功：%1 预约 %2 %3，该时段剩余号源 %4")
                    .arg(patientName(h, patient), doctorName(h, doctor), timeText(st.day, st.slot))
                    .arg(left));

  if (box.clickedButton() == goManage) onShowMyBookings();

  grid_->setDoctor(doctorCombo_->currentData().toInt());
  updateSlotInfo();
}

void BookingPage::onShowMyBookings() {
  QtState& st = state();
  st.patient = patientCombo_->currentIndex();
  notifier().info(QStringLiteral("跳到预约管理，查看 %1 的预约").arg(patientName(db(), st.patient)));
  if (auto* w = qobject_cast<MainWindow*>(window())) w->switchPage(PAGE_MANAGE);
}

void BookingPage::updateSlotInfo() {
  Hospital& h = db();
  QtState& st = state();
  if (doctorCombo_->count() == 0) {
    slotInfo_->setText(QStringLiteral("该科室下没有出诊医生。"));
    bookBtn_->setEnabled(false);
    return;
  }
  const int doctor = doctorCombo_->currentData().toInt();
  st.doctor = doctor;

  const SlotState ss = slotState(h, doctor, st.day, st.slot);
  const QString stateText = slotStateText(h, doctor, st.day, st.slot);
  QString info = QStringLiteral("<b>%1 %2</b><br>%3 %4<br>")
                     .arg(QString::fromUtf8(h.doctors[doctor].code),
                          QString::fromUtf8(h.doctors[doctor].name), deptName(h, h.doctors[doctor].dept),
                          QString::fromUtf8(h.doctors[doctor].title));
  info += QStringLiteral("出诊房间：%1<br>").arg(roomName(h, h.doctors[doctor].room));

  // 扩展要求第 3 条：推荐科室时参考评分 —— 这里把这位医生的评分直接摆出来，
  // 患者在「症状匹配」之外还能看到「大家对这个医生评价如何」。
  char rankText[64];
  Rating::text(h.doctors[doctor], rankText, sizeof(rankText));
  info += QStringLiteral("患者评分：<b>%1</b><br>").arg(QString::fromUtf8(rankText));

  info += QStringLiteral("选中时段：<b>%1</b><br>").arg(timeText(st.day, st.slot));
  if (ss == SLOT_NONE || ss == SLOT_STOPPED) {
    info += QStringLiteral("<span style='color:#b04a4a;'>%1</span>").arg(stateText);
    bookBtn_->setEnabled(false);
  } else {
    info += QStringLiteral("当前余号：<b>%1 / %2</b><br>状态：%3")
                .arg(h.remaining(doctor, st.day, st.slot))
                .arg(h.slot(doctor, st.day, st.slot).quota)
                .arg(stateText);
    bookBtn_->setEnabled(ss != SLOT_FULL);
    if (ss == SLOT_FULL) info += QStringLiteral("<br><span style='color:#b04a4a;'>号已满，无法预约</span>");
  }

  // ---- 扩展要求第 3 条：推荐医生时参考评分 ----
  // 综合分 = 评分 50% + 职称 30% + 该时段余号 20%（见 DoctorRank::score）
  {
    int ranked[4];
    const int n = DoctorRank::rank(h, h.doctors[doctor].dept, st.day, st.slot, ranked, 4);
    if (n > 0) {
      info += QStringLiteral("<hr><b>该时段推荐医生</b>（评分+职称+余号综合）<br>");
      for (int i = 0; i < n; ++i) {
        const int doc = ranked[i];
        char rankText[64];
        Rating::text(h.doctors[doc], rankText, sizeof(rankText));
        info += QStringLiteral("%1. %2 %3（%4 · %5）综合分 %6<br>")
                    .arg(i + 1)
                    .arg(QString::fromUtf8(h.doctors[doc].name),
                         QString::fromUtf8(h.doctors[doc].title), QString::fromUtf8(rankText),
                         roomName(h, h.doctors[doc].room))
                    .arg(DoctorRank::score(h, doc, st.day, st.slot));
      }
    }
  }

  // ---- 扩展要求第 4 条：科室号源紧张时推荐相近科室 ----
  // 只在这个科室该时段没号（已满 / 全部停诊）时提示，否则没必要打扰患者。
  const int dept = h.doctors[doctor].dept;
  if (Referral::tightness(h, dept, st.day, st.slot) == 2) {
    int rec[4];
    const int nrec = Referral::recommend(h, dept, st.day, st.slot, rec, 4);
    info += QStringLiteral("<hr><span style='color:#d8b050;'><b>%1 的号源已满</b></span><br>")
                .arg(deptName(h, dept));
    if (nrec == 0) {
      info += QStringLiteral("该时段没有其它科室还有号，建议换一个时段。");
    } else {
      info += QStringLiteral("相近科室（按症状重叠度）：<br>");
      for (int i = 0; i < nrec; ++i) {
        const int freeCount = Referral::freeSlots(h, rec[i], st.day, st.slot);
        info += QStringLiteral("· %1（相似度 %2%%，余号 %3）<br>")
                    .arg(deptName(h, rec[i]))
                    .arg(Referral::similarity(h, dept, rec[i]))
                    .arg(freeCount);
      }
      info += QStringLiteral("<span style='color:#8a9199;'>换到相近科室：在「科室」下拉框里改选即可</span>");
    }
  }
  slotInfo_->setText(info);
}

}  // namespace qt
