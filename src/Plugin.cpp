#include "SettingsIni.hpp"
#include "DataHandler.hpp"
#include "Events.h"
#include "Main.h"
#include "Papyrus.hpp"
#include "Serialization.hpp"
#include "API.h"

#include "API/CIF-API.h"
#include "API/NGD-API.h"

ModData::JSONHandler* JSONhandler;

static inline bool postLoadEventsLoaded = false;

static void PostLoadEvents()
{
	if (postLoadEventsLoaded) return;
	postLoadEventsLoaded = true;

	if (NGDecapitationsAPI::LoadAPI()) {
		ModData::DataHandler::nextGenDecapitationsEnabled = true;
		logger::info("Successfully registered Next-Gen Decapitations API.");
	}
	if (CoreImpactFrameworkAPI::LoadAPI()) {
		ModData::DataHandler::coreImpactFrameworkEnabled = true;
		ModData::DataHandler::GetSingleton()->ProcessBloodCollisionLayer();
		logger::info("Successfully registered Core Impact Framework API.");
	}

	JSONhandler->WaitUntilReady();
};

static void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
{
	auto postLoadEventsAlternate = []() {
		std::jthread([]() {
			while (!postLoadEventsLoaded) {
				static std::atomic_bool taskRunning = false;
				if (!taskRunning.exchange(true)) {
					SKSE::GetTaskInterface()->AddTask([]() {
						auto player = RE::PlayerCharacter::GetSingleton();
						if (player && player->Is3DLoaded() && player->GetParentCell() && player->GetParentCell()->IsAttached()) {
							PostLoadEvents();
						}
						taskRunning = false;
					});
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}).detach();
	};

	switch (a_msg->type) {
	case SKSE::MessagingInterface::kPostLoad:
		if (!SKSE::GetMessagingInterface()->RegisterListener(NULL, [](SKSE::MessagingInterface::Message* message) {
			switch (message->type) {
			case DF_API_TYPE_KEY:
				message->dataLen = sizeof(DismemberingFrameworkAPI::DismemberingFrameworkAPI*);
				*(DismemberingFrameworkAPI::DismemberingFrameworkAPI**)message->data = DismemberingFrameworkAPI::g_API;
				break;
			}
		})) REPORT_AND_FAIL("Unable to register API message listener.");
		else logger::info("Successfully registered API message listener.");
		break;

	case SKSE::MessagingInterface::kDataLoaded:
		ModData::DataHandler::GetSingleton()->LoadData();
		ModData::Serialization::RegisterSerializationCallbacks();
		Events::ModEventSink::LoadEvents();
		Events::MainEvent::InstallHitHook();
		JSONhandler->Load(); // Load JSON data
		if (!DismemberingFrameworkAPI::g_API) DismemberingFrameworkAPI::g_API = new DismemberingFrameworkAPI::DismemberingFrameworkAPI;
		postLoadEventsAlternate();
		break;

	case SKSE::MessagingInterface::kPostLoadGame:
	case SKSE::MessagingInterface::kNewGame:
		PostLoadEvents();
		break;
	}
}

static void InitializeLog(std::string_view pluginName, spdlog::level::level_enum a_level = spdlog::level::info)
{
	auto path = logger::log_directory();
	if (!path) REPORT_AND_FAIL("Failed to find standard logging directory.");

	*path /= std::format("{}.log", pluginName);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

	const auto level = a_level;

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
	log->set_level(level);
	log->flush_on(spdlog::level::info);

	spdlog::set_default_logger(std::move(log));
	if (level == spdlog::level::trace) spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");
	else spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	const auto plugin{ SKSE::PluginDeclaration::GetSingleton() };
	const auto name{ plugin->GetName() };
	const auto version{ plugin->GetVersion() };

	SKSE::Init(a_skse);

	 if (!SettingsIni::ReadSettings()) {
		InitializeLog(name, spdlog::level::info);
		logger::warn("Failed to load settings file. Default settings will be used.");
	} else {
		if (SettingsIni::iVerboseMode <= 0) {
			InitializeLog(name, spdlog::level::err);
		} else if (SettingsIni::iVerboseMode >= 2) {
			InitializeLog(name, spdlog::level::trace);
		} else {
			InitializeLog(name, spdlog::level::info);
		}
	}

	logger::info("{} v{} by Seb263 : Loaded - Game version : {}", name, version.string("."), REL::Module::get().version().string("."));

	JSONhandler = ModData::JSONHandler::GetSingleton();

	auto g_message = SKSE::GetMessagingInterface();
	if (!g_message) REPORT_AND_FAIL("Messaging Interface not found.");
	else if (!g_message->RegisterListener(MessageHandler)) REPORT_AND_FAIL("Failed to register MessageHandler listener.");
	else logger::info("Successfully registered MessageHandler listener.");

	auto g_papyrus = SKSE::GetPapyrusInterface();
	if (!g_papyrus) REPORT_AND_FAIL("Papyrus Interface not found.");
	else if (!g_papyrus->Register(Papyrus::BindPapyrusFunctions)) REPORT_AND_FAIL("Failed to register Papyrus functions.");
	else logger::info("Successfully registered Papyrus functions.");

	return true;
}
