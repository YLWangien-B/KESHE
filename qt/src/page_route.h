/* =============================================================================
 *  page_route.h  —  就诊路线页（含楼层平面图）
 * ========================================================================== */
#pragma once

#include "qt_widgets.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;

namespace qt {

class FloorPlanView;
class FloorLegend;

// 功能 6：选起点终点 → 查询最短路线 → 在楼层平面图上把路线描出来。
// 平面图上点房间可以直接设起点（Shift+点击设终点），与两个下拉框双向联动。
class RoutePage : public PageBase {
  Q_OBJECT

 public:
  explicit RoutePage(QWidget* parent = nullptr);
  void refreshPage() override;

 private Q_SLOTS:
  void onQuery();
  void onSwap();
  void onFromChanged(int index);
  void onToChanged(int index);
  void onRoomClicked(int roomIndex, bool shift);
  void onShowCodeToggled(bool on);
  void onImportDestination();  // 从「我的预约」导入目的地诊室

 private:
  void fillRoomCombos();
  void applyRouteQuery();
  void fillRouteTable();
  void syncPlanToState();

  QComboBox* fromCombo_ = nullptr;
  QComboBox* toCombo_ = nullptr;
  QTableWidget* stepTable_ = nullptr;
  QLabel* summary_ = nullptr;
  QLabel* floorGuide_ = nullptr;
  QCheckBox* showCodeCheck_ = nullptr;
  FloorPlanView* plan_ = nullptr;
  FloorLegend* legend_ = nullptr;
  bool syncing_ = false;  // 防止下拉框与平面图互相触发
};

}  // namespace qt
