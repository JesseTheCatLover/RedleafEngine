//  Copyright 2025-2026 JesseTheCatLover. All Rights Reserved.

#include "AssetBrowserPanel.h"

#include <vector>
#include <cctype>
#include <iostream>

#include "imgui.h"
#include "imgui_internal.h"

#include "Panels/Controllers/Inputs/FAssetBrowserPanelInput.h"
#include "Panels/Controllers/Outputs/FAssetBrowserOutput.h"

#include "EditorCore/EditorHost.h"
#include "Panels/Subsystems/AssetBrowserSubsystem.h"
#include "InkBlue/UI/Dialogs/AssetImporterDialog.h"

namespace
{
    static bool DrawToolbarButton(const char* label, const char* tooltip = nullptr)
    {
        const bool pressed = ImGui::Button(label);
        if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("%s", tooltip);
        return pressed;
    }

    static bool Splitter(bool vertical, float thickness,
                         float* size0, float* size1,
                         float min_size0, float min_size1)
    {
        ImVec2 backup_pos = ImGui::GetCursorPos();

        ImGui::PushStyleColor(ImGuiCol_Button,        ImGui::GetStyleColorVec4(ImGuiCol_Separator));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_SeparatorHovered));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImGui::GetStyleColorVec4(ImGuiCol_SeparatorActive));

        ImGui::Button("##splitter", vertical ? ImVec2(thickness, -1.0f) : ImVec2(-1.0f, thickness));

        ImGui::PopStyleColor(3);

        ImGui::SetNextItemAllowOverlap();

        bool changed = false;
        if (ImGui::IsItemActive())
        {
            float delta = vertical ? ImGui::GetIO().MouseDelta.x : ImGui::GetIO().MouseDelta.y;

            if (delta < 0.0f && *size0 + delta < min_size0) delta = min_size0 - *size0;
            if (delta > 0.0f && *size1 - delta < min_size1) delta = *size1 - min_size1;

            *size0 += delta;
            *size1 -= delta;
            changed = (delta != 0.0f);
        }

        ImGui::SetCursorPos(backup_pos);
        return changed;
    }

    static std::string TruncateWithEllipsis(const std::string& text, float maxWidth)
    {
        const std::string ellipsis = "...";

        if (ImGui::CalcTextSize(text.c_str()).x <= maxWidth)
            return text;

        std::string result;
        result.reserve(text.size());

        for (const char i : text)
        {
            result.push_back(i);

            std::string test = result + ellipsis;

            if (ImGui::CalcTextSize(test.c_str()).x > maxWidth)
            {
                result.pop_back();
                return result + ellipsis;
            }
        }

        return result + ellipsis;
    }

    static void NormalizePathSlashes(std::string& s)
    {
        for (char& c : s)
            if (c == '\\') c = '/';
    }

    static std::vector<std::string> SplitPath(const std::string& path)
    {
        std::vector<std::string> parts;
        std::string cur;

        for (char c : path)
        {
            if (c == '/')
            {
                if (!cur.empty()) parts.push_back(cur);
                cur.clear();
            }
            else
            {
                cur.push_back(c);
            }
        }

        if (!cur.empty())
            parts.push_back(cur);

        return parts;
    }

    static std::string JoinPathPrefix(const std::vector<std::string>& parts, int countInclusive)
    {
        std::string out;
        for (int i = 0; i <= countInclusive; ++i)
        {
            out += parts[i];
            if (i != countInclusive) out += "/";
        }
        return out;
    }

    static bool CaseInsensitiveContains(const std::string& text, const char* queryCStr)
    {
        if (!queryCStr || queryCStr[0] == '\0')
            return true;

        std::string q(queryCStr);
        std::string t(text);

        auto lower = [](unsigned char ch) { return (char)std::tolower(ch); };
        for (char& c : q) c = lower((unsigned char)c);
        for (char& c : t) c = lower((unsigned char)c);

        return t.find(q) != std::string::npos;
    }
} // namespace

