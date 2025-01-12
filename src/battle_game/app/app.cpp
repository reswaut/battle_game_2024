#include "battle_game/app/app.h"

#include <string>

#include "battle_game/core/object.h"
#include "battle_game/graphics/util.h"
#include "battle_game/localization/localization_manager.h"

namespace {
#include "built_in_shaders.inl"
}

namespace battle_game {
static void HelpMarker(const char *desc) {
  ImGui::TextColored(ImVec4(1, 1, 0, 1), "(?)");
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
    ImGui::TextUnformatted(desc);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

App::App(const AppSettings &app_settings, GameCore *game_core) {
  game_core_ = game_core;
  vulkan_legacy::framework::CoreSettings core_settings;
  static std::string title;
  title = LocalizationManager::GetInstance()->GetLocalizationString({"app", "window_title"});
  core_settings.window_title = title.c_str();
  core_settings.window_width = app_settings.width;
  core_settings.window_height = app_settings.height;
  core_ = std::make_unique<vulkan_legacy::framework::Core>(core_settings);
  SetGlobalCore(core_.get());
}

void App::Run() {
  OnInit();

  while (!glfwWindowShouldClose(core_->GetWindow())) {
    OnLoop();
    glfwPollEvents();
  }

  core_->GetDevice()->WaitIdle();

  OnClose();
}

void App::OnInit() {
  frame_image_ = std::make_unique<vulkan_legacy::framework::TextureImage>(
      core_.get(), core_->GetFramebufferWidth(), core_->GetFramebufferHeight(),
      VK_FORMAT_B8G8R8A8_UNORM);

  core_->SetFrameSizeCallback([this](int width, int height) {
    frame_image_->Resize(width, height);
    BuildRenderNodes();
  });

  device_global_settings_ =
      std::make_unique<vulkan_legacy::framework::DynamicBuffer<GlobalSettings>>(
          core_.get(), 1);
  device_texture_infos_ =
      std::make_unique<vulkan_legacy::framework::DynamicBuffer<TextureInfo>>(
          core_.get(), 1048576);
  device_object_settings_ =
      std::make_unique<vulkan_legacy::framework::DynamicBuffer<ObjectSettings>>(
          core_.get(), 1048576);

  linear_sampler_ = std::make_unique<vulkan_legacy::Sampler>(core_->GetDevice(),
                                                             VK_FILTER_LINEAR);
  nearest_sampler_ = std::make_unique<vulkan_legacy::Sampler>(
      core_->GetDevice(), VK_FILTER_NEAREST);

  SyncDeviceAssets();
  BuildRenderNodes();

  SetScene();
  core_->ImGuiInit(frame_image_.get(),
                   BATTLE_GAME_ASSETS_DIR "fonts/NotoSansSC-Regular.otf",
                   20.0f);
}

void App::OnLoop() {
  OnUpdate();
  OnRender();
}

void App::OnClose() {
}

void App::OnUpdate() {
  UpdateImGui();
  NewFrame();
  CaptureInput();
  UpdateDrawCommands();
  UpdateDynamicBuffer();

  auto mgr = AssetsManager::GetInstance();
  if (!mgr->GetSyncState()) {
    SyncDeviceAssets();
  }
}

void App::OnRender() {
  core_->BeginCommandRecord();
  frame_image_->ClearColor({0.8f, 0.8, 0.8f, 1.0f});

  auto &model_ids = GetModelIds();
  render_node_->BeginDraw();
  for (int obj_id = 0; obj_id < model_ids.size(); obj_id++) {
    auto &device_model = device_models_[model_ids[obj_id]];
    render_node_->DrawDirect(device_model.vertex_buffer.get(),
                             device_model.index_buffer.get(),
                             device_model.index_buffer->Size(), obj_id);
  }
  render_node_->EndDraw();
  core_->TemporalSubmit();
  core_->ImGuiRender();
  core_->Output(frame_image_.get());
  core_->EndCommandRecordAndSubmit();
}

void App::BuildRenderNodes() {
  render_node_ =
      std::make_unique<vulkan_legacy::framework::RenderNode>(core_.get());
  render_node_->AddColorAttachment(frame_image_.get(), true);

  auto vertex_shader = vulkan::CompileGLSLToSPIRV(
      GetShaderCode("shaders/render.vert"), VK_SHADER_STAGE_VERTEX_BIT);
  auto fragment_shader = vulkan::CompileGLSLToSPIRV(
      GetShaderCode("shaders/render.frag"), VK_SHADER_STAGE_FRAGMENT_BIT);
  {
    std::ofstream vertex_shader_file("render.vert.spv", std::ios::binary);
    vertex_shader_file.write((const char *)vertex_shader.data(),
                             vertex_shader.size() * sizeof(uint32_t));
    vertex_shader_file.close();

    std::ofstream fragment_shader_file("render.frag.spv", std::ios::binary);
    fragment_shader_file.write((const char *)fragment_shader.data(),
                               fragment_shader.size() * sizeof(uint32_t));
    fragment_shader_file.close();
  }

  render_node_->AddShader("render.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
  render_node_->AddShader("render.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
  render_node_->VertexInput({VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32_SFLOAT,
                             VK_FORMAT_R32G32B32A32_SFLOAT});
  render_node_->AddBufferBinding(
      device_texture_infos_.get(),
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  std::vector<std::pair<vulkan_legacy::framework::TextureImage *,
                        vulkan_legacy::Sampler *>>
      texture_sampler_pairs;
  for (auto &device_particle_texture : device_particle_textures_) {
    texture_sampler_pairs.emplace_back(device_particle_texture.get(),
                                       linear_sampler_.get());
  }
  render_node_->AddUniformBinding(
      texture_sampler_pairs,
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  render_node_->AddUniformBinding(
      device_global_settings_.get(),
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  render_node_->AddBufferBinding(
      device_object_settings_.get(),
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  render_node_->BuildRenderNode();
}

void App::SyncDeviceAssets() {
  auto mgr = AssetsManager::GetInstance();
  auto &particle_textures = mgr->GetTextures();
  while (device_particle_textures_.size() < particle_textures.size()) {
    auto &particle_texture =
        particle_textures[device_particle_textures_.size()];
    device_particle_textures_.push_back(
        std::make_unique<vulkan_legacy::framework::TextureImage>(
            core_.get(), particle_texture.GetWidth(),
            particle_texture.GetHeight()));
  }
  for (int i = 0; i < device_particle_textures_.size(); i++) {
    auto &particle_texture = particle_textures[i];
    auto host_buffer = particle_texture.GetBuffer();
    vulkan_legacy::Buffer upload_buffer(
        core_->GetDevice(),
        sizeof(glm::vec4) * particle_texture.GetWidth() *
            particle_texture.GetHeight(),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    std::memcpy(upload_buffer.Map(), host_buffer,
                sizeof(glm::vec4) * particle_texture.GetWidth() *
                    particle_texture.GetHeight());
    upload_buffer.Unmap();
    vulkan_legacy::UploadImage(core_->GetCommandPool(),
                               device_particle_textures_[i]->GetImage(),
                               &upload_buffer);
  }

  auto &models = mgr->GetModels();
  while (device_models_.size() < models.size()) {
    auto &model = models[device_models_.size()];
    auto &vertices = model.GetVertices();
    auto &indices = model.GetIndices();
    device_models_.push_back(
        {std::make_unique<vulkan_legacy::framework::StaticBuffer<ObjectVertex>>(
             core_.get(), vertices.size()),
         std::make_unique<vulkan_legacy::framework::StaticBuffer<uint32_t>>(
             core_.get(), indices.size())});
    auto &device_model = *device_models_.rbegin();
    device_model.vertex_buffer->Upload(vertices.data());
    device_model.index_buffer->Upload(indices.data());
  }

  mgr->GetSyncState() = true;
}

void App::UpdateDrawCommands() {
  static auto begin_time = std::chrono::steady_clock::now();
  auto current_time = std::chrono::steady_clock::now();
  auto time_passed =
      double((current_time - begin_time) / std::chrono::nanoseconds(1)) * 1e-9;
  static uint64_t updated_step = 0;
  uint64_t target_update_step = std::lround(time_passed / kSecondPerTick);
  while (updated_step < target_update_step) {
    game_core_->Update();
    updated_step++;
  }
  game_core_->Render();
}

void App::UpdateDynamicBuffer() {
  auto &object_settings = GetObjectSettings();
  auto &texture_infos = GetTextureInfos();
  std::memcpy(&(*device_object_settings_)[0], object_settings.data(),
              object_settings.size() * sizeof(ObjectSettings));
  std::memcpy(&(*device_texture_infos_)[0], texture_infos.data(),
              texture_infos.size() * sizeof(TextureInfo));
  (*device_global_settings_)[0].world_to_camera = GetCameraTransform(fov_y_);
}

void App::CaptureInput() {
  InputData input_data;
  auto window = core_->GetWindow();
  for (int i = 0; i < kKeyRange; i++) {
    input_data.key_down[i] = (glfwGetKey(window, i) == GLFW_PRESS);
  }
  static bool mouse_button_state[kMouseButtonRange] = {};
  for (int i = 0; i < kMouseButtonRange; i++) {
    if (i == GLFW_MOUSE_BUTTON_LEFT && ImGui::GetIO().WantCaptureMouse) {
      input_data.mouse_button_down[i] = false;
      continue;
    }
    input_data.mouse_button_down[i] =
        (glfwGetMouseButton(window, i) == GLFW_PRESS);
    input_data.mouse_button_clicked[i] =
        (input_data.mouse_button_down[i] && !mouse_button_state[i]);
    mouse_button_state[i] = input_data.mouse_button_down[i];
  }
  double xpos, ypos;
  glfwGetCursorPos(window, &xpos, &ypos);
  int width, height;
  glfwGetWindowSize(window, &width, &height);
  xpos += 0.5f;
  ypos += 0.5f;
  input_data.mouse_cursor_position =
      glm::inverse(GetCameraTransform(fov_y_)) *
      glm::vec4{
          (glm::vec2{xpos, ypos} / glm::vec2{width, height}) * 2.0f - 1.0f,
          0.0f, 1.0f};
  game_core_->GetPlayer(my_player_id_)->SetInputData(input_data);
}

void App::SetScene() {
  my_player_id_ = game_core_->AddPlayer();
  auto enemy_player_id = game_core_->AddPlayer();
  game_core_->SetRenderPerspective(my_player_id_);
}

glm::mat4 App::GetCameraTransform(float fov_y) const {
  auto inv_fov_y = 1.0f / fov_y;
  return glm::scale(
             glm::mat4{1.0f},
             glm::vec3{inv_fov_y / ((float)core_->GetFramebufferWidth() /
                                    (float)core_->GetFramebufferHeight()),
                       -inv_fov_y, 1.0f}) *
         glm::inverse(
             glm::translate(glm::mat4{1.0f},
                            glm::vec3{game_core_->GetCameraPosition(), 0.0f}) *
             glm::rotate(glm::mat4{1.0f}, game_core_->GetCameraRotation(),
                         glm::vec3{0.0f, 0.0f, 1.0f}));
}

void App::UpdateImGui() {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  ImGui::SetNextWindowPos(ImVec2{0.0f, 0.0f}, ImGuiCond_Once);
  ImGui::SetNextWindowBgAlpha(0.3f);
  auto mgr = LocalizationManager::GetInstance();
  if (ImGui::Begin(mgr->GetLocalizationString({"app", "debug_window_title"}).c_str(), nullptr,
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar |
                       ImGuiWindowFlags_AlwaysAutoResize)) {
    auto player = game_core_->GetPlayer(my_player_id_);
    if (player) {
      auto selectable_list = game_core_->GetSelectableUnitList();
      ImGui::Combo(mgr->GetLocalizationString({"app", "select_unit_description"}).c_str(), &player->SelectedUnit(),
                   selectable_list.data(), selectable_list.size());
      if (ImGui::Button(mgr->GetLocalizationString({"app", "self_destruct_button"}).c_str())) {
        auto unit = game_core_->GetUnit(player->GetPrimaryUnitId());
        if (unit) {
          game_core_->PushEventRemoveUnit(unit->GetId());
        }
      }
      auto unit = game_core_->GetUnit(player->GetPrimaryUnitId());
      if (unit) {
        ImGui::Text(mgr->GetLocalizationString({"app", "life_total"}).c_str(),
                    unit->GetHealth() * unit->GetMaxHealth(),
                    unit->GetMaxHealth());
        ImGui::ProgressBar(unit->GetHealth());
      } else {
        ImGui::Text(mgr->GetLocalizationString({"app", "wait_for_revival"}).c_str(),
                    player->GetResurrectionCountDown() / kTickPerSecond);
      }
    }
    ImGui::End();
  }
  ImGui::Render();
}
}  // namespace battle_game
