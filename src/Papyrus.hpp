#pragma once

#include "DataHandler.hpp"
#include "Main.h"
#include "JSON.h"

namespace Papyrus
{
	void CastBloodSpray(RE::StaticFunctionTag*, std::vector<RE::SpellItem*> spells, RE::TESObjectREFR* object, RE::BSFixedString node,
		const float strength = 1.0f, const bool swapNodes = false)
	{
		if (spells.empty() || !object) return;

		SKSE::GetTaskInterface()->AddTask([spells, objectFormID = object->formID, node, strength, swapNodes]() {
			const auto& dataHandler = ModData::DataHandler::GetSingleton();

			RE::TESObjectREFR* object = RE::TESForm::LookupByID<RE::TESObjectREFR>(objectFormID);
			if (!object || !object->Is3DLoaded()) return;

			RE::NiAVObject* nodeTarget = object->GetNodeByName(node);
			if (!nodeTarget) return;
		
			RE::NiAVObject* nodeSource = nodeTarget->parent ? nodeTarget->parent : nodeTarget;
			if (!nodeSource) return;

			const RE::NiPoint3 rotatedLocal = nodeSource->world.rotate * nodeTarget->local.translate;
			const RE::NiPoint3 posSourceFinal = ModUtils::GetSpreadPosition(nodeTarget->world.translate, nodeTarget->world.translate + rotatedLocal, 0.20f);

			RE::NiPoint3 position = nodeTarget->world.translate;
			RE::Projectile::ProjectileRot rotation = MiscUtils::DirToAngles(posSourceFinal - position, true);

			if (swapNodes) rotation = MiscUtils::DirToAngles(posSourceFinal - position, false);

			for (auto* spell : spells) {
				if (!spell) continue;
				if (ModData::DataHandler::coreImpactFrameworkEnabled) ModUtils::ApplyCIFBloodCollisionLayer(spell);

				RE::Actor* player = RE::PlayerCharacter::GetSingleton();
				if (!player || !player->Is3DLoaded()) return;

				RE::Projectile::LaunchData launchData(player, position, rotation, spell);
				launchData.shooter = object;
				launchData.parentCell = object->GetParentCell();
				launchData.desiredTarget = nullptr;
				launchData.combatController = nullptr;
				launchData.autoAim = false;

				RE::ProjectileHandle projectileHandle;
				RE::Projectile::Launch(&projectileHandle, launchData);
				if (auto* genProjectile = projectileHandle ? projectileHandle.get().get() : nullptr) {
					genProjectile->inGameFormFlags.set(RE::TESForm::InGameFormFlag::kWantsDelete, RE::TESForm::InGameFormFlag::kRefPermanentlyDeleted);
					if (SettingsIni::bTrueDirectionalMovementFix) genProjectile->GetProjectileRuntimeData().shooter = RE::ObjectRefHandle();
					genProjectile->GetProjectileRuntimeData().speedMult *= (0.4f + 0.75f * strength);
				}

				if (ModUtils::GetGlobalValue<int>(ModData::DataHandler::PluginGlobal::Features_BloodSpray_Mode) < 1) break;
			}
		});
	}

	void ClearLinkedData(RE::StaticFunctionTag*, RE::TESObjectREFR* limb)
	{
		if (!limb) return;

		ModUtils::DeleteLimbData(limb->formID);
	}
	
	void Dismember(RE::StaticFunctionTag*, RE::Actor* target, RE::BSFixedString node, RE::Actor* aggressor = nullptr, RE::TESObjectWEAP* weapon = nullptr, bool noLimbImpulse = true, bool noSoundEffect = true, bool noPlayerEffect = true)
	{
		if (!target || node.empty()) return;
		if (!target->IsDead()) {
			target->KillImpl(aggressor, 1.0f, true, true);
		}

		Events::DismembermentParams f_params;
		f_params.forceExecution = true;
		f_params.specificNode = node;
		f_params.ignoreArmorClass = true;
		f_params.noLimbImpulse = noLimbImpulse;
		f_params.noSoundEffect = noSoundEffect;
		f_params.noPlayerEffect = noPlayerEffect;

		RE::HitData hitData;
		hitData.target = target ? target->GetHandle() : RE::ActorHandle();
		hitData.aggressor = aggressor ? aggressor->GetHandle() : RE::ActorHandle();
		hitData.weapon = weapon;
		const RE::NiAVObject* niAvObj = target->GetNodeByName(f_params.specificNode);
		if (niAvObj) {
			hitData.hitPosition = niAvObj->world.translate;
		}

		Events::MainEvent::ProcessDismemberment(target, hitData, f_params);
	}

	std::vector<uint32_t> GetDFVersion(RE::StaticFunctionTag*)
	{
		using namespace SKSE;
        const auto* plugin = PluginDeclaration::GetSingleton();
        auto version = plugin->GetVersion();

        uint32_t versionMajor = plugin->GetVersion().major();
        uint32_t versionMinor = plugin->GetVersion().minor();
        uint32_t versionPatch = plugin->GetVersion().patch();

		std::vector<uint32_t> versionVector;
		versionVector.push_back(versionMajor);
		versionVector.push_back(versionMinor);
		versionVector.push_back(versionPatch);

		return versionVector;
	}