void AssetBrowserPanel::OnDestroy(EditorHost& /*host*/)
{
    // Clear any cached UI state if desired
    m_SearchBuf[0] = '\0';
}

void AssetBrowserPanel::Draw(EditorHost& host)
{
    if (!ImGui::Begin(GetName()))
    {
        ImGui::End();
        return;
    }

    // 1) Get subsystem
    AssetBrowserSubsystem& subsystem = host.GetSubsystem<AssetBrowserSubsystem>();

    // 2) Retrieve last output snapshot for this panel key
    const FAssetBrowserOutput* output = subsystem.GetOutput(GetPanelKey());

    // 3) Build input (panel -> controller commands)
    FAssetBrowserPanelInput input{};
    input.panelKey = GetPanelKey();

    input.bFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    input.bHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    ImGuiIO& io = ImGui::GetIO();
    input.bCtrl  = io.KeyCtrl;
    input.bShift = io.KeyShift;
    input.bAlt   = io.KeyAlt;
    input.bSuper = io.KeySuper;

    if (!output || !output->bValid)
    {
        ImGui::TextDisabled("Asset Browser: no output yet.");
        ImGui::TextDisabled("PanelKey: %s", GetPanelKey());
        ImGui::End();

        // Still submit input so controller can bootstrap on first frame
        subsystem.SubmitInput(input);
        return;
    }

    input.bNavigateToPath = false;
    input.bNavigateUp     = false;
    input.bNavigateHome   = false;

    // ---------------- Toolbar ----------------

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.f, 3.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.f, 2.f));
    if (DrawToolbarButton("Create", "Add a new asset"))
    {
    }

    ImGui::SameLine();
    if (DrawToolbarButton("Import", "Import Assets From System Files"))
    {
        if (auto importerDialog = host.GetDialogManager().OpenDialog<AssetImporterDialog>())
        {
            importerDialog->SetDefaultDestinationPath(output->currentContentNavigationPath);
        }
    }

    ImGui::SameLine();
    if (DrawToolbarButton("Save All", "Save all the edited assets"))
    {

    }
    ImGui::PopStyleVar(2);

    // ---------------- Navigation Bar ----------------
    {
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2.f);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.f, 3.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.f, 3.f));

        ImGui::BeginGroup();

        if (DrawToolbarButton("Home", "Go to assets root"))
            input.bNavigateHome = true;

        ImGui::SameLine();
        if (DrawToolbarButton("Up", "Go to parent directory"))
            input.bNavigateUp = true;

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical, 1.f);

        ImGui::SameLine();
        ImGui::BeginDisabled(!output->bCanNavigateBack);
        if (ImGui::ArrowButton("##Back", ImGuiDir_Left))
        {
            input.bNavigatePrevious = true;
        }
        ImGui::EndDisabled();

        ImGui::SameLine();

        ImGui::BeginDisabled(!output->bCanNavigateForward);
        if (ImGui::ArrowButton("##Forward", ImGuiDir_Right))
        {
            input.bNavigateNext = true;
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical, 1.f);

        ImGui::EndGroup();
    }

    // ---------------- Breadcrumbs ----------------
    {
        ImGui::SameLine();
        const std::string currentPath = output->currentContentNavigationPath;

        if (m_bEditingBreadcrumbs)
        {
            ImGui::SetNextItemWidth(-FLT_MIN);

            if (ImGui::InputText("##BreadcrumbEditor",
                m_BreadcrumbEditBuffer,
                sizeof(m_BreadcrumbEditBuffer),
                ImGuiInputTextFlags_EnterReturnsTrue))
            { // Returned successfully:

                if (std::string newPath = m_BreadcrumbEditBuffer; IsValidPath(newPath, host))
                {
                    input.bNavigateToPath = true;
                    input.navigateToPath = newPath;
                }
                else
                {
                    // invalid -> revert
                    input.bNavigateToPath = true;
                    input.navigateToPath = m_BreadcrumbOriginalPath;
                }

                EndBreadcrumbEditing();
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                EndBreadcrumbEditing();
            }
        }
        else
        {
            ImVec2 regionStart = ImGui::GetCursorScreenPos();

            const auto parts = SplitPath(currentPath);

            if (parts.empty())
            {
                ImGui::TextUnformatted("/Project");
            }
            else
            {
                for (int i = 0; i < (int) parts.size(); ++i)
                {
                    ImGui::PushID(i);

                    ImGui::PushStyleVar(
                        ImGuiStyleVar_FramePadding,
                        ImVec2(6, 4));

                    const bool clicked =
                            ImGui::SmallButton(parts[i].c_str());

                    ImGui::PopStyleVar();

                    if (clicked)
                    {
                        input.bNavigateToPath = true;
                        input.navigateToPath =
                                JoinPathPrefix(parts, i);
                    }

                    ImGui::PopID();

                    if (i != (int) parts.size() - 1)
                    {
                        ImGui::SameLine();
                        ImGui::TextUnformatted("/");
                        ImGui::SameLine();
                    }
                }
            }

            float rowWidth = ImGui::GetContentRegionAvail().x;
            ImVec2 regionEnd(regionStart.x + rowWidth, ImGui::GetCursorScreenPos().y);

            const ImVec2 mouse = ImGui::GetMousePos();

            const bool insideBreadcrumbRegion =
                mouse.x >= regionStart.x &&
                mouse.x <= regionEnd.x &&
                mouse.y >= regionStart.y &&
                mouse.y <= regionEnd.y;

            if (insideBreadcrumbRegion &&
                ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                BeginBreadcrumbEditing(currentPath);
            }
        }
        ImGui::PopStyleVar(2);
    }

    // ---------------- Split layout ----------------
    ImVec2 avail = ImGui::GetContentRegionAvail();

    // Clamp widths
    if (m_LeftPaneWidth < m_MinLeftPaneWidth)
        m_LeftPaneWidth = m_MinLeftPaneWidth;

    float splitterThickness = 6.0f;
    float rightWidth = avail.x - m_LeftPaneWidth - splitterThickness;
    if (rightWidth < m_MinRightPaneWidth)
    {
        rightWidth = m_MinRightPaneWidth;
        m_LeftPaneWidth = avail.x - rightWidth - splitterThickness;
        if (m_LeftPaneWidth < m_MinLeftPaneWidth)
            m_LeftPaneWidth = m_MinLeftPaneWidth;
    }

    // Left pane: Tree view
    ImGui::BeginChild("##AssetBrowserLeft", ImVec2(m_LeftPaneWidth, 0.0f), true);
    {
        const auto& treeView = output->treeView;

        for (AssetBrowserNodeID rootID : treeView.viewNodeIDs)
        {
            DrawTreeNode(rootID, treeView, output->selectedTreeNodes, input);
        }

        // Clear selection when clicking empty space in the tree pane
        if (ImGui::IsWindowHovered() &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !ImGui::IsAnyItemHovered())
        {
            input.bClearTreeSelection = true;
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Splitter (draggable)
    {
        ImGui::BeginGroup();
        float left = m_LeftPaneWidth;
        float right = avail.x - left - splitterThickness;

        // draw splitter as a child-sized button
        ImGui::InvisibleButton("##splitter_hit", ImVec2(splitterThickness, avail.y));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - splitterThickness); // keep position

        // Use our splitter logic with a real button so it can be dragged.
        // We'll render the button at same place:
        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX(), ImGui::GetCursorPosY() - avail.y));
        // simpler: just call Splitter which creates the button:
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 0.0f);
        Splitter(true, splitterThickness, &left, &right, m_MinLeftPaneWidth, m_MinRightPaneWidth);

        m_LeftPaneWidth = left;
        ImGui::EndGroup();
    }

    ImGui::SameLine();

    // Right pane: grid
    ImGui::BeginChild("##AssetBrowserRight", ImVec2(0.0f, 0.0f), true);
    {
        ImGui::TextUnformatted("Assets");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.0f);
        ImGui::SliderFloat("##IconSize", &m_IconSize, 32.0f, 128.0f, "Icon %.0f");

        ImGui::Separator();

        const float cellW = m_IconSize + (m_GridPadding * 2.0f) + 40.0f;
        const float regionW = ImGui::GetContentRegionAvail().x;

        int columns = (int)(regionW / cellW);
        if (columns < 1) columns = 1;

        if (ImGui::BeginTable("##AssetGrid", columns, ImGuiTableFlags_SizingFixedFit))
        {
            const FAssetBrowserViewProjection& content = output->contentView;

            if (content.viewNodeIDs.empty())
            {
                ImGui::BeginChild("ContentTileView", ImVec2(cellW, 0.0f), true);
                ImGui::TextDisabled("Empty folder");
                ImGui::EndChild();
            }

            for (AssetBrowserNodeID nodeID : content.viewNodeIDs)
            {
                auto it = content.nodeCache.find(nodeID);
                if (it == content.nodeCache.end())
                    continue;

                const FAssetBrowserNode& node = it->second;
                const bool bSelected = output->selectedContentNodes.contains(node.nodeID);

                switch (node.type)
                {
                    case EAssetBrowserNodeType::Folder:
                        DrawFolderTile(node, bSelected, cellW, input, *output);
                        break;

                    case EAssetBrowserNodeType::Asset:
                        DrawAssetTile(node, bSelected, cellW, input, *output);
                        break;
                }
            }

            if (ImGui::IsWindowHovered() &&
                (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right) ) &&
                !ImGui::IsAnyItemHovered())
            {
                input.bClearContentSelection = true;
                m_bItemPopupsOpen = false;
            }

            if (!m_bItemPopupsOpen)
            {
                if (ImGui::BeginPopupContextWindow("##AssetGridContext",
                   ImGuiPopupFlags_MouseButtonRight |
                   ImGuiPopupFlags_NoOpenOverItems))
                {
                    if (ImGui::MenuItem("Create Folder", "Cmd+N"))
                    {
                        FAssetBrowserPanelInput::FMutationRequest req;
                        req.type = FAssetBrowserPanelInput::EMutationType::CreateFolder;
                        req.destinationPath = output->currentContentNavigationPath;

                        input.mutations.push_back(req);
                    }

                    ImGui::Separator();

                    if (ImGui::BeginMenu("Create Schematic"))
                    {
                        if (ImGui::MenuItem("ActorSchematic"))
                        {

                        }
                        ImGui::EndMenu();
                    }

                    ImGui::EndPopup();
                }
            }

            ImGui::EndTable();
        }
    }
    ImGui::EndChild();

    ImGui::End();

    // 5) Submit input back
    subsystem.SubmitInput(input);
}

