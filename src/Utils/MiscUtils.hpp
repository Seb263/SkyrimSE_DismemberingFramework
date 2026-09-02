#pragma once

class MiscUtils
{
	public:

	template <typename T>
	static T* GetFormIDFromString(const std::string& a_string)
	{
		static std::unordered_map<std::string, RE::TESForm*> cache;

		auto it = cache.find(a_string);
		if (it != cache.end()) {
			if constexpr (std::is_same_v<T, RE::TESForm>) {
				return it->second;
			} else {
				if (T* form = it->second->As<T>()) {
					return form;
				} else {
					logger::warn("GetFormIDFromString: Cached form ID \"{}\" is incompatible with \"{}\" type.", a_string, typeid(T).name());
					return nullptr;
				}
			}
		}

		size_t pos = a_string.find(":");
		if (pos == std::string::npos && a_string != "") {
			logger::warn("GetFormIDFromString: Invalid format for \"{}\"", a_string);
			return nullptr;
		}

		std::string modName = a_string.substr(0, pos);
		std::string hexPart = a_string.substr(pos + 1);
		std::uint32_t number = 0;

		try {
			number = std::stoul(hexPart, nullptr, 16);
		} catch (const std::invalid_argument&) {
			return nullptr;
		} catch (const std::out_of_range&) {
			logger::warn("GetFormIDFromString: Form ID out of range \"{}\"", a_string);
			return nullptr;
		}

		const auto& TESdataHandler = RE::TESDataHandler::GetSingleton();
		const auto lookup_form = TESdataHandler->LookupForm(static_cast<RE::FormID>(number), modName);
		if (!lookup_form) {
			logger::warn("GetFormIDFromString: Form ID \"{}\" could not be found.", a_string);
			return nullptr;
		}

		cache[a_string] = lookup_form;

		if constexpr (std::is_same_v<T, RE::TESForm>) {
			return lookup_form;
		} else {
			if (T* form = lookup_form->As<T>()) {
				return form;
			} else {
				logger::warn("GetFormIDFromString: Form ID \"{}\" is incompatible with \"{}\" type.", a_string, typeid(T).name());
				return nullptr;
			}
		}
	}

	static std::string GetAssocStringFromForm(RE::TESForm* a_form, const std::string& defaultValue = "")
	{
		if (!a_form) return defaultValue;

		auto* file = a_form->GetFile();
		if (!file) return defaultValue;

		const std::string& fileName = file->fileName; // Using const reference to avoid copying
		std::ostringstream formIDHex;
		formIDHex << "0x" << std::uppercase << std::hex << a_form->GetLocalFormID();

		std::string formIDString = fileName + ":" + formIDHex.str();
		std::transform(formIDString.begin(), formIDString.end(), formIDString.begin(), [](unsigned char c) { return std::tolower(c); });

		return formIDString;
	}

	template <bool RECURSIVE = false>
    static std::vector<std::string> GetAllFiles(std::string_view a_path = {}, std::string_view a_ext = {},
                                                std::string_view a_prefix = {}, std::string_view a_suffix = {}) noexcept {
        using dir_iterator =  std::conditional_t<RECURSIVE, std::filesystem::recursive_directory_iterator, std::filesystem::directory_iterator>;

        std::vector<std::string> files;
        auto file_iterator = [&](const std::filesystem::directory_entry& a_file) {
            if (a_file.exists() && !a_file.path().empty()) {
                if (!a_ext.empty() && a_file.path().extension() != a_ext) {
                    return;
                }

                const auto path = a_file.path().string();

                if (!a_prefix.empty() && path.find(a_prefix) != std::string::npos) {
                    files.push_back(path);
                } else if (!a_suffix.empty() && path.rfind(a_suffix) != std::string::npos) {
                    files.push_back(path);
                } else if (a_prefix.empty() && a_suffix.empty()) {
                    files.push_back(path);
                }
            }
        };

        std::string dir(MAX_PATH + 1, ' ');
        auto res = GetModuleFileNameA(nullptr, dir.data(), MAX_PATH + 1);
        if (res == 0) {
            logger::critical("Unable to acquire valid path using default null path argument!\nExpected: Current directory\nResolved: NULL");
        }

        auto eol = dir.find_last_of("\\/");
        dir = dir.substr(0, eol);

        auto path = a_path.empty() ? std::filesystem::path{dir} : std::filesystem::path{a_path};
        if (!is_directory(path.parent_path())) {
            path = dir / path;
        }

        std::ranges::for_each(dir_iterator(path), file_iterator);
        std::ranges::sort(files);

        return files;
    }

	static RE::Projectile::ProjectileRot DirToAngles(const RE::NiPoint3& dir, bool invert = false)
	{
		RE::Projectile::ProjectileRot rot{};
		RE::NiPoint3                  norm = dir;
		norm.Unitize();

		if (!invert) {
			rot.x = -asinf(norm.z);
			rot.z = atan2f(norm.x, norm.y);
		} else {
			rot.x = asinf(norm.z);             // inversion du pitch
			rot.z = atan2f(-norm.x, -norm.y);  // inversion du yaw
		}

		return rot;
	}

