#pragma once

#include "DataHandler.hpp"
#include "Main.h"

#include "Utils/ModUtils.hpp"

namespace Events
{
	class ModEventSink :
		public RE::BSTEventSink<RE::TESCellFullyLoadedEvent>,
		public RE::BSTEventSink<RE::TESLoadGameEvent>,
		public RE::BSTEventSink<RE::TESDeathEvent>,
		public RE::BSTEventSink<RE::TESObjectLoadedEvent>,
		public RE::BSTEventSink<RE::TESResetEvent>,
		public RE::BSTEventSink<RE::TESFormDeleteEvent>
	{
		ModEventSink() = default;
		ModEventSink(const ModEventSink&) = delete;
		ModEventSink(ModEventSink&&) = delete;
		ModEventSink& operator=(const ModEventSink&) = delete;
		ModEventSink& operator=(ModEventSink&&) = delete;

	public:
		#define continueEvent RE::BSEventNotifyControl::kContinue

		static ModEventSink* GetSingleton()
		{
			static ModEventSink singleton;
			return &singleton;
		}

		static void LoadEvents()
		{
			auto* eventSink = GetSingleton();
			auto* eventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
			eventSourceHolder->AddEventSink<RE::TESCellFullyLoadedEvent>(eventSink);
			eventSourceHolder->AddEventSink<RE::TESLoadGameEvent>(eventSink);
			eventSourceHolder->AddEventSink<RE::TESDeathEvent>(eventSink);
			eventSourceHolder->AddEventSink<RE::TESObjectLoadedEvent>(eventSink);
			eventSourceHolder->AddEventSink<RE::TESResetEvent>(eventSink);
			eventSourceHolder->AddEventSink<RE::TESFormDeleteEvent>(eventSink);
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::TESCellFullyLoadedEvent* event, RE::BSTEventSource<RE::TESCellFullyLoadedEvent>*);
		RE::BSEventNotifyControl ProcessEvent(const RE::TESLoadGameEvent* event, RE::BSTEventSource<RE::TESLoadGameEvent>*);
		RE::BSEventNotifyControl ProcessEvent(const RE::TESDeathEvent* event, RE::BSTEventSource<RE::TESDeathEvent>*);
		RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* event, RE::BSTEventSource<RE::TESObjectLoadedEvent>*);
		RE::BSEventNotifyControl ProcessEvent(const RE::TESResetEvent* event, RE::BSTEventSource<RE::TESResetEvent>*);
		RE::BSEventNotifyControl ProcessEvent(const RE::TESFormDeleteEvent* event, RE::BSTEventSource<RE::TESFormDeleteEvent>*);
	};
};
