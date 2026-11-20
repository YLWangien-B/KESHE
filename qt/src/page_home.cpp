/* =============================================================================
 *  page_home.cpp  —  患者端首页
 * ========================================================================== */
#include "page_home.h"

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "qt_app.h"
#include "qt_common.h"
#include "service_extra.h"

namespace qt {

namespace {
// 首页里的"快捷入口"按钮：点一下就跳到对应页面
QPushButton* makeEntry(QWidget* parent, const QString& text, const QString& tip) {
  auto* b = new QPushButton(text, parent);
  b->setMinimumHeight(64);
  b->setToolTip(tip);
  b->setStyleSheet(QStringLiteral(
      "QPushButton{font-size:15px;border:1px solid #3a4048;border-radius:8px;"
      "background:#242a31;color:#d7dde5;}"
      "QPushButton:hover{background:#2d3540;border-color:#4a9eff;}"));
  return b;
}
}  // namespace

HomePage::HomePage(QWidget* parent) : PageBase(parent) {
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(16, 12, 16, 12);
  root->setSpacing(12);

  greetLabel_ = new QLabel(this);
  greetLabel_->setStyleSheet(QStringLiteral("font-size:20px;font-weight:bold;color:#d7dde5;"));
  root->addWidget(greetLabel_);

  summaryLabel_ = makeHint(QString());
  root->addWidget(summaryLabel_);

  // ---- 待就诊卡片区 ----
  cardLayout_ = new QVBoxLayout();
  cardLayout_->setSpacing(10);
  root->addLayout(cardLayout_);

  // ---- 快捷入口 ----
  auto* entryBox = new QGroupBox(QStringLiteral("我要…"), this);
  auto* grid = new QGridLayout(entryBox);
  grid->setSpacing(10);
  auto* triageBtn = makeEntry(entryBox, QStringLiteral("不知道挂什么科？\n智能导诊"),
                              QStringLiteral("按症状推荐科室"));
  auto* bookBtn = makeEntry(entryBox, QStringLiteral("去预约\n选医生与时段"),
                            QStringLiteral("在线预约"));
  auto* mineBtn = makeEntry(entryBox, QStringLiteral("我的预约\n查看 / 取消 / 评价"),
                            QStringLiteral("预约管理"));
  auto* routeBtn = makeEntry(entryBox, QStringLiteral("怎么走\n就诊路线"),
                             QStringLiteral("查诊室之间的走法"));
  grid->addWidget(triageBtn, 0, 0);
  grid->addWidget(bookBtn, 0, 1);
  grid->addWidget(mineBtn, 1, 0);
  grid->addWidget(routeBtn, 1, 1);
  root->addWidget(entryBox);

  root->addStretch(1);

  connect(triageBtn, &QPushButton::clicked, this, [this]() {
    if (auto* w = qobject_cast<MainWindow*>(window())) w->switchPage(PAGE_TRIAGE);
  });
  connect(bookBtn, &QPushButton::clicked, this, [this]() {
    if (auto* w = qobject_cast<MainWindow*>(window())) w->switchPage(PAGE_BOOK);
  });
  connect(mineBtn, &QPushButton::clicked, this, [this]() {
    if (auto* w = qobject_cast<MainWindow*>(window())) w->switchPage(PAGE_MANAGE);
  });
  connect(routeBtn, &QPushButton::clicked, this, [this]() {
    if (auto* w = qobject_cast<MainWindow*>(window())) w->switchPage(PAGE_ROUTE);
  });
}

void HomePage::refreshPage() { rebuildCards(); }

void HomePage::rebuildCards() {
  Hospital& h = db();
  const int patient = state().patient;

  if (patient < 0 || patient >= h.patients.size()) {
    greetLabel_->setText(QStringLiteral("你好"));
    summaryLabel_->setText(QString());
    return;
  }
  greetLabel_->setText(
      QStringLiteral("你好，%1").arg(patientName(h, patient)));

  // ---- 统计 ----
  int booked = 0, finished = 0, toRate = 0;
  for (int i = 0; i < h.bookings.size(); ++i) {
    const Booking& b = h.bookings[i];
    if (b.patient != patient) continue;
    if (b.status == BOOKED) ++booked;
    if (b.status == FINISHED) {
      ++finished;
      if (b.rated == 0) ++toRate;
    }
  }
  summaryLabel_->setText(
      QStringLiteral("已预约 <b>%1</b> 条 · 已完成 <b>%2</b> 条%3")
          .arg(booked)
          .arg(finished)
          .arg(toRate > 0 ? QStringLiteral(" · <span style='color:#d8b050;'>%1 条待评价</span>")
                                .arg(toRate)
                          : QString()));

  // ---- 清空旧卡片 ----
  // 注意：takeAt 只是把它移出布局，控件本身仍然有父对象、仍然可见，
  // 会叠在左上角 —— 必须先 hide + 断开父对象，再 deleteLater。
  while (QLayoutItem* item = cardLayout_->takeAt(0)) {
    if (QWidget* w = item->widget()) {
      w->hide();
      w->setParent(nullptr);
      w->deleteLater();
    }
    delete item;
  }

  // ---- 找最近的一条「已预约」----
  // 本系统的时间是"周几 + 上午/下午"这种抽象时刻，没有真实日期，
  // 所以"最近"按 周几×2+时段 取最小值 —— 相当于这周里最早的一次。
  int best = -1, bestKey = 1 << 30;
  for (int i = 0; i < h.bookings.size(); ++i) {
    const Booking& b = h.bookings[i];
    if (b.patient != patient || b.status != BOOKED) continue;
    const int key = b.day * 2 + b.slot;
    if (key < bestKey) {
      bestKey = key;
      best = i;
    }
  }

  if (best < 0) {
    auto* box = new QGroupBox(QStringLiteral("待就诊"), this);
    auto* v = new QVBoxLayout(box);
    auto* empty = new QLabel(
        toRate > 0
            ? QStringLiteral("<span style='color:#d8b050;'>当前没有待就诊预约，"
                             "但有 %1 条已完成的记录可以评价。</span>")
                  .arg(toRate)
            : QStringLiteral("<span style='color:#8a9199;'>当前没有待就诊预约。"
                             "可以先做智能导诊，或直接去预约。</span>"),
        box);
    empty->setWordWrap(true);
    v->addWidget(empty);
    cardLayout_->addWidget(box);
    return;
  }

  const Booking& b = h.bookings[best];
  const Doctor& d = h.doctors[b.doctor];

  auto* box = new QGroupBox(QStringLiteral("待就诊 · 下次就诊"), this);
  box->setStyleSheet(QStringLiteral(
      "QGroupBox{border:1px solid #4a9eff;border-radius:10px;margin-top:10px;"
      "padding:12px;background:#1f2630;}"
      "QGroupBox::title{subcontrol-origin:margin;left:12px;padding:0 6px;color:#6fb3ff;}"));

  auto* v = new QVBoxLayout(box);
  v->setSpacing(6);

  auto* title = new QLabel(
      QStringLiteral("<span style='font-size:18px;font-weight:bold;color:#e8eef5;'>"
                     "%1 %2 · %3 %4</span>")
          .arg(dayText(b.day), slotText(b.slot), deptName(h, d.dept),
               QString::fromUtf8(d.name)),
      box);
  v->addWidget(title);

  char rankText[64];
  Rating::text(d, rankText, sizeof(rankText));
  auto* detail = new QLabel(
      QStringLiteral("%1（%2）· 评分 %3<br>就诊地点：<b>%4</b><br>凭证号：%5")
          .arg(QString::fromUtf8(d.code), QString::fromUtf8(d.title),
               QString::fromUtf8(rankText), roomName(h, d.room),
               QString::fromUtf8(b.ticket)),
      box);
  detail->setTextFormat(Qt::RichText);
  detail->setWordWrap(true);
  v->addWidget(detail);

  auto* btnRow = new QHBoxLayout();
  auto* queueBtn = new QPushButton(QStringLiteral("签到候诊"), box);
  queueBtn->setMinimumHeight(40);
  auto* routeBtn = new QPushButton(QStringLiteral("怎么去"), box);
  routeBtn->setMinimumHeight(40);
  btnRow->addWidget(queueBtn);
  btnRow->addWidget(routeBtn);
  btnRow->addStretch(1);
  v->addLayout(btnRow);

  cardLayout_->addWidget(box);

  connect(queueBtn, &QPushButton::clicked, this, [this]() {
    if (auto* w = qobject_cast<MainWindow*>(window())) w->switchPage(PAGE_QUEUE);
  });
  connect(routeBtn, &QPushButton::clicked, this, [this]() {
    if (auto* w = qobject_cast<MainWindow*>(window())) w->switchPage(PAGE_ROUTE);
  });
}

}  // namespace qt
