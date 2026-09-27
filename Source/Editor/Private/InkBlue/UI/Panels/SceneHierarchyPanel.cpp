//  Copyright 2025-2026 JesseTheCatLover. All Rights Reserved.

#include "SceneHierarchyPanel.h"

#include "imgui.h"
#include "EditorCore/EditorHost.h"
#include "Panels/Subsystems/SceneHierarchySubsystem.h"
#include "Panels/Controllers/Outputs/FHierarchyOutput.h"
#include "EditorCore/Services/Selection/SelectionService.h"

// helpers
static bool FindNodeById(const std::vector<FHierarchySnapshot>& actors, ActorID id, FHierarchySnapshot& outNode)
{
    for (const auto& n : actors)
        if (n.id == id) { outNode = n; return true; }
    return false;
}

static void BuildParentChain(const std::vector<FHierarchySnapshot>& actors, ActorID leaf, std::vector<ActorID>& outChain)
{
    ActorID cur = leaf;
    while (cur != 0)
    {
        outChain.push_back(cur);
        FHierarchySnapshot node{};
        if (!FindNodeById(actors, cur, node)) break;
        cur = node.parentID;
    }
}

void SceneHierarchyPanel::ApplyRevealRequest(const std::vector<FHierarchySnapshot>& actors, ActorID reveal)
{
    if (reveal == 0) return;

    std::vector<ActorID> chain;
    chain.reserve(8);
    BuildParentChain(actors, reveal, chain);

    for (ActorID id : chain)
        m_OpenNodes.insert(id);

    m_ScrollTo = reveal;
}

void SceneHierarchyPanel::DrawActorNode(const FHierarchySnapshot& node,
                                        const std::vector<FHierarchySnapshot>& allActors,
                                        FHierarchyPanelInput& ioInput,
                                        const TSelectionModel<ActorID>& selection)
{
    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_SpanAvailWidth;

    if (!node.bHasChildren)
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    if (selection.IsSelected(node.id))
        flags |= ImGuiTreeNodeFlags_Selected;

    // --- Node Expansion State ---
    // Only force the node open if the backend specifically requested a reveal.
    // Otherwise, we let ImGui handle the standard user click toggles internally.
    if (m_OpenNodes.count(node.id) > 0)
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        m_OpenNodes.erase(node.id); // Consume the request so it isn't locked open forever
    }

    // 1. Draw the Node (or an InputText if we are renaming)
    bool bOpened = false;
    if (m_RenamingNodeID == node.id)
    {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 30.0f);
        if (ImGui::InputText("##Rename", m_RenameBuffer, sizeof(m_RenameBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
        {
            ioInput.bRenameRequested = true;
            ioInput.targetActorToModify = node.id;
            ioInput.newName = m_RenameBuffer;
            m_RenamingNodeID = 0;
        }

        if (!ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_RenamingNodeID = 0;
        }

        bOpened = ImGui::TreeNodeEx((void*)(intptr_t)node.id, flags, "");
    }
    else
    {
        bOpened = ImGui::TreeNodeEx((void*)(intptr_t)node.id, flags, "%s", node.name.c_str());
    }

    // =======================================================================================
    // --- Order of Operations ---
    // ALL context menus, drag/drops, and click checks MUST happen immediately after TreeNodeEx
    // =======================================================================================

    // Click selection
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen() && m_RenamingNodeID != node.id)
    {
        m_bClickedAnyItemThisFrame = true;
        ioInput.bClickedItem = true;
        ioInput.clickedActor = node.id;
    }

    // Context Menu (Right Click)
    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Rename"))
        {
            m_RenamingNodeID = node.id;
            strncpy(m_RenameBuffer, node.name.c_str(), sizeof(m_RenameBuffer));
        }
        if (ImGui::MenuItem("Delete"))
        {
            ioInput.bDeleteRequested = true;
            ioInput.targetActorToModify = node.id;
        }
        ImGui::EndPopup();
    }

    // --- CACHE THE NODE STATE IMMEDIATELY ---
    bool bNodeClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    bool bNodeToggled = ImGui::IsItemToggledOpen();

    // Drag and Drop Source
    if (ImGui::BeginDragDropSource())
    {
        ImGui::SetDragDropPayload("JENGINE_ACTOR_ID", &node.id, sizeof(ActorID));
        ImGui::Text("Move %s", node.name.c_str());
        ImGui::EndDragDropSource();
    }

    // Drag and Drop Target
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("JENGINE_ACTOR_ID"))
        {
            ActorID droppedActorID = *(const ActorID*)payload->Data;
            ioInput.bReparentRequested = true;
            ioInput.draggedActor = droppedActorID;
            ioInput.targetParentActor = node.id;
        }
        ImGui::EndDragDropTarget();
    }

    // --- Button Overlap Hit-Testing ---
    // Tell ImGui to allow the upcoming button to be clicked, even though SpanAvailWidth is covering the line
    ImGui::SetNextItemAllowOverlap();

    // 2. Visibility Toggle (Right-aligned)
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 24.0f);

    const char* visLabel = node.bVisible ? "[O]" : "[-]";

    ImGui::PushID((int)node.id);
    if (ImGui::SmallButton(visLabel))
    {
        ioInput.bToggleVisibilityRequested = true;
        ioInput.targetActorToModify = node.id;
    }
    ImGui::PopID();

    // Scroll reveal
    if (m_ScrollTo == node.id)
    {
        ImGui::SetScrollHereY(0.25f);
        m_ScrollTo = 0;
    }

    // 3. Draw Children
    if (bOpened && node.bHasChildren)
    {
        for (const auto& child : allActors)
            if (child.parentID == node.id)
                DrawActorNode(child, allActors, ioInput, selection);

        if (node.bHasChildren)
        {
            ImGui::TreePop();
        }
    }
}

void SceneHierarchyPanel::OnDestroy(EditorHost& host)
{
}

void SceneHierarchyPanel::Draw(EditorHost& host)
{
    if (!ImGui::Begin(GetName()))
    {
        ImGui::End();
        return;
    }

    // Build input for this frame
    FHierarchyPanelInput input{};
    input.panelKey = GetPanelKey();

    input.bFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    input.bHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

    ImGuiIO& io = ImGui::GetIO();
    input.bCtrl  = io.KeyCtrl;
    input.bShift = io.KeyShift;
    input.bSuper = io.KeySuper;

    m_bClickedAnyItemThisFrame = false;

    // Output snapshot
    const FHierarchyOutput* out = host.GetSubsystem<SceneHierarchySubsystem>().GetOutput(GetPanelKey());
    if (!out || !out->bHasSnapshot || !out->snapshot)
    {
        ImGui::TextUnformatted("Hierarchy: waiting for snapshot...");
        // IMPORTANT: still submit input so controller gets created next tick
        host.GetSubsystem<SceneHierarchySubsystem>().SubmitInput(input);
        ImGui::End();
        return;
    }

    const auto& actors = *out->snapshot;

    // Apply reveal request
    ApplyRevealRequest(actors, out->revealActorID);

    const auto& sceneSelection = host.GetService<SelectionService>().GetSceneActorSelection();

    // Draw tree roots
    for (const auto& actor : actors)
        if (actor.parentID == 0)
            DrawActorNode(actor, actors, input, sceneSelection);

    // Background click clears selection
    if (ImGui::IsWindowHovered() &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !m_bClickedAnyItemThisFrame)
    {
        input.bClearSelection = true;
    }

    // Submit input last
    host.GetSubsystem<SceneHierarchySubsystem>().SubmitInput(input);

    ImGui::End();
}