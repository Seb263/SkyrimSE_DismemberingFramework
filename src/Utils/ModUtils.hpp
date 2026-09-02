#pragma once

#include "DataHandler.hpp"
#include "MiscUtils.hpp"
#include "JSON.h"
#include "SettingsIni.hpp"
#include "Serialization.hpp"

#include "API/NGD-API.h"

#define FRAME_DELAY_MS() std::chrono::milliseconds(static_cast<int>(std::lround(ModUtils::GetFrameDelay() * 1000.0f)))

class ModUtils
{
public:

	template <typename T>
	static T GetGlobalValue(ModData::DataHandler::PluginGlobal global_enum) {
		using namespace ModData;

		const auto it = DataHandler::pluginGlobalPointers.find(global_enum);
		if (it != DataHandler::pluginGlobalPointers.end() && it->second) {
			if constexpr (std::is_same_v<T, float> || std::is_same_v<T, int>) {
				return static_cast<T>(it->second->value);
			} else if constexpr (std::is_same_v<T, bool>) {
				return static_cast<T>(it->second->value > 0.0f);
			}
		}

		logger::warn("Global Value \"{}\" not found.", DataHandler::GetPluginGlobalEditorID(global_enum));
		return static_cast<T>(0);
	}

	enum class ActorsAction
	{
		kCheck,
		kAdd,
		kRemove
	};
	static bool AlteredActorsAction(ActorsAction action, RE::FormID formID)
	{
		static std::vector<RE::FormID> alteredActorsMap;

		if (action == ActorsAction::kCheck) {
			const auto& it = std::find(alteredActorsMap.begin(), alteredActorsMap.end(), formID);
			return it != alteredActorsMap.end();
		} else if (action == ActorsAction::kAdd) {
			alteredActorsMap.push_back(formID);
		} else if (action == ActorsAction::kRemove) {
			alteredActorsMap.erase(std::remove(alteredActorsMap.begin(), alteredActorsMap.end(), formID), alteredActorsMap.end());
		} else {
			return false;
		}

		return true;
	}

	static void ProcessGridCells(std::function<RE::BSContainer::ForEachResult(RE::TESObjectREFR*)> forEachLambda, RE::TESObjectCELL* cell = nullptr)
	{
		const auto& tes = RE::TES::GetSingleton();
		if (!tes) return;

		if (!cell) cell = tes->interiorCell;

		if (cell && cell->IsAttached()) {
			cell->ForEachReference(forEachLambda);
			return;
		}

		if (const auto gridLength = tes->gridCells ? tes->gridCells->length : 0; gridLength > 0) {
			std::uint32_t x = 0;
			do {
				std::uint32_t y = 0;
				do {
					if (const auto cell_alt = tes->gridCells->GetCell(x, y); cell_alt && cell_alt->IsAttached()) {
						cell_alt->ForEachReference(forEachLambda);
					}
					++y;
				} while (y < gridLength);
				++x;
			} while (x < gridLength);
		}
	}

	static void ActorMaintenanceProcess(RE::TESObjectCELL* cell = nullptr)
	{
		using namespace std::chrono;
		using namespace ModData;

		static std::unordered_map<RE::TESObjectCELL*, system_clock::time_point> lastCallTimes;

		const auto currentTime = system_clock::now();
		auto& lastCallTime = lastCallTimes[cell];

		auto elapsedTime = duration_cast<milliseconds>(currentTime - lastCallTime).count();
		if (elapsedTime < 100) return;

		auto* player = RE::TESForm::LookupByID<RE::Actor>(0x14);
		if (!player || !player->GetParentCell() || !player->GetParentCell()->IsAttached() || !player->Get3D()) {
			return;
		}
		lastCallTime = currentTime;

		const auto& dataHandler = DataHandler::GetSingleton();
		const auto& forEachLambda = [&cell, &dataHandler](RE::TESObjectREFR* ref) -> RE::BSContainer::ForEachResult {
			if (!ref || !ref->GetParentCell() || !ref->GetParentCell()->IsAttached() || !ref->Get3D() || (cell && ref->GetParentCell() != cell)) {
				return RE::BSContainer::ForEachResult::kContinue;
			}

			if (ref->formType == RE::FormType::ActorCharacter) { // Actor
				RE::Actor* actor = ref->As<RE::Actor>();
				if (!actor) return RE::BSContainer::ForEachResult::kContinue;

				const int limbFlags = Serialization::GetDismemberedLimbs(actor->formID);
				if (limbFlags < 1 && !AlteredActorsAction(ActorsAction::kCheck, actor->formID)) {
					return RE::BSContainer::ForEachResult::kContinue;
				}

				RefreshActorDismemberedState(actor);
			} else if (ref->HasKeyword(dataHandler->limbKeyword)) { // Limb
				ref->ActivateRef(ref, 0, ref->GetObjectReference(), 1, false);
			}

			return RE::BSContainer::ForEachResult::kContinue;
		};

		ProcessGridCells(forEachLambda, cell);
	}