void AssetBrowserPanel::DrawFolderTile(const FAssetBrowserNode& node, bool bSelected, float cellW, FAssetBrowserPanelInput& input,
    const FAssetBrowserOutput& output)
{
    ImGui::TableNextColumn();

    ImGui::PushID(node.nodeID);

    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 tileSize(cellW - m_GridPadding, m_IconSize + 40.0f);
    const ImVec2 p1(p0.x + tileSize.x, p0.y + tileSize.y);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::InvisibleButton("##FolderTile", tileSize);

    if (ImGui::IsItemHovered())
    {
        dl->AddRectFilled( // Unselected hover highlight
           p0,
           p1,
           IM_COL32(255,255,255,20),
           4.0f);
    }

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        FAssetBrowserPanelInput::FNodeSelection sel;
        sel.nodeID = node.nodeID;
        sel.bToggle = input.bCtrl;
        sel.bRange = input.bShift;

        input.contentSelections.push_back(sel);
    }

    if (ImGui::IsItemHovered() &&
        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        input.bNavigateToPath = true;
        input.navigateToPath = node.virtualPath;
    }

    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
    {
        m_ContextMenuNode = node.nodeID;

        if (!bSelected)
        {
            input.bClearContentSelection = true;

            FAssetBrowserPanelInput::FNodeSelection sel;
            sel.nodeID = node.nodeID;

            input.contentSelections.push_back(sel);
        }

        ImGui::OpenPopup("##FolderItemContextMenu"); // TODO: To make the context menu behave better and stop stealing the focus... render a custom window as the context menu
        m_bItemPopupsOpen = true;
    }

    if (ImGui::BeginPopup("##FolderItemContextMenu"))
    {
        if (ImGui::MenuItem("Rename", "Cmd+R"))
        {
            FAssetBrowserPanelInput::FMutationRequest req;
            req.type = FAssetBrowserPanelInput::EMutationType::Rename;
            req.nodeID = node.nodeID;

            input.mutations.push_back(req);
        }

        if (ImGui::MenuItem("Copy")) // TODO: Implement these
        {
            // FAssetBrowserPanelInput::FMutationRequest req;
            // req.type = FAssetBrowserPanelInput::EMutationType::Copy;
            // req.nodeID = node.nodeID;
            //
            // input.mutations.push_back(req);
        }

        if (ImGui::MenuItem("Move"))
        {
            // FAssetBrowserPanelInput::FMutationRequest req;
            // req.type = FAssetBrowserPanelInput::EMutationType::Move;
            // req.nodeID = node.nodeID;
            //
            // input.mutations.push_back(req);
        }

        if (ImGui::MenuItem("Delete"))
        {
            FAssetBrowserPanelInput::FMutationRequest req;
            req.type = FAssetBrowserPanelInput::EMutationType::Delete;
            req.nodeID = node.nodeID;

            input.mutations.push_back(req);
        }

        ImGui::EndPopup();
    }

    const ImVec2 center(
        p0.x + tileSize.x * 0.5f,
        p0.y + tileSize.y * 0.5f
    );

    const ImVec2 iconSize(m_IconSize, m_IconSize);

    const ImVec2 iconP0(
        center.x - iconSize.x * 0.5f,
        p0.y + m_GridPadding
    );

    const ImVec2 iconP1(
        iconP0.x + iconSize.x,
        iconP0.y + iconSize.y
    );

    const float maxTextWidth = tileSize.x - 8.0f;

    const std::string displayName =
    TruncateWithEllipsis(node.GetDisplayName(), maxTextWidth);

    const ImVec2 textSize = ImGui::CalcTextSize(displayName.c_str());

    const ImVec2 textPos(
        p0.x + (tileSize.x - textSize.x) * 0.5f,
        iconP1.y + 6.0f
    );

    if (bSelected)
    {
        dl->AddRectFilled(
            p0,
            p1,
            IM_COL32(60, 120, 255, 100),
            6.0f);
    }

    auto icon = node.HasAnyChildren() ? output.icons.folder : output.icons.folderEmpty;
    dl->AddImage(
       (ImTextureID)icon,
       iconP0,
       iconP1,
       ImVec2(0,1),
       ImVec2(1, 0));

    dl->AddText(
        textPos,
        IM_COL32(230, 230, 230, 255),
        displayName.c_str());

    ImGui::PopID();
}

