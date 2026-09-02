#pragma once

#include "API.h"
#include "DataHandler.hpp"
#include "JSON.h"
#include "Serialization.hpp"

#include "Utils/MiscUtils.hpp"
#include "Utils/ModUtils.hpp"

#include "API/NGD-API.h"

namespace Events
{
	using namespace ModData;
	using ActorType = DataHandler::ActorType;
	using WeaponType = DataHandler::WeaponType;
	using ImpulseData = DataHandler::ImpulseData;
	using DismembermentParams = DismemberingFrameworkAPI::DismembermentParams;

	using SkinIteratorType = decltype(JSONHandler::GetSingleton()->GetData().begin()->second.begin()->second.begin());

	inline std::unordered_map<RE::FormID, RE::HitData> deferredHitMap;
	inline std::shared_mutex deferredHitMapMutex;

	class MainEvent
	{
		public:

			using DecapitateParams = NGDecapitationsAPI::DecapitateParams;

			static void InstallHitHook()
			{
				REL::Relocation<uintptr_t> hook{ REL::RelocationID(37673, 38627) };
				SKSE::AllocTrampoline(1 << 4);
				auto& trampoline = SKSE::GetTrampoline();
				_ProcessHit = trampoline.write_call<5>(hook.address() + REL::Relocate(0x3C0, 0x4A8), ProcessHitTemplate);
				logger::info("ProcessHit hooked at address: 0x{:X}", _ProcessHit.address());

				REL::Relocation<std::uintptr_t> characterVtbl{ RE::VTABLE_Character[0] };
				_ResurrectHandler = characterVtbl.write_vfunc(REL::Module::IsVR() ? 0x0AD : 0x0AB, ResurrectHookTemplate);
				logger::info("ResurrectHandler hooked at address: 0x{:X}", _ResurrectHandler.address());
				
				REL::Relocation<std::uintptr_t> reanimateVtbl{ RE::VTABLE_ReanimateEffect[0] };
				_ReanimateHandler = reanimateVtbl.write_vfunc(0x14, ReanimateHookTemplate);
				logger::info("ReanimateHandler hooked at address: 0x{:X}", _ReanimateHandler.address());
			};

			static bool ProcessHit(RE::Actor* target, RE::HitData& hitData)
			{
				const DismembermentParams defaultParams;
				return ProcessDismemberment(target, hitData, defaultParams);
			}

			static void ProcessDeferredHit(RE::Actor* actor)
			{
				if (!actor || !SettingsIni::bDeferredHitProcess) return;

				{
					std::shared_lock lock(deferredHitMapMutex);
					auto it = deferredHitMap.find(actor->formID);
					if (it == deferredHitMap.end()) return;
					RE::HitData hitData = it->second;

					const float actorHealth = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth);
					if (actor && (actorHealth <= 0.0f || actor->IsDead())) {
						ProcessHit(actor, hitData);
						deferredHitMap.erase(it);
					}
				}
			}

			static bool ProcessDismemberment(RE::Actor* target, RE::HitData& hitData, const DismembermentParams& params);

			static void CastTargetEffectSpell(RE::Actor* target, RE::SpellItem* spell);

		private:

			static inline REL::Relocation<decltype(ProcessHit)> _ProcessHit;

			static inline REL::Relocation<void (*)(RE::Character*, bool, bool)> _ResurrectHandler;

			static inline REL::Relocation<void (*)(RE::ReanimateEffect*)> _ReanimateHandler;

			static void ResurrectHookTemplate(RE::Character* a_this, bool a_resetInventory, bool a_attach3D)
			{
				SKSE::GetTaskInterface()->AddTask([a_this]() {
					if (RE::Actor* actor = a_this->As<RE::Actor>()) {
						ModUtils::ResetActorLimbs(actor);
					}
				});

				_ResurrectHandler(a_this, a_resetInventory, a_attach3D);
			}

