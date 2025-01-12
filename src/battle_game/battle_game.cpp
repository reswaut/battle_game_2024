#include <cstring>
#include <string>

#include "battle_game/app/app.h"
#include "battle_game/core/game_core.h"
#include "battle_game/graphics/graphics.h"
#include "battle_game/localization/localization_manager.h"

int main(int argc, char *argv[]) {
  std::string localization = "eng";
  if (argc >= 3 && !strcmp(argv[1], "--lang")) {
    localization = argv[2];
  }
  battle_game::LocalizationManager::GetInstance()->LoadLocalization(localization);
  battle_game::LocalizationManager::GetInstance()->SetLocalization(localization);
  battle_game::GameCore game_core;
  battle_game::AppSettings app_settings;
  battle_game::App app(app_settings, &game_core);
  app.Run();
}
