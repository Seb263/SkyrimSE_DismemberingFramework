#include "JSON.h"

namespace ModData
{
	void JSONHandler::LoadJSONFiles()
	{
		const auto start = std::chrono::high_resolution_clock::now();
		logger::info("Loading JSON files ({})...", (SettingsIni::bAsynchronousStartup ? "asynchronous" : "synchronous"));

		const auto& dataHandler = DataHandler::GetSingleton();
		limbKeyword = dataHandler->limbKeyword;
        
		// Processing _TEMPLATE.json files
		const auto& templateFiles = MiscUtils::GetAllFiles("Data\\SKSE\\DismemberingFramework"sv, ".json"sv, {}, "_TEMPLATES"sv);
		for (const auto& fileName : templateFiles) {
            logger::info("Parsing JSON Templates In \"{}\"", fileName);
			ProcessTemplatesFile(fileName);
		}

		// Processing _LIMBS.json files
		const auto& limbsFiles = MiscUtils::GetAllFiles("Data\\SKSE\\DismemberingFramework"sv, ".json"sv, {}, "_LIMBS"sv);
		for (const auto& fileName : limbsFiles) {
            logger::info("Parsing JSON Limbs In \"{}\"", fileName);
			ProcessLimbsFile(fileName);
		}

		// Processing _DATA.json files
		const auto& dataFiles = MiscUtils::GetAllFiles("Data\\SKSE\\DismemberingFramework"sv, ".json"sv, {}, "_DATA"sv);
		nlohmann::json mergedData;
		for (const auto& fileName : dataFiles) {
			try {
				std::ifstream fileStream(fileName);
				json fileData = json::parse(fileStream);
				logger::info("Parsing JSON Data In \"{}\"", fileName);

				ProcessKeysWithPipes(fileData);
				MergeJson(mergedData, fileData);
			} catch (const std::exception& e) {
				REPORT_AND_FAIL("Error while processing template JSON file '" + fileName + "': " + e.what());
			}
		}

		ReplaceAnyWithSiblings(mergedData);

		// Sort JSON objects by "Priority" key
		SortObjectsByPriority(mergedData);

		// Process the merged and sorted data
		ProcessDataFile(mergedData);

		const auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> elapsed = end - start;
		logger::info("Loading JSON files ({}): DONE after {} seconds", (SettingsIni::bAsynchronousStartup ? "asynchronous" : "synchronous"), elapsed.count());

		if (debugVerboseMode > 1) {
			logger::info("Content of compiled JSON: {}", mergedData.dump(4));

			logger::info("Content of dataMap:");
			for (const auto& [race, raceMap] : dataMap) {
				for (const auto& [skin, skinMap] : raceMap) {
					for (const auto& [sex, sexMap] : skinMap) {
						for (const auto& [node, dataList] : sexMap) {
							logger::info("Race: '{}', Skin: '{}', Sex: '{}', Node: '{}', DismemberingDataList size: {}", race, skin, sex, node, dataList.size());
						}
					}
				}
			}
		}
	}

	void JSONHandler::ProcessTemplatesFile(const std::string& fileName)
	{
		try {
			std::ifstream fileStream(fileName);
			const auto jsonData = json::parse(fileStream);

			for (const auto& [templateID, templateData] : jsonData.items()) {
				templatesMap[templateID] = templateData;
			}
		} catch (const std::exception& e) {
			REPORT_AND_FAIL("Error while processing template JSON file '" + fileName + "': " + e.what());
		}
	}

	void JSONHandler::ProcessLimbsFile(const std::string& fileName)
	{
		try {
			std::ifstream fileStream(fileName);
			auto jsonData = json::parse(fileStream);

			for (const auto& [limbID, limbData] : jsonData.items()) {
				json combinedData = limbData;

				if (limbData.contains("Templates") && limbData["Templates"].is_array()) {
					for (const auto& templateID : limbData["Templates"]) {
						if (templatesMap.find(templateID) != templatesMap.end()) {
							combinedData.update(templatesMap[templateID]);
						} else {
							logger::warn("Template \"{}\" not found in _TEMPLATES.json files.", templateID.get<std::string>());
						}
					}
				}

				limbsMap[limbID] = CreateObject(combinedData, limbID);
			}
		} catch (const std::exception& e) {
			REPORT_AND_FAIL("Error while processing template JSON file '" + fileName + "': " + e.what());
		}
	}