	static void RefreshActorDismemberedState(RE::Actor* actor)
	{
		using namespace ModData;
		
		if (!actor) return;

		ResetActorNodesAndCollisionLayers(actor);

		const std::vector<DismemberingData>& dismemberingDataList = FindLimbsDismemberingData(actor);
		if (!dismemberingDataList.empty()) {
			for (const DismemberingData& dismemberingData : dismemberingDataList) {
				if (dismemberingData.method == DismemberingData::Methods::kDismemberingFramework) {
					if (dismemberingData.disableCollision) ToggleCollisionLayers(actor, dismemberingData.collisionNode, false);
					if (dismemberingData.resizeNode) ToggleNode(actor, dismemberingData.targetedNode, false);
				}
			}
		} else {
			ResetActorLimbFlags(actor);
		}
	}

	static void ResetActorLimbs(RE::Actor* actor)
	{
		const int limbFlags = ModData::Serialization::GetDismemberedLimbs(actor->formID);
		if (limbFlags > 0) {
			const auto& dataHandler = ModData::DataHandler::GetSingleton();
			const auto& forEachLambda = [&actor, &dataHandler](RE::TESObjectREFR* ref) -> RE::BSContainer::ForEachResult {
				if (!ref || !ref->GetParentCell() || !ref->GetParentCell()->IsAttached() || !ref->Get3D()) {
					return RE::BSContainer::ForEachResult::kContinue;
				}

				if (ref->HasKeyword(dataHandler->limbKeyword)) {
					ref->ActivateRef(actor, 0, ref->GetObjectReference(), 1, false);
				}

				return RE::BSContainer::ForEachResult::kContinue;
			};

			ProcessGridCells(forEachLambda);
			ResetActorNodesAndCollisionLayers(actor, true);
			ResetActorLimbFlags(actor);
		}
	}

	static bool IsDismembered(RE::Actor* actor)
	{
		if (!actor) return false;

		if (ModData::Serialization::GetDismemberedLimbs(actor->formID) > 0) return true;

		if (ModData::DataHandler::nextGenDecapitationsEnabled && NGDecapitationsAPI::g_API->IsDecapitated(actor)) return true;

		return false;
	}

	static bool IsDismemberedNode(RE::Actor* actor, const RE::BSFixedString& node)
	{
		if (!actor) return false;
		auto* niNode = actor->GetNodeByName(node);
		if (!niNode || !niNode->AsNode()) return false;

		return IsDismemberedNodeTree(niNode->AsNode());
	}

	static bool IsDismemberedLimb(RE::Actor* actor, int idKeyword)
	{
		if (!actor) return false;

		const int limbFlags = ModData::Serialization::GetDismemberedLimbs(actor->formID);
		return (limbFlags & (1 << idKeyword)) != 0;
	}

	static bool IsDecapitated(RE::Actor* target)
	{
		if (!target) return false;

		const RE::TESRace* race = target->GetRace();
		if (!race) return false;

		const RE::TESObjectARMO* decapitateArmor = race->decapitateArmors[target->GetActorBase()->GetSex()];
		if (!decapitateArmor) return false;

		const RE::InventoryChanges* inventoryChanges = target->GetInventoryChanges();
		RE::BSSimpleList<RE::InventoryEntryData*>::iterator itr, itrEnd;
		itrEnd = inventoryChanges->entryList->end();

		for (itr = inventoryChanges->entryList->begin(); itr != itrEnd; ++itr) {
			RE::InventoryEntryData* inventoryEntryData = *itr;
			RE::TESForm*            item = inventoryEntryData->object;
			if (item->formID == decapitateArmor->formID) return true;
		}

		return false;
	}

