#include "battle_game/localization/localization_manager.h"

#include <fstream>
#include <iostream>

namespace battle_game {
LocalizationManager *LocalizationManager::GetInstance() {
  static LocalizationManager localization_manager;
  return &localization_manager;
}

void LocalizationManager::LoadLocalization(std::string localization_id) {
  std::string localization_file = "localization/" + localization_id + ".json";
  std::ifstream localization_fs(localization_file, std::ios::in | std::ios::binary);
  if (!localization_fs.is_open()) {
    std::cerr << "Cannot open localization file: " << localization_file << std::endl;
    return;
  }

  Json::Value root;
  Json::CharReaderBuilder builder;
  std::string errors;
  if (!Json::parseFromStream(builder, localization_fs, &root, &errors)) {
    std::cerr << "Failed to parse localization file: " << localization_file << std::endl;
    std::cerr << "Errors: " << errors << std::endl;
    return;
  }

  localization_strings_[localization_id] = std::move(root);
}

void LocalizationManager::SetLocalization(std::string localization_id) {
  if (localization_strings_.count(localization_id) == 0) {
    std::cerr << "Failed to set localization to " << localization_id << " because it has not been loaded." << std::endl;
    return;
  }
  localization_id_ = localization_id;
}

std::string LocalizationManager::GetLocalizationString(const std::initializer_list<std::string> &text_ids) {
  if (std::empty(text_ids)) {
    std::cerr << "No text IDs provided." << std::endl;
    return "";
  }
  std::string default_text = "";
  for (const std::string &text_id : text_ids) {
    if (text_id.empty()) {
      std::cerr << "Empty text ID." << std::endl;
      return "";
    }
    if (!default_text.empty()) {
      default_text += ".";
    }
    default_text += text_id;
  }

  if (localization_strings_.count(localization_id_) == 0) {
    return default_text;
  }
  const Json::Value *root = &localization_strings_[localization_id_];
  for (const std::string &text_id : text_ids) {
    if (!root->isMember(text_id)) {
      return default_text;
    }
    root = &(*root)[text_id];
  }
  if (!root->isString()) {
    return default_text;
  }
  return root->asString();
}

}  // namespace battle_game