	void JSONHandler::ProcessDataFile(const nlohmann::json& jsonData)
	{
		std::unordered_map<std::size_t, DismemberingData> cache;

		for (const auto& [raceSkin, raceSkinValue] : jsonData.items()) {
			ProcessRaceSkinData(raceSkin, raceSkinValue, cache);
		}
	}

	void JSONHandler::ProcessRaceSkinData(const std::string& raceSkin, const nlohmann::json& raceSkinValue, std::unordered_map<std::size_t, DismemberingData>& cache)
	{
		for (const auto& [skin, skinValue] : raceSkinValue.items()) {
			std::string skinKeyFormated = skin;
			std::transform(skinKeyFormated.begin(), skinKeyFormated.end(), skinKeyFormated.begin(), ::tolower);

			for (const auto& [sex, limbValue] : skinValue.items()) {
				for (const auto& limbNamePair : limbValue.items()) {
					const std::string& limbName = limbNamePair.key();
					const auto& limbInfoArray = limbNamePair.value();
					if (!limbInfoArray.is_array()) continue;

					for (const json& limbInfo : limbInfoArray) {
						if (!limbInfo.contains("IDLimb")) {
							logger::warn("Missing \"IDLimb\" in JSON data for race \"{}\", skin \"{}\", sex \"{}\", limb \"{}\".", raceSkin, skinKeyFormated, sex, limbName);
							continue;
						}

						const std::size_t& cacheKey = compute_json_checksum(limbInfo);

						if (cache.find(cacheKey) == cache.end()) {
							std::string IDLimb = limbInfo["IDLimb"];
							if (limbsMap.find(IDLimb) != limbsMap.end()) {
								nlohmann::json conditions = limbInfo.value("Conditions", nlohmann::json::array());

								const DismemberingData& dismemberingData = SetDismemberingInfos(limbsMap[IDLimb], limbName, conditions);
								cache[cacheKey] = dismemberingData;

								AddObject(raceSkin, skinKeyFormated, sex, limbName, dismemberingData);
							} else {
								logger::warn("IDLimb \"{}\" not found in _LIMBS.json files.", IDLimb);
							}
						} else {
							AddObject(raceSkin, skinKeyFormated, sex, limbName, cache[cacheKey]);
						}
					}
				}
			}
		}
	}

	JSONObject JSONHandler::CreateObject(const nlohmann::json& element, std::string nodeKey) const
	{
		JSONObject data;
		data.id = nodeKey;
		data.method = element.value("Method", "DismemberingFramework");
		data.collisionNode = element.value("CollisionNode", "");
		data.refNode = element.value("RefNode", "");
		data.bipedSlot = element.value("BipedSlot", 0);
		data.limbFormID = element.value("LimbFormID", "");
		data.limbArtObject = element.value("LimbArtObject", "");
		data.actorArmorAddon = element.value("ActorArmorAddon", "");
		data.actorArtObject = element.value("ActorArtObject", "");
		data.playerEffectSpell = element.value("PlayerEffectSpell", "");
		data.targetEffectSpell = element.value("TargetEffectSpell", "");
		data.dropItem = element.value("DropItem", "");
		data.endDialogue = element.value("EndDialogue", false);
		data.resizeNode = element.value("ResizeNode", true);
		data.disableCollision = element.value("DisableCollision", true);
		data.typeOverride = element.value("TypeOverride", "");

		if (element.contains("DismemberingSFX")) {
			const auto& dismemberingSFX = element.at("DismemberingSFX");
			data.dismemberingSFX.oneHandSword = dismemberingSFX.value("OneHandSword", "");
			data.dismemberingSFX.oneHandSwordHeavy = dismemberingSFX.value("OneHandSwordHeavy", "");
			data.dismemberingSFX.twoHandSword = dismemberingSFX.value("TwoHandSword", "");
			data.dismemberingSFX.twoHandSwordHeavy = dismemberingSFX.value("TwoHandSwordHeavy", "");
			data.dismemberingSFX.oneHandAxe = dismemberingSFX.value("OneHandAxe", "");
			data.dismemberingSFX.oneHandAxeHeavy = dismemberingSFX.value("OneHandAxeHeavy", "");
			data.dismemberingSFX.twoHandAxe = dismemberingSFX.value("TwoHandAxe", "");
			data.dismemberingSFX.twoHandAxeHeavy = dismemberingSFX.value("TwoHandAxeHeavy", "");
			data.dismemberingSFX.oneHandMace = dismemberingSFX.value("OneHandMace", "");
			data.dismemberingSFX.oneHandMaceHeavy = dismemberingSFX.value("OneHandMaceHeavy", "");
			data.dismemberingSFX.twoHandMace = dismemberingSFX.value("TwoHandMace", "");
			data.dismemberingSFX.twoHandMaceHeavy = dismemberingSFX.value("TwoHandMaceHeavy", "");
			data.dismemberingSFX.handToHand = dismemberingSFX.value("HandToHand", "");
			data.dismemberingSFX.handToHandHeavy = dismemberingSFX.value("HandToHandHeavy", "");
			data.dismemberingSFX.dagger = dismemberingSFX.value("Dagger", "");
			data.dismemberingSFX.daggerHeavy = dismemberingSFX.value("DaggerHeavy", "");
			data.dismemberingSFX.ranged = dismemberingSFX.value("Ranged", "");
			data.dismemberingSFX.rangedHeavy = dismemberingSFX.value("RangedHeavy", "");
			data.dismemberingSFX.other = dismemberingSFX.value("Other", "");
			data.dismemberingSFX.otherHeavy = dismemberingSFX.value("OtherHeavy", "");
			data.dismemberingSFX.parrying = dismemberingSFX.value("Parrying", "");
		}

		if (element.contains("Translation")) {
			const auto& translation = element.at("Translation");
			data.translation.x = translation.value("x", 0.0f);
			data.translation.y = translation.value("y", 0.0f);
			data.translation.z = translation.value("z", 0.0f);
		}

		if (const auto& limbFormID = MiscUtils::GetFormIDFromString<RE::TESObjectMISC>(data.limbFormID)) {
			if (limbFormID && !limbFormID->HasKeyword(limbKeyword)) {
				limbFormID->AddKeyword(limbKeyword);
			}
		}

		return data;
	}

	DismemberingData JSONHandler::SetDismemberingInfos(JSONObject JSONobject, std::string targetedNode, const nlohmann::json& conditions) const
	{
		DismemberingData data;

		data.id = JSONobject.id;
		data.method = JSONobject.method == "Decapitate" ? DismemberingData::Methods::kDecapitate : DismemberingData::Methods::kDismemberingFramework;
		data.targetedNode = targetedNode;
		data.collisionNode = (!JSONobject.collisionNode.empty() ? JSONobject.collisionNode : targetedNode);
		data.refNode = (!JSONobject.refNode.empty() ? JSONobject.refNode : targetedNode);
		data.bipedSlot = (JSONobject.bipedSlot > 0 ? JSONobject.bipedSlot : 0);
		data.limbFormID = MiscUtils::GetFormIDFromString<RE::TESObjectMISC>(JSONobject.limbFormID);
		data.limbArtObject = MiscUtils::GetFormIDFromString<RE::BGSArtObject>(JSONobject.limbArtObject);
		data.actorArtObject = MiscUtils::GetFormIDFromString<RE::BGSArtObject>(JSONobject.actorArtObject);
		data.actorArmorAddon = MiscUtils::GetFormIDFromString<RE::TESObjectARMO>(JSONobject.actorArmorAddon);
		data.playerEffectSpell = MiscUtils::GetFormIDFromString<RE::SpellItem>(JSONobject.playerEffectSpell);
		data.targetEffectSpell = MiscUtils::GetFormIDFromString<RE::SpellItem>(JSONobject.targetEffectSpell);
		data.dropItem = (JSONobject.dropItem == "All")       ? DismemberingData::DropItem::kAll :
		                (JSONobject.dropItem == "Armor") ? DismemberingData::DropItem::kArmor :
			                                                   DismemberingData::DropItem::kNone;
		data.endDialogue = JSONobject.endDialogue;
		data.resizeNode = JSONobject.resizeNode;
		data.disableCollision = JSONobject.disableCollision;
		data.translation = RE::NiPoint3(JSONobject.translation.x, JSONobject.translation.y, JSONobject.translation.z);
		data.typeOverride = (JSONobject.typeOverride == "Heavy") ? DismemberingData::TypeOverride::kHeavyArmor :
		                    (JSONobject.typeOverride == "Light") ? DismemberingData::TypeOverride::kLightArmor :
		                                                           DismemberingData::TypeOverride::kClothing;

		data.dismemberingSFX.oneHandSword = MiscUtils::GetFormIDFromString<RE::BGSSoundDescriptorForm>(JSONobject.dismemberingSFX.oneHandSword);
		data.dismemberingSFX.oneHandSwordHeavy = MiscUtils::GetFormIDFromString<RE::BGSSoundDescriptorForm>(JSONobject.dismemberingSFX.oneHandSwordHeavy);
		data.dismemberingSFX.twoHandSword = MiscUtils::GetFormIDFromString<RE::BGSSoundDescriptorForm>(JSONobject.dismemberingSFX.twoHandSword);
		data.dismemberingSFX.twoHandSwordHeavy = MiscUtils::GetFormIDFromString<RE::BGSSoundDescriptorForm>(JSONobject.dismemberingSFX.twoHandSwordHeavy);
		data.dismemberingSFX.oneHandAxe = MiscUtils::GetFormIDFromString<RE::BGSSoundDescriptorForm>(JSONobject.dismemberingSFX.oneHandAxe);
		data.dismemberingSFX.oneHandAxeHeavy = MiscUtils::GetFormIDFromString<RE::BGSSoundDescriptorForm>(JSONobject.dismemberingSFX.oneHandAxeHeavy);
		data.dismemberingSFX.twoHandAxe = MiscUtils::GetFormIDFromString<RE::BGSSoundDescriptorForm>(JSONobject.dismemberingSFX.twoHandAxe);
		data.dismemberingSFX.twoHandAxeHeavy = MiscUtils::GetFormIDFromString<RE::BGSSoundDescriptorForm>(JSONobject.dismemberingSFX.twoHandAxeHeavy);
		data.dismemberingSFX.oneHandMace = MiscUtils::GetFormIDFromString<RE::BGSSoundDescriptorForm>(JSONobject.dismemberingSFX.oneHandMace);
		data.dismemberingSFX.oneHandMaceHeavy = MiscUtils::GetFormIDFromString<RE::BGSSoundDescriptorForm>(JSONobject.dismemberingSFX.oneHandMaceHeavy);
		data.dismemberingSFX.twoHandMace = MiscUtils::GetFormIDFromString<RE::BGSSoundDescriptorForm>(JSONobject.dismemberingSFX.twoHandMace);
		data.dismemberingSFX.twoHandMaceHeavy = MiscUtils::GetFormIDFromString<RE::BGSSoundDescriptorForm>(JSONobject.dismemberingSFX.twoHandMaceHeavy);
		data.dismemberingSFX.handToHand = MiscUtils::GetFormIDFromString<RE::BGSSoundDescriptorForm>(JSONobject.dismemberingSFX.handToHand);
		data.dismemberingSFX.handToHandHeavy = MiscUtils::GetFormIDFromString<RE::BGSSoundDescriptorForm>(JSONobject.dismemberingSFX.handToHandHeavy);
		data.dismemberingSFX.dagger = MiscUtils::GetFormIDFromString<RE::BGSSoundDescriptorForm>(JSONobject.dismemberingSFX.dagger);
		data.dismemberingSFX.daggerHeavy = MiscUtils::GetFormIDFromString<RE::BGSSoundDescriptorForm>(JSONobject.dismemberingSFX.daggerHeavy);
		data.dismemberingSFX.ranged = MiscUtils::GetFormIDFromString<RE::BGSSoundDescriptorForm>(JSONobject.dismemberingSFX.ranged);
		data.dismemberingSFX.rangedHeavy = MiscUtils::GetFormIDFromString<RE::BGSSoundDescriptorForm>(JSONobject.dismemberingSFX.rangedHeavy);
		data.dismemberingSFX.other = MiscUtils::GetFormIDFromString<RE::BGSSoundDescriptorForm>(JSONobject.dismemberingSFX.other);
		data.dismemberingSFX.otherHeavy = MiscUtils::GetFormIDFromString<RE::BGSSoundDescriptorForm>(JSONobject.dismemberingSFX.otherHeavy);
		data.dismemberingSFX.parrying = MiscUtils::GetFormIDFromString<RE::BGSSoundDescriptorForm>(JSONobject.dismemberingSFX.parrying);

		// Parse conditions
		for (const auto& condition : conditions) {
			Condition cond;
			if (condition.contains("Name") && condition["Name"].is_string()) {
				cond.name = condition["Name"].get<std::string>();
			} else {
				logger::warn("Missing or invalid \"Name\" in condition.");
				continue;
			}

			for (auto& [key, value] : condition.items()) {
				if (key == "Name") continue;
				if (value.is_string()) {
					std::string str = value.get<std::string>();

					size_t pos = str.find(":0x");
					if (pos == std::string::npos) {
						cond.parameters[key] = str;
					} else {
						if (RE::TESForm* form = MiscUtils::GetFormIDFromString<RE::TESForm>(str)) {
							cond.parameters[key] = form;
						} else {
							cond.parameters[key] = nullptr;
						}
					}
				} else if (value.is_number_integer()) {
					cond.parameters[key] = value.get<int>();
				} else if (value.is_number_float()) {
					cond.parameters[key] = value.get<float>();
				}
			}
			data.conditions.push_back(cond);
		}

		return data;
	}

	void JSONHandler::AddObject(const std::string& race, const std::string& skin, const std::string& sex, const std::string& node, DismemberingData obj)
	{
		if (combinationMap.find(race) == combinationMap.end()) {
			combinationMap[race] = {};
			reverseCombinationMap[race] = {};
		}

		if (combinationMap[race].find(skin) == combinationMap[race].end()) {
			combinationMap[race][skin] = {};
			reverseCombinationMap[race][skin] = {};
		}

		if (combinationMap[race][skin].find(sex) == combinationMap[race][skin].end()) {
			combinationMap[race][skin][sex] = {};
			reverseCombinationMap[race][skin][sex] = {};
		}

		if (combinationMap[race][skin][sex].find(node) == combinationMap[race][skin][sex].end()) {
            int nextId = static_cast<int>(combinationMap[race][skin][sex].size());
			combinationMap[race][skin][sex][node] = nextId;
			reverseCombinationMap[race][skin][sex][nextId] = obj;
		}

		obj.idKeyword = combinationMap[race][skin][sex][node];

		dataMap[race][skin][sex][node].push_back(std::make_shared<DismemberingData>(obj));
	}

	std::optional<DismemberingData> JSONHandler::GetDismemberingDataFromIdKeyword(const std::string& race, const std::string& skin, const std::string& sex, int idKeyword)
	{
		if (race.empty() || skin.empty() || sex.empty()) return std::nullopt;

		try {
			const auto& raceMap = reverseCombinationMap.at(race);

			if (raceMap.find(skin) != raceMap.end()) {
				const auto& skinMap = raceMap.at(skin);
				if (skinMap.find(sex) != skinMap.end()) {
					const auto& sexMap = skinMap.at(sex);
					if (sexMap.find(idKeyword) != sexMap.end()) {
						return sexMap.at(idKeyword);
					}
				}
			}

			if (raceMap.find("any") != raceMap.end()) {
				const auto& anySkinMap = raceMap.at("any");
				if (anySkinMap.find(sex) != anySkinMap.end()) {
					const auto& sexMap = anySkinMap.at(sex);
					if (sexMap.find(idKeyword) != sexMap.end()) {
						return sexMap.at(idKeyword);
					}
				}
			}
		} catch (const std::out_of_range&) {
			logger::warn("Invalid access in GetDismemberingDataFromIdKeyword for race = \"{}\", skin = \"{}\", sex = \"{}\", idKeyword = \"{}\"", race, skin, sex, idKeyword);
		}

		return std::nullopt;
	}

	std::unordered_map<int, DismemberingData>* JSONHandler::GetNodeMap(const std::string& race, const std::string& skin, const std::string& sex)
	{
		if (race.empty() || skin.empty() || sex.empty()) return nullptr;

		auto raceIt = reverseCombinationMap.find(race);
		if (raceIt != reverseCombinationMap.end()) {
			auto& skinMap = raceIt->second;
			auto  skinIt = skinMap.find(skin);
			if (skinIt != skinMap.end()) {
				auto& sexMap = skinIt->second;
				auto  sexIt = sexMap.find(sex);
				if (sexIt != sexMap.end()) {
					return &sexIt->second;
				}
			} else {
				auto anySkinIt = skinMap.find("any");
				if (anySkinIt != skinMap.end()) {
					auto& sexMap = anySkinIt->second;
					auto  sexIt = sexMap.find(sex);
					if (sexIt != sexMap.end()) {
						return &sexIt->second;
					}
				}
			}
		}

		return nullptr;
	}
}
