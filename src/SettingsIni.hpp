#pragma once

class SettingsIni
{
public:
	// General
	static inline int  iVerboseMode = 1;
	static inline bool bAsynchronousStartup = true;
	static inline bool bDeferredHitProcess = true;
	static inline bool bShouldIgnoreMaintenanceChecks = false;

	// Misc
	static inline bool bDismemberedCanBeReanimated = true;
	static inline bool bAllowLimbScaling = true;
	static inline bool bTrueDirectionalMovementFix = true;

	static bool ReadSettings()
	{
		constexpr auto path = L"Data/SKSE/Plugins/DismemberingFramework.ini";

		if (!std::filesystem::exists(path)) return false;

		CSimpleIniA ini;
		ini.SetUnicode();
		SI_Error rc = ini.LoadFile(path);

		if (rc < 0) return false;

		// General
		iVerboseMode = ini.GetLongValue("General", "iVerboseMode", 1);
		bAsynchronousStartup = ini.GetBoolValue("General", "bAsynchronousStartup", true);
		bDeferredHitProcess = ini.GetBoolValue("General", "bDeferredHitProcess", true);
		bShouldIgnoreMaintenanceChecks = ini.GetBoolValue("General", "bShouldIgnoreMaintenanceChecks", false);

		// Misc
		bDismemberedCanBeReanimated = ini.GetBoolValue("Misc", "bDismemberedCanBeReanimated", true);
		bAllowLimbScaling = ini.GetBoolValue("Misc", "bAllowLimbScaling", true);
		bTrueDirectionalMovementFix = ini.GetBoolValue("Misc", "bTrueDirectionalMovementFix", true);

		debugVerboseMode = iVerboseMode;

		return true;
	}
};