	static bool IsLatestEnemyAlive(RE::Actor* target, RE::Actor* aggressor)
	{
		if (!target || !aggressor) return false;
		
		int nb_hostiles = 0;
		const auto& forEachLambda = [&nb_hostiles, &target, &aggressor](RE::TESObjectREFR* ref) -> RE::BSContainer::ForEachResult {
			if (!ref || ref->formType != RE::FormType::ActorCharacter || !ref->GetParentCell() || !ref->GetParentCell()->IsAttached() ||
				ref->GetDistance(aggressor) > 4096.0f) return RE::BSContainer::ForEachResult::kContinue;
			
			RE::Actor* actor = ref->As<RE::Actor>();
			if (!actor || actor == target || actor == aggressor || !actor->Is3DLoaded()) return RE::BSContainer::ForEachResult::kContinue;

			if (!actor->AsActorState() || !actor->AsActorState()->IsBleedingOut()) {
				if (actor->IsHostileToActor(aggressor) && actor->IsInCombat() && actor->RequestDetectionLevel(aggressor) > 1) {
					nb_hostiles++;
				}
			}

			return RE::BSContainer::ForEachResult::kContinue;
		};

		ProcessGridCells(forEachLambda);

		return (nb_hostiles < 1);
	}

	static std::vector<ModData::DismemberingData> FindLimbsDismemberingData(RE::Actor* actor)
	{
		using namespace ModData;
		JSONHandler* JSONHandler = JSONHandler::GetSingleton();
		std::vector<DismemberingData> dismemberingDataList;

		if (!actor) return dismemberingDataList;

		if (!actor->GetRace()) return dismemberingDataList;
		std::string race = actor->GetRace()->GetFormEditorID();
		
		std::string skin = "";
		if (actor->GetSkin()) {
			skin = MiscUtils::GetAssocStringFromForm(actor->GetSkin()->As<RE::TESForm>());
		}
		std::string sex = (actor->GetActorBase()->GetSex() == RE::SEX::kFemale ? "Female" : "Male");

		const int limbFlags = Serialization::GetDismemberedLimbs(actor->formID);
		for (int idKeyword = 0; idKeyword < sizeof(limbFlags) * 8; ++idKeyword) {
			if (limbFlags & (1 << idKeyword)) {
				auto dismemberingDataOpt = JSONHandler->GetDismemberingDataFromIdKeyword(race, skin, sex, idKeyword);
				if (dismemberingDataOpt) {
					dismemberingDataList.push_back(dismemberingDataOpt.value());
				}
			}
		}

		return dismemberingDataList;
	}

	static void ToggleCollisionLayers(RE::Actor* actor, const std::string& node, bool enable)
	{
		if (!actor) return;

		auto niNode = actor->GetNodeByName(node);
		if (!niNode || !niNode->AsNode()) return;

		ToggleCollisionForNodeTree(niNode->AsNode(), enable);
	}

	static void ToggleNode(RE::Actor* actor, const std::string& node, bool enable)
	{
		if (!actor) return;

		// Lambda function to scale the node
		auto ScaleNode = [](RE::NiNode* node, bool enable) {
			if (node->name == "NPC Neck [Neck]") {
				node->local.scale = (enable ? 1.0f : -0.01f); // (enable ? 1.0f : -0.15f)
			} else {
				node->local.scale = (enable ? 1.0f : -0.01f); // (enable ? 1.0f : -0.05f)
			}
			ToggleNodeForNodeTree(node, enable);
		};

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (actor == player) {
			// Handle first-person view
			auto* niNode1st = actor->Get3D(true) ? actor->Get3D(true)->GetObjectByName(node) : nullptr;
			if (niNode1st && niNode1st->AsNode()) {
				ScaleNode(niNode1st->AsNode(), enable);
			}

			// Handle third-person view
			auto* niNode3rd = actor->Get3D(false) ? actor->Get3D(false)->GetObjectByName(node) : nullptr;
			if (niNode3rd && niNode3rd->AsNode()) {
				ScaleNode(niNode3rd->AsNode(), enable);
			}

			if ((!niNode1st || !niNode1st->AsNode()) && (!niNode3rd || !niNode3rd->AsNode())) return;
		} else {
			auto* niNode = actor->GetNodeByName(node);
			if (!niNode || !niNode->AsNode()) return;
			ScaleNode(niNode->AsNode(), enable);
		}

		if (!enable && !AlteredActorsAction(ActorsAction::kCheck, actor->formID)) {
			AlteredActorsAction(ActorsAction::kAdd, actor->formID);
		}
	}