void AssetBrowserPanel::DrawAssetTile(const FAssetBrowserNode& node, bool bSelected, float cellW, FAssetBrowserPanelInput& input,
    const FAssetBrowserOutput& output)
{
    ImGui::TableNextColumn();

    ImGui::PushID(node.nodeID);

    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 tileSize(cellW - m_GridPadding, m_IconSize + 40.0f);
    const ImVec2 p1(p0.x + tileSize.x, p0.y + tileSize.y);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::InvisibleButton("##AssetTile", tileSize);

    if (ImGui::IsItemHovered())
    {
        dl->AddRectFilled( // Unselected hover highlight
           p0,
           p1,
           IM_COL32(255,255,255,20),
           4.0f);
    }

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        FAssetBrowserPanelInput::FNodeSelection sel;
        sel.nodeID = node.nodeID;
        sel.bToggle = input.bCtrl;
        sel.bRange = input.bShift;

        input.contentSelections.push_back(sel);
    }

    if (ImGui::IsItemHovered() &&
        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        input.bOpenNode = true;
        input.openNodeID = node.nodeID;
    }

    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
    {
        m_ContextMenuNode = node.nodeID;

        if (!bSelected)
        {
            input.bClearContentSelection = true;

            FAssetBrowserPanelInput::FNodeSelection sel;
            sel.nodeID = node.nodeID;

            input.contentSelections.push_back(sel);
        }
        ImGui::OpenPopup("##AssetItemContextMenu");
        m_bItemPopupsOpen = true;
    }

    if (ImGui::BeginPopup("##AssetItemContextMenu"))
    {
        if (ImGui::MenuItem("Rename", "Cmd+R"))
        {
            FAssetBrowserPanelInput::FMutationRequest req;
            req.type = FAssetBrowserPanelInput::EMutationType::Rename;
            req.nodeID = node.nodeID;

            input.mutations.push_back(req);
        }

        if (ImGui::MenuItem("Copy")) // TODO: Implement these
        {
            // FAssetBrowserPanelInput::FMutationRequest req;
            // req.type = FAssetBrowserPanelInput::EMutationType::Copy;
            // req.nodeID = node.nodeID;
            //
            // input.mutations.push_back(req);
        }

        if (ImGui::MenuItem("Move"))
        {
            // FAssetBrowserPanelInput::FMutationRequest req;
            // req.type = FAssetBrowserPanelInput::EMutationType::Move;
            // req.nodeID = node.nodeID;
            //
            // input.mutations.push_back(req);
        }

        if (ImGui::MenuItem("Duplicate"))
        {
            FAssetBrowserPanelInput::FMutationRequest req;
            req.type = FAssetBrowserPanelInput::EMutationType::Duplicate;
            req.nodeID = node.nodeID;

            input.mutations.push_back(req);
        }

        if (ImGui::MenuItem("Delete"))
        {
            FAssetBrowserPanelInput::FMutationRequest req;
            req.type = FAssetBrowserPanelInput::EMutationType::Delete;
            req.nodeID = node.nodeID;

            input.mutations.push_back(req);
        }

        ImGui::EndPopup();
    }

    const ImVec2 center(
        p0.x + tileSize.x * 0.5f,
        p0.y + tileSize.y * 0.5f
    );

    const ImVec2 iconSize(m_IconSize, m_IconSize);

    const ImVec2 iconP0(
        center.x - iconSize.x * 0.5f,
        p0.y + m_GridPadding
    );

    const ImVec2 iconP1(
        iconP0.x + iconSize.x,
        iconP0.y + iconSize.y
    );

    const float maxTextWidth = tileSize.x - 8.0f;

    const std::string displayName =
    TruncateWithEllipsis(node.GetDisplayName(), maxTextWidth);

    const ImVec2 textSize = ImGui::CalcTextSize(displayName.c_str());

    const ImVec2 textPos(
        p0.x + (tileSize.x - textSize.x) * 0.5f,
        iconP1.y + 6.0f
    );

    if (bSelected)
    {
        dl->AddRectFilled(
            p0,
            p1,
            IM_COL32(60, 120, 255, 100),
            6.0f);
    }

    dl->AddImage(
       (ImTextureID)output.icons.asset,
       iconP0,
       iconP1,
       ImVec2(0, 1),
       ImVec2(1, 0));

    dl->AddText(
        textPos,
        IM_COL32(230, 230, 230, 255),
        displayName.c_str());

    ImGui::PopID();
}

