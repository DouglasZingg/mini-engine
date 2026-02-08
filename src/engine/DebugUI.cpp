#include "engine/DebugUI.h"
#include "platform/SdlPlatform.h"

#include <imgui.h>

// These must be included so the ImGui_ImplSDL2_* symbols are declared:
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include <SDL.h>
#include "DebugState.h"
#include <cstdio>

bool DebugUI::Init(SdlPlatform& platform) {
    if (m_initialized) return true;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard; // disables tab/arrow nav

    // Optional: basic style
    ImGui::StyleColorsDark();

    SDL_Window* win = platform.WindowRaw();
    SDL_Renderer* ren = platform.RendererRaw();

    if (!ImGui_ImplSDL2_InitForSDLRenderer(win, ren)) {
        return false;
    }
    if (!ImGui_ImplSDLRenderer2_Init(ren)) {
        return false;
    }

    m_initialized = true;
    return true;
}

void DebugUI::Shutdown() {
    if (!m_initialized) return;

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    m_initialized = false;
}

void DebugUI::BeginFrame() {
    if (!m_initialized || !m_enabled) return;

    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void DebugUI::EndFrame(SdlPlatform& platform) {
    if (!m_initialized || !m_enabled) return;

    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), platform.RendererRaw());
}

void DebugUI::OnSdlEvent(void* userData, const void* sdlEvent) {
    DebugUI* self = reinterpret_cast<DebugUI*>(userData);
    if (!self || !self->m_initialized || !self->m_enabled) return;

    const SDL_Event* e = reinterpret_cast<const SDL_Event*>(sdlEvent);
    ImGui_ImplSDL2_ProcessEvent(e);
}

void DebugUI::Draw(DebugState& dbg) {
    ImGuiIO& io = ImGui::GetIO();
    dbg.imguiWantsKeyboard = io.WantCaptureKeyboard;
    dbg.imguiWantsMouse = io.WantCaptureMouse;

    if (!m_initialized || !m_enabled) return;
    if (!dbg.showUI) return;

    ImGui::Begin("Mini Engine Debug");

    ImGui::Text("Performance");
    ImGui::Text("dt: %.4f", dbg.dt);
    ImGui::Text("fps: %.1f", dbg.fps);
    ImGui::Separator();

    ImGui::Text("World");
    ImGui::Text("player: (%.1f, %.1f)", dbg.playerPos.x, dbg.playerPos.y);
    ImGui::Text("camera: (%.1f, %.1f)", dbg.cameraPos.x, dbg.cameraPos.y);
    ImGui::Text("entities: %d", dbg.entityCount);
    ImGui::Separator();

    ImGui::Checkbox("Show Grid", &dbg.showGrid);
    ImGui::Checkbox("Show Colliders", &dbg.showColliders);
    ImGui::Checkbox("Pause Simulation", &dbg.pause);

    ImGui::SliderFloat("Shake Strength", &dbg.shakeStrength, 0.0f, 25.0f, "%.1f");

    ImGui::SliderFloat("Zoom", &dbg.zoom, 0.5f, 2.0f, "%.2f");
    ImGui::Separator();

    if (ImGui::Button("Reload Config")) {
        dbg.requestReloadConfig = true;
    }

    ImGui::End();

    ImGui::Begin("Entities");

    for (int i = 0; i < dbg.debugEntityCount; ++i) {
        const auto& e = dbg.debugEntities[i];

        char label[64];
        std::snprintf(label, sizeof(label), "%s #%u",
            (e.type == 0) ? "Player" : "Enemy",
            e.id);

        bool selected = (dbg.selectedEntityId == e.id);
        if (ImGui::Selectable(label, selected)) {
            dbg.selectedEntityId = e.id;
        }
    }

    ImGui::Separator();
    ImGui::Text("Selected: %u", dbg.selectedEntityId);

    ImGui::End();
}