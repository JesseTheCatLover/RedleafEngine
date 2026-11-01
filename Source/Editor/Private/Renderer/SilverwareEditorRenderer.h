// Copyright 2026 JesseTheCatLover. All Rights Reserved.

#pragma once

#include "Silverware/Core/Events/EventConnection.h"
#include "Silverware/Memory/SmartPointers.h"

class SilverwareBackend;

namespace Silverware
{
    class ContainerWidget;
}

class SilverwareEditorRenderer
{
private:
    [[nodiscard]] bool RegisterPlaygroundTheme();
    [[nodiscard]] bool BuildPlaygroundLayout();

    SilverwareBackend* m_Backend = nullptr;

    Silverware::TUniquePtr<Silverware::ContainerWidget> m_RootWidget;

    // Mount points for the widget examples we are going to implement.
    Silverware::ContainerWidget* m_ButtonPlayground = nullptr;
    Silverware::ContainerWidget* m_InteractiveBlockPlayground = nullptr;

    Silverware::FEventConnection m_PrimaryButtonClickConnection;

public:
    SilverwareEditorRenderer();
    ~SilverwareEditorRenderer();

    SilverwareEditorRenderer(const SilverwareEditorRenderer&) = delete;
    SilverwareEditorRenderer& operator=(const SilverwareEditorRenderer&) = delete;

    [[nodiscard]] bool Initialize(SilverwareBackend& backend);

    void Tick(float deltaTime);
    void Shutdown();
};