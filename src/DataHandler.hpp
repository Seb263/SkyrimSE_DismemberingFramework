#pragma once

#include "JSON.h"

#include "API/CIF-API.h"

namespace ModData
{
	class DataHandler
	{
	public:
		static DataHandler* GetSingleton()
		{
			static DataHandler singleton;
			return &singleton;
		}

		static inline bool coreImpactFrameworkEnabled = false;
		static inline bool nextGenDecapitationsEnabled = false;

		struct ImpulseData
		{
			RE::NiPoint3 fromPosition;
			RE::NiPoint3 toPosition;
			float magnitude;
		};

		struct LimbData
		{
			RE::BSFixedString  nodeName;
			RE::TESObjectREFR* linkedLimb;
			RE::Actor*         linkedActor;
			RE::BGSArtObject*  limbArtObject;
			RE::TESObjectREFR* droppedItem;
			ImpulseData        impulse;
		};
		std::unordered_map<RE::FormID, LimbData> limbDataMap;

		enum class ActorType
		{
			kPlayer,
			kNPC,
			kOther,
			kNone
		};

		enum class WeaponType
		{
			kNone,
			kHandToHand,
			kOneHandSword,
			kTwoHandSword,
			kOneHandAxe,
			kTwoHandAxe,
			kOneHandMace,
			kTwoHandMace,
			kDagger,
			kRanged,
			kBeast,
			kOther
		};

		const std::string_view PLUGIN_NAME = "Dismembering Framework.esm";
		RE::TESDataHandler*    TESdataHandler;
		static inline auto NonCollidableLayer = RE::COL_LAYER::kUnused3; // 49

		// Properties storing game form references
		RE::BGSKeyword*   limbKeyword;
		RE::BGSKeyword*   MagicSummonUndead;
		RE::SpellItem*    DF_Humanoid_Decapitate_Spell;

		struct PluginForm
		{
			std::string_view name;
			void**           formPtr;
			uint32_t         formID;
			std::string_view pluginName;
			bool             optional = false;
		};

		const std::vector<PluginForm> pluginForms = {
			{ "limbKeyword", reinterpret_cast<void**>(&limbKeyword), 0x802, PLUGIN_NAME },
			{ "MagicSummonUndead", reinterpret_cast<void**>(&MagicSummonUndead), 0x2482B, "Skyrim.esm" },
			{ "DF_Humanoid_Decapitate_Spell", reinterpret_cast<void**>(&DF_Humanoid_Decapitate_Spell), 0x876, "DF - Official Humanoid Pack.esp", true }
		};

		enum class PluginGlobal
		{
			ModStatus,
			Features_PlayerEffect_Status,
			Features_PlayerEffect_LatestOnly,
			Features_PlayerEffect_Chances,
			Features_BloodSpray_Mode,
			Features_Misc_DismemberSFX,
			Cond_Global_Player_CanBeDismembered,
			Cond_Global_Player_CanDismember,
			Cond_Global_Player_CanDismember_IfFatal,
			Cond_Global_Player_CanDismember_IfDead,
			Cond_Global_Player_CanDismember_IfKillmove,
			Cond_Global_NPC_CanBeDismembered,
			Cond_Global_NPC_CanDismember,
			Cond_Global_NPC_CanDismember_IfFatal,
			Cond_Global_NPC_CanDismember_IfDead,
			Cond_Global_NPC_CanDismember_IfKillmove,
			Cond_Global_Other_CanBeDismembered,
			Cond_Global_Other_CanDismember,
			Cond_Global_Other_CanDismember_IfFatal,
			Cond_Global_Other_CanDismember_IfDead,
			Cond_Global_Other_CanDismember_IfKillmove,
			Cond_WeapType_HandToHand_CanDismember,
			Cond_WeapType_HandToHand_RegularAttackChance,
			Cond_WeapType_HandToHand_PowerAttackChance,
			Cond_WeapType_HandToHand_Precision,
			Cond_WeapType_HandToHand_Multiple,
			Cond_WeapType_HandToHand_IfHeavy,
			Cond_WeapType_HandToHand_IfBlocked,
			Cond_WeapType_HandToHand_Impulse,
			Cond_WeapType_OneHandSword_CanDismember,
			Cond_WeapType_OneHandSword_RegularAttackChance,
			Cond_WeapType_OneHandSword_PowerAttackChance,
			Cond_WeapType_OneHandSword_Precision,
			Cond_WeapType_OneHandSword_Multiple,
			Cond_WeapType_OneHandSword_IfHeavy,
			Cond_WeapType_OneHandSword_IfBlocked,
			Cond_WeapType_OneHandSword_Impulse,
			Cond_WeapType_TwoHandSword_CanDismember,
			Cond_WeapType_TwoHandSword_RegularAttackChance,
			Cond_WeapType_TwoHandSword_PowerAttackChance,
			Cond_WeapType_TwoHandSword_Precision,
			Cond_WeapType_TwoHandSword_Multiple,
			Cond_WeapType_TwoHandSword_IfHeavy,
			Cond_WeapType_TwoHandSword_IfBlocked,
			Cond_WeapType_TwoHandSword_Impulse,
			Cond_WeapType_OneHandAxe_CanDismember,
			Cond_WeapType_OneHandAxe_RegularAttackChance,
			Cond_WeapType_OneHandAxe_PowerAttackChance,
			Cond_WeapType_OneHandAxe_Precision,
			Cond_WeapType_OneHandAxe_Multiple,
			Cond_WeapType_OneHandAxe_IfHeavy,
			Cond_WeapType_OneHandAxe_IfBlocked,
			Cond_WeapType_OneHandAxe_Impulse,
			Cond_WeapType_TwoHandAxe_CanDismember,
			Cond_WeapType_TwoHandAxe_RegularAttackChance,
			Cond_WeapType_TwoHandAxe_PowerAttackChance,
			Cond_WeapType_TwoHandAxe_Precision,
			Cond_WeapType_TwoHandAxe_Multiple,
			Cond_WeapType_TwoHandAxe_IfHeavy,
			Cond_WeapType_TwoHandAxe_IfBlocked,
			Cond_WeapType_TwoHandAxe_Impulse,
			Cond_WeapType_OneHandMace_CanDismember,
			Cond_WeapType_OneHandMace_RegularAttackChance,
			Cond_WeapType_OneHandMace_PowerAttackChance,
			Cond_WeapType_OneHandMace_Precision,
			Cond_WeapType_OneHandMace_Multiple,
			Cond_WeapType_OneHandMace_IfHeavy,
			Cond_WeapType_OneHandMace_IfBlocked,
			Cond_WeapType_OneHandMace_Impulse,
			Cond_WeapType_TwoHandMace_CanDismember,
			Cond_WeapType_TwoHandMace_RegularAttackChance,
			Cond_WeapType_TwoHandMace_PowerAttackChance,
			Cond_WeapType_TwoHandMace_Precision,
			Cond_WeapType_TwoHandMace_Multiple,
			Cond_WeapType_TwoHandMace_IfHeavy,
			Cond_WeapType_TwoHandMace_IfBlocked,
			Cond_WeapType_TwoHandMace_Impulse,
			Cond_WeapType_Dagger_CanDismember,
			Cond_WeapType_Dagger_RegularAttackChance,
			Cond_WeapType_Dagger_PowerAttackChance,
			Cond_WeapType_Dagger_Precision,
			Cond_WeapType_Dagger_Multiple,
			Cond_WeapType_Dagger_IfHeavy,
			Cond_WeapType_Dagger_IfBlocked,
			Cond_WeapType_Dagger_Impulse,
			Cond_WeapType_Ranged_CanDismember,
			Cond_WeapType_Ranged_RegularAttackChance,
			Cond_WeapType_Ranged_Precision,
			Cond_WeapType_Ranged_Multiple,
			Cond_WeapType_Ranged_IfHeavy,
			Cond_WeapType_Ranged_IfBlocked,
			Cond_WeapType_Ranged_Impulse,
			Cond_WeapType_Beast_CanDismember,
			Cond_WeapType_Beast_RegularAttackChance,
			Cond_WeapType_Beast_PowerAttackChance,
			Cond_WeapType_Beast_Precision,
			Cond_WeapType_Beast_Multiple,
			Cond_WeapType_Beast_IfHeavy,
			Cond_WeapType_Beast_IfBlocked,
			Cond_WeapType_Beast_Impulse,
			Cond_WeapType_Other_CanDismember,
			Cond_WeapType_Other_RegularAttackChance,
			Cond_WeapType_Other_Precision,
			Cond_WeapType_Other_Multiple,
			Cond_WeapType_Other_IfHeavy,
			Cond_WeapType_Other_IfBlocked,
			Cond_WeapType_Other_Impulse,
			Misc_AllowVanillaBeheading,
			Misc_PreventFullHelmetBeheading,
			Misc_PlayArtObjectOnLimbs,
			COUNT
		};

