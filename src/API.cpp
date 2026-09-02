#include "API.h"
#include "DataHandler.hpp"
#include "Main.h"

#include "Utils/ModUtils.hpp"

size_t DismemberingFrameworkAPI::DismemberingFrameworkAPI::GetVersion() const
{
	return DF_API_VERSION;
}

void DismemberingFrameworkAPI::DismemberingFrameworkAPI::Dismember(RE::Actor* target, const RE::BSFixedString& node, RE::Actor* aggressor, RE::TESObjectWEAP* weapon, const DismembermentParams* params) const
{
	if (!target) return;

	DismembermentParams f_params;
	if (!params) {
		f_params.forceExecution = true;
		f_params.specificNode = node;
		f_params.ignoreArmorClass = true;
		f_params.noLimbImpulse = false;
		f_params.noSoundEffect = true;
		f_params.noPlayerEffect = true;
	} else {
		f_params = *params;
	}

	RE::HitData hitData;
	if (!f_params.hitData) {
		hitData.target = target ? target->GetHandle() : RE::ActorHandle();
		hitData.aggressor = aggressor ? aggressor->GetHandle() : RE::ActorHandle();
		hitData.weapon = weapon;
		if (f_params.specificNode != "") {
			const RE::NiAVObject* niAvObj = target->GetNodeByName(f_params.specificNode);
			if (niAvObj) {
				hitData.hitPosition = niAvObj->world.translate;
			}
		}
		
	} else {
		hitData = *f_params.hitData;
	}

	Events::MainEvent::ProcessDismemberment(target, hitData, f_params);
}

bool DismemberingFrameworkAPI::DismemberingFrameworkAPI::IsDismembered(RE::Actor* actor) const
{
	return ModUtils::IsDismembered(actor);
}

bool DismemberingFrameworkAPI::DismemberingFrameworkAPI::IsDismemberedNode(RE::Actor* actor, const RE::BSFixedString& node) const
{
	return ModUtils::IsDismemberedNode(actor, node);
}

void DismemberingFrameworkAPI::DismemberingFrameworkAPI::PostDecapitate(RE::Actor* actor, RE::Actor* head) const
{
	const auto* dataHandler = ModData::DataHandler::GetSingleton();
	if (!dataHandler->DF_Humanoid_Decapitate_Spell) return;
	
	Events::MainEvent::CastTargetEffectSpell(actor, dataHandler->DF_Humanoid_Decapitate_Spell);
	Events::MainEvent::CastTargetEffectSpell(head, dataHandler->DF_Humanoid_Decapitate_Spell);
}

void DismemberingFrameworkAPI::DismemberingFrameworkAPI::RefreshActorDismemberedState(RE::Actor* actor) const
{
	SKSE::GetTaskInterface()->AddTask([actor]() {
		ModUtils::RefreshActorDismemberedState(actor);
	});
}
