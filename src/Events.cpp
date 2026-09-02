#include "Events.h"

namespace Events
{
	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::TESLoadGameEvent* event, RE::BSTEventSource<RE::TESLoadGameEvent>*)
	{
        (void)event;
		MiscUtils::SetGlobalTimeMultiplier(1.0f, false);
		ModUtils::ActorMaintenanceProcess();
		
		return continueEvent;
	}

	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::TESCellFullyLoadedEvent* event, RE::BSTEventSource<RE::TESCellFullyLoadedEvent>*)
	{
		ModUtils::ActorMaintenanceProcess(event->cell);
		
		return continueEvent;
	}

	static void DelayedApplyLinearImpulse(const ModData::DataHandler::LimbData& limbData)
	{
		ModUtils::WaitUntilRagdollReady(limbData.linkedLimb, [limbData](RE::TESObjectREFR* objectRef, const bool result) {
			if (!result || !objectRef) return;
					
			ModUtils::ApplyLinearImpulse(objectRef, limbData.impulse.fromPosition, limbData.impulse.toPosition, limbData.impulse.magnitude);
			if (limbData.droppedItem && limbData.droppedItem->Is3DLoaded()) {
				RE::NiPoint3 toPositionElevated = limbData.impulse.toPosition;
				if (limbData.linkedActor && limbData.linkedActor->IsInKillMove()) toPositionElevated.z += 25.0f;
				ModUtils::ApplyLinearImpulse(limbData.droppedItem, limbData.impulse.fromPosition, toPositionElevated, limbData.impulse.magnitude);
			}
		});
	}

	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::TESObjectLoadedEvent* event, RE::BSTEventSource<RE::TESObjectLoadedEvent>*)
	{
		if (!event->loaded || !event->formID) return continueEvent;

		RE::TESObjectREFR* loadedItem = RE::TESForm::LookupByID<RE::TESObjectREFR>(event->formID);
		if (!loadedItem) return continueEvent;
		
		const auto& dataHandler = ModData::DataHandler::GetSingleton();
		if (loadedItem->HasKeyword(dataHandler->limbKeyword)) {
			auto limbData = ModUtils::GetLimbData(event->formID);
			if (limbData) {
				ModUtils::UpdateLimbScaleAndTint(loadedItem, limbData->linkedActor);

				DelayedApplyLinearImpulse(*limbData);

				if (ModUtils::GetGlobalValue<bool>(ModData::DataHandler::PluginGlobal::Misc_PlayArtObjectOnLimbs)) {
					MiscUtils::PlayArtObject(loadedItem, limbData->limbArtObject, 5.0f, loadedItem);
				}
			}
		}

		return continueEvent;
	}

	static void DelayedToggleCollisionLayers(RE::FormID targetFormID, const std::string& limbNode, bool enable, int delayMs)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
		
		SKSE::GetTaskInterface()->AddTask([targetFormID, limbNode, enable]() {
			RE::Actor* target = RE::TESForm::LookupByID<RE::Actor>(targetFormID);
			if (!target || target->formType != RE::FormType::ActorCharacter) return;

			ModUtils::ToggleCollisionLayers(target, limbNode, enable);
		});
	}

	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::TESDeathEvent* event, RE::BSTEventSource<RE::TESDeathEvent>*)
	{
		RE::Actor* target = event->actorDying->As<RE::Actor>();
		if (!target) return continueEvent;

		Events::MainEvent::ProcessDeferredHit(target);

		std::vector<ModData::DismemberingData> dismemberingDataList = ModUtils::FindLimbsDismemberingData(target);
		if (!dismemberingDataList.empty()) {
			for (const ModData::DismemberingData& dismemberingData : dismemberingDataList) {
				if (!dismemberingData.disableCollision) continue;
				std::thread(DelayedToggleCollisionLayers, target->formID, dismemberingData.collisionNode, false, 10).detach();
			}
		}

		return continueEvent;
	}

	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::TESResetEvent* event, RE::BSTEventSource<RE::TESResetEvent>*)
	{
		if (!event->object || !event->object.get()) return continueEvent;
		
		RE::Actor* resetActor = event->object->As<RE::Actor>();
		if (!resetActor) return continueEvent;

		RE::TESNPC* baseResetActor = resetActor->GetActorBase();
		if (!baseResetActor || resetActor->IsDisabled() || baseResetActor->Respawns()) {
			ModUtils::ResetActorLimbFlags(resetActor);
		}

		return continueEvent;
	}

	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::TESFormDeleteEvent* event, RE::BSTEventSource<RE::TESFormDeleteEvent>*)
	{
		if (!event->formID) return continueEvent;

		ModUtils::ResetActorLimbFlags(event->formID);

		return continueEvent;
	}
}
