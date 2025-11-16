#include <TiltedOnlinePCH.h>

#include <Services/BrandingService.h>

#include <Services/ImguiService.h>
#include <World.h>

#include <Games/Skyrim/Interface/UI.h>
#include <imgui.h>

BrandingService::BrandingService(World& aWorld, entt::dispatcher& aDispatcher) noexcept
    : m_world(aWorld)
{
    // Hook ImGui draw
    auto& imgui = m_world.ctx().at<ImguiService>();
    m_drawImGuiConnection = imgui.OnDraw.connect<&BrandingService::OnDraw>(this);
}

void BrandingService::OnDraw() noexcept
{
    UI* pUI = UI::Get();
    if (!pUI)
        return;

    // Main menu names vary; check a few common ones
    const bool onMainMenu = pUI->GetMenuOpen(BSFixedString("Main Menu"))
                         || pUI->GetMenuOpen(BSFixedString("MainMenu"))
                         || pUI->GetMenuOpen(BSFixedString("Title Menu"));

    if (!onMainMenu)
        return;

    auto& imguiSvc = m_world.ctx().at<ImguiService>();
    ImFont* pFont = imguiSvc.GetSkyrimFont();
    if (pFont)
        ImGui::PushFont(pFont);

    // Draw message at top-left with slight shadow for readability
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const float line = ImGui::GetFontSize();
    const ImVec2 pos = ImVec2(20.f, 20.f);

    const char* line1 = "Skyrim Together";
    const char* line2 = "FazeUnion Edition";

    // Line 1 shadow + text
    draw->AddText(ImVec2(pos.x + 1.f, pos.y + 1.f), IM_COL32(0, 0, 0, 190), line1);
    draw->AddText(pos, IM_COL32(214, 49, 43, 255), line1);

    // Line 2 below
    ImVec2 pos2 = ImVec2(pos.x, pos.y + line + 2.f);
    draw->AddText(ImVec2(pos2.x + 1.f, pos2.y + 1.f), IM_COL32(0, 0, 0, 190), line2);
    draw->AddText(pos2, IM_COL32(214, 49, 43, 255), line2);

    if (pFont)
        ImGui::PopFont();
}
