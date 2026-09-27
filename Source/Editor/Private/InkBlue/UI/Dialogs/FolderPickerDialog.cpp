//  Copyright 2025-2026 JesseTheCatLover. All Rights Reserved.

#include "FolderPickerDialog.h"

#include <unordered_set>
#include <algorithm>
#include <functional>

#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "Utilities/UPath.h"
#include "EditorRuntime.h"
#include "EditorCore/EditorHost.h"
#include "EditorCore/Services/AssetBrowser/AssetBrowserService.h"
#include "EditorCore/Services/AssetBrowser/AssetBrowserViewController.h"

namespace
{
    static bool IsAncestorPath(const std::string& normalizedAncestor, const std::string& normalizedPath)
    {
        if (normalizedAncestor.empty() || normalizedPath.empty())
            return false;

        if (normalizedAncestor == normalizedPath)
            return false;

        if (normalizedPath.size() <= normalizedAncestor.size())
            return false;

        if (normalizedPath.rfind(normalizedAncestor, 0) != 0)
            return false;

        return normalizedPath[normalizedAncestor.size()] == '/';
    }

    static std::string MakeNumberedName(const std::string& base, int n)
    {
        if (n <= 1) return base;
        return base + " (" + std::to_string(n) + ")";
    }

    static bool IsValidSingleFolderName(const std::string& name)
    {
        return UPath::IsValidFileSystemName(name);
    }
}

FolderPickerDialog::FolderPickerDialog() = default;
FolderPickerDialog::~FolderPickerDialog() = default;

void FolderPickerDialog::OnCreate(EditorHost &host, EditorRuntime &runtime)
{
    m_Service = &host.GetService<AssetBrowserService>();

    FAssetBrowserViewSettings settings;
    settings.bShowFolders = true;
    settings.bShowAssets  = false;
    settings.selectionPolicy = EAssetBrowserSelectionPolicy::LocalSelection;
    settings.projectionMode = EAssetBrowserProjectionMode::Tree;
    settings.rootPath = "/Project";
    settings.currentPath = "/Project";

    m_Controller = MakeUnique<AssetBrowserViewController>(settings);

    m_AssetsMutatedHandle = m_Service->OnAssetsMutated().Add([this](const FAssetOpResult&)
    {
        m_Controller->RequestRefresh();
    });

    m_PendingFoldersModifierHandle = m_Controller->AddProjectionModifier(
        [this](FAssetBrowserViewProjection& projection)
        {
            ApplyPendingFolderModifier(projection);
        });
}

void FolderPickerDialog::OnDestroy(EditorHost &host, EditorRuntime &runtime)
{
    if (m_AssetsMutatedHandle.IsValid())
    {
        host.GetService<AssetBrowserService>().OnAssetsMutated().Remove(m_AssetsMutatedHandle);
        m_AssetsMutatedHandle = {};
    }

    if (m_PendingFoldersModifierHandle.IsValid())
    {
        m_Controller->RemoveProjectionModifier(m_PendingFoldersModifierHandle);

        m_PendingFoldersModifierHandle = {};
    }

    m_Controller->Clear();

    m_OnAccepted = nullptr;
}

void FolderPickerDialog::OnOpen(EditorHost& host, EditorRuntime& runtime)
{
    m_bIsOpen = true;
    m_bJustOpened = true;

    // dialog-specific expansion state is stored in the view:
    m_Controller->RequestCollapseAll();
    m_Controller->RequestExpandRoot(*m_Service); // ensure root opens
}

void FolderPickerDialog::OnClose()
{
    m_PendingFolders = {};

    m_NextPendingNodeID = -1;
    m_PendingNodeIDs.clear();
    m_PendingNodeIDsByPath.clear();

    m_bIsOpen = false;
}

void FolderPickerDialog::OnRequestFocus(EditorHost& /*host*/, EditorRuntime& /*runtime*/)
{
    // Bring to front without re-running OnOpen logic
    m_bJustOpened = true;
}

