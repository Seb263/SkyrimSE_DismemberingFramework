#include "Main.h"

namespace Events
{
	bool MainEvent::ProcessDismemberment(RE::Actor* target, RE::HitData& hitData, const DismembermentParams& params)
	{
		using PluginGlobal = ModData::DataHandler::PluginGlobal;

		if (!ModUtils::GetGlobalValue<bool>(PluginGlobal::ModStatus)) return false;
		
		if (!target) return false;

		RE::Actor* aggressor = (hitData.aggressor.get() ? hitData.aggressor.get()->As<RE::Actor>() : nullptr);
		if (!params.forceExecution && !aggressor) return false;

		TRACE("Dismemberment request for an actor : <\"{}\" [REF:{:08X}] [BASE:{:08X}]>",
			target->GetName(), target->formID, (target->GetActorBase() ? target->GetActorBase()->formID : 0x0));

		if (ModData::DataHandler::nextGenDecapitationsEnabled && NGDecapitationsAPI::g_API->IsHead(target)) return false;
		if (!params.forceExecution && !IsDismemberable(target, aggressor, hitData)) return false;

		RE::TESRace* aggressorRace = aggressor ? aggressor->GetRace() : nullptr;
		const WeaponType weapType = GetWeaponType(hitData.weapon, aggressorRace);
		WeapTypeConditions* weapTypeConditions = GetWeapTypeConditions(weapType);
		if (params.noLimbImpulse) weapTypeConditions->impulse = 0.0f;

		if (!params.forceExecution) {
			ActorTypeConditions* actorTypeConditions = GetActorTypeConditions(target, aggressor);
			if (!TestActorTypes(actorTypeConditions, target, aggressor)) return false;
			if (!TestWeaponType(weapTypeConditions, hitData)) return false;
		}

		const bool heavyForbidden = !weapTypeConditions->ifHeavy;
		const auto& isHeavyCondition = new ModData::Condition{};
		isHeavyCondition->name = "IsEquippedItemWeightClass";
		isHeavyCondition->parameters = { { "Type", "Heavy" } };

		auto SkinIterator = ValidActorData(target);
		if (!SkinIterator) return false;

		std::vector<std::string> validNodes;
		if (params.specificNode == "") {
			validNodes = ValidActorNodes(*SkinIterator, target, hitData.hitPosition, weapTypeConditions->precision, weapTypeConditions->multiple);
		} else {
			validNodes = ValidActorNode(*SkinIterator, target, params.specificNode);
		}
		if (validNodes.empty()) return false;

		bool setDismembered = false;
		for (const std::string& node : validNodes) {
			if (ModUtils::IsDismemberedNode(target, node)) continue;
			const SkinIteratorType& skinObj = *SkinIterator;

			const auto& nodeObj = skinObj->second.find(node);
			if (nodeObj == skinObj->second.end()) continue;

			for (const auto& elementPtr : nodeObj->second) {
				if (!elementPtr) continue;
				DismemberingData& dismemberingData = *elementPtr;
				if (ModUtils::IsDismemberedLimb(target, dismemberingData.idKeyword)) return false;
			}

			for (const auto& elementPtr : nodeObj->second) {
				if (!elementPtr) continue;
				DismemberingData& dismemberingData = *elementPtr;

				if (!params.ignoreArmorClass) {
					if (heavyForbidden && EvaluateCondition(dismemberingData, target, *isHeavyCondition)) break;
				}

				if (!AreConditionsValid(dismemberingData, target)) continue;

				if (dismemberingData.method == DismemberingData::Methods::kDecapitate) {
					if (ModData::DataHandler::nextGenDecapitationsEnabled) {
						if (!NGDecapitationsAPI::g_API->IsDecapitated(target)) {
							DecapitateParams decapitateParams;
							decapitateParams.customHitData = true;
							decapitateParams.hitFromPosition = hitData.hitPosition;
							decapitateParams.hitToPosition = (hitData.hitPosition + hitData.hitDirection);
							decapitateParams.hitPower = weapTypeConditions->impulse / 20.0f;
							decapitateParams.callback = [dismemberingData](RE::Actor* headRef) {
								if (!headRef) return;
								CastTargetEffectSpell(headRef, dismemberingData.targetEffectSpell);
							};

							if (!NGDecapitationsAPI::g_API->Decapitate(target, &decapitateParams)) continue;
						} else continue;
					} else {
						const int allowBeheading = ModUtils::GetGlobalValue<int>(PluginGlobal::Misc_AllowVanillaBeheading);
						if (ModUtils::IsDecapitated(target) || allowBeheading == 0 ||
							(allowBeheading == 1 && target->GetActorBase() && target->GetActorBase()->IsUnique())) continue;
					
						if (ModUtils::GetGlobalValue<bool>(PluginGlobal::Misc_PreventFullHelmetBeheading)) {
							const RE::TESObjectARMO* currentHelmet = target->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kHair);
							if (currentHelmet && currentHelmet == target->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kHead)) continue;
						}

						target->StopCurrentDialogue();
						target->Decapitate();
					}
				} else {
					if (dismemberingData.refNode == "" || !dismemberingData.limbFormID) continue;

					RE::TESObjectREFR* limbRef = CutLimb(target, dismemberingData);
					RE::TESObjectREFR* droppedItem = DropTargetItem(target, dismemberingData);

					if (dismemberingData.disableCollision && target->IsDead()) ModUtils::ToggleCollisionLayers(target, dismemberingData.collisionNode, false);
					if (dismemberingData.resizeNode) ModUtils::ToggleNode(target, node, false);

					RegisterLimbData(limbRef, target, hitData, dismemberingData, weapTypeConditions->impulse, droppedItem);

					if (dismemberingData.actorArmorAddon) EquipArmorAddon(target, dismemberingData);
				}

				if (dismemberingData.actorArtObject != nullptr) {
					MiscUtils::PlayArtObject(target, dismemberingData.actorArtObject, 5.0f);
				}

				ModUtils::AddActorLimbFlag(target, dismemberingData.idKeyword);
				CastTargetEffectSpell(target, dismemberingData.targetEffectSpell);
				
				if (dismemberingData.endDialogue) target->StopCurrentDialogue();

				if (!setDismembered) {
					if (!params.noSoundEffect) PlayDismemberSound(target, weapType, dismemberingData, hitData.flags.any(RE::HitData::Flag::kBlockWithWeapon));
					if (!params.noPlayerEffect) CastPlayerEffectSpell(target, aggressor, dismemberingData.playerEffectSpell, target->IsDead());
				}

				TRACE("Dismembered actor : <\"{}\" [REF:{:08X}] [BASE:{:08X}]> | Severed limb node : \"{}\"",
					target->GetName(), target->formID, (target->GetActorBase() ? target->GetActorBase()->formID : 0x0), dismemberingData.targetedNode);

				setDismembered = true;
				break;
			}
		}

		return setDismembered;
	}

	bool MainEvent::AreConditionsValid(const DismemberingData& dismemberingData, RE::Actor* target)
	{
		bool shouldContinueOuterLoop = true;
		for (const auto& condition : dismemberingData.conditions) {
			if (!EvaluateCondition(dismemberingData, target, condition)) {
				shouldContinueOuterLoop = false;
				break;
			}
		}

		return shouldContinueOuterLoop;
	}

	void MainEvent::CastPlayerEffectSpell(RE::Actor* target, RE::Actor* aggressor, RE::SpellItem* spell, bool targetIsDead)
	{
		using PluginGlobal = ModData::DataHandler::PluginGlobal;
		if (!spell || !aggressor || !aggressor->IsPlayerRef() || targetIsDead) return;

		if (!ModUtils::GetGlobalValue<bool>(PluginGlobal::Features_PlayerEffect_Status)) return;

		if (ModUtils::GetGlobalValue<bool>(PluginGlobal::Features_PlayerEffect_LatestOnly) && !ModUtils::IsLatestEnemyAlive(target, aggressor)) return;

		if (ModUtils::GetGlobalValue<float>(PluginGlobal::Features_PlayerEffect_Chances) < MiscUtils::GetRandomNumber()) return;

		auto* caster = aggressor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
		caster->CastSpellImmediate(spell, false, aggressor, 1.0, false, 1.0, nullptr);
	}

	void MainEvent::CastTargetEffectSpell(RE::Actor* target, RE::SpellItem* spell)
	{
		if (!spell) return;

		auto* caster = target->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
		caster->CastSpellImmediate(spell, false, target, 1.0, false, 1.0, nullptr);
	}

	RE::TESObjectREFR* MainEvent::CutLimb(RE::Actor* target, const DismemberingData& dismemberingData)
	{
		if (!dismemberingData.limbFormID) return nullptr;
		
		RE::NiPoint3 translation = (dismemberingData.translation * target->GetScale());
		RE::TESObjectREFR* placedLimbRef = PlaceLimbCenter(dismemberingData.limbFormID, target, dismemberingData.refNode, translation);
		if (!placedLimbRef) return nullptr;

		placedLimbRef->SetActivationBlocked(true);
		placedLimbRef->SetDisplayName("", true);

		return placedLimbRef;
	}

	RE::TESObjectREFR* MainEvent::DropTargetItem(RE::Actor* target, const DismemberingData& dismemberingData)
	{
		RE::TESObjectREFR* droppedRef = nullptr;
		if (dismemberingData.dropItem == DismemberingData::DropItem::kNone) return droppedRef;

		int biped_slot = (dismemberingData.bipedSlot - 30) >= 0 ? 1 << (dismemberingData.bipedSlot - 30) : 0;
		RE::TESObjectARMO* armor = target->GetWornArmor(static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(biped_slot));
		if (!armor || !armor->GetPlayable() ||
			(armor->GetArmorType() == RE::BIPED_MODEL::ArmorType::kClothing && dismemberingData.dropItem == DismemberingData::DropItem::kArmor)) {
			return droppedRef;
		}

		if (!target->GetNodeByName(dismemberingData.refNode) || !target->GetNodeByName(dismemberingData.refNode)->AsNode()) return droppedRef;
		const RE::NiNode* niNode = target->GetNodeByName(dismemberingData.refNode)->AsNode();

		RE::InventoryChanges* inventoryChanges = target->GetInventoryChanges();
		RE::BSSimpleList<RE::InventoryEntryData*>::iterator itr, itrEnd;
		itrEnd = inventoryChanges->entryList->end();

		for (itr = inventoryChanges->entryList->begin(); itr != itrEnd; ++itr) {
			RE::InventoryEntryData* inventoryEntryData = *itr;
			if (!inventoryEntryData->IsWorn() || !inventoryEntryData->object) continue;
			if (inventoryEntryData->object->As<RE::TESObjectARMO>() != armor) continue;

			RE::NiPoint3 nodeAngles;
			if (!niNode->world.rotate.ToEulerAnglesXYZ(nodeAngles)) return droppedRef;

			auto item = target->RemoveItem(inventoryEntryData->object, 1, RE::ITEM_REMOVE_REASON::kDropping, &(target->extraList), target, &(niNode->world.translate), &(nodeAngles));
			droppedRef = item.get()->As<RE::TESObjectREFR>();
			break;
		}

		return droppedRef;
	}

	void MainEvent::EquipArmorAddon(RE::Actor* target, const DismemberingData& dismemberingData)
	{
		RE::TESObjectARMO* armor = dismemberingData.actorArmorAddon;
		if (!armor) return;

		target->AddObjectToContainer(armor, nullptr, 1, nullptr);

		auto equipType = armor->As<RE::BGSEquipType>();
		RE::BGSEquipSlot* slot = equipType ? equipType->GetEquipSlot() : nullptr;

		const auto& itemManager = RE::ActorEquipManager::GetSingleton();
		if (!itemManager) REPORT_AND_FAIL("Item Manager could not be initialized.");
		itemManager->EquipObject(target, armor, nullptr, 1, slot, false, true, false, false);
	}

	bool MainEvent::EvaluateCondition(const DismemberingData& dismemberingData, RE::Actor* target, const Condition& condition)
	{
		if (condition.name == "IsEquipped") {
		
			auto itemPtr = std::get_if<RE::TESForm*>(&condition.parameters.at("Item"));
			if (!itemPtr || !(*itemPtr)) {
                logger::warn("The form obtained from the \"{}\" condition parameters is not valid.", condition.name);
				return false;
			}

			if (RE::BGSListForm* itemsList = (*itemPtr)->As<RE::BGSListForm>()) {
				bool found = false;

				itemsList->ForEachForm([&](RE::TESForm* form) -> RE::BSContainer::ForEachResult {
					Condition tempCondition = condition;
					tempCondition.parameters["Item"] = form;

					if (EvaluateCondition(dismemberingData, target, tempCondition))
						return found = true, RE::BSContainer::ForEachResult::kStop;
					return RE::BSContainer::ForEachResult::kContinue;
				});

				return found;
			} else if ((*itemPtr)->As<RE::TESObjectARMO>() || (*itemPtr)->As<RE::TESObjectARMA>()) {
				const RE::TESObjectARMO* itemArmo = (*itemPtr)->As<RE::TESObjectARMO>();
				const RE::TESObjectARMA* itemArma = (*itemPtr)->As<RE::TESObjectARMA>();
				RE::TESRace* targetRace = target->GetRace();
				
				RE::InventoryChanges* inventoryChanges = target->GetInventoryChanges();
				RE::BSSimpleList<RE::InventoryEntryData*>::iterator itr, itrEnd;
				itrEnd = inventoryChanges->entryList->end();

				for (itr = inventoryChanges->entryList->begin(); itr != itrEnd; ++itr) {
					RE::InventoryEntryData* inventoryEntryData = *itr;
					if (!inventoryEntryData->IsWorn()) continue;
					RE::TESForm* invItem = inventoryEntryData->object;
					if (!invItem || !invItem->As<RE::TESObjectARMO>()) continue;
					
					RE::TESObjectARMO* currentArmo = invItem->As<RE::TESObjectARMO>();
					if (itemArmo) {
						if (currentArmo->formID == itemArmo->formID) return true;
						else if (currentArmo->templateArmor == itemArmo) return true;
					} else if (itemArma) {
						const RE::TESObjectARMA* currentArma = currentArmo->GetArmorAddon(targetRace);
						if (currentArma && currentArma->formID == itemArma->formID) return true;
						if (currentArmo->templateArmor) {
							const RE::TESObjectARMA* currentTemplateArma = currentArmo->templateArmor->GetArmorAddon(targetRace);
							if (currentTemplateArma && currentTemplateArma == itemArma) return true;
						}
					}
				}
			} else if (RE::BGSHeadPart* itemHeadPart = (*itemPtr)->As<RE::BGSHeadPart>()) {
				auto* actorBase = target->GetActorBase();
				if (!actorBase) return false;

				RE::BGSHeadPart** headPartArray = actorBase->headParts;
				for (std::int8_t i = 0; i < actorBase->numHeadParts; i++) {
					RE::BGSHeadPart* headPart = headPartArray[i];
					if (headPart && headPart == itemHeadPart) return true;
				}
			} else if (RE::BGSOutfit* itemOutfit = (*itemPtr)->As<RE::BGSOutfit>()) {
				return (target->GetActorBase()->defaultOutfit == itemOutfit);
			} else {
				logger::warn("The form obtained from the \"{}\" condition parameters does not match its expected type ({}).", condition.name, "TESObjectARMO/TESObjectARMA/BGSOutfit/BGSHeadPart/BGSListForm");
			}

		} else if (condition.name == "IsEquippedKeyword") {
		
			if (dismemberingData.bipedSlot < 1) return false;

			const std::string keyword = std::get<std::string>(condition.parameters.at("Keyword"));

			int biped_slot = (dismemberingData.bipedSlot - 30) >= 0 ? 1 << (dismemberingData.bipedSlot - 30) : 0;
			RE::TESObjectARMO* armor = target->GetWornArmor(static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(biped_slot));

			if (armor && armor->HasKeywordString(keyword)) return true;

		} else if (condition.name == "IsEquippedItemWeightClass") {
		
			if (dismemberingData.bipedSlot < 1) return false;
			const std::string type = std::get<std::string>(condition.parameters.at("Type"));

			const RE::BIPED_MODEL::ArmorType armorType = (type == "Heavy") ? RE::BIPED_MODEL::ArmorType::kHeavyArmor :
			                                       (type == "Light") ? RE::BIPED_MODEL::ArmorType::kLightArmor :
			                                                           RE::BIPED_MODEL::ArmorType::kClothing;

			const int biped_slot = (dismemberingData.bipedSlot - 30) >= 0 ? 1 << (dismemberingData.bipedSlot - 30) : 0;
			RE::TESObjectARMO* armor = target->GetWornArmor(static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(biped_slot));

			if (armor && armor->GetArmorType() == armorType) return true;
		}

		return false;
	}

	ActorType MainEvent::GetActorType(RE::Actor* actor)
	{
		using Types = ActorType;
		if (!actor) return Types::kNone;

		if (actor->IsPlayerRef()) return Types::kPlayer;

		RE::TESRace* race = actor->GetRace();
		if (race) {
			using Flags = RE::TESRace::EquipmentFlag;
			if (race->validEquipTypes.any(Flags::kOneHandSword) ||
				race->validEquipTypes.any(Flags::kTwoHandSword) ||
				race->validEquipTypes.any(Flags::kOneHandAxe) ||
				race->validEquipTypes.any(Flags::kTwoHandAxe) ||
				race->validEquipTypes.any(Flags::kOneHandMace) ||
				race->validEquipTypes.any(Flags::kOneHandDagger)) {
				return ActorType::kNPC;
			}
		}
		return Types::kOther;
	}

	MainEvent::ActorTypeConditions* MainEvent::GetActorTypeConditions(RE::Actor* target, RE::Actor* aggressor)
	{
		using PluginGlobal = ModData::DataHandler::PluginGlobal;
		using Types = ActorType;
		ActorTypeConditions* conditions = new ActorTypeConditions{};

		switch (GetActorType(target)) {
		case Types::kPlayer:
			conditions->targetCanBeDismembered = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_Global_Player_CanBeDismembered);
			break;
		case Types::kNPC:
			conditions->targetCanBeDismembered = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_Global_NPC_CanBeDismembered);
			break;
		default:
			conditions->targetCanBeDismembered = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_Global_Other_CanBeDismembered);
			break;
		}

		switch (GetActorType(aggressor)) {
		case Types::kPlayer:
			conditions->aggressorCanDismember = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_Global_Player_CanDismember);
			conditions->aggressorCanDismemberIfFatal = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_Global_Player_CanDismember_IfFatal);
			conditions->aggressorCanDismemberIfDead = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_Global_Player_CanDismember_IfDead);
			conditions->aggressorCanDismemberIfKillmove = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_Global_Player_CanDismember_IfKillmove);
			break;
		case Types::kNPC:
			conditions->aggressorCanDismember = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_Global_NPC_CanDismember);
			conditions->aggressorCanDismemberIfFatal = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_Global_NPC_CanDismember_IfFatal);
			conditions->aggressorCanDismemberIfDead = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_Global_NPC_CanDismember_IfDead);
			conditions->aggressorCanDismemberIfKillmove = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_Global_NPC_CanDismember_IfKillmove);
			break;
		case Types::kOther:
			conditions->aggressorCanDismember = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_Global_Other_CanDismember);
			conditions->aggressorCanDismemberIfFatal = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_Global_Other_CanDismember_IfFatal);
			conditions->aggressorCanDismemberIfDead = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_Global_Other_CanDismember_IfDead);
			conditions->aggressorCanDismemberIfKillmove = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_Global_Other_CanDismember_IfKillmove);
			break;
		default:
			conditions->aggressorCanDismember = true;
			conditions->aggressorCanDismemberIfFatal = true;
			conditions->aggressorCanDismemberIfDead = true;
			conditions->aggressorCanDismemberIfKillmove = true;
			break;
		}
		
		return conditions;
	}

	WeaponType MainEvent::GetWeaponType(RE::TESObjectWEAP* weapon, RE::TESRace* targetRace)
	{
		if (!weapon) return WeaponType::kOther;

		const RE::WEAPON_TYPE weaponType = weapon->GetWeaponType();

		if (weapon->HasKeywordString("WeapTypeWarhammer")) {
			return WeaponType::kTwoHandMace;
		} else if (weaponType == RE::WEAPON_TYPE::kOneHandSword) {
			return WeaponType::kOneHandSword;
		} else if (weaponType == RE::WEAPON_TYPE::kTwoHandSword) {
			return WeaponType::kTwoHandSword;
		} else if (weaponType == RE::WEAPON_TYPE::kOneHandAxe) {
			return WeaponType::kOneHandAxe;
		} else if (weaponType == RE::WEAPON_TYPE::kTwoHandAxe) {
			return WeaponType::kTwoHandAxe;
		} else if (weaponType == RE::WEAPON_TYPE::kOneHandDagger) {
			return WeaponType::kDagger;
		} else if (weaponType == RE::WEAPON_TYPE::kOneHandMace) {
			return WeaponType::kOneHandMace;
		} else if (weaponType == RE::WEAPON_TYPE::kBow || weaponType == RE::WEAPON_TYPE::kCrossbow) {
			return WeaponType::kRanged;
		} else if (weaponType == RE::WEAPON_TYPE::kHandToHandMelee) {
			return (!targetRace || targetRace->HasKeywordByEditorID("ActorTypeNPC") ? WeaponType::kHandToHand : WeaponType::kBeast);
		} else {
			return WeaponType::kOther;
		}
	}

	MainEvent::WeapTypeConditions* MainEvent::GetWeapTypeConditions(WeaponType weaponType)
	{
		using PluginGlobal = ModData::DataHandler::PluginGlobal;
		WeapTypeConditions* conditions = new WeapTypeConditions{};

		const float randomPercent = MiscUtils::GetRandomNumber();

		switch (weaponType) {
		case WeaponType::kHandToHand:
			conditions->canDismember = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_HandToHand_CanDismember);
			conditions->regularAttackChance = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_HandToHand_RegularAttackChance) >= randomPercent;
			conditions->powerAttackChance = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_HandToHand_PowerAttackChance) >= randomPercent;
			conditions->precision = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_HandToHand_Precision);
			conditions->multiple = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_HandToHand_Multiple);
			conditions->ifHeavy = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_HandToHand_IfHeavy);
			conditions->ifBlocked = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_HandToHand_IfBlocked);
			conditions->impulse = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_HandToHand_Impulse);
			break;
		case WeaponType::kOneHandSword:
			conditions->canDismember = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_OneHandSword_CanDismember);
			conditions->regularAttackChance = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_OneHandSword_RegularAttackChance) >= randomPercent;
			conditions->powerAttackChance = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_OneHandSword_PowerAttackChance) >= randomPercent;
			conditions->precision = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_OneHandSword_Precision);
			conditions->multiple = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_OneHandSword_Multiple);
			conditions->ifHeavy = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_OneHandSword_IfHeavy);
			conditions->ifBlocked = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_OneHandSword_IfBlocked);
			conditions->impulse = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_OneHandSword_Impulse);
			break;
		case WeaponType::kTwoHandSword:
			conditions->canDismember = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_TwoHandSword_CanDismember);
			conditions->regularAttackChance = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_TwoHandSword_RegularAttackChance) >= randomPercent;
			conditions->powerAttackChance = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_TwoHandSword_PowerAttackChance) >= randomPercent;
			conditions->precision = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_TwoHandSword_Precision);
			conditions->multiple = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_TwoHandSword_Multiple);
			conditions->ifHeavy = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_TwoHandSword_IfHeavy);
			conditions->ifBlocked = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_TwoHandSword_IfBlocked);
			conditions->impulse = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_TwoHandSword_Impulse);
			break;
		case WeaponType::kOneHandAxe:
			conditions->canDismember = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_OneHandAxe_CanDismember);
			conditions->regularAttackChance = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_OneHandAxe_RegularAttackChance) >= randomPercent;
			conditions->powerAttackChance = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_OneHandAxe_PowerAttackChance) >= randomPercent;
			conditions->precision = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_OneHandAxe_Precision);
			conditions->multiple = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_OneHandAxe_Multiple);
			conditions->ifHeavy = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_OneHandAxe_IfHeavy);
			conditions->ifBlocked = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_OneHandAxe_IfBlocked);
			conditions->impulse = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_OneHandAxe_Impulse);
			break;
		case WeaponType::kTwoHandAxe:
			conditions->canDismember = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_TwoHandAxe_CanDismember);
			conditions->regularAttackChance = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_TwoHandAxe_RegularAttackChance) >= randomPercent;
			conditions->powerAttackChance = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_TwoHandAxe_PowerAttackChance) >= randomPercent;
			conditions->precision = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_TwoHandAxe_Precision);
			conditions->multiple = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_TwoHandAxe_Multiple);
			conditions->ifHeavy = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_TwoHandAxe_IfHeavy);
			conditions->ifBlocked = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_TwoHandAxe_IfBlocked);
			conditions->impulse = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_TwoHandAxe_Impulse);
			break;
		case WeaponType::kOneHandMace:
			conditions->canDismember = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_OneHandMace_CanDismember);
			conditions->regularAttackChance = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_OneHandMace_RegularAttackChance) >= randomPercent;
			conditions->powerAttackChance = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_OneHandMace_PowerAttackChance) >= randomPercent;
			conditions->precision = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_OneHandMace_Precision);
			conditions->multiple = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_OneHandMace_Multiple);
			conditions->ifHeavy = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_OneHandMace_IfHeavy);
			conditions->ifBlocked = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_OneHandMace_IfBlocked);
			conditions->impulse = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_OneHandMace_Impulse);
			break;
		case WeaponType::kTwoHandMace:
			conditions->canDismember = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_TwoHandMace_CanDismember);
			conditions->regularAttackChance = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_TwoHandMace_RegularAttackChance) >= randomPercent;
			conditions->powerAttackChance = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_TwoHandMace_PowerAttackChance) >= randomPercent;
			conditions->precision = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_TwoHandMace_Precision);
			conditions->multiple = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_TwoHandMace_Multiple);
			conditions->ifHeavy = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_TwoHandMace_IfHeavy);
			conditions->ifBlocked = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_TwoHandMace_IfBlocked);
			conditions->impulse = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_TwoHandMace_Impulse);
			break;
		case WeaponType::kDagger:
			conditions->canDismember = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_Dagger_CanDismember);
			conditions->regularAttackChance = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_Dagger_RegularAttackChance) >= randomPercent;
			conditions->powerAttackChance = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_Dagger_PowerAttackChance) >= randomPercent;
			conditions->precision = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_Dagger_Precision);
			conditions->multiple = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_Dagger_Multiple);
			conditions->ifHeavy = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_Dagger_IfHeavy);
			conditions->ifBlocked = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_Dagger_IfBlocked);
			conditions->impulse = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_Dagger_Impulse);
			break;
		case WeaponType::kRanged:
			conditions->canDismember = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_Ranged_CanDismember);
			conditions->regularAttackChance = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_Ranged_RegularAttackChance) >= randomPercent;
			conditions->powerAttackChance = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_Ranged_RegularAttackChance) >= randomPercent;
			conditions->precision = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_Ranged_Precision);
			conditions->multiple = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_Ranged_Multiple);
			conditions->ifHeavy = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_Ranged_IfHeavy);
			conditions->ifBlocked = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_Ranged_IfBlocked);
			conditions->impulse = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_Ranged_Impulse);
			break;
		case WeaponType::kBeast:
			conditions->canDismember = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_Beast_CanDismember);
			conditions->regularAttackChance = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_Beast_RegularAttackChance) >= randomPercent;
			conditions->powerAttackChance = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_Beast_PowerAttackChance) >= randomPercent;
			conditions->precision = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_Beast_Precision);
			conditions->multiple = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_Beast_Multiple);
			conditions->ifHeavy = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_Beast_IfHeavy);
			conditions->ifBlocked = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_Beast_IfBlocked);
			conditions->impulse = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_Beast_Impulse);
			break;
		default:
			conditions->canDismember = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_Other_CanDismember);
			conditions->regularAttackChance = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_Other_RegularAttackChance) >= randomPercent;
			conditions->powerAttackChance = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_Other_RegularAttackChance) >= randomPercent;
			conditions->precision = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_Other_Precision);
			conditions->multiple = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_Other_Multiple);
			conditions->ifHeavy = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_Other_IfHeavy);
			conditions->ifBlocked = ModUtils::GetGlobalValue<bool>(PluginGlobal::Cond_WeapType_Other_IfBlocked);
			conditions->impulse = ModUtils::GetGlobalValue<float>(PluginGlobal::Cond_WeapType_Other_Impulse);
			break;
		}

		return conditions;
	}

	bool MainEvent::IsDismemberable(RE::Actor* target, RE::Actor* aggressor, RE::HitData& hitData)
	{
		auto actorBase = target ? target->GetActorBase() : nullptr;
		if (!actorBase) return false;
		if (!actorBase->Bleeds()) return false;

		if (!SettingsIni::bDeferredHitProcess) {
			const float targetHealth = target->AsActorValueOwner()->GetActorValue(RE::ActorValue::kHealth);
			const bool isFatal = ((targetHealth - hitData.totalDamage < 0.0f) && hitData.flags.any(RE::HitData::Flag::kFatal));
			if (!isFatal && !target->IsDead()) return false;
		}

		if (target->IsEssential()) return false;
		if (target->IsProtected() && aggressor && !aggressor->IsPlayerRef()) return false;
		if (const auto state = target->AsActorState(); state && (state->actorState2.reanimating || state->IsReanimated())) return false;
		return true;
	}

	RE::TESObjectREFR* MainEvent::PlaceLimbCenter(RE::TESForm* limb, RE::Actor* target, std::string refNode, RE::NiPoint3 localTranslation)
	{
		const auto& nodeObj = target->GetNodeByName(refNode);
		if (!nodeObj || !nodeObj->AsNode()) {
            logger::warn("The reference node for limb placement could not be found.");
			return nullptr;
		}
		const RE::NiNode* node = nodeObj->AsNode();

		RE::NiMatrix3 rotMatrix;
		float xAngle, yAngle, zAngle;
		if (node->world.rotate.ToEulerAnglesXYZ(xAngle, yAngle, zAngle)) {
			rotMatrix.SetEulerAnglesXYZ(xAngle, yAngle, zAngle);
		} else {
            logger::warn("Failed to retrieve Euler angles for limb placement.");
			return nullptr;
		}

		auto globalPos = node->world.translate;
		auto globalTranslation = rotMatrix * -localTranslation;
		
		RE::NiPoint3 position = { globalPos + globalTranslation };
		RE::NiPoint3 angle = { xAngle, yAngle, zAngle };
		RE::TESObjectREFR* placedLimbRef = MiscUtils::PlaceAtMe(target, limb, position, angle);
		
		if (placedLimbRef && SettingsIni::bAllowLimbScaling) {
			if (std::abs(target->GetScale() - 1.0f) > 0.05f) {
				placedLimbRef->Disable();
				placedLimbRef->SetScale(target->GetScale());
				
				ModUtils::WaitAndCall(FRAME_DELAY_MS(), [placedLimbFormID = placedLimbRef->formID]() {
					if (RE::TESObjectREFR* placedLimbRef = RE::TESForm::LookupByID<RE::TESObjectREFR>(placedLimbFormID)) {
						placedLimbRef->Enable(false);
						placedLimbRef->AddChange(RE::TESObjectREFR::ChangeFlags::kScale);
					}
				});
			}
		}

		return placedLimbRef;
	}

	void MainEvent::PlayDismemberSound(RE::Actor* target, const WeaponType weapType, const DismemberingData& dismemberingData, bool isParrying)
	{
		if (!ModUtils::GetGlobalValue<bool>(ModData::DataHandler::PluginGlobal::Features_Misc_DismemberSFX)) return;

		bool isHeavy = false;
		if (dismemberingData.typeOverride == DismemberingData::TypeOverride::kHeavyArmor) {
			isHeavy = true;
		} else {
			int bipedSlot = dismemberingData.bipedSlot;
			if (bipedSlot > 0) {
				bipedSlot = (bipedSlot - 30) >= 0 ? 1 << (bipedSlot - 30) : 0;
				RE::TESObjectARMO* armor = target->GetWornArmor(static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(bipedSlot));
				if (armor && armor->GetArmorType() == RE::BIPED_MODEL::ArmorType::kHeavyArmor) isHeavy = true;
			}
		}

		const auto structSFX = dismemberingData.dismemberingSFX;

		if (isParrying && structSFX.parrying) {
			MiscUtils::PlaySound(target, structSFX.parrying);
			return;
		}

		RE::BGSSoundDescriptorForm* dismemberSFX = nullptr;
		switch (weapType) {
		case WeaponType::kOneHandSword:
			dismemberSFX = (isHeavy ? structSFX.oneHandSwordHeavy : structSFX.oneHandSword);
			break;
		case WeaponType::kTwoHandSword:
			dismemberSFX = (isHeavy ? structSFX.twoHandSwordHeavy : structSFX.twoHandSword);
			break;
		case WeaponType::kOneHandAxe:
			dismemberSFX = (isHeavy ? structSFX.oneHandAxeHeavy : structSFX.oneHandAxe);
			break;
		case WeaponType::kTwoHandAxe:
			dismemberSFX = (isHeavy ? structSFX.twoHandAxeHeavy : structSFX.twoHandAxe);
			break;
		case WeaponType::kOneHandMace:
			dismemberSFX = (isHeavy ? structSFX.oneHandMaceHeavy : structSFX.oneHandMace);
			break;
		case WeaponType::kTwoHandMace:
			dismemberSFX = (isHeavy ? structSFX.twoHandMaceHeavy : structSFX.twoHandMace);
			break;
		case WeaponType::kDagger:
			dismemberSFX = (isHeavy ? structSFX.daggerHeavy : structSFX.dagger);
			break;
		case WeaponType::kRanged:
			dismemberSFX = (isHeavy ? structSFX.rangedHeavy : structSFX.ranged);
			break;
		case WeaponType::kHandToHand:
			dismemberSFX = (isHeavy ? structSFX.handToHandHeavy : structSFX.handToHand);
			break;
		case WeaponType::kOther:
			dismemberSFX = (isHeavy ? structSFX.otherHeavy : structSFX.other);
			break;
		}

		if (dismemberSFX) MiscUtils::PlaySound(target, dismemberSFX);
	}

	void MainEvent::RegisterLimbData(RE::TESObjectREFR* limb, RE::Actor* target, const RE::HitData& hitData, const DismemberingData& dismemberingData, float impulseMagnitude, RE::TESObjectREFR* droppedItem)
	{
		DataHandler::LimbData limbData;
		limbData.nodeName = dismemberingData.targetedNode;
		limbData.linkedLimb = limb;
		limbData.linkedActor = target;
		limbData.limbArtObject = dismemberingData.limbArtObject;
		limbData.droppedItem = droppedItem;
		limbData.impulse = SetLimbDataImpulse(hitData, impulseMagnitude);

		ModData::DataHandler::GetSingleton()->limbDataMap[limb->formID] = limbData;
	}

	bool MainEvent::TestActorTypes(ActorTypeConditions* actorTypeConditions, RE::Actor* target, RE::Actor* aggressor)
	{
		if (!actorTypeConditions->targetCanBeDismembered) return false;
		if (!actorTypeConditions->aggressorCanDismember) return false;
		if ((target->IsInKillMove() || aggressor->IsInKillMove()) && !actorTypeConditions->aggressorCanDismemberIfKillmove) return false;
		if (target->IsDead()) {
			if (!actorTypeConditions->aggressorCanDismemberIfDead) return false;
		} else {
			if (!actorTypeConditions->aggressorCanDismemberIfFatal) return false;
		}

		return true;
	}

	bool MainEvent::TestWeaponType(WeapTypeConditions* weapTypeConditions, RE::HitData& hitData)
	{
		if (!weapTypeConditions->canDismember) return false;
		
		bool isPowerAttack = hitData.flags.any(RE::HitData::Flag::kPowerAttack);
		if (!isPowerAttack && !weapTypeConditions->regularAttackChance) return false;
		if (isPowerAttack && !weapTypeConditions->powerAttackChance) return false;
		if (hitData.flags.any(RE::HitData::Flag::kBlockWithWeapon) && !weapTypeConditions->ifBlocked) return false;

		return true;
	}

	ImpulseData MainEvent::SetLimbDataImpulse(const RE::HitData& hitData, float magnitude)
	{
		ImpulseData impulseData;

		impulseData.magnitude = magnitude;
		impulseData.fromPosition = hitData.hitPosition;
		impulseData.toPosition = (hitData.hitPosition + hitData.hitDirection);

		return impulseData;
	}

	auto MainEvent::ValidActorData(RE::Actor* target) 
    -> std::optional<SkinIteratorType>
	{
		if (!target || !target->GetRace() || !target->GetSkin() || !target->GetActorBase()) return std::nullopt;

		const std::string& race = target->GetRace()->GetFormEditorID();
		const std::string& sex = target->GetActorBase()->GetSex() == RE::SEX::kFemale ? "Female" : "Male";
		if (race.empty() || sex.empty()) return std::nullopt;

		const std::string& skin = target->GetSkin() ? MiscUtils::GetAssocStringFromForm(target->GetSkin()->As<RE::TESForm>(), "any") : "any";

		const auto& dataMap = JSONHandler::GetSingleton()->GetData();
		auto raceIt = dataMap.find(race);
		if (raceIt == dataMap.end()) return std::nullopt;

		auto skinIt = raceIt->second.find(skin);
		if (skinIt == raceIt->second.end() && skin != "any") {
			skinIt = raceIt->second.find("any");
		}
		if (skinIt == raceIt->second.end()) return std::nullopt;

		auto sexIt = skinIt->second.find(sex);
		if (sexIt == skinIt->second.end()) return std::nullopt;

		return sexIt;
	}

	std::vector<std::string> MainEvent::ValidActorNode(SkinIteratorType skinObj, RE::Actor* target, const std::string& node)
	{
		std::vector<std::string> validNodes;

		if (skinObj->second.find(node) != skinObj->second.end()) {
			RE::NiAVObject* targetNode = target->GetNodeByName(node);
			if (targetNode) {
				validNodes.push_back(node);
			}
		}

		return validNodes;
	}

	std::vector<std::string> MainEvent::ValidActorNodes(SkinIteratorType skinObj, RE::Actor* target, RE::NiPoint3 hitPosition, float hitPrecision, bool multipleNodes)
	{
		std::vector<std::pair<float, std::string>> distanceNodePairs;

		for (const auto& [nodeKey, nodeValues] : skinObj->second) {
			RE::NiAVObject* targetNode = target->GetNodeByName(nodeKey);
			if (!targetNode) continue;

			RE::NiPoint3 nodePosition = targetNode->world.translate;
			float distance = hitPosition.GetDistance(nodePosition);
			if (distance <= hitPrecision) {
				distanceNodePairs.emplace_back(distance, nodeKey);
			}
		}

		std::sort(distanceNodePairs.begin(), distanceNodePairs.end(),
			[](const std::pair<float, std::string>& a, const std::pair<float, std::string>& b) {
				return a.first < b.first;
			});

		std::vector<std::string> validNodes;
		if (!distanceNodePairs.empty()) {
			if (multipleNodes) {
				for (const auto& [distance, nodeKey] : distanceNodePairs) {
					validNodes.push_back(nodeKey);
				}
			} else {
				validNodes.push_back(distanceNodePairs.front().second);
			}
		}

		return validNodes;
	}
}