	static RE::NiPoint3 GetSpreadPosition(const RE::NiPoint3& from, const RE::NiPoint3& to, const float spread = 0.0f)
	{
		const float distance = from.GetDistance(to);
		const float spread_radius = distance * spread;

		RE::NiPoint3 offset{};
		offset.x = MiscUtils::GetRandomNumber(-spread_radius, spread_radius);
		offset.y = MiscUtils::GetRandomNumber(-spread_radius, spread_radius);
		offset.z = MiscUtils::GetRandomNumber(-spread_radius, spread_radius);

		offset.z += distance * 0.22f;

		return to + offset;
	}

	static void ApplyCIFBloodCollisionLayer(RE::SpellItem* spell) {
		static std::unordered_set<RE::FormID> processedSpells;
		const auto& dataHandler = ModData::DataHandler::GetSingleton();

		if (!spell || spell->effects.empty()) return;
		if (!processedSpells.insert(spell->formID).second) return;
		if (!dataHandler->ModRuntimeBlood_CollisionLayer) return;

		for (auto* effect : spell->effects) {
			if (!effect || !effect->baseEffect || !effect->baseEffect->data.projectileBase) continue;

			effect->baseEffect->data.projectileBase->data.collisionLayer = dataHandler->ModRuntimeBlood_CollisionLayer;
		}
	};

	static void AddActorLimbFlag(RE::Actor* actor, int idKeyword)
	{
		if (!actor) return;

		ModData::Serialization::AddDismemberedLimbKeyword(actor->formID, (1 << idKeyword));
	}

	static void ResetActorLimbFlags(RE::Actor* actor)
	{
		if (!actor) return;

		ModData::Serialization::ResetDismemberedLimbs(actor->formID);
	}

	static void ResetActorLimbFlags(RE::FormID actorFormID)
	{
		if (!actorFormID) return;

		ModData::Serialization::ResetDismemberedLimbs(actorFormID);
	}

	static void ResetActorNodesAndCollisionLayers(RE::Actor* actor, const bool resetAddons = false)
	{
		if (!actor) return;

		using namespace ModData;
		JSONHandler* JSONHandler = JSONHandler::GetSingleton();
		std::string race = actor->GetRace()->GetFormEditorID();
		std::string skin = MiscUtils::GetAssocStringFromForm(actor->GetSkin()->As<RE::TESForm>());
		std::string sex = (actor->GetActorBase()->GetSex() == RE::SEX::kFemale ? "Female" : "Male");

		auto nodeMapPtr = JSONHandler->GetNodeMap(race, skin, sex);
		if (nodeMapPtr != nullptr) {
			for (const auto& [id, dismemberingData] : *nodeMapPtr) {
				auto* niNode = actor->GetNodeByName(dismemberingData.targetedNode);
				if (!niNode || !niNode->AsNode() || niNode->local.scale > 0.0f) continue;

				ToggleCollisionLayers(actor, dismemberingData.collisionNode, true);
				ToggleNode(actor, dismemberingData.targetedNode, true);
				if (resetAddons && dismemberingData.actorArmorAddon) {
					for (const auto& [form, data] : actor->GetInventory()) {
						if (form->formID == dismemberingData.actorArmorAddon->formID && data.second > 0) {
							actor->RemoveItem(form, data.first, RE::ITEM_REMOVE_REASON::kRemove, &(actor->extraList), nullptr);
							break;
						}
					}
				}
			}
		}

		AlteredActorsAction(ActorsAction::kRemove, actor->formID);
	}

	static std::optional<ModData::DataHandler::LimbData> GetLimbData(RE::FormID limbFormId)
	{
		auto& limbDataMap = ModData::DataHandler::GetSingleton()->limbDataMap;
		auto it = limbDataMap.find(limbFormId);
		if (it == limbDataMap.end()) return std::nullopt;

		return it->second;
	}

	static void DeleteLimbData(RE::FormID limbFormId)
	{
		auto& limbDataMap = ModData::DataHandler::GetSingleton()->limbDataMap;
		auto it = limbDataMap.find(limbFormId);
		if (it == limbDataMap.end()) return;

		limbDataMap.erase(limbFormId);
	}