void AssetBrowserPanel::DrawTreeNode(AssetBrowserNodeID nodeID, const FAssetBrowserViewProjection& treeView,
                                     const std::unordered_set<AssetBrowserNodeID>& selectedNodes, FAssetBrowserPanelInput &input)
{
    auto nodeIt = treeView.nodeCache.find(nodeID);
    if (nodeIt == treeView.nodeCache.end())
        return;

    const FAssetBrowserNode& node = nodeIt->second;

    ImGuiTreeNodeFlags flags = 0;
    flags |= ImGuiTreeNodeFlags_OpenOnDoubleClick;
    flags |= ImGuiTreeNodeFlags_OpenOnArrow;
    flags |= ImGuiTreeNodeFlags_SpanAvailWidth;

    if (!node.HasFolderChildren())
    {
        flags |= ImGuiTreeNodeFlags_Leaf;

    }

    const bool bSelected = selectedNodes.contains(nodeID);

    if (bSelected)
    {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    ImGui::PushID((void*)(uintptr_t)nodeID);

    bool opened = ImGui::TreeNodeEx(
        (void*)(uintptr_t)nodeID,
        flags,
        "%s",
        node.GetDisplayName().c_str());

    // Left-click selection (skip when the click was an arrow toggle)
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
    {
        FAssetBrowserPanelInput::FNodeSelection sel;
        sel.nodeID  = node.nodeID;
        sel.bToggle = input.bCtrl;
        sel.bRange  = input.bShift;

        input.treeSelections.push_back(sel);
    }

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        input.bNavigateToPath = true;
        input.navigateToPath = node.virtualPath;
    }

    if (ImGui::IsItemToggledOpen())
    {
        if (opened)
            input.expandNodes.push_back(nodeID);
        else
            input.collapseNodes.push_back(nodeID);
    }

    // Right-click context menu
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
    {
        m_ContextMenuNode = node.nodeID;

        // Right-clicking an unselected node selects just it;
        // right-clicking inside a multi-selection keeps the selection.
        if (!bSelected)
        {
            input.bClearTreeSelection = true;

            FAssetBrowserPanelInput::FNodeSelection sel;
            sel.nodeID = node.nodeID;
            input.treeSelections.push_back(sel);
        }

        ImGui::OpenPopup("##TreeItemContextMenu");
        m_bItemPopupsOpen = true;
    }

    if (ImGui::BeginPopup("##TreeItemContextMenu"))
    {
        if (ImGui::MenuItem("Rename", "Cmd+R"))
        {
            FAssetBrowserPanelInput::FMutationRequest req;
            req.type   = FAssetBrowserPanelInput::EMutationType::Rename;
            req.nodeID = node.nodeID;
            input.mutations.push_back(req);
        }

        if (ImGui::MenuItem("New Folder", "Cmd+N"))
        {
            FAssetBrowserPanelInput::FMutationRequest req;
            req.type            = FAssetBrowserPanelInput::EMutationType::CreateFolder;
            req.destinationPath = node.virtualPath; // create inside this folder
            input.mutations.push_back(req);
        }

        if (ImGui::MenuItem("Move"))
        {
            // FAssetBrowserPanelInput::FMutationRequest req;
            // req.type = FAssetBrowserPanelInput::EMutationType::Move;
            // req.nodeID = node.nodeID;
            // input.mutations.push_back(req);
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Delete"))
        {
            FAssetBrowserPanelInput::FMutationRequest req;
            req.type   = FAssetBrowserPanelInput::EMutationType::Delete;
            req.nodeID = node.nodeID;
            input.mutations.push_back(req);
        }

        ImGui::EndPopup();
    }

    if (opened)
    {
        auto childIt = treeView.children.find(node.nodeID);
        if (childIt != treeView.children.end())
        {
            for (AssetBrowserNodeID childID : childIt->second)
            {
                DrawTreeNode(childID, treeView, selectedNodes, input);
            }
        }

        ImGui::TreePop();
    }

    ImGui::PopID();
}

void AssetBrowserPanel::BeginBreadcrumbEditing(const std::string& currentPath)
{
    m_bEditingBreadcrumbs = true;
    m_BreadcrumbOriginalPath = currentPath;

    std::snprintf(m_BreadcrumbEditBuffer,
        sizeof(m_BreadcrumbEditBuffer),
        "%s",
        currentPath.c_str()
     );
}

void AssetBrowserPanel::EndBreadcrumbEditing()
{
    m_bEditingBreadcrumbs = false;
}

bool AssetBrowserPanel::IsValidPath(const std::string &path, EditorHost &host)
{
    auto& service = host.GetService<AssetBrowserService>();

    const AssetBrowserNodeID id = service.TryGetID(path);

    return id != 0;

}