void FolderPickerDialog::Draw(EditorHost& host, EditorRuntime& runtime)
{
    if (!m_bIsOpen)
        return;

    // One-shot focus to bring the dialog to front
    if (m_bJustOpened)
    {
        ImGui::SetNextWindowFocus();
        m_bJustOpened = false;
    }

    // Initial size (first time)
    ImGui::SetNextWindowSize(ImVec2(m_InitialWidth, m_InitialHeight), ImGuiCond_FirstUseEver);

    // Size limits
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(m_MinWidth, m_MinHeight), // min size
        ImVec2(m_MaxWidth, m_MaxHeight)  // max size
    );

    // Center on first appearance
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(),
        ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f)); // pivot = center

    const ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
        | ImGuiWindowFlags_NoCollapse;


    bool bOpen = m_bIsOpen;
    const std::string windowTitle = m_Title.empty() ? "Folder Picker" : "Folder Picker - " + m_Title;
    if (!ImGui::Begin(windowTitle.c_str(), &bOpen, windowFlags))
    {
        m_bIsOpen = bOpen;
        ImGui::End();
        return;
    }
    m_bIsOpen = bOpen;

    RefreshView(host, runtime);
    DrawContent(host.GetService<AssetBrowserService>());

    ImGui::End();
}

void FolderPickerDialog::DrawContent(AssetBrowserService& service)
{
    // Top bar: Up, Home, current path
    DrawTopBar();

    ImGui::Separator();

    // Directory list
    DrawDirectoryList();

    ImGui::Separator();

    // Bottom bar: Create Folder / Cancel / Select
    DrawBottomBar(service);
}

void FolderPickerDialog::DrawTopBar()
{
    if (ImGui::Button("Home"))
    {
        SetNavigationToPath("/Project");
    }

    ImGui::SameLine();

    if (ImGui::Button("Up"))
    {
        const std::string parent = UPath::GetParent(GetCurrentNavigationPath());
        if (UPath::IsSameOrUnder("/Project", parent))
            SetNavigationToPath(parent);
        else
            SetNavigationToPath("/Project");
    }

    ImGui::SameLine();

    if (ImGui::Button("Expand All"))
    {
        ExpandAll();
    }

    ImGui::SameLine();

    if (ImGui::Button("Collapse All"))
    {
        CollapseAll();
    }
}

void FolderPickerDialog::DrawDirectoryList()
{
    ImGui::TextUnformatted("Folders:");
    ImGui::BeginChild("##FolderPicker_DirTree",
                      ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2.0f),
                      true,
                      ImGuiWindowFlags_HorizontalScrollbar);

    // Ensure we have something projected
    auto itRoot = GetProjection().pathToID.find(UPath::NormalizeVirtual(GetSettings().rootPath));
    if (itRoot == GetProjection().pathToID.end())
    {
        ImGui::TextDisabled("No folders available.");
        ImGui::EndChild();
        return;
    }

    DrawNode(itRoot->second);

    ImGui::EndChild();
}

void FolderPickerDialog::DrawBottomBar(AssetBrowserService& service)
{
    ImGui::TextUnformatted("Destination:");

    ImGui::SameLine();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 2.0f));
    ImGui::SetNextItemWidth(-1.f);

    if (ImGui::InputText("##FolderPickerDestination",
                         &m_PathInputBuffer,
                         ImGuiInputTextFlags_EnterReturnsTrue))
    {
        ApplyPathInput(service);
    }

    if (ImGui::IsItemDeactivatedAfterEdit())
    {
        ApplyPathInput(service);
    }

    ImGui::PopStyleVar();

    float fullWidth = ImGui::GetContentRegionAvail().x;

    const float buttonWidth = 80.0f;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float totalRightButtonsW = buttonWidth * 2.0f + spacing;

    if (ImGui::Button("Create Folder", ImVec2(0, 0)))
    {
        std::string created = OnCreateFolderClicked();

        m_PendingFolders.renameTargetPath = created;
        m_PendingFolders.renameBuffer = UPath::GetFileName(created);
        m_PendingFolders.bStartRenameFocus = true;
    }

    const float rightButtonsX = ImGui::GetCursorPosX() + (fullWidth - totalRightButtonsW);
    ImGui::SameLine();
    ImGui::SetCursorPosX(rightButtonsX);

    bool bRequestClose = false;
    bool bAccepted = false;

    if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)))
    {
        bRequestClose = true;
    }

    ImGui::SameLine();

    const bool bCanSelect = !GetCurrentNavigationPath().empty();
    if (!bCanSelect)
        ImGui::BeginDisabled();

    if (ImGui::Button("Select", ImVec2(buttonWidth, 0)))
    {
        if (ApplyPathInput(service))
        {
            if (CommitPendingFolders(service))
            {
                bAccepted = true;
                bRequestClose = true;
            }
        }
    }

    if (!bCanSelect)
        ImGui::EndDisabled();

    if (bAccepted && m_OnAccepted)
    {
        m_OnAccepted(GetCurrentNavigationPath());
    }

    if (bRequestClose)
    {
        OnClose();
    }
}