	static void UpdateLimbScaleAndTint(RE::TESObjectREFR* limb, RE::Actor* actor)
	{
		if (!limb || !actor) return;

		auto* limbObj = limb->Get3D();
		if (!limbObj) return;

		RE::TESNPC* baseActorForm = actor->GetActorBase();
		if (!baseActorForm) return;

		baseActorForm = ModUtils::GetTraitTemplate(baseActorForm);
		if (!baseActorForm) return;
		
		RE::NiColor bodyColor = baseActorForm->bodyTintColor;
		RE::NiColor hairColor;

		if (baseActorForm->headRelatedData && baseActorForm->headRelatedData->hairColor) {
			hairColor = baseActorForm->headRelatedData->hairColor->color;
		} else {
			const RE::TESRace* race = actor->GetRace();
			if (race && race->faceRelatedData && race->faceRelatedData[baseActorForm->GetSex()]) {
				const RE::BGSColorForm* defaultHairColor = race->faceRelatedData[baseActorForm->GetSex()]->defaultHairColor;
				if (defaultHairColor) hairColor = defaultHairColor->color;
			}
		}

		RE::BSVisit::TraverseScenegraphGeometries(limbObj, [&](RE::BSGeometry* a_geometry) -> RE::BSVisit::BSVisitControl {
			using Feature = RE::BSShaderMaterial::Feature;

			if (auto shaderProp = a_geometry->GetGeometryRuntimeData().shaderProperty.get()) {
				auto lightingShader = netimmerse_cast<RE::BSLightingShaderProperty*>(shaderProp);
				if (lightingShader) {
					auto material = lightingShader->material;
					if (material && material->GetFeature() == Feature::kFaceGenRGBTint) {
						auto facegenTint = static_cast<RE::BSLightingShaderMaterialFacegenTint*>(material);

						const auto& flags = a_geometry->GetFlags();
						if (flags & RE::NiAVObject::Flag::kNoDecals) {
							facegenTint->tintColor = hairColor;
						} else {
							facegenTint->tintColor = bodyColor;
						}
					}
				}
			}

			return RE::BSVisit::BSVisitControl::kContinue;
		});

		RE::NiUpdateData updateData{ 0.0f, RE::NiUpdateData::Flag::kNone };
		limbObj->Update(updateData);
	}

	static void ApplyLinearImpulse(RE::TESObjectREFR* limb, RE::NiPoint3 fromPosition, RE::NiPoint3 toPosition, float impulseMagnitude)
	{
		if (!limb) return;

		auto limbObject = limb->Get3D();
		if (!limbObject) return;

		auto bhkCollisionObject = limbObject->GetCollisionObject();
		if (!bhkCollisionObject) return;

		auto rb = bhkCollisionObject->GetRigidBody();
		if (!rb) return;

		auto rigidBody = RE::NiPointer<RE::bhkRigidBody>(rb);
		if (!rigidBody) return;

		auto rigidBodyRef = rigidBody->referencedObject;
		if (!rigidBodyRef || !rigidBodyRef.get()) return;

		auto hkpRigidBody = RE::hkRefPtr<RE::hkpRigidBody>(static_cast<RE::hkpRigidBody*>(rigidBodyRef.get()));
		if (!hkpRigidBody) return;

		float dx = toPosition.x - fromPosition.x;
		float dy = toPosition.y - fromPosition.y;
		float dz = toPosition.z - fromPosition.z;
		float norm = std::sqrt(dx * dx + dy * dy + dz * dz);

		if (norm != 0.0f) {
			float ux = dx / norm;
			float uy = dy / norm;
			float uz = dz / norm;

			RE::hkVector4 impulse = { ux * impulseMagnitude, uy * impulseMagnitude, uz * impulseMagnitude, 0.0f };
			hkpRigidBody->motion.ApplyLinearImpulse(impulse);
		}
	}