			static void ReanimateHookTemplate(RE::ReanimateEffect* a_this)
			{
				auto cancelReanimate = [&]() -> void {
					a_this->conditionStatus = RE::ActiveEffect::ConditionStatus::kFalse;
					a_this->flags.set(RE::ActiveEffect::Flag::kDispelled);
					a_this->flags.set(RE::ActiveEffect::Flag::kInactive);
					a_this->magnitude = 0.0f;
					a_this->duration = 0.0f;
				};

				if (a_this->commandedActor.get()) {
					RE::Actor* victim = a_this->commandedActor.get()->As<RE::Actor>();
					if (victim) {
						if (ModUtils::IsDismembered(victim)) {
							if (SettingsIni::bDismemberedCanBeReanimated) {
								ModUtils::ResetActorLimbs(victim);
							} else {
								cancelReanimate();
								return;
							}
						}
					}
				}
		
				_ReanimateHandler(a_this);
			}

			static void ProcessHitTemplate(RE::Actor* target, RE::HitData& hitData)
			{
				if (target->IsDead() || target->IsInKillMove()) {
					ProcessHit(target, hitData);
				} else if (!SettingsIni::bDeferredHitProcess) {
					if (ProcessHit(target, hitData)) {
						hitData.totalDamage *= 10.0f;
					}
				} else {
					{
						std::lock_guard lock(deferredHitMapMutex);
						deferredHitMap[target->formID] = hitData;
					}

					ModUtils::WaitAndCall(std::chrono::milliseconds(150), [victimFormID = target->formID]() {
						std::lock_guard lock(deferredHitMapMutex);
						deferredHitMap.erase(victimFormID);
					}, false);
				}

				_ProcessHit(target, hitData);
			}

			struct ActorTypeConditions
			{
				bool targetCanBeDismembered;
				bool aggressorCanDismember;
				bool aggressorCanDismemberIfFatal;
				bool aggressorCanDismemberIfDead;
				bool aggressorCanDismemberIfKillmove;
			};

			struct WeapTypeConditions
			{
				bool  canDismember;
				bool  regularAttackChance;
				bool  powerAttackChance;
				float precision;
				bool  multiple;
				bool  ifHeavy;
				bool  ifBlocked;
				float impulse;
			};

			static bool AreConditionsValid(const DismemberingData& dismemberingData, RE::Actor* target);

			static void CastPlayerEffectSpell(RE::Actor* target, RE::Actor* aggressor, RE::SpellItem* spell, bool targetIsDead);

			static RE::TESObjectREFR* CutLimb(RE::Actor* target, const DismemberingData& dismemberingData);

			static RE::TESObjectREFR* DropTargetItem(RE::Actor* target, const DismemberingData& dismemberingData);

			static bool EvaluateCondition(const DismemberingData& dismemberingData, RE::Actor* target, const Condition& condition);

			static ActorType GetActorType(RE::Actor* actor);

			static ActorTypeConditions* GetActorTypeConditions(RE::Actor* target, RE::Actor* aggressor);

			static WeaponType GetWeaponType(RE::TESObjectWEAP* weapon, RE::TESRace* targetRace);

			static WeapTypeConditions* GetWeapTypeConditions(WeaponType weaponType);

			static bool IsDismemberable(RE::Actor* target, RE::Actor* aggressor, RE::HitData& hitData);

			static void EquipArmorAddon(RE::Actor* target, const DismemberingData& dismemberingData);

			static RE::TESObjectREFR* PlaceLimbCenter(RE::TESForm* limb, RE::Actor* target, std::string refNode, RE::NiPoint3 localTranslation);

			static void PlayDismemberSound(RE::Actor* target, const WeaponType weapType, const DismemberingData& dismemberingData, bool isParrying);

			static void RegisterLimbData(RE::TESObjectREFR* limb, RE::Actor* target, const RE::HitData& hitData, const DismemberingData& dismemberingData, float impulseMagnitude, RE::TESObjectREFR* droppedItem);

			static ImpulseData SetLimbDataImpulse(const RE::HitData& hitData, float magnitude);

			static bool TestActorTypes(ActorTypeConditions* actorTypeConditions, RE::Actor* target, RE::Actor* aggressor);

			static bool TestWeaponType(WeapTypeConditions* weapTypeConditions, RE::HitData& hitData);

			static std::vector<std::string> ValidActorNode(SkinIteratorType skinObj, RE::Actor* target, const std::string& node);

			static std::vector<std::string> ValidActorNodes(SkinIteratorType skinObj, RE::Actor* target, RE::NiPoint3 hitPosition, float hitPrecision, bool multipleNodes);

			static std::optional<SkinIteratorType> ValidActorData(RE::Actor* target);
	};
};