		struct GlobalMapping
		{
			PluginGlobal global_enum;
			const char*  editor_id;
		};

		static inline constexpr const char* GetPluginGlobalEditorID(PluginGlobal global) {
			switch (global) {
				case PluginGlobal::ModStatus: return "DF_ModStatus_GV";
				case PluginGlobal::Features_PlayerEffect_Status: return "DF_Features_PlayerEffect_Status_GV";
				case PluginGlobal::Features_PlayerEffect_LatestOnly: return "DF_Features_PlayerEffect_LatestOnly_GV";
				case PluginGlobal::Features_PlayerEffect_Chances: return "DF_Features_PlayerEffect_Chances_GV";
				case PluginGlobal::Features_BloodSpray_Mode: return "DF_Features_BloodSpray_Mode_GV";
				case PluginGlobal::Features_Misc_DismemberSFX: return "DF_Features_Misc_DismemberSFX_GV";
				case PluginGlobal::Cond_Global_Player_CanBeDismembered: return "DF_Cond_Global_Player_CanBeDismembered_GV";
				case PluginGlobal::Cond_Global_Player_CanDismember: return "DF_Cond_Global_Player_CanDismember_GV";
				case PluginGlobal::Cond_Global_Player_CanDismember_IfFatal: return "DF_Cond_Global_Player_CanDismember_IfFatal_GV";
				case PluginGlobal::Cond_Global_Player_CanDismember_IfDead: return "DF_Cond_Global_Player_CanDismember_IfDead_GV";
				case PluginGlobal::Cond_Global_Player_CanDismember_IfKillmove: return "DF_Cond_Global_Player_CanDismember_IfKillmove_GV";
				case PluginGlobal::Cond_Global_NPC_CanBeDismembered: return "DF_Cond_Global_NPC_CanBeDismembered_GV";
				case PluginGlobal::Cond_Global_NPC_CanDismember: return "DF_Cond_Global_NPC_CanDismember_GV";
				case PluginGlobal::Cond_Global_NPC_CanDismember_IfFatal: return "DF_Cond_Global_NPC_CanDismember_IfFatal_GV";
				case PluginGlobal::Cond_Global_NPC_CanDismember_IfDead: return "DF_Cond_Global_NPC_CanDismember_IfDead_GV";
				case PluginGlobal::Cond_Global_NPC_CanDismember_IfKillmove: return "DF_Cond_Global_NPC_CanDismember_IfKillmove_GV";
				case PluginGlobal::Cond_Global_Other_CanBeDismembered: return "DF_Cond_Global_Other_CanBeDismembered_GV";
				case PluginGlobal::Cond_Global_Other_CanDismember: return "DF_Cond_Global_Other_CanDismember_GV";
				case PluginGlobal::Cond_Global_Other_CanDismember_IfFatal: return "DF_Cond_Global_Other_CanDismember_IfFatal_GV";
				case PluginGlobal::Cond_Global_Other_CanDismember_IfDead: return "DF_Cond_Global_Other_CanDismember_IfDead_GV";
				case PluginGlobal::Cond_Global_Other_CanDismember_IfKillmove: return "DF_Cond_Global_Other_CanDismember_IfKillmove_GV";
				case PluginGlobal::Cond_WeapType_HandToHand_CanDismember: return "DF_Cond_WeapType_HandToHand_CanDismember_GV";
				case PluginGlobal::Cond_WeapType_HandToHand_RegularAttackChance: return "DF_Cond_WeapType_HandToHand_RegularAttackChance_GV";
				case PluginGlobal::Cond_WeapType_HandToHand_PowerAttackChance: return "DF_Cond_WeapType_HandToHand_PowerAttackChance_GV";
				case PluginGlobal::Cond_WeapType_HandToHand_Precision: return "DF_Cond_WeapType_HandToHand_Precision_GV";
				case PluginGlobal::Cond_WeapType_HandToHand_Multiple: return "DF_Cond_WeapType_HandToHand_Multiple_GV";
				case PluginGlobal::Cond_WeapType_HandToHand_IfHeavy: return "DF_Cond_WeapType_HandToHand_IfHeavy_GV";
				case PluginGlobal::Cond_WeapType_HandToHand_IfBlocked: return "DF_Cond_WeapType_HandToHand_IfBlocked_GV";
				case PluginGlobal::Cond_WeapType_HandToHand_Impulse: return "DF_Cond_WeapType_HandToHand_Impulse_GV";
				case PluginGlobal::Cond_WeapType_OneHandSword_CanDismember: return "DF_Cond_WeapType_OneHandSword_CanDismember_GV";
				case PluginGlobal::Cond_WeapType_OneHandSword_RegularAttackChance: return "DF_Cond_WeapType_OneHandSword_RegularAttackChance_GV";
				case PluginGlobal::Cond_WeapType_OneHandSword_PowerAttackChance: return "DF_Cond_WeapType_OneHandSword_PowerAttackChance_GV";
				case PluginGlobal::Cond_WeapType_OneHandSword_Precision: return "DF_Cond_WeapType_OneHandSword_Precision_GV";
				case PluginGlobal::Cond_WeapType_OneHandSword_Multiple: return "DF_Cond_WeapType_OneHandSword_Multiple_GV";
				case PluginGlobal::Cond_WeapType_OneHandSword_IfHeavy: return "DF_Cond_WeapType_OneHandSword_IfHeavy_GV";
				case PluginGlobal::Cond_WeapType_OneHandSword_IfBlocked: return "DF_Cond_WeapType_OneHandSword_IfBlocked_GV";
				case PluginGlobal::Cond_WeapType_OneHandSword_Impulse: return "DF_Cond_WeapType_OneHandSword_Impulse_GV";
				case PluginGlobal::Cond_WeapType_TwoHandSword_CanDismember: return "DF_Cond_WeapType_TwoHandSword_CanDismember_GV";
				case PluginGlobal::Cond_WeapType_TwoHandSword_RegularAttackChance: return "DF_Cond_WeapType_TwoHandSword_RegularAttackChance_GV";
				case PluginGlobal::Cond_WeapType_TwoHandSword_PowerAttackChance: return "DF_Cond_WeapType_TwoHandSword_PowerAttackChance_GV";
				case PluginGlobal::Cond_WeapType_TwoHandSword_Precision: return "DF_Cond_WeapType_TwoHandSword_Precision_GV";
				case PluginGlobal::Cond_WeapType_TwoHandSword_Multiple: return "DF_Cond_WeapType_TwoHandSword_Multiple_GV";
				case PluginGlobal::Cond_WeapType_TwoHandSword_IfHeavy: return "DF_Cond_WeapType_TwoHandSword_IfHeavy_GV";
				case PluginGlobal::Cond_WeapType_TwoHandSword_IfBlocked: return "DF_Cond_WeapType_TwoHandSword_IfBlocked_GV";
				case PluginGlobal::Cond_WeapType_TwoHandSword_Impulse: return "DF_Cond_WeapType_TwoHandSword_Impulse_GV";
				case PluginGlobal::Cond_WeapType_OneHandAxe_CanDismember: return "DF_Cond_WeapType_OneHandAxe_CanDismember_GV";
				case PluginGlobal::Cond_WeapType_OneHandAxe_RegularAttackChance: return "DF_Cond_WeapType_OneHandAxe_RegularAttackChance_GV";
				case PluginGlobal::Cond_WeapType_OneHandAxe_PowerAttackChance: return "DF_Cond_WeapType_OneHandAxe_PowerAttackChance_GV";
				case PluginGlobal::Cond_WeapType_OneHandAxe_Precision: return "DF_Cond_WeapType_OneHandAxe_Precision_GV";
				case PluginGlobal::Cond_WeapType_OneHandAxe_Multiple: return "DF_Cond_WeapType_OneHandAxe_Multiple_GV";
				case PluginGlobal::Cond_WeapType_OneHandAxe_IfHeavy: return "DF_Cond_WeapType_OneHandAxe_IfHeavy_GV";
				case PluginGlobal::Cond_WeapType_OneHandAxe_IfBlocked: return "DF_Cond_WeapType_OneHandAxe_IfBlocked_GV";
				case PluginGlobal::Cond_WeapType_OneHandAxe_Impulse: return "DF_Cond_WeapType_OneHandAxe_Impulse_GV";
				case PluginGlobal::Cond_WeapType_TwoHandAxe_CanDismember: return "DF_Cond_WeapType_TwoHandAxe_CanDismember_GV";
				case PluginGlobal::Cond_WeapType_TwoHandAxe_RegularAttackChance: return "DF_Cond_WeapType_TwoHandAxe_RegularAttackChance_GV";
				case PluginGlobal::Cond_WeapType_TwoHandAxe_PowerAttackChance: return "DF_Cond_WeapType_TwoHandAxe_PowerAttackChance_GV";
				case PluginGlobal::Cond_WeapType_TwoHandAxe_Precision: return "DF_Cond_WeapType_TwoHandAxe_Precision_GV";
				case PluginGlobal::Cond_WeapType_TwoHandAxe_Multiple: return "DF_Cond_WeapType_TwoHandAxe_Multiple_GV";
				case PluginGlobal::Cond_WeapType_TwoHandAxe_IfHeavy: return "DF_Cond_WeapType_TwoHandAxe_IfHeavy_GV";
				case PluginGlobal::Cond_WeapType_TwoHandAxe_IfBlocked: return "DF_Cond_WeapType_TwoHandAxe_IfBlocked_GV";
				case PluginGlobal::Cond_WeapType_TwoHandAxe_Impulse: return "DF_Cond_WeapType_TwoHandAxe_Impulse_GV";
				case PluginGlobal::Cond_WeapType_OneHandMace_CanDismember: return "DF_Cond_WeapType_OneHandMace_CanDismember_GV";
				case PluginGlobal::Cond_WeapType_OneHandMace_RegularAttackChance: return "DF_Cond_WeapType_OneHandMace_RegularAttackChance_GV";
				case PluginGlobal::Cond_WeapType_OneHandMace_PowerAttackChance: return "DF_Cond_WeapType_OneHandMace_PowerAttackChance_GV";
				case PluginGlobal::Cond_WeapType_OneHandMace_Precision: return "DF_Cond_WeapType_OneHandMace_Precision_GV";
				case PluginGlobal::Cond_WeapType_OneHandMace_Multiple: return "DF_Cond_WeapType_OneHandMace_Multiple_GV";
				case PluginGlobal::Cond_WeapType_OneHandMace_IfHeavy: return "DF_Cond_WeapType_OneHandMace_IfHeavy_GV";
				case PluginGlobal::Cond_WeapType_OneHandMace_IfBlocked: return "DF_Cond_WeapType_OneHandMace_IfBlocked_GV";
				case PluginGlobal::Cond_WeapType_OneHandMace_Impulse: return "DF_Cond_WeapType_OneHandMace_Impulse_GV";
				case PluginGlobal::Cond_WeapType_TwoHandMace_CanDismember: return "DF_Cond_WeapType_TwoHandMace_CanDismember_GV";
				case PluginGlobal::Cond_WeapType_TwoHandMace_RegularAttackChance: return "DF_Cond_WeapType_TwoHandMace_RegularAttackChance_GV";
				case PluginGlobal::Cond_WeapType_TwoHandMace_PowerAttackChance: return "DF_Cond_WeapType_TwoHandMace_PowerAttackChance_GV";
				case PluginGlobal::Cond_WeapType_TwoHandMace_Precision: return "DF_Cond_WeapType_TwoHandMace_Precision_GV";
				case PluginGlobal::Cond_WeapType_TwoHandMace_Multiple: return "DF_Cond_WeapType_TwoHandMace_Multiple_GV";
				case PluginGlobal::Cond_WeapType_TwoHandMace_IfHeavy: return "DF_Cond_WeapType_TwoHandMace_IfHeavy_GV";
				case PluginGlobal::Cond_WeapType_TwoHandMace_IfBlocked: return "DF_Cond_WeapType_TwoHandMace_IfBlocked_GV";
				case PluginGlobal::Cond_WeapType_TwoHandMace_Impulse: return "DF_Cond_WeapType_TwoHandMace_Impulse_GV";
				case PluginGlobal::Cond_WeapType_Dagger_CanDismember: return "DF_Cond_WeapType_Dagger_CanDismember_GV";
				case PluginGlobal::Cond_WeapType_Dagger_RegularAttackChance: return "DF_Cond_WeapType_Dagger_RegularAttackChance_GV";
				case PluginGlobal::Cond_WeapType_Dagger_PowerAttackChance: return "DF_Cond_WeapType_Dagger_PowerAttackChance_GV";
				case PluginGlobal::Cond_WeapType_Dagger_Precision: return "DF_Cond_WeapType_Dagger_Precision_GV";
				case PluginGlobal::Cond_WeapType_Dagger_Multiple: return "DF_Cond_WeapType_Dagger_Multiple_GV";
				case PluginGlobal::Cond_WeapType_Dagger_IfHeavy: return "DF_Cond_WeapType_Dagger_IfHeavy_GV";
				case PluginGlobal::Cond_WeapType_Dagger_IfBlocked: return "DF_Cond_WeapType_Dagger_IfBlocked_GV";
				case PluginGlobal::Cond_WeapType_Dagger_Impulse: return "DF_Cond_WeapType_Dagger_Impulse_GV";
				case PluginGlobal::Cond_WeapType_Ranged_CanDismember: return "DF_Cond_WeapType_Ranged_CanDismember_GV";
				case PluginGlobal::Cond_WeapType_Ranged_RegularAttackChance: return "DF_Cond_WeapType_Ranged_RegularAttackChance_GV";
				case PluginGlobal::Cond_WeapType_Ranged_Precision: return "DF_Cond_WeapType_Ranged_Precision_GV";
				case PluginGlobal::Cond_WeapType_Ranged_Multiple: return "DF_Cond_WeapType_Ranged_Multiple_GV";
				case PluginGlobal::Cond_WeapType_Ranged_IfHeavy: return "DF_Cond_WeapType_Ranged_IfHeavy_GV";
				case PluginGlobal::Cond_WeapType_Ranged_IfBlocked: return "DF_Cond_WeapType_Ranged_IfBlocked_GV";
				case PluginGlobal::Cond_WeapType_Ranged_Impulse: return "DF_Cond_WeapType_Ranged_Impulse_GV";
				case PluginGlobal::Cond_WeapType_Beast_CanDismember: return "DF_Cond_WeapType_Beast_CanDismember_GV";
				case PluginGlobal::Cond_WeapType_Beast_RegularAttackChance: return "DF_Cond_WeapType_Beast_RegularAttackChance_GV";
				case PluginGlobal::Cond_WeapType_Beast_PowerAttackChance: return "DF_Cond_WeapType_Beast_PowerAttackChance_GV";
				case PluginGlobal::Cond_WeapType_Beast_Precision: return "DF_Cond_WeapType_Beast_Precision_GV";
				case PluginGlobal::Cond_WeapType_Beast_Multiple: return "DF_Cond_WeapType_Beast_Multiple_GV";
				case PluginGlobal::Cond_WeapType_Beast_IfHeavy: return "DF_Cond_WeapType_Beast_IfHeavy_GV";
				case PluginGlobal::Cond_WeapType_Beast_IfBlocked: return "DF_Cond_WeapType_Beast_IfBlocked_GV";
				case PluginGlobal::Cond_WeapType_Beast_Impulse: return "DF_Cond_WeapType_Beast_Impulse_GV";
				case PluginGlobal::Cond_WeapType_Other_CanDismember: return "DF_Cond_WeapType_Other_CanDismember_GV";
				case PluginGlobal::Cond_WeapType_Other_RegularAttackChance: return "DF_Cond_WeapType_Other_RegularAttackChance_GV";
				case PluginGlobal::Cond_WeapType_Other_Precision: return "DF_Cond_WeapType_Other_Precision_GV";
				case PluginGlobal::Cond_WeapType_Other_Multiple: return "DF_Cond_WeapType_Other_Multiple_GV";
				case PluginGlobal::Cond_WeapType_Other_IfHeavy: return "DF_Cond_WeapType_Other_IfHeavy_GV";
				case PluginGlobal::Cond_WeapType_Other_IfBlocked: return "DF_Cond_WeapType_Other_IfBlocked_GV";
				case PluginGlobal::Cond_WeapType_Other_Impulse: return "DF_Cond_WeapType_Other_Impulse_GV";
				case PluginGlobal::Misc_AllowVanillaBeheading: return "DF_Misc_AllowVanillaBeheading_GV";
				case PluginGlobal::Misc_PreventFullHelmetBeheading: return "DF_Misc_PreventFullHelmetBeheading_GV";
				case PluginGlobal::Misc_PlayArtObjectOnLimbs: return "DF_Misc_PlayArtObjectOnLimbs_GV";
				default: return "";
			}
		}
		static inline std::unordered_map<PluginGlobal, RE::TESGlobal*> pluginGlobalPointers = {};