void FolderPickerDialog::SetDestinationPathOnPicker(const std::string &path)
{
    auto normalizedPath = UPath::NormalizeVirtual(path);
    if (!UPath::IsSameOrUnder("/Project", normalizedPath))
        normalizedPath = "/Project";

    m_PathInputBuffer = normalizedPath;
    m_Controller->RequestNavigateTo(normalizedPath);

    if (m_Service)
    {
        ApplyPathInput(*m_Service); // will AddPendingFolderPath if missing, expand parents, etc.
    }
}

void FolderPickerDialog::RefreshView(EditorHost& host, EditorRuntime& runtime)
{
    if (!m_Service)
        m_Service = &host.GetService<AssetBrowserService>();

    m_Controller->Update(*m_Service);
}

void FolderPickerDialog::ApplyPendingFolderModifier(FAssetBrowserViewProjection& projection)
{
    for (auto& [parentID, children] : projection.children)
    {
        children.erase(
            std::remove_if(
                children.begin(),
                children.end(),
                [this](AssetBrowserNodeID childID)
                {
                    return IsPendingNodeID(childID);
                }),
            children.end());
    }

    for (auto it = projection.children.begin();
     it != projection.children.end();)
    {
        if (IsPendingNodeID(it->first))
            it = projection.children.erase(it);
        else
            ++it;
    }

    if (m_PendingFolders.pendingPaths.empty())
        return;

    // Sort by depth so parents are created before children

    std::vector<std::string> paths(
        m_PendingFolders.pendingPaths.begin(),
        m_PendingFolders.pendingPaths.end());

    std::sort(
        paths.begin(),
        paths.end(),
        [](const std::string& a, const std::string& b)
        {
            return std::count(a.begin(), a.end(), '/') <
                   std::count(b.begin(), b.end(), '/');
        });

    // Create pending nodes

    for (const std::string& path : paths)
    {
        if (projection.pathToID.contains(path))
            continue;

        const AssetBrowserNodeID id = GetOrCreatePendingNodeID(path);

        FAssetBrowserNode node;
        node.nodeID = id;
        node.type = EAssetBrowserNodeType::Folder;
        node.virtualPath = path;

        node.childFolderState = EAssetBrowserChildState::Unknown;

        projection.nodeCache[id] = std::move(node);
        projection.pathToID[path] = id;
    }

    // Build hierarchy

    for (const std::string& path : paths)
    {
        auto itChild = projection.pathToID.find(path);
        if (itChild == projection.pathToID.end())
            continue;

        const AssetBrowserNodeID childID = itChild->second;

        const std::string parentPath = UPath::GetParent(path);

        auto itParent = projection.pathToID.find(parentPath);

        if (itParent == projection.pathToID.end())
            continue;

        const AssetBrowserNodeID parentID = itParent->second;

        auto& children = projection.children[parentID];

        if (std::ranges::find(children, childID) == children.end())
        {
            children.push_back(childID);
        }

        std::ranges::sort(children, [&](AssetBrowserNodeID a, AssetBrowserNodeID b)
           {
           const auto& na = projection.nodeCache[a];
           const auto& nb = projection.nodeCache[b];

           return na.GetDisplayName() < nb.GetDisplayName();
           });

        auto& childNode = projection.nodeCache[childID];

        childNode.parentID = parentID;

        auto& parentNode = projection.nodeCache[parentID];

        parentNode.childFolderState = EAssetBrowserChildState::Present;
    }
}

void FolderPickerDialog::SetNavigationToPath(const std::string& rawPath)
{
    NavigateToPath(*m_Service, rawPath, false);
}

