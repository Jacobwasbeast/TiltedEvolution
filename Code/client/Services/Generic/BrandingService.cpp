#include <TiltedOnlinePCH.h>

#include <Services/BrandingService.h>

#include <Services/ImguiService.h>
#include <World.h>

#include <Games/Skyrim/Interface/UI.h>
#include <imgui.h>
#include <algorithm>

namespace
{
    enum class BrandingLook
    {
        ClassicShadow,
        GlowPanel,
        AccentBadge,
    };

    constexpr BrandingLook kBrandingLook = BrandingLook::GlowPanel;

    constexpr ImU32 kPrimaryColor = IM_COL32(214, 49, 43, 255);
    constexpr ImU32 kPrimaryGlowOuter = IM_COL32(214, 49, 43, 40);
    constexpr ImU32 kPrimaryGlowInner = IM_COL32(214, 49, 43, 90);
    constexpr ImU32 kSecondaryColor = IM_COL32(255, 255, 255, 240);
    constexpr ImU32 kShadowColor = IM_COL32(0, 0, 0, 210);

    constexpr float kAnchorMarginX = 30.f;
    constexpr float kAnchorMarginY = 30.f;
    constexpr float kPanelPadX = 18.f;
    constexpr float kPanelPadTop = 14.f;
    constexpr float kPanelPadBottom = 10.f;
    constexpr float kPanelTextSpacing = 6.f;
    constexpr float kAccentHeight = 5.f;
    constexpr float kBadgePadLeft = 26.f;
    constexpr float kBadgePadRight = 20.f;
    constexpr float kBadgePadTop = 8.f;
    constexpr float kBadgePadBottom = 6.f;
    constexpr float kBadgeStripWidth = 6.f;
    constexpr float kBadgeLineSpacing = 4.f;

    void DrawLayeredShadowText(ImDrawList* aDraw, const ImVec2& aPos, const char* aText, ImU32 aColor)
    {
        const ImVec2 offsets[] = {
            {1.f, 1.f},
            {2.f, 2.f},
            {-1.f, 1.f},
            {1.f, -1.f},
        };

        for (const ImVec2& offset : offsets)
            aDraw->AddText(ImVec2(aPos.x + offset.x, aPos.y + offset.y), kShadowColor, aText);

        aDraw->AddText(aPos, aColor, aText);
    }

    void DrawSoftGlowText(ImDrawList* aDraw, const ImVec2& aPos, const char* aText)
    {
        const ImVec2 glowOffsets[] = {
            {-2.f, -2.f}, {2.f, 2.f},
            {0.f, -3.f}, {0.f, 3.f},
            {-3.f, 0.f}, {3.f, 0.f},
        };

        for (size_t i = 0; i < IM_ARRAYSIZE(glowOffsets); ++i)
        {
            const ImVec2& offset = glowOffsets[i];
            const ImU32 color = (i % 2) == 0 ? kPrimaryGlowOuter : kPrimaryGlowInner;
            aDraw->AddText(ImVec2(aPos.x + offset.x, aPos.y + offset.y), color, aText);
        }

        aDraw->AddText(aPos, kPrimaryColor, aText);
    }

    void DrawClassicShadow(ImDrawList* aDraw, const ImVec2& aPos, float aLineHeight, const char* aLine1, const char* aLine2)
    {
        DrawLayeredShadowText(aDraw, aPos, aLine1, kPrimaryColor);

        ImVec2 pos2 = ImVec2(aPos.x, aPos.y + aLineHeight + 4.f);
        DrawLayeredShadowText(aDraw, pos2, aLine2, kSecondaryColor);
    }

