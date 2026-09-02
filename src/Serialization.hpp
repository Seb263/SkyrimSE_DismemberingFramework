#pragma once

namespace ModData
{
	constexpr std::uint32_t coSaveId = std::byteswap('DISM');
	constexpr std::uint32_t dismMapId = std::byteswap('DMAP');
	inline std::unordered_map<RE::FormID, uint32_t> dismemberingMap;

	class Serialization
	{
	public:
		static void RegisterSerializationCallbacks()
		{
			auto serialization = SKSE::GetSerializationInterface();
			serialization->SetUniqueID(coSaveId);
			serialization->SetSaveCallback(OnSKSESave);
			serialization->SetLoadCallback(OnSKSELoad);
			serialization->SetRevertCallback(OnSKSERevert);
		}

		static uint32_t GetDismemberedLimbs(RE::FormID form_id)
		{
			if (!form_id) return 0;

			auto it = dismemberingMap.find(form_id);
			if (it != dismemberingMap.end()) return it->second;
			
			return 0;
		}

		static void AddDismemberedLimbKeyword(RE::FormID form_id, uint32_t keyword)
		{
			if (!form_id) return;

			uint32_t& value = dismemberingMap[form_id];
			if (!(value & keyword)) {
				value |= keyword;
			}
		}

		static void RemoveDismemberedLimbKeyword(RE::FormID form_id, uint32_t keyword)
		{
			if (!form_id) return;

			auto it = dismemberingMap.find(form_id);
			if (it != dismemberingMap.end()) {
				it->second &= ~keyword;
				
				if (it->second == 0) dismemberingMap.erase(it);
			}
		}

		static void ResetDismemberedLimbs(RE::FormID form_id)
		{
			if (!form_id) return;

			dismemberingMap.erase(form_id);
		}

	private:
		static void OnSKSESave(SKSE::SerializationInterface* intfc)
		{
			if (!intfc->OpenRecord(dismMapId, 1)) {
				logger::critical("Failed to open the DISM record for serialization.");
				return;
			}

			std::size_t map_size = dismemberingMap.size();
			intfc->WriteRecordData(&map_size, sizeof(map_size));

			for (const auto& [form_id, value] : dismemberingMap) {
				intfc->WriteRecordData(&form_id, sizeof(form_id));
				intfc->WriteRecordData(&value, sizeof(value));
			}
		}

		static void OnSKSELoad(SKSE::SerializationInterface* intfc)
		{
			dismemberingMap.clear();

			std::uint32_t type, version, length;
			while (intfc->GetNextRecordInfo(type, version, length)) {
				if (type != dismMapId || version != 1) continue;

				std::size_t map_size;
				if (!intfc->ReadRecordData(&map_size, sizeof(map_size))) {
					logger::critical("Unable to read the map size during record deserialization.");
					continue;
				}

				for (std::size_t i = 0; i < map_size; ++i) {
					RE::FormID formID;
					uint32_t   value;

					if (!intfc->ReadRecordData(&formID, sizeof(formID)) ||
						!intfc->ReadRecordData(&value, sizeof(value))) {
						logger::critical("Unable to read a FormID -> Value pair during record deserialization.");
						continue;
					}

					RE::Actor* boundActor = RE::TESForm::LookupByID<RE::Actor>(formID);
					if (!boundActor || boundActor->IsDisabled() || !boundActor->IsDead()) continue;

					dismemberingMap[formID] = value;
				}
			}
		}

		static void OnSKSERevert(SKSE::SerializationInterface*)
		{
			dismemberingMap.clear();
		}
	};
};