	static bool WaitForGameReady(bool ignoreLoadingMenu = false)
	{
		bool wasPaused = false;

		while (true) {
			if (auto ui = RE::UI::GetSingleton(); ui && ui->GameIsPaused()) {
				static auto loadingMenu = ui->GetMenu("Loading Menu");
				if (ignoreLoadingMenu && ui->numPausesGame == 1 && loadingMenu && loadingMenu->OnStack()) break;

				std::this_thread::sleep_for(FRAME_DELAY_MS());
				wasPaused = true;
				continue;
			}

			std::promise<void> p;
			auto f = p.get_future();

			SKSE::GetTaskInterface()->AddTask([&p]() { p.set_value(); });
        
			auto start = std::chrono::high_resolution_clock::now();
			f.get();

			if ((std::chrono::high_resolution_clock::now() - start) > 100ms) {
				wasPaused = true;
				continue;
			}

			break;
		}

		return wasPaused;
	}

	template <typename TDuration, typename TCallback>
	static void WaitAndCall(TDuration delay, TCallback&& callback, const bool secureFrame = true)
	{
		std::jthread([delay, callback = std::forward<TCallback>(callback), secureFrame]() mutable {
			WaitForGameReady();
			auto failure = std::make_shared<std::atomic_bool>(false);
			const auto deadline = (std::chrono::steady_clock::now() + delay);

			while (true) {
				auto remaining = (deadline - std::chrono::steady_clock::now());
				std::this_thread::sleep_for(remaining > 100ms ? 100ms : (remaining > 0ns ? remaining : FRAME_DELAY_MS()));

				const bool last = (std::chrono::steady_clock::now() >= deadline);
				SKSE::GetTaskInterface()->AddTask([callback, secureFrame, failure, last, taskStart = std::chrono::steady_clock::now()]() {
					if (secureFrame && std::chrono::steady_clock::now() - taskStart > 300ms) *failure = true;
					if (last) {
						if (*failure) TRACE("WaitAndCall: Task was delayed and invalidated due to frame timing (>{}ms)", 300);
						else callback();
					}
				});
				if (last) break;
			}

		}).detach();
	}

	template <typename TCallback>
	static void WaitUntilRagdollReady(RE::TESObjectREFR* ref, TCallback&& callback, std::chrono::milliseconds timeout = 3s, const bool secureFrame = true)
	{
		if (!ref) {
			callback(ref, false);
			return;
		}

		std::jthread([formId = ref->formID, callback = std::forward<TCallback>(callback), timeout, secureFrame]() mutable {
			auto start = std::chrono::steady_clock::now();
			while (std::chrono::steady_clock::now() - start < timeout) {
				const std::chrono::milliseconds delay = FRAME_DELAY_MS();
				RE::TESObjectREFR* ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(formId);
				if (ref && ModUtils::IsReferenceRagdollReady(ref)) {
					SKSE::GetTaskInterface()->AddTask([callback, ref, secureFrame, start = std::chrono::steady_clock::now(), delay]() {
						if (secureFrame && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() > 300 + delay.count()) {
							TRACE("WaitUntilRagdollReady: Task was delayed and invalidated due to frame timing (>{}ms)", 300 + delay.count());
							return;
						}
						callback(ref, true);
					});
					return;
				}
				std::this_thread::sleep_for(delay);
			}
        
			SKSE::GetTaskInterface()->AddTask([callback, formId, secureFrame, start = std::chrono::steady_clock::now()]() {
				if (secureFrame && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() > 300) {
					TRACE("WaitUntilRagdollReady: Task was delayed and invalidated due to frame timing (>{}ms)", 300);
					return;
				}
				RE::TESObjectREFR* ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(formId);
				callback(ref, false);
			});
		}).detach();
	}

	static bool IsReferenceRagdollReady(RE::TESObjectREFR* ref)
	{
		if (!ref || !ref->Is3DLoaded()) return false;

		RE::NiAVObject* niAVObject = ref->Get3D(false);
		if (!niAVObject) return false;

		if (RE::NiPointer<RE::NiAVObject> root = RE::NiPointer<RE::NiAVObject>(niAVObject)) {
			if (auto rb = GetRigidBody(root.get())) {
				if (auto hkpRigidBody = static_cast<RE::hkpRigidBody*>(rb->referencedObject.get())) {
					if (hkpRigidBody->world && hkpRigidBody->motion.GetMass() > 0.0f) return true;
				}
			}
		}

		return false;
	}

	static float GetFrameDelay()
	{
		RE::BSTimer* bsTimer = RE::BSTimer::GetSingleton();
		if (!bsTimer) return 0.00694444f; // 144Hz

		float frame_delay = bsTimer->realTimeDelta / bsTimer->QGlobalTimeMultiplier();
		frame_delay = std::clamp(frame_delay, 0.004f, 0.1f);

		return frame_delay;
	}