bool FolderPickerDialog::NavigateToPath(AssetBrowserService& service,const std::string& rawPath,
    bool bAllowCreatePending)
{
    std::string normalizedPath = UPath::NormalizeVirtual(rawPath);

    if (normalizedPath.empty())
        normalizedPath = "/Project";

    if (!UPath::IsSameOrUnder("/Project", normalizedPath))
        return false;

    if (bAllowCreatePending)
    {
        if (!PathExistsInPicker(normalizedPath))
            AddPendingFolderPath(normalizedPath);
    }

    m_PathInputBuffer = normalizedPath;
    m_Controller->RequestNavigateTo(normalizedPath);

    EnsurePathVisible(service, normalizedPath);

    return true;
}

void FolderPickerDialog::DrawNode(AssetBrowserNodeID id)
{
    const auto itNode = GetProjection().nodeCache.find(id);
    if (itNode == GetProjection().nodeCache.end())
        return;

    const FAssetBrowserNode& node = itNode->second;

    const std::string& nodePath = node.virtualPath;

    if (node.type != EAssetBrowserNodeType::Folder)
        return;

    const bool bIsPending = IsPendingNode(nodePath);

    const bool bIsRenaming = bIsPending && IsRenaming(nodePath);

    const bool bIsSelected = (GetCurrentNavigationPath() == nodePath);

    const bool bAllowExpansionArrow = FolderHasChildren(id);

    ImGuiTreeNodeFlags flags = 0;

    if (bIsSelected)
        flags |= ImGuiTreeNodeFlags_Selected;

    if (bAllowExpansionArrow)
    {
        flags |= bIsPending ? ImGuiTreeNodeFlags_OpenOnArrow :
            ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_OpenOnDoubleClick;
    }
    else
    {
        flags |=
            ImGuiTreeNodeFlags_Leaf |
            ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    ImGui::SetNextItemOpen(m_Controller->IsFolderExpanded(id), ImGuiCond_Once);

    const std::string label = BuildNodeLabel(node);

    if (bIsPending)
        PushPendingNodeStyle();

    bool bOpened = false;

    if (!bIsRenaming)
    {
        bOpened = ImGui::TreeNodeEx((void*)(intptr_t)id, flags, "%s", label.c_str());
    }
    else
    {
        bOpened = ImGui::TreeNodeEx((void*)(intptr_t)id, flags, "%s", "");

        DrawPendingRenameWidget();
    }

    if (bIsPending)
        PopPendingNodeStyle();

    if (ImGui::IsItemClicked())
        SetNavigationToPath(nodePath);

    bool bDeleted = false;

    if (bIsPending && !bIsRenaming)
    {
        HandlePendingNodeInteractions(id, node);
        bDeleted = HandlePendingDeleteButton(nodePath, static_cast<int>(id));
    }

    if (bAllowExpansionArrow && ImGui::IsItemToggledOpen())
    {
        if (bOpened)
            m_Controller->RequestExpand(id);
        else
            m_Controller->RequestCollapse(id);
    }

    if (bAllowExpansionArrow && bOpened)
    {
        if (auto it = GetProjection().children.find(id);
            it != GetProjection().children.end())
        {
            for (AssetBrowserNodeID childID : it->second)
            {
                DrawNode(childID);
            }
        }

        ImGui::TreePop();

        if (bDeleted)
            return;
    }
}

std::string FolderPickerDialog::BuildNodeLabel(const FAssetBrowserNode& node) const
{
    if (IsPendingNode(node.virtualPath))
        return node.GetDisplayName() + " *";

    return node.GetDisplayName();
}

void FolderPickerDialog::DrawPendingRenameWidget()
{
    if (m_PendingFolders.bStartRenameFocus)
    {
        ImGui::SetKeyboardFocusHere();
        m_PendingFolders.bStartRenameFocus = false;
    }

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        IM_COL32(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  IM_COL32(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_Border,         IM_COL32(0,0,0,0));

    ImGui::SameLine();

    ImGui::PushStyleVar(
        ImGuiStyleVar_FramePadding,
        ImVec2(ImGui::GetStyle().FramePadding.x, 0.0f));

    ImGui::SetNextItemWidth(220.0f);

    const bool bEnter =
        ImGui::InputText(
            "##PendingRenameInput",
            &m_PendingFolders.renameBuffer,
            ImGuiInputTextFlags_EnterReturnsTrue |
            ImGuiInputTextFlags_AutoSelectAll);

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    const bool enterPressed = bEnter;
    const bool deactivatedAfterEdit = ImGui::IsItemDeactivatedAfterEdit();

    const bool deactivated = ImGui::IsItemDeactivated();

    const bool clickAway =
        ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !ImGui::IsItemHovered() &&
        !ImGui::IsAnyItemActive();

    const bool commit = enterPressed || deactivatedAfterEdit;

    const bool cancel =
        (!enterPressed) &&
        deactivated &&
        !deactivatedAfterEdit;

    if (commit)
    {
        CommitPendingRename(m_PendingFolders.renameTargetPath, m_PendingFolders.renameBuffer);
        EndPendingRename();
    }
    else if (cancel || clickAway)
    {
        CancelPendingRename();
    }
}

bool FolderPickerDialog::HandlePendingDeleteButton(const std::string &nodePath, const int id)
{
    ImGui::SameLine();

    const float xW =
        ImGui::CalcTextSize("X").x +
        ImGui::GetStyle().FramePadding.x * 2.0f;

    const float rightLocal =
        ImGui::GetWindowContentRegionMax().x;

    float x = rightLocal - xW;

    if (x > ImGui::GetCursorPosX())
        ImGui::SetCursorPosX(x);

    ImGui::SetNextItemAllowOverlap();

    ImGui::PushID(id);
    if (ImGui::SmallButton("X"))
    {
        RemovePendingFolderPathAndDescendants(nodePath);
        ImGui::PopID();
        return true;
    }
    ImGui::PopID();
    return false;
}

void FolderPickerDialog::PushPendingNodeStyle()
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
}

void FolderPickerDialog::PopPendingNodeStyle()
{
    ImGui::PopStyleColor();
}

void FolderPickerDialog::HandlePendingNodeInteractions(AssetBrowserNodeID id, const FAssetBrowserNode& node)
{
    const bool hovered =
        ImGui::IsItemHovered();

    const bool dbl =
        hovered &&
        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

    if (dbl)
    {
        m_PendingFolders.renameTargetPath = node.virtualPath;
        m_PendingFolders.renameBuffer = node.GetDisplayName();
        m_PendingFolders.bStartRenameFocus = true;
    }

    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Rename"))
        {
            m_PendingFolders.renameTargetPath = node.virtualPath;
            m_PendingFolders.renameBuffer = node.GetDisplayName();
            m_PendingFolders.bStartRenameFocus = true;
        }

        if (ImGui::MenuItem("Delete"))
        {
            RemovePendingFolderPathAndDescendants(
                node.virtualPath);
        }

        ImGui::EndPopup();
    }
}

