#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Utils/MiscUtils.hpp"

namespace ModData
{
	using json = nlohmann::json;

	struct Translation
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
	};
	
	struct DismemberingSFX
	{
		RE::BGSSoundDescriptorForm* oneHandSword;
		RE::BGSSoundDescriptorForm* oneHandSwordHeavy;
		RE::BGSSoundDescriptorForm* twoHandSword;
		RE::BGSSoundDescriptorForm* twoHandSwordHeavy;
		RE::BGSSoundDescriptorForm* oneHandAxe;
		RE::BGSSoundDescriptorForm* oneHandAxeHeavy;
		RE::BGSSoundDescriptorForm* twoHandAxe;
		RE::BGSSoundDescriptorForm* twoHandAxeHeavy;
		RE::BGSSoundDescriptorForm* oneHandMace;
		RE::BGSSoundDescriptorForm* oneHandMaceHeavy;
		RE::BGSSoundDescriptorForm* twoHandMace;
		RE::BGSSoundDescriptorForm* twoHandMaceHeavy;
		RE::BGSSoundDescriptorForm* handToHand;
		RE::BGSSoundDescriptorForm* handToHandHeavy;
		RE::BGSSoundDescriptorForm* dagger;
		RE::BGSSoundDescriptorForm* daggerHeavy;
		RE::BGSSoundDescriptorForm* ranged;
		RE::BGSSoundDescriptorForm* rangedHeavy;
		RE::BGSSoundDescriptorForm* other;
		RE::BGSSoundDescriptorForm* otherHeavy;
		RE::BGSSoundDescriptorForm* parrying;
	};

	struct JSONDismemberingSFX
	{
		std::string oneHandSword;
		std::string oneHandSwordHeavy;
		std::string twoHandSword;
		std::string twoHandSwordHeavy;
		std::string oneHandAxe;
		std::string oneHandAxeHeavy;
		std::string twoHandAxe;
		std::string twoHandAxeHeavy;
		std::string oneHandMace;
		std::string oneHandMaceHeavy;
		std::string twoHandMace;
		std::string twoHandMaceHeavy;
		std::string handToHand;
		std::string handToHandHeavy;
		std::string dagger;
		std::string daggerHeavy;
		std::string ranged;
		std::string rangedHeavy;
		std::string other;
		std::string otherHeavy;
		std::string parrying;
	};

	struct JSONObject
	{
		std::string id;
		std::string method;
		std::string typeOverride;
		std::string collisionNode;
		std::string refNode;
		int			bipedSlot;
		std::string limbFormID;
		std::string limbArtObject;
		std::string actorArmorAddon;
		std::string actorArtObject;
		std::string playerEffectSpell;
		std::string targetEffectSpell;
		std::string dropItem;
		bool endDialogue;
		bool resizeNode;
		bool disableCollision;
		JSONDismemberingSFX dismemberingSFX;
		Translation translation;
	};

	struct Condition
	{
		std::string name;
		std::unordered_map<std::string, std::variant<std::string, int, float, RE::TESForm*>> parameters;
	};

	struct DismemberingData
	{
		enum class Methods
		{
			kDismemberingFramework,
			kDecapitate
		};
		
		enum class TypeOverride
		{
			kClothing,
			kLightArmor,
			kHeavyArmor
		};
		
		enum class DropItem
		{
			kAll,
			kArmor,
			kNone
		};

		std::string            id;
		int                    idKeyword;
		Methods                method = Methods::kDismemberingFramework;
		TypeOverride           typeOverride = TypeOverride::kClothing;
		std::string            targetedNode = "";
		std::string            collisionNode = "";
		std::string            refNode = "";
		int					   bipedSlot = 0;
		RE::TESObjectMISC*     limbFormID;
		RE::BGSArtObject*      limbArtObject;
		RE::TESObjectARMO*     actorArmorAddon;
		RE::BGSArtObject*      actorArtObject;
		RE::SpellItem*         playerEffectSpell;
		RE::SpellItem*         targetEffectSpell;
		DropItem               dropItem;
		bool                   endDialogue;
		bool                   resizeNode;
		bool                   disableCollision;
		DismemberingSFX        dismemberingSFX;
		RE::NiPoint3           translation;
		std::vector<Condition> conditions;
	};

	class JSONHandler
	{
    public:
		static JSONHandler* GetSingleton() {
            static JSONHandler singleton;
            return &singleton;
        }

		void Load()
		{
			if (!loadingStarted) {
				loadingStarted = true;

				if (SettingsIni::bAsynchronousStartup) {
					loadFuture = std::async(std::launch::async, &JSONHandler::LoadJSONFiles, this);
				} else {
					JSONHandler::LoadJSONFiles();
				}
			}
		}

		void WaitUntilReady()
		{
			if (SettingsIni::bAsynchronousStartup && loadFuture.valid()) {
				loadFuture.get();
			}
		}

		using DismemberingDataPtr = std::shared_ptr<DismemberingData>;
		using DismemberingDataList = std::vector<DismemberingDataPtr>;

		const std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, DismemberingDataList>>>>& GetData() const
		{
			return dataMap;
		}

		std::optional<DismemberingData> GetDismemberingDataFromIdKeyword(const std::string& race, const std::string& skin, const std::string& sex, int idKeyword);
		std::unordered_map<int, DismemberingData>* GetNodeMap(const std::string& race, const std::string& skin, const std::string& sex);

	private:

		JSONHandler() = default;
		std::future<void> loadFuture;
		bool loadingStarted = false;

		// Data structures to store parsed JSON data
		std::unordered_map<std::string, json>                                                                                                                    templatesMap;
		std::unordered_map<std::string, JSONObject>                                                                                                              limbsMap;
		std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, DismemberingDataList>>>> dataMap;
		std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, int>>>>                  combinationMap;
		std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<int, DismemberingData>>>>             reverseCombinationMap;
		RE::BGSKeyword* limbKeyword;

		void LoadJSONFiles();
		void ProcessDataFile(const nlohmann::json& jsonData);
		void ProcessLimbsFile(const std::string& fileName);
		void ProcessTemplatesFile(const std::string& fileName);

		void ProcessRaceSkinData(const std::string& raceSkin, const nlohmann::json& raceSkinValue, std::unordered_map<std::size_t, DismemberingData>& cache);
		JSONObject CreateObject(const json& element, std::string nodeKey) const;
		DismemberingData SetDismemberingInfos(JSONObject JSONobject, std::string targetedNode, const json& limbInfo) const;
		void AddObject(const std::string& race, const std::string& skin, const std::string& sex, const std::string& node, DismemberingData obj);

		static std::vector<std::string> SplitString(const std::string& str, const std::string& delimiter)
		{
			std::vector<std::string> substrings;
			size_t pos_start = 0, pos_end, delim_len = delimiter.length();
			while ((pos_end = str.find(delimiter, pos_start)) != std::string::npos) {
				substrings.push_back(str.substr(pos_start, pos_end - pos_start));
				pos_start = pos_end + delim_len;
			}
			substrings.push_back(str.substr(pos_start));
			return substrings;
		}

		static void ReplaceAnyWithSiblings(json& data)
		{
			for (const auto& [raceSkin, raceSkinValue] : data.items()) {
				if (raceSkinValue.contains("ANY")) {
					const auto& anyValue = raceSkinValue["ANY"];
					for (auto& [key, value] : raceSkinValue.items()) {
						if (key != "ANY") {
							MergeJson(value, anyValue);
						}
					}
				}
			}
		}

		static void SortObjectsByPriority(json& data)
		{
			auto sortArrayByPriority = [](json& array) {
				std::sort(array.begin(), array.end(), [](json& a, json& b) {
					int priorityA = a.contains("Priority") ? a["Priority"].get<int>() : 0;
					int priorityB = b.contains("Priority") ? b["Priority"].get<int>() : 0;
					return priorityA > priorityB;
				});
			};

			std::function<void(json&)> sortRecursively = [&](json& obj) {
				if (obj.is_object()) {
					for (auto& [key, value] : obj.items()) {
						if (value.is_array()) {
							sortArrayByPriority(value);
						} else if (value.is_object()) {
							sortRecursively(value);
						}
					}
				}
			};

			sortRecursively(data);
		}

		static void MergeJson(json& target, const json& source)
		{
			for (auto it = source.begin(); it != source.end(); ++it) {
				if (target.contains(it.key())) {
					if (target[it.key()].is_object() && it.value().is_object()) {
						MergeJson(target[it.key()], it.value());
					} else if (target[it.key()].is_array() && it.value().is_array()) {
						for (const auto& item : it.value()) {
							bool exists = std::find(target[it.key()].begin(), target[it.key()].end(), item) != target[it.key()].end();
							if (!exists) {
								target[it.key()].push_back(item);
							}
						}
					} else {
						target[it.key()] = it.value();
					}
				} else {
					target[it.key()] = it.value();
				}
			}
		}

		static void ProcessKeysWithPipes(json& jsonData)
		{
			json newJsonData;

			for (auto it = jsonData.begin(); it != jsonData.end(); ++it) {
				auto keys = SplitString(it.key(), "|");
				json processedValue = it.value();

				if (it->is_object()) {
					ProcessKeysWithPipes(processedValue);
				}

				for (const auto& key : keys) {
					if (newJsonData.contains(key)) {
						MergeJson(newJsonData[key], processedValue);
					} else {
						newJsonData[key] = processedValue;
					}
				}
			}

			jsonData = newJsonData;
		}

		static std::size_t compute_json_checksum(const nlohmann::json& j)
		{
			return std::hash<std::string>{}(j.dump());
		}
	};
}
