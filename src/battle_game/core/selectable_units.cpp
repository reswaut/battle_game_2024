#include "battle_game/core/game_core.h"
#include "battle_game/localization/localization_manager.h"

namespace battle_game {

template <class UnitType, class... Args>
void GameCore::AddPrimaryUnitAllocationFunction(Args... args) {
  primary_unit_allocation_functions_.emplace_back([=](uint32_t player_id) {
    return AddUnit<UnitType>(player_id, args...);
  });
}

void GameCore::GeneratePrimaryUnitList() {
  std::unique_ptr<Unit> unit;
  auto mgr = LocalizationManager::GetInstance();
  auto pre_author = mgr->GetLocalizationString({"core", "select_unit_list_item", "pre_author"});
  auto post_author = mgr->GetLocalizationString({"core", "select_unit_list_item", "post_author"});

#define ADD_SELECTABLE_UNIT(UnitType)                                        \
  unit = std::make_unique<UnitType>(nullptr, 0, 0);                          \
  AddPrimaryUnitAllocationFunction<UnitType>();                              \
  selectable_unit_list_.push_back(unit->UnitName() + pre_author +            \
                                  unit->Author() + post_author);

  /*
   * TODO: Add Your Unit Here!
   * */
  ADD_SELECTABLE_UNIT(unit::Tank);

  unit.reset();
}
}  // namespace battle_game