void FolderPickerDialog::ExpandAll()
{
    m_Controller->RequestExpandAll();
}

void FolderPickerDialog::CollapseAll()
{
    m_Controller->RequestCollapseAll();

    if (m_Service)
        m_Controller->RequestExpandRoot(*m_Service); // Keep root open
}

AssetBrowserNodeID FolderPickerDialog::GetOrCreatePendingNodeID(const std::string& path)
{
    auto it = m_PendingNodeIDsByPath.find(path);
    if (it != m_PendingNodeIDsByPath.end())
        return it->second;

    const AssetBrowserNodeID id = m_NextPendingNodeID--;

    m_PendingNodeIDs.insert(id);
    m_PendingNodeIDsByPath.emplace(path, id);

    return id;
}

void FolderPickerDialog::RemovePendingNodeID(const std::string& path)
{
    auto it = m_PendingNodeIDsByPath.find(path);
    if (it == m_PendingNodeIDsByPath.end())
        return;

    m_PendingNodeIDs.erase(it->second);
    m_PendingNodeIDsByPath.erase(it);
}

void FolderPickerDialog::SyncInputBufferWithCurrentNavigation()
{
    m_PathInputBuffer = GetCurrentNavigationPath();
}

bool FolderPickerDialog::ApplyPathInput(AssetBrowserService& service)
{
    return NavigateToPath(service, m_PathInputBuffer, true);
}

void FolderPickerDialog::EnsurePathVisible(AssetBrowserService& service, const std::string& path)
{
    std::vector<std::string> chain;

    std::string cur = path;

    while (!cur.empty() && cur != "/")
    {
        chain.push_back(cur);

        if (cur == "/Project")
            break;

        cur = UPath::GetParent(cur);
    }

    std::reverse(chain.begin(), chain.end());

    for (const std::string& p : chain)
    {
        auto* node = service.GetNodeByPath(GetProjection(), p);
        if (!node)
            continue;

        m_Controller->RequestExpand(node->nodeID);
    }
}

