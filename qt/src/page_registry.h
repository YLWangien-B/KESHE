/* =============================================================================
 *  page_registry.h  —  人员管理（管理端专有）
 *
 *  补的是原来缺的一块：数据只能从 data\*.txt 装载，运行期没法加人。
 *  医院来了新患者、新医生入职，只能改文本文件再重启。
 *
 *  这一页做两件事：
 *      · 左侧：患者表 + 「新增患者」
 *      · 右侧：医生表 + 「新增医生」
 *
 *  登记时会同时写进内存表和数据文件 —— 前者让当前运行期立刻可用，
 *  后者保证删掉 state.bin 重新装载时人还在。业务逻辑在
 *  service_extra.h 的 Registry 命名空间里，这一页只负责收集输入与显示结果。
 * ========================================================================== */
#pragma once

#include "qt_widgets.h"

class QTableWidget;

namespace qt {

class RegistryPage : public PageBase {
  Q_OBJECT

 public:
  explicit RegistryPage(QWidget* parent = nullptr);
  void refreshPage() override;

 private Q_SLOTS:
  void onAddPatient();
  void onAddDoctor();

 private:
  void fillPatients();
  void fillDoctors();

  QTableWidget* patientTable_ = nullptr;
  QTableWidget* doctorTable_ = nullptr;
};

}  // namespace qt