    void DrawGlowPanel(ImDrawList* aDraw, const ImVec2& aPos, const char* aLine1, const char* aLine2)
    {
        const ImVec2 size1 = ImGui::CalcTextSize(aLine1);
        const ImVec2 size2 = ImGui::CalcTextSize(aLine2);
        const float maxWidth = std::max(size1.x, size2.x);
        const float totalHeight = size1.y + size2.y + 6.f;

        const ImVec2 panelMin = ImVec2(aPos.x - kPanelPadX, aPos.y - kPanelPadTop);
        const ImVec2 panelMax = ImVec2(aPos.x + maxWidth + kPanelPadX, aPos.y + totalHeight + kPanelPadBottom);

        aDraw->AddRectFilledMultiColor(panelMin, panelMax,
                                       IM_COL32(8, 8, 8, 220), IM_COL32(24, 24, 24, 220),
                                       IM_COL32(10, 10, 10, 220), IM_COL32(4, 4, 4, 220));
        aDraw->AddRect(panelMin, panelMax, IM_COL32(255, 255, 255, 40), 6.f);

        const ImVec2 accentMin = ImVec2(panelMin.x, panelMin.y);
        const ImVec2 accentMax = ImVec2(panelMax.x, panelMin.y + kAccentHeight);
        aDraw->AddRectFilled(accentMin, accentMax, kPrimaryColor, 6.f, 0);

        DrawSoftGlowText(aDraw, aPos, aLine1);

        ImVec2 subPos = ImVec2(aPos.x, aPos.y + size1.y + kPanelTextSpacing);
        DrawLayeredShadowText(aDraw, subPos, aLine2, kSecondaryColor);
    }

    void DrawAccentBadge(ImDrawList* aDraw, const ImVec2& aPos, const char* aLine1, const char* aLine2)
    {
        const ImVec2 size1 = ImGui::CalcTextSize(aLine1);
        const ImVec2 size2 = ImGui::CalcTextSize(aLine2);
        const float width = std::max(size1.x, size2.x);
        const float height = size1.y + size2.y + 10.f;

        const ImVec2 badgeMin = ImVec2(aPos.x - kBadgePadLeft, aPos.y - kBadgePadTop);
        const ImVec2 badgeMax = ImVec2(aPos.x + width + kBadgePadRight, aPos.y + height + kBadgePadBottom);

        aDraw->AddRectFilled(badgeMin, badgeMax, IM_COL32(6, 6, 6, 200), 5.f);
        aDraw->AddRect(badgeMin, badgeMax, IM_COL32(255, 255, 255, 25), 5.f);

        const ImVec2 stripMin = ImVec2(badgeMin.x - kBadgeStripWidth, badgeMin.y);
        const ImVec2 stripMax = ImVec2(badgeMin.x, badgeMax.y);
        aDraw->AddRectFilled(stripMin, stripMax, kPrimaryColor, 5.f);

        DrawLayeredShadowText(aDraw, aPos, aLine1, kSecondaryColor);

        ImVec2 tagPos = ImVec2(aPos.x, aPos.y + size1.y + kBadgeLineSpacing);
        aDraw->AddText(tagPos, kPrimaryColor, aLine2);
    }
}

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
    {
        ImGui::PushFont(pFont);
    }
    else
    {
        spdlog::debug("BrandingService: Skyrim font unavailable, using default ImGui font");
    }

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 origin = viewport ? viewport->Pos : ImVec2(0.f, 0.f);
    const ImVec2 pos = ImVec2(origin.x + kAnchorMarginX, origin.y + kAnchorMarginY);

    const char* line1 = "Skyrim Together";
    const char* line2 = "FazeUnion Edition";

    switch (kBrandingLook)
    {
    case BrandingLook::ClassicShadow:
        DrawClassicShadow(draw, pos, ImGui::GetFontSize(), line1, line2);
        break;
    case BrandingLook::GlowPanel:
        DrawGlowPanel(draw, pos, line1, line2);
        break;
    case BrandingLook::AccentBadge:
        DrawAccentBadge(draw, pos, line1, line2);
        break;
    }

    if (pFont)
        ImGui::PopFont();
}