	template <class T = RE::COL_LAYER>
	[[nodiscard]] static T GetCollisionLayer(std::uint32_t filter)
	{
		std::uint32_t raw = filter & 0x7F;
		if constexpr (std::is_same_v<T, std::uint32_t>) return raw;
		else return static_cast<T>(raw);
	}

	template <class T = RE::COL_LAYER>
	static void SetCollisionLayer(std::uint32_t& filter, T layer)
	{
		constexpr std::uint32_t kLayerMask = 0x7F;
		filter &= ~kLayerMask;
		filter |= (static_cast<std::uint32_t>(layer) & kLayerMask);
	}

private:
	static bool IsDismemberedNodeTree(RE::NiNode* node)
	{
		if (!node) return false;

		if (node->local.scale < 0.0f && node->local.scale > -1.0f) return true;

		for (auto& child : node->GetChildren()) {
			if (!child) continue;
			if (IsDismemberedNodeTree(child->AsNode())) return true;
		}

		return false;
	}

	static void ToggleCollisionForNodeTree(RE::NiNode* node, bool enable)
	{
		if (!node) return;

		auto* objectRef = node->GetUserData();
		if (!objectRef) return;

		const bool ragdollReady = IsReferenceRagdollReady(objectRef);

		auto isUpperCase = [](const RE::BSFixedString& str) {
			for (auto c : std::string(str.c_str())) {
				if (!std::isupper(c)) return false;
			}
			return true;
		};

		if (isUpperCase(node->name)) return;

		if (auto collisionObject = node->collisionObject) {
			if (collisionObject) {
				auto bhkCollisionObject = RE::NiPointer<RE::bhkCollisionObject>(static_cast<RE::bhkCollisionObject*>(collisionObject.get()));
				if (bhkCollisionObject) {
					if (auto rb = bhkCollisionObject->GetRigidBody()) {
						auto rigidBody = RE::NiPointer<RE::bhkRigidBody>(rb);
						if (rigidBody && rigidBody->referencedObject) {
							auto hkpRigidBody = RE::hkRefPtr<RE::hkpRigidBody>(static_cast<RE::hkpRigidBody*>(rigidBody->referencedObject.get()));
							if (hkpRigidBody && hkpRigidBody->world) {
								auto& collisionFilterInfo = hkpRigidBody->collidable.broadPhaseHandle.collisionFilterInfo;
								if (enable) {
									if (collisionFilterInfo.GetCollisionLayer() == ModData::DataHandler::NonCollidableLayer) {
										auto layer = ragdollReady ? RE::COL_LAYER::kDeadBip : RE::COL_LAYER::kBiped;
										collisionFilterInfo.SetCollisionLayer(layer);
									}
								} else {
									collisionFilterInfo.SetCollisionLayer(ModData::DataHandler::NonCollidableLayer);
								}
							}
						}
					}
				}
			}
		}

		for (auto& child : node->GetChildren()) {
			if (!child || !child->AsNode()) continue;
			ToggleCollisionForNodeTree(child->AsNode(), enable);
		}
	}

	static void ToggleNodeForNodeTree(RE::NiNode* node, bool enable)
	{
		if (!node) return;

		auto isUpperCase = [](const RE::BSFixedString& str) {
			for (auto c : std::string(str.c_str())) {
				if (!std::isupper(c)) return false;
			}
			return true;
		};

		if (isUpperCase(node->name)) {
			node->local.scale = (enable ? 1.0f : -100.0f); // (enable ? 1.0f : -20.0f)
			return;
		}

		for (auto& child : node->GetChildren()) {
			if (!child || !child->AsNode()) continue;
			ToggleNodeForNodeTree(child->AsNode(), enable);
		}
	}

	static RE::bhkRigidBody* GetRigidBody(RE::NiAVObject* a_object)
	{
		if (auto collisionObject = a_object->GetCollisionObject()) {
			return collisionObject->GetRigidBody();
		}
		return nullptr;
	}

	static RE::TESNPC* GetTraitTemplate(RE::TESNPC* baseForm)
	{
		auto npc = baseForm;
		if (!npc) return nullptr;

		while (npc->faceNPC && npc->formID >= 0xFF000000) {
			npc = npc->faceNPC;
		}

		return npc;
	}
};
