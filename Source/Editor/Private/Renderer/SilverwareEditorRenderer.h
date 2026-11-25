    // Copyright 2026 JesseTheCatLover. All Rights Reserved.

    #pragma once

    #include "Silverware/Core/Events/EventConnection.h"
    #include "Silverware/Memory/SmartPointers.h"

    class SilverwareEditorBackend;

    namespace Silverware
    {
        class ContainerWidget;
    }

    class SilverwareEditorRenderer
    {
    private:
        [[nodiscard]] bool BuildPlaygroundLayout();

        SilverwareEditorBackend* m_Backend = nullptr;

        Silverware::TUniquePtr<Silverware::ContainerWidget> m_RootWidget;

        // Mount points for the widget examples we are going to implement.
        Silverware::ContainerWidget* m_ButtonPlayground = nullptr;
        Silverware::ContainerWidget* m_InteractiveBlockPlayground = nullptr;
        Silverware::ContainerWidget* m_ProgressBarPlayground = nullptr;

        Silverware::FEventConnection m_PrimaryButtonClickConnection;
        Silverware::FEventConnection m_ProgressBarSliderValueChangedConnection;
        Silverware::FEventConnection m_ProgressBarValueChangedConnection;
        Silverware::FEventConnection m_TextInputChangedConnection;
        Silverware::FEventConnection m_TextInputCommittedConnection;
        decltype(m_PrimaryButtonClickConnection) m_FontButtonClickConnections[3];
        decltype(m_PrimaryButtonClickConnection) m_CheckboxChangedConnections[2];
        decltype(m_PrimaryButtonClickConnection) m_RadioButtonChangedConnections[4];
        decltype(m_PrimaryButtonClickConnection) m_ToggleChangedConnections[4];
        decltype(m_PrimaryButtonClickConnection) m_SliderValueChangedConnections[3];
        decltype(m_PrimaryButtonClickConnection) m_SliderValueCommittedConnections[3];
        decltype(m_PrimaryButtonClickConnection) m_RangedSliderValuesChangedConnections[3];
        decltype(m_PrimaryButtonClickConnection) m_RangedSliderValuesCommittedConnections[3];

    public:
        SilverwareEditorRenderer();
        ~SilverwareEditorRenderer();

        SilverwareEditorRenderer(const SilverwareEditorRenderer&) = delete;
        SilverwareEditorRenderer& operator=(const SilverwareEditorRenderer&) = delete;

        [[nodiscard]] bool Initialize(SilverwareEditorBackend& backend);

        void Tick(float deltaTime);
        void Shutdown();
    };
