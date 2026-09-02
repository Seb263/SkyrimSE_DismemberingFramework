#pragma once

/*******************************************************************
* CORE IMPACT FRAMEWORK - API
* Do not forget to include this source file to your project!
*******************************************************************/

/* How to create a hook to the API and use it:
SKSE::GetMessagingInterface()->RegisterListener([](MessagingInterface::Message* message) 
{
    switch (message->type) 
    {
        case MessagingInterface::kPostLoadGame:
        case MessagingInterface::kNewGame:
        {
            if (!CoreImpactFrameworkAPI::LoadAPI()) {
				util::report_and_fail("Failed to bound to the Core Impact Framework API");
			}
			CoreImpactFrameworkAPI::g_API->GetVersion();
        }
        break;
    }
});
*/

// Define the API type key
#define CIF_API_TYPE_KEY static_cast<uint32_t>(std::byteswap('CIF'))

// Define the API version in a structured format
#define CIF_API_VERSION_MAJOR 1
#define CIF_API_VERSION_MINOR 0
#define CIF_API_VERSION_PATCH 1

// Combine the version numbers into a single value
#define CIF_API_VERSION ((CIF_API_VERSION_MAJOR << 16) | (CIF_API_VERSION_MINOR << 8) | CIF_API_VERSION_PATCH)

namespace CoreImpactFrameworkAPI
{
	using BipedBonesMap = std::optional<std::unordered_map<int, std::vector<std::string>>>;

	class CoreImpactFrameworkAPI
	{
	public:
		// API functions
		virtual size_t GetAPIVersion() const;
		
		virtual std::vector<uint32_t> GetVersion() const;

		virtual BipedBonesMap GetBipedBonesMap(RE::Actor* actor) const;

		virtual std::variant<bool, int, float, std::string> GetIniValue(const std::string& key_section) const;
		
		virtual bool UpdateIniValue(const std::string& key_section, const std::variant<bool, int, float, std::string>& value) const;
		
		virtual RE::BGSCollisionLayer* GetBloodCollisionLayer() const;
	};

	// Global API pointer
	inline extern CoreImpactFrameworkAPI* g_API = nullptr;

	// Call this function only after the kDataLoaded event
	inline bool LoadAPI()
	{
		if (g_API != nullptr) return true;
		SKSE::GetMessagingInterface()->Dispatch(CIF_API_TYPE_KEY, (void*)&g_API, sizeof(void*), NULL);
		if (g_API) { // API successfully received!
			// Check if the API version matches
			return (g_API->GetAPIVersion() == CIF_API_VERSION);
		}
		return false;
	}
}
