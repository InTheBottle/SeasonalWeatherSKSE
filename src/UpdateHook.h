#pragma once

#include "pch.h"

namespace SWF {

    class UpdateHook {
    public:
        static UpdateHook& GetSingleton() {
            static UpdateHook instance;
            return instance;
        }

        void Install();

        bool IsInstalled() const { return installed_; }

    private:
        UpdateHook() = default;
        ~UpdateHook() = default;
        UpdateHook(const UpdateHook&) = delete;
        UpdateHook& operator=(const UpdateHook&) = delete;

        bool installed_ = false;

        class MenuEventSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
        public:
            static MenuEventSink& GetSingleton() {
                static MenuEventSink instance;
                return instance;
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::MenuOpenCloseEvent* a_event,
                RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_eventSource) override;
        };

        class CellChangeEventSink : public RE::BSTEventSink<RE::TESCellAttachDetachEvent> {
        public:
            static CellChangeEventSink& GetSingleton() {
                static CellChangeEventSink instance;
                return instance;
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::TESCellAttachDetachEvent* a_event,
                RE::BSTEventSource<RE::TESCellAttachDetachEvent>* a_eventSource) override;
        };
    };
}