		std::unordered_map<std::string, RE::TESGlobal*> pluginGlobalVariables;
		std::unordered_map<RE::Actor*, RE::HitData> deferredHitMap;
		RE::BGSCollisionLayer* ModRuntimeBlood_CollisionLayer;

		void LoadData();
		void ProcessBloodCollisionLayer();
	};

	inline void DataHandler::LoadData()
	{
        logger::info("Loading data...");
		TESdataHandler = RE::TESDataHandler::GetSingleton();

		for (const auto& formInfo : pluginForms) {
			*formInfo.formPtr = TESdataHandler->LookupForm(formInfo.formID, formInfo.pluginName.data());
			if (!*formInfo.formPtr && !formInfo.optional) {
				if (formInfo.pluginName == PLUGIN_NAME) {
					REPORT_AND_FAIL(
						"ERROR: The required plugin \"{}\" is missing! This means the mod is either not installed correctly or your mod manager failed to enable it.\n"
						"If you believe you installed the mod properly, please redo the manual installation without using a mod manager.\n\n"
						"This is NOT a bug - DO NOT report it! Instructions for manual installation are available on the mod's page.\n\n"
						"DETAILS: Form \"{}\" not found in \"{}\".",
						formInfo.pluginName, formInfo.name, formInfo.pluginName);
				} else {
					REPORT_AND_FAIL("ERROR: Form \"{}\" not found in \"{}\".", formInfo.pluginName, formInfo.name, formInfo.pluginName);
				}
			}
		}

		static std::array<PluginGlobal, static_cast<size_t>(PluginGlobal::COUNT)> all_globals = {};
		for (size_t i = 0; i < static_cast<size_t>(PluginGlobal::COUNT); ++i) {
			all_globals[i] = static_cast<PluginGlobal>(i); // Remplir avec les valeurs de l'énumération
		}

		// Vérifier et initialiser les global variables
		for (const auto& global_enum : all_globals) {
			const char* editor_id = GetPluginGlobalEditorID(global_enum);
			auto form = RE::TESForm::LookupByEditorID<RE::TESGlobal>(editor_id);
			if (!form || !form->formID) {
				REPORT_AND_FAIL("Error: Global Variable \"{}\" not found.", editor_id);
			} else {
				pluginGlobalPointers[global_enum] = form;
			}
		}

		logger::info("Loading data: DONE");
	}
	
	inline void DataHandler::ProcessBloodCollisionLayer()
	{
		const auto& dataHandler = ModData::DataHandler::GetSingleton();

		if (!ModData::DataHandler::nextGenDecapitationsEnabled) return;
		
		RE::BGSCollisionLayer* layer = CoreImpactFrameworkAPI::g_API->GetBloodCollisionLayer();
		dataHandler->ModRuntimeBlood_CollisionLayer = (layer ? layer : nullptr);
	}
}