bool FolderPickerDialog::AddPendingFolderPath(const std::string& rawPath)
{
    std::string path = UPath::NormalizeVirtual(rawPath);
    if (path.empty())
        return false;

    if (!UPath::IsSameOrUnder("/Project", path))
        return false;

    if (path == "/Project")
        return false;

    bool bAdded = false;

    // Build full chain (parents -> leaf)
    std::vector<std::string> chain;

    for (std::string cur = path;
         !cur.empty() && cur != "/" && cur != "/Project";
         cur = UPath::GetParent(cur))
    {
        chain.push_back(cur);
    }

    std::reverse(chain.begin(), chain.end());

    for (const std::string& folder : chain)
    {
        std::string normalized = UPath::NormalizeVirtual(folder);

        if (normalized.empty())
            continue;

        if (!UPath::IsSameOrUnder("/Project", normalized))
            continue;

        const std::string parent = UPath::GetParent(normalized);
        const std::string name   = UPath::GetFileName(normalized);

        std::string safeName = UPath::SanitizeFileSystemName(name);

        if (!IsValidSingleFolderName(safeName))
            continue;

        const std::string safePath = UPath::NormalizeVirtual(UPath::Join(parent, safeName));

        const std::string siblingPath = UPath::NormalizeVirtual(UPath::Join(parent, safeName));

        if (PathExistsInPicker(siblingPath))
            continue;

        const bool bRealExists = m_Service->GetNodeByPath(GetProjection(), safePath);

        if (bRealExists)
            continue;

        if (!m_PendingFolders.pendingPaths.contains(safePath))
        {
            m_PendingFolders.pendingPaths.insert(safePath);
            bAdded = true;
        }
    }

    return bAdded;
}

void FolderPickerDialog::RemovePendingFolderPathAndDescendants(const std::string& path)
{
    if (m_PendingFolders.renameTargetPath == path || IsAncestorPath(path, m_PendingFolders.renameTargetPath))
    {
        EndPendingRename();
    }

    std::vector<std::string> remaining;
    remaining.reserve(m_PendingFolders.pendingPaths.size());

    for (const auto& p : m_PendingFolders.pendingPaths)
    {
        if (p == path || IsAncestorPath(path, p))
        {
            RemovePendingNodeID(p);
            continue;
        }

        remaining.push_back(p);
    }

    m_PendingFolders.pendingPaths.clear();
    for (auto& p : remaining)
        m_PendingFolders.pendingPaths.insert(std::move(p));

    m_Controller->RequestRefresh();
}

bool FolderPickerDialog::CommitPendingFolders(AssetBrowserService& service)
{
    std::vector<std::string> paths(
        m_PendingFolders.pendingPaths.begin(),
        m_PendingFolders.pendingPaths.end());

    std::sort(paths.begin(), paths.end(),
        [](const std::string& a, const std::string& b)
        {
            return std::count(a.begin(), a.end(), '/') <
                   std::count(b.begin(), b.end(), '/');
        });

    for (const std::string& path : paths)
    {
        if (auto* node = service.GetNodeByPath(GetProjection(), path))
        {
            if (!IsPendingNode(node->virtualPath))
                continue; // already real folder
        }

        auto result = service.CreateFolder(path);

        if (!result.bSuccess)
            return false;
    }

    m_PendingFolders.pendingPaths.clear();

    m_PendingNodeIDs.clear();
    m_PendingNodeIDsByPath.clear();

    return true;
}

