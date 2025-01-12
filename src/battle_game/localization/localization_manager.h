#pragma once
#include <map>
#include <string>
#include <initializer_list>

#include "json/json.h"

namespace battle_game {
class LocalizationManager {
 public:
  static LocalizationManager *GetInstance();
  void LoadLocalization(std::string localization_id);
  void SetLocalization(std::string localization_id);
  std::string GetLocalizationString(const std::initializer_list<std::string> &text_id);

 private:
  std::string localization_id_;
  std::map<std::string, Json::Value> localization_strings_;
};
}  // namespace battle_game
