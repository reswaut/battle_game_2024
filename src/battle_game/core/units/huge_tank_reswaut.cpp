#include "tiny_tank.h"

#include "battle_game/core/bullets/bullets.h"
#include "battle_game/core/game_core.h"
#include "battle_game/graphics/graphics.h"

namespace battle_game::unit {

namespace {
uint32_t huge_tank_reswaut_body_model_index = 0xffffffffu;
uint32_t huge_tank_reswaut_turret_model_index = 0xffffffffu;
}  // namespace

HugeTankReswaut::HugeTankReswaut(GameCore *game_core, uint32_t id, uint32_t player_id)
    : Unit(game_core, id, player_id) {
  if (!~huge_tank_reswaut_body_model_index) {
    auto mgr = AssetsManager::GetInstance();
    {
      /* Tank Body */
      huge_tank_reswaut_body_model_index = mgr->RegisterModel(
          {
              {{-1.6f, 1.6f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
              {{-1.6f, -2.0f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
              {{1.6f, 1.6f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
              {{1.6f, -2.0f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
              // distinguish front and back
              {{1.2f, 2.0f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
              {{-1.2f, 2.0f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
          },
          {0, 1, 2, 1, 2, 3, 0, 2, 5, 2, 4, 5});
    }

    {
      /* Tank Turret */
      std::vector<ObjectVertex> turret_vertices;
      std::vector<uint32_t> turret_indices;
      const int precision = 60;
      const float inv_precision = 1.0f / float(precision);
      for (int i = 0; i < precision; i++) {
        auto theta = (float(i) + 0.5f) * inv_precision;
        theta *= glm::pi<float>() * 2.0f;
        auto sin_theta = std::sin(theta);
        auto cos_theta = std::cos(theta);
        turret_vertices.push_back({{sin_theta, cos_theta},
                                   {0.0f, 0.0f},
                                   {0.7f, 0.7f, 0.7f, 1.0f}});
        turret_indices.push_back(i);
        turret_indices.push_back((i + 1) % precision);
        turret_indices.push_back(precision);
      }
      turret_vertices.push_back(
          {{0.0f, 0.0f}, {0.0f, 0.0f}, {0.7f, 0.7f, 0.7f, 1.0f}});
      turret_vertices.push_back(
          {{-0.2f, 0.0f}, {0.0f, 0.0f}, {0.7f, 0.7f, 0.7f, 1.0f}});
      turret_vertices.push_back(
          {{0.2f, 0.0f}, {0.0f, 0.0f}, {0.7f, 0.7f, 0.7f, 1.0f}});
      turret_vertices.push_back(
          {{-0.2f, 2.4f}, {0.0f, 0.0f}, {0.7f, 0.7f, 0.7f, 1.0f}});
      turret_vertices.push_back(
          {{0.2f, 2.4f}, {0.0f, 0.0f}, {0.7f, 0.7f, 0.7f, 1.0f}});
      turret_indices.push_back(precision + 1 + 0);
      turret_indices.push_back(precision + 1 + 1);
      turret_indices.push_back(precision + 1 + 2);
      turret_indices.push_back(precision + 1 + 1);
      turret_indices.push_back(precision + 1 + 2);
      turret_indices.push_back(precision + 1 + 3);
      huge_tank_reswaut_turret_model_index =
          mgr->RegisterModel(turret_vertices, turret_indices);
    }
  }
}

void HugeTankReswaut::Render() {
  battle_game::SetTransformation(position_, rotation_);
  battle_game::SetTexture(0);
  battle_game::SetColor(game_core_->GetPlayerColor(player_id_));
  battle_game::DrawModel(huge_tank_reswaut_body_model_index);
  battle_game::SetRotation(turret_rotation_);
  battle_game::DrawModel(huge_tank_reswaut_turret_model_index);
}

void HugeTankReswaut::Update() {
  TankMove(3.0f, glm::radians(180.0f));
  TurretRotate();
  Fire();
}

void HugeTankReswaut::TankMove(float move_speed, float rotate_angular_speed) {
  auto player = game_core_->GetPlayer(player_id_);
  if (player) {
    auto &input_data = player->GetInputData();
    glm::vec2 offset{0.0f};
    if (input_data.key_down[GLFW_KEY_W]) {
      offset.y += 1.0f;
    }
    if (input_data.key_down[GLFW_KEY_S]) {
      offset.y -= 1.0f;
    }
    float speed = move_speed * GetSpeedScale() * 2.0f;
    offset *= kSecondPerTick * speed;
    auto new_position =
        position_ + glm::vec2{glm::rotate(glm::mat4{1.0f}, rotation_,
                                          glm::vec3{0.0f, 0.0f, 1.0f}) *
                              glm::vec4{offset, 0.0f, 0.0f}};
    if (!game_core_->IsBlockedByObstacles(new_position)) {
      game_core_->PushEventMoveUnit(id_, new_position);
    }
    float rotation_offset = 0.0f;
    if (input_data.key_down[GLFW_KEY_A]) {
      rotation_offset += 1.0f;
    }
    if (input_data.key_down[GLFW_KEY_D]) {
      rotation_offset -= 1.0f;
    }
    rotation_offset *= kSecondPerTick * rotate_angular_speed * GetSpeedScale();
    game_core_->PushEventRotateUnit(id_, rotation_ + rotation_offset);
  }
}

void HugeTankReswaut::TurretRotate() {
  auto player = game_core_->GetPlayer(player_id_);
  if (player) {
    auto &input_data = player->GetInputData();
    auto diff = input_data.mouse_cursor_position - position_;
    if (glm::length(diff) < 1e-4) {
      turret_rotation_ = rotation_;
    } else {
      turret_rotation_ = std::atan2(diff.y, diff.x) - glm::radians(90.0f);
    }
  }
}

void HugeTankReswaut::Fire() {
  if (fire_count_down_ == 0) {
    auto player = game_core_->GetPlayer(player_id_);
    if (player) {
      auto &input_data = player->GetInputData();
      if (input_data.mouse_button_down[GLFW_MOUSE_BUTTON_LEFT]) {
        auto velocity = Rotate(glm::vec2{0.0f, 40.0f}, turret_rotation_);
        GenerateBullet<bullet::CannonBall>(
            position_ + Rotate({0.0f, 2.4f}, turret_rotation_),
            turret_rotation_, GetDamageScale(), velocity);
        fire_count_down_ = kTickPerSecond;  // Fire interval 1 second.
      }
    }
  }
  if (fire_count_down_) {
    fire_count_down_--;
  }
}

bool HugeTankReswaut::IsHit(glm::vec2 position) const {
  position = WorldToLocal(position);
  return position.x > -1.6f && position.x < 1.6f && position.y > -2.0f &&
         position.y < 2.0f && position.x + position.y < 3.2f &&
         position.y - position.x < 3.2f;
}

const char *HugeTankReswaut::UnitName() const {
  return "Huge Tank";
}

const char *HugeTankReswaut::Author() const {
  return "Reswaut";
}
}  // namespace battle_game::unit