bool FolderPickerDialog::CommitPendingRename(const std::string& targetPath, const std::string& newNameRaw)
{
    const std::string oldPath = UPath::NormalizeVirtual(targetPath);

    if (!m_PendingFolders.pendingPaths.contains(oldPath))
        return false;

    // Only pending folders are renameable
    if (!IsPendingNode(oldPath))
        return false;

    std::string newName =
        UPath::SanitizeFileSystemName(newNameRaw);

    if (!IsValidSingleFolderName(newName))
        return false;

    const std::string parent = UPath::GetParent(oldPath);

    std::string desiredNewPath = UPath::NormalizeVirtual(UPath::Join(parent, newName));

    if (desiredNewPath != oldPath && PathExistsInPicker(desiredNewPath))
    {
        desiredNewPath = MakeUniqueChildPath(parent, newName);
    }

    if (desiredNewPath == oldPath)
        return true;

    std::unordered_set<std::string> rewritten;

    rewritten.reserve(m_PendingFolders.pendingPaths.size());

    std::unordered_map<std::string, AssetBrowserNodeID> remappedIDs;

    for (const auto& [path, id] : m_PendingNodeIDsByPath)
    {
        std::string newPath = path;

        if (path == oldPath || IsAncestorPath(oldPath, path))
        {
            newPath = desiredNewPath + path.substr(oldPath.size());
        }

        rewritten.insert(newPath);
        remappedIDs.emplace(std::move(newPath), id);
    }

    m_PendingNodeIDsByPath = std::move(remappedIDs);
    m_PendingFolders.pendingPaths = std::move(rewritten);

    if (GetCurrentNavigationPath() == oldPath ||IsAncestorPath(oldPath, GetCurrentNavigationPath()))
    {
        SetNavigationToPath(desiredNewPath + GetCurrentNavigationPath().substr(oldPath.size()));
    }

    if (m_PendingFolders.renameTargetPath == oldPath)
    {
        m_PendingFolders.renameTargetPath = desiredNewPath;
    }

    return true;
}

void FolderPickerDialog::EndPendingRename()
{
    m_PendingFolders.renameTargetPath.clear();
    m_PendingFolders.renameBuffer.clear();
    m_PendingFolders.bStartRenameFocus = false;
}

void FolderPickerDialog::CancelPendingRename()
{
    // If we want cancel to revert, just discard buffer:
    EndPendingRename();
}

std::string FolderPickerDialog::MakeUniqueChildPath(const std::string& parent, const std::string& baseName) const
{
    const std::string p = UPath::NormalizeVirtual(parent);

    for (int n = 1; n < 10000; ++n)
    {
        const std::string name = MakeNumberedName(baseName, n);
        const std::string candidate = UPath::Join(p, name);
        if (!PathExistsInPicker(candidate))
            return candidate;
    }

    // Fallback (should never happen)
    return UPath::Join(p, baseName);
}

std::string FolderPickerDialog::OnCreateFolderClicked()
{
    std::string base = GetCurrentNavigationPath();
    if (!UPath::IsSameOrUnder("/Project", base)) base = "/Project";

    const std::string uniquePath = MakeUniqueChildPath(base, "NewFolder");

    AddPendingFolderPath(uniquePath);
    SetNavigationToPath(uniquePath);

    return uniquePath;
}

bool FolderPickerDialog::IsRootPath(const std::string& path)
{
    std::string p = UPath::NormalizeVirtual(path);
    return p == "/" || p.empty();
}

bool FolderPickerDialog::IsRenaming(const std::string& path) const
{
    return path == m_PendingFolders.renameTargetPath;
}

bool FolderPickerDialog::IsPendingNode(const std::string& path) const
{
    return m_PendingFolders.pendingPaths.contains(path);
}

bool FolderPickerDialog::IsPendingNodeID(AssetBrowserNodeID id) const
{
    return m_PendingNodeIDs.contains(id);
}

bool FolderPickerDialog::IsRealFolderPresent(const std::string& path) const
{
    const auto* node = m_Service->GetModelNodeByPath(path);

    return node &&
           node->type == EAssetBrowserNodeType::Folder &&
           !IsPendingNode(path);
}

bool FolderPickerDialog::PathExistsInPicker(const std::string& path) const
{
    return IsRealFolderPresent(path);
}

bool FolderPickerDialog::FolderHasChildren(AssetBrowserNodeID id) const
{
    auto it = GetProjection().nodeCache.find(id);

    if (it == GetProjection().nodeCache.end())
        return false; // node not found in the view

    return it->second.HasFolderChildren();
}

const FAssetBrowserViewProjection & FolderPickerDialog::GetProjection() const
{
    return m_Controller->GetProjection();
}

const FAssetBrowserViewSettings & FolderPickerDialog::GetSettings() const
{
    return m_Controller->GetSettings();
}

const std::string & FolderPickerDialog::GetCurrentNavigationPath() const
{
    return m_Controller->GetCurrentNavigationPath();
}

AssetBrowserNodeID FolderPickerDialog::GetRootNodeID(EditorHost &host) const
{
    return host.GetService<AssetBrowserService>().GetRootNodeID(m_Controller->GetSettings().rootPath);
}