	bool IsDismembered(RE::StaticFunctionTag*, RE::Actor* actor)
	{
		if (!actor) return false;

		return ModUtils::IsDismembered(actor);
	}

	bool IsDismemberedNode(RE::StaticFunctionTag*, RE::Actor* actor, RE::BSFixedString node)
	{
		if (!actor) return false;

		return ModUtils::IsDismemberedNode(actor, node);
	}

	bool IsNGDInstalled(RE::StaticFunctionTag*)
	{
		return ModData::DataHandler::nextGenDecapitationsEnabled;
	}

	RE::Actor* GetLinkedActor(RE::StaticFunctionTag*, RE::TESObjectREFR* limb)
	{
		if (!limb) return nullptr;

		using namespace ModData;
		auto& limbDataMap = ModData::DataHandler::GetSingleton()->limbDataMap;
		auto it = limbDataMap.find(limb->formID);
		if (it == limbDataMap.end()) return nullptr;

		DataHandler::LimbData limbData = it->second;

		return limbData.linkedActor;
	}

	RE::BSFixedString GetLinkedNode(RE::StaticFunctionTag*, RE::TESObjectREFR* limb)
	{
		if (!limb) return "";

		using namespace ModData;
		auto& limbDataMap = ModData::DataHandler::GetSingleton()->limbDataMap;
		auto it = limbDataMap.find(limb->formID);
		if (it == limbDataMap.end()) return "";

		DataHandler::LimbData limbData = it->second;

		return limbData.nodeName;
	}

	void RefreshActorDismemberedState(RE::StaticFunctionTag*, RE::Actor* actor)
	{
		if (!actor) return;

		ModUtils::RefreshActorDismemberedState(actor);
	}

	void RequestVariablesUpdate(RE::StaticFunctionTag*)
	{
		// Empty Function
	}

	void ResetActorLimbs(RE::StaticFunctionTag*, RE::Actor* actor)
	{
		if (!actor) return;

		ModUtils::ResetActorLimbs(actor);
	}

	void SetGlobalTimeMultiplier(RE::StaticFunctionTag*, float multiplier)
	{
		if (!REL::Module::IsVR() && multiplier < 1.0f) {  // Crash with HasEffectWithArchetype on VR (MagicTarget.cpp:54)
			if (auto* player = RE::PlayerCharacter::GetSingleton();
				player && player->AsMagicTarget() &&
				player->AsMagicTarget()->HasEffectWithArchetype(RE::EffectArchetypes::ArchetypeID::kSlowTime)) {
				return;
			}
		}

		MiscUtils::SetGlobalTimeMultiplier(multiplier, false);
	}
	
	void SetLimbName(RE::StaticFunctionTag*, RE::TESObjectREFR* limb, RE::Actor* actor)
	{
		if (!limb || !actor) return;

		limb->SetDisplayName(actor->GetName() ? std::string(actor->GetName()) + " " : "", true);
	}

	bool ShouldIgnoreMaintenanceChecks(RE::StaticFunctionTag*)
	{
		return SettingsIni::bShouldIgnoreMaintenanceChecks;
	}

	void UpdateLimbScaleAndTint(RE::StaticFunctionTag*, RE::TESObjectREFR* limb, RE::Actor* actor)
	{
		ModUtils::UpdateLimbScaleAndTint(limb, actor);
	}

	bool BindPapyrusFunctions(RE::BSScript::IVirtualMachine* vm)
	{
		vm->RegisterFunction("CastBloodSpray", "DismemberingFramework", CastBloodSpray);
		vm->RegisterFunction("ClearLinkedData", "DismemberingFramework", ClearLinkedData);
		vm->RegisterFunction("Dismember", "DismemberingFramework", Dismember);
		vm->RegisterFunction("GetDFVersion", "DismemberingFramework", GetDFVersion);
		vm->RegisterFunction("GetLinkedActor", "DismemberingFramework", GetLinkedActor);
		vm->RegisterFunction("GetLinkedNode", "DismemberingFramework", GetLinkedNode);
		vm->RegisterFunction("IsDismembered", "DismemberingFramework", IsDismembered);
		vm->RegisterFunction("IsDismemberedNode", "DismemberingFramework", IsDismemberedNode);
		vm->RegisterFunction("IsNGDInstalled", "DismemberingFramework", IsNGDInstalled);
		vm->RegisterFunction("RefreshActorDismemberedState", "DismemberingFramework", RefreshActorDismemberedState);
		vm->RegisterFunction("RequestVariablesUpdate", "DismemberingFramework", RequestVariablesUpdate);
		vm->RegisterFunction("ResetActorLimbs", "DismemberingFramework", ResetActorLimbs);
		vm->RegisterFunction("SetGlobalTimeMultiplier", "DismemberingFramework", SetGlobalTimeMultiplier);
		vm->RegisterFunction("SetLimbName", "DismemberingFramework", SetLimbName);
		vm->RegisterFunction("ShouldIgnoreMaintenanceChecks", "DismemberingFramework", ShouldIgnoreMaintenanceChecks);
		vm->RegisterFunction("UpdateLimbScaleAndTint", "DismemberingFramework", UpdateLimbScaleAndTint);
		return true;
	}
};