	static RE::BSTimer* BSTimerGetSingleton() noexcept
	{
		REL::Relocation<RE::BSTimer*> singleton{ RELOCATION_ID(523657, 410196) };
		return singleton.get();
	}

	static void SetGlobalTimeMultiplier(float a_multiplier, bool a_arg2)
	{
		using func_t = void(*)(RE::BSTimer*, float, bool);
		REL::Relocation<func_t> func{ RELOCATION_ID(66988, 68245) };
		return func(BSTimerGetSingleton(), a_multiplier, a_arg2);
	}
	
	static void PlayArtObject(RE::TESObjectREFR* a_target, RE::BGSArtObject* a_artObject, float a_lifetime, RE::TESObjectREFR* a_facingObject = nullptr, bool a_bAttachToCamera = false, bool a_bInheritRotation = false, void* unk01 = nullptr, void* unk02 = nullptr)
	{
		using func_t = decltype(&PlayArtObject);
		REL::Relocation<func_t> func{ RELOCATION_ID(22289, 22769) };
		return func(a_target, a_artObject, a_lifetime, a_facingObject, a_bAttachToCamera, a_bInheritRotation, unk01, unk02);
	}

	static RE::TESObjectREFR* PlaceAtMe(RE::TESObjectREFR* self, RE::TESForm* a_form, std::uint32_t count = 1, bool forcePersist = false, bool initiallyDisabled = false)
	{
		using func_t = RE::TESObjectREFR* (RE::BSScript::Internal::VirtualMachine*, RE::VMStackID, RE::TESObjectREFR*, RE::TESForm*, std::uint32_t, bool, bool);
		RE::VMStackID frame = 0;

		REL::Relocation<func_t> func{ RELOCATION_ID(55672, 56203) };
		auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();

		return func(vm, frame, self, a_form, count, forcePersist, initiallyDisabled);
	};

	static RE::TESObjectREFR* PlaceAtMe(RE::TESObjectREFR* ref, RE::TESForm* baseForm, RE::NiPoint3 position, RE::NiPoint3 angle = RE::NiPoint3(), bool forcePersist = false)
	{
		const auto boundObject = baseForm->As<RE::TESBoundObject>();
		if (!boundObject || !ref) return nullptr;

		const auto handle = RE::TESDataHandler::GetSingleton()->CreateReferenceAtLocation(boundObject, position, angle, ref->GetParentCell(), ref->GetWorldspace(), nullptr, nullptr, RE::ObjectRefHandle(), forcePersist, true);
		const auto handlePtr = handle.get();
		return (handlePtr && handlePtr.get() ? handlePtr.get() : nullptr);
	};

	static int soundHelper_a(void* manager, RE::BSSoundHandle* a2, int a3, int a4)
	{
		using func_t = decltype(&soundHelper_a);
		REL::Relocation<func_t> func{ RELOCATION_ID(66401, 67663) };
		return func(manager, a2, a3, a4);
	}

	static void soundHelper_b(RE::BSSoundHandle* a1, RE::NiAVObject* source_node)
	{
		using func_t = decltype(&soundHelper_b);
		REL::Relocation<func_t> func{ RELOCATION_ID(66375, 67636) };
		return func(a1, source_node);
	}

	static char __fastcall soundHelper_c(RE::BSSoundHandle* a1)
	{
		using func_t = decltype(&soundHelper_c);
		REL::Relocation<func_t> func{ RELOCATION_ID(66355, 67616) };
		return func(a1);
	}

	static char set_sound_position(RE::BSSoundHandle* a1, float x, float y, float z)
	{
		using func_t = decltype(&set_sound_position);
		REL::Relocation<func_t> func{ RELOCATION_ID(66370, 67631) };
		return func(a1, x, y, z);
	}

	/*Play sound with formid at a certain actor's position.
	@param a: actor on which to play sonud.
	@param formid: formid of the sound descriptor.*/
	static void PlaySound(RE::Actor* a, RE::BGSSoundDescriptorForm* a_descriptor)
	{
		RE::BSSoundHandle handle;
		handle.soundID = static_cast<uint32_t>(-1);
		handle.assumeSuccess = false;
		*(uint32_t*)&handle.state = 0;


		soundHelper_a(RE::BSAudioManager::GetSingleton(), &handle, a_descriptor->GetFormID(), 16);
		if (set_sound_position(&handle, a->data.location.x, a->data.location.y, a->data.location.z)) {
			soundHelper_b(&handle, a->Get3D());
			soundHelper_c(&handle);
		}
	}

	static float GetRandomNumber(float min = 0.0f, float max = 1.0f)
	{
		static std::mt19937                   generator(std::random_device{}());
		std::uniform_real_distribution<float> distribution(min, max);
		return distribution(generator);
	}
};
