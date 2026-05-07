#include "SettingsDialog.h"

#include <Alert.h>
#include <Box.h>
#include <Button.h>
#include <GroupView.h>
#include <CheckBox.h>
#include <LayoutBuilder.h>
#include <Menu.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <Message.h>
#include <OS.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>

#include <curl/curl.h>

#include <cstdlib>
#include <string>
#include <vector>

#include "../json.h"
#include "../oauth.h"
#include "../util.h"

namespace pluto {
namespace gui {

namespace {

const char* kKeyPlaceholder = "\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2\xe2\x80\xa2";  // "••••••••"

const uint32 kMsgOk            = 'plPo';
const uint32 kMsgCancel        = 'plPc';
const uint32 kMsgModelPreset   = 'plMP';
const uint32 kMsgProfilePick   = 'plPP';
const uint32 kMsgFetchModels   = 'plFM';
const uint32 kMsgFetchDone     = 'plFD';
const uint32 kMsgToggleAdvanced = 'plTA';
const uint32 kMsgColorScheme   = 'plCS';

// ----------------------------------------------------------------
// Provider profiles with default base_url, api_key_env, and model
// presets. The first model in each list is the default.
// ----------------------------------------------------------------

struct ModelEntry {
	const char* label;
	const char* id;
	int         max_tokens;      // suggested max output tokens
	int         context_window;  // model context size
};

struct ProviderProfile {
	const char* label;
	const char* provider_id;    // "anthropic" or "openai-compat"
	const char* base_url;       // empty = not applicable
	const char* api_key_env;    // empty = not required
	ModelEntry  models[14];     // null-terminated
};

// Only Anthropic profiles stay hardcoded (need OAuth vs API key split).
// All openai-compat providers are loaded from data/providers/*.conf files.
const ProviderProfile kProfiles[] = {
	{
		"Anthropic (API key)", "anthropic", "", "ANTHROPIC_API_KEY",
		{
			{ "Opus 4.7",        "claude-opus-4-7",          128000, 1000000 },
			{ "Opus 4.6",        "claude-opus-4-6",          128000, 1000000 },
			{ "Sonnet 4.6",      "claude-sonnet-4-6",        128000, 1000000 },
			{ "Opus 4.5",        "claude-opus-4-5-20251101",  64000,  200000 },
			{ "Haiku 4.5",       "claude-haiku-4-5-20251001", 64000,  200000 },
			{ nullptr, nullptr, 0, 0 }
		}
	},
	{
		"Anthropic (Claude Max/Pro)", "anthropic", "", "OAUTH",
		{
			{ "Opus 4.7",        "claude-opus-4-7",          128000, 1000000 },
			{ "Opus 4.6",        "claude-opus-4-6",          128000, 1000000 },
			{ "Sonnet 4.6",      "claude-sonnet-4-6",        128000, 1000000 },
			{ "Opus 4.5",        "claude-opus-4-5-20251101",  64000,  200000 },
			{ "Haiku 4.5",       "claude-haiku-4-5-20251001", 64000,  200000 },
			{ nullptr, nullptr, 0, 0 }
		}
	},
	// All openai-compat providers are now loaded from .conf files:
	//   data/providers/*.conf           (built-in, shipped with package)
	//   ~/config/settings/pluto/providers/*.conf  (user-defined)
	// Only "Custom OpenAI-compat" stays hardcoded as a blank template.
	{
		"Custom OpenAI-compat", "openai-compat", "", "",
		{
			{ "(enter model ID below)", "", 4096, 128000 },
			{ nullptr, nullptr, 0, 0 }
		}
	},
};

constexpr int kProfileCount = sizeof(kProfiles) / sizeof(kProfiles[0]);

// ----------------------------------------------------------------
// User-defined provider profiles — now loaded via config.h functions.
// load_user_providers() wraps LoadProviderConfs() for local use.
// ----------------------------------------------------------------

std::vector<UserProvider> load_user_providers() {
	return pluto::LoadProviderConfs();
}

// ----------------------------------------------------------------
// Unified profile lookup across built-in + user profiles
// ----------------------------------------------------------------

int find_profile_for_config(const Config& cfg,
                            const std::vector<UserProvider>& user_profiles) {
	// For Anthropic, distinguish API key vs OAuth.
	if (cfg.provider == "anthropic") {
		pluto::OAuthTokens tok = pluto::LoadOAuthTokens();
		for (int i = 0; i < kProfileCount; ++i) {
			if (std::string(kProfiles[i].provider_id) != "anthropic") continue;
			bool is_oauth_profile = (std::string(kProfiles[i].api_key_env) == "OAUTH");
			if (!tok.empty() && is_oauth_profile) return i;
			if (tok.empty() && !is_oauth_profile) return i;
		}
	}
	// Search built-in profiles.
	for (int i = 0; i < kProfileCount; ++i) {
		if (cfg.provider != kProfiles[i].provider_id) continue;
		if (std::string(kProfiles[i].base_url).empty()) {
			if (cfg.provider == "anthropic") return i;
		} else if (cfg.base_url == kProfiles[i].base_url) {
			return i;
		}
	}
	// Search user-defined profiles (indices offset by kProfileCount).
	for (size_t i = 0; i < user_profiles.size(); ++i) {
		if (cfg.base_url == user_profiles[i].base_url)
			return kProfileCount + static_cast<int>(i);
	}
	return kProfileCount - 1;  // Custom
}

}  // namespace

SettingsDialog::SettingsDialog(BMessenger target, const Config& current)
	: BWindow(BRect(0, 0, 580, 560), "Pluto — Preferences",
	          B_TITLED_WINDOW,
	          B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS),
	  fTarget(target) {

	// --- Load user-defined provider profiles ---
	fUserProfiles = load_user_providers();

	// --- Provider selector ---
	auto* providerMenu = new BPopUpMenu("Provider");
	for (int i = 0; i < kProfileCount; ++i) {
		auto* msg = new BMessage(kMsgProfilePick);
		msg->AddInt32("index", i);
		providerMenu->AddItem(new BMenuItem(kProfiles[i].label, msg));
	}
	// Append user-defined profiles after built-in ones (before "Custom").
	// Insert before the last item ("Custom OpenAI-compat").
	int32 insert_at = providerMenu->CountItems() - 1;
	if (!fUserProfiles.empty() && insert_at >= 0)
		providerMenu->AddSeparatorItem();
	for (size_t i = 0; i < fUserProfiles.size(); ++i) {
		auto* msg = new BMessage(kMsgProfilePick);
		msg->AddInt32("index", kProfileCount + static_cast<int32>(i));
		providerMenu->AddItem(new BMenuItem(
			fUserProfiles[i].label.c_str(), msg));
	}
	fProviderField = new BMenuField("Provider:", providerMenu);

	// --- Connection fields ---
	fBaseUrl   = new BTextControl("baseurl", "Base URL:",
		current.base_url.c_str(), nullptr);
	// fApiKeyEnv is kept as a hidden field to store the env var name
	// internally (used for per-provider key storage).
	fApiKeyEnv = new BTextControl("apikey", "", "", nullptr);
	fApiKeyEnv->Hide();
	fApiKeyField = new BTextControl("keyfield", "API key:", "", nullptr);
	fApiKeyField->TextView()->HideTyping(true);
	fKeyStatus = new BStringView("keystatus", "");

	// OAuth login button (shown for Claude Max/Pro profile).
	pluto::OAuthTokens existing = pluto::LoadOAuthTokens();
	fOAuthBtn = new BButton("oauth",
		existing.empty() ? "Login with Claude account"
		                 : "Logged in \xe2\x80\x94 Re-authenticate",
		new BMessage('plOL'));
	fOAuthStatus = new BStringView("oauthStatus", "");
	if (!existing.empty()) {
		fOAuthStatus->SetText("\xe2\x9c\x93 Logged in");
		fOAuthStatus->SetHighColor(30, 140, 70, 255);
	}
	fOAuthBtn->Hide();
	fOAuthStatus->Hide();

	// --- Model ---
	// Hidden text field — stores the selected model ID internally.
	fModel = new BTextControl("model", "", current.model.c_str(), nullptr);
	fModel->Hide();

	auto* modelPresetsMenu = new BPopUpMenu(current.model.c_str());
	fModelPresets = new BMenuField("Model:", modelPresetsMenu);

	fFetchBtn = new BButton("fetch", "Fetch Models",
		new BMessage(kMsgFetchModels));

	// --- Tuning ---
	fMaxTokens = new BTextControl("max_tokens", "Max tokens:",
		std::to_string(current.max_tokens).c_str(), nullptr);
	fContextWin = new BTextControl("ctx", "Context window:",
		std::to_string(current.context_window).c_str(), nullptr);

	// --- Color scheme ---
	auto* colorMenu = new BPopUpMenu("Color scheme");
	struct { const char* label; const char* value; } schemes[] = {
		{ "Auto (system theme)", "auto"  },
		{ "Dark",                "dark"  },
		{ "Light",               "light" },
	};
	for (const auto& s : schemes) {
		auto* m = new BMessage(kMsgColorScheme);
		m->AddString("scheme", s.value);
		auto* item = new BMenuItem(s.label, m);
		if (current.color_scheme == s.value)
			item->SetMarked(true);
		colorMenu->AddItem(item);
	}
	fColorScheme = new BMenuField("Color scheme:", colorMenu);

	fSystem = new BTextView("system");
	fSystem->SetWordWrap(true);
	fSystem->SetText(current.user_system_prompt.c_str());
	fSystem->SetExplicitMinSize(BSize(B_SIZE_UNSET, 100));
	fSysScroll = new BScrollView("sysScroll", fSystem,
		0, false, true, B_FANCY_BORDER);

	fCache = new BCheckBox("cache",
		"Prompt caching (cache_control on system+tools)",
		nullptr);
	fCache->SetValue(current.cache ? B_CONTROL_ON : B_CONTROL_OFF);

	fAdvancedToggle = new BButton("advanced", "Advanced \xe2\x96\xb6",
		new BMessage(kMsgToggleAdvanced));
	fAdvancedToggle->SetExplicitAlignment(
		BAlignment(B_ALIGN_LEFT, B_ALIGN_VERTICAL_CENTER));

	auto* sysLabel = new BStringView("sysLabel",
		"System prompt (prepended to memory and skills):");
	sysLabel->SetToolTip(
		"Custom instruction sent at the start of every conversation.\n"
		"Usually you can leave this empty — pluto automatically\n"
		"injects ~/config/settings/pluto/pluto.md (user memory)\n"
		"and ./pluto.md (project memory) into the system prompt.");

	fAdvancedBox = new BBox("advancedBox");
	fAdvancedBox->SetLabel("Advanced");
	BLayoutBuilder::Group<>(fAdvancedBox, B_VERTICAL, B_USE_SMALL_SPACING)
		.SetInsets(B_USE_DEFAULT_SPACING, B_USE_BIG_SPACING,
		           B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
		.Add(sysLabel)
		.Add(fSysScroll)
		.Add(fCache)
	.End();
	fAdvancedBox->Hide();

	auto* ok     = new BButton("ok",     "OK",     new BMessage(kMsgOk));
	auto* cancel = new BButton("cancel", "Cancel", new BMessage(kMsgCancel));

	// --- Grouped layout (Haiku preference-panel style) ---

	auto* providerBox = new BBox("providerBox");
	providerBox->SetLabel("Provider");
	BLayoutBuilder::Group<>(providerBox, B_VERTICAL, B_USE_SMALL_SPACING)
		.SetInsets(B_USE_DEFAULT_SPACING, B_USE_BIG_SPACING,
		           B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
		.Add(fProviderField)
		.Add(fBaseUrl)
		.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
			.Add(fApiKeyField)
			.Add(fKeyStatus)
		.End()
		.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
			.Add(fOAuthBtn)
			.Add(fOAuthStatus)
		.End()
	.End();

	auto* modelBox = new BBox("modelBox");
	modelBox->SetLabel("Model");
	BLayoutBuilder::Group<>(modelBox, B_VERTICAL, B_USE_SMALL_SPACING)
		.SetInsets(B_USE_DEFAULT_SPACING, B_USE_BIG_SPACING,
		           B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
		.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
			.Add(fModelPresets)
			.Add(fFetchBtn)
		.End()
		.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
			.Add(fMaxTokens)
			.Add(fContextWin)
		.End()
	.End();

	auto* appearBox = new BBox("appearBox");
	appearBox->SetLabel("Appearance");
	BLayoutBuilder::Group<>(appearBox, B_VERTICAL, B_USE_SMALL_SPACING)
		.SetInsets(B_USE_DEFAULT_SPACING, B_USE_BIG_SPACING,
		           B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
		.Add(fColorScheme)
	.End();

	BGroupView* root = new BGroupView(B_VERTICAL, B_USE_HALF_ITEM_SPACING);
	root->SetExplicitMinSize(BSize(560, B_SIZE_UNSET));
	root->SetExplicitMaxSize(BSize(560, B_SIZE_UNLIMITED));

	BLayoutBuilder::Group<>(root)
		.SetInsets(B_USE_WINDOW_SPACING)
		.Add(providerBox)
		.Add(modelBox)
		.Add(appearBox)
		.Add(fAdvancedToggle)
		.Add(fAdvancedBox)
		.AddGlue()
		.AddGroup(B_HORIZONTAL)
			.AddGlue()
			.Add(cancel)
			.Add(ok)
		.End();

	BLayoutBuilder::Group<>(this, B_VERTICAL)
		.Add(root);

	// Select matching profile and populate presets.
	int32 idx = find_profile_for_config(current, fUserProfiles);
	_SelectProfile(idx);
	// Mark the provider menu item. User profiles are offset in the menu
	// by +1 for the separator item (if any user profiles exist).
	int32 menu_idx = idx;
	if (idx >= kProfileCount && !fUserProfiles.empty())
		menu_idx = idx + 1;  // account for separator
	if (BMenuItem* item = providerMenu->ItemAt(menu_idx))
		item->SetMarked(true);

	CenterOnScreen();
	// Focus on the model preset menu rather than the hidden text field.
	fModelPresets->MakeFocus(true);
}

void SettingsDialog::_SelectProfile(int32 index) {
	int total = kProfileCount + static_cast<int>(fUserProfiles.size());
	if (index < 0 || index >= total) return;
	fCurrentProfile = index;

	// Extract profile fields — built-in or user-defined.
	std::string label, base_url, api_key_env, provider_id;
	if (index < kProfileCount) {
		const auto& p = kProfiles[index];
		label       = p.label;
		base_url    = p.base_url;
		api_key_env = p.api_key_env;
		provider_id = p.provider_id;
	} else {
		const auto& up = fUserProfiles[index - kProfileCount];
		label       = up.label;
		base_url    = up.base_url;
		api_key_env = up.api_key_env;
		provider_id = "openai-compat";
	}

	fBaseUrl->SetText(base_url.c_str());
	if (!api_key_env.empty())
		fApiKeyEnv->SetText(api_key_env.c_str());

	fBaseUrl->SetEnabled(provider_id == "openai-compat");

	// Store the env var name internally.
	fApiKeyEnv->SetText(api_key_env.c_str());

	bool is_oauth = (api_key_env == "OAUTH");
	bool needs_key = !api_key_env.empty() && !is_oauth;

	// Show API key field or OAuth button, not both.
	if (is_oauth) {
		if (!fApiKeyField->IsHidden()) fApiKeyField->Hide();
		if (!fKeyStatus->IsHidden())   fKeyStatus->Hide();
		if (fOAuthBtn->IsHidden())     fOAuthBtn->Show();
		if (fOAuthStatus->IsHidden())  fOAuthStatus->Show();
		// Update OAuth status.
		pluto::OAuthTokens tok = pluto::LoadOAuthTokens();
		if (!tok.empty()) {
			fOAuthBtn->SetLabel("Logged in \xe2\x80\x94 Re-authenticate");
			fOAuthStatus->SetText("\xe2\x9c\x93 Logged in");
			fOAuthStatus->SetHighColor(30, 140, 70, 255);
		} else {
			fOAuthBtn->SetLabel("Login with Claude account");
			fOAuthStatus->SetText("Not logged in");
			fOAuthStatus->SetHighColor(200, 50, 50, 255);
		}
	} else {
		if (!fOAuthBtn->IsHidden())    fOAuthBtn->Hide();
		if (!fOAuthStatus->IsHidden()) fOAuthStatus->Hide();
		if (needs_key) {
			if (fApiKeyField->IsHidden()) fApiKeyField->Show();
			if (fKeyStatus->IsHidden())   fKeyStatus->Show();
		} else {
			if (!fApiKeyField->IsHidden()) fApiKeyField->Hide();
			if (!fKeyStatus->IsHidden())   fKeyStatus->Hide();
		}
		_UpdateKeyStatus();
	}

	_RebuildModelPresets(index);

	// Auto-select the first model of the new profile and update all fields.
	std::string first_id;
	int first_mt = 0, first_cw = 0;
	if (index < kProfileCount) {
		const auto& p = kProfiles[index];
		if (p.models[0].label != nullptr) {
			first_id = p.models[0].id;
			first_mt = p.models[0].max_tokens;
			first_cw = p.models[0].context_window;
		}
	} else {
		int ui = index - kProfileCount;
		if (ui >= 0 && ui < (int)fUserProfiles.size()
		    && !fUserProfiles[ui].models.empty()) {
			const auto& m = fUserProfiles[ui].models[0];
			first_id = m.id;
			first_mt = m.max_tokens;
			first_cw = m.context_window;
		}
	}
	if (!first_id.empty()) {
		fModel->SetText(first_id.c_str());
		fMaxTokens->SetText(std::to_string(first_mt).c_str());
		fContextWin->SetText(std::to_string(first_cw).c_str());
		// Mark the first item in the presets menu so the label updates.
		BMenu* menu = fModelPresets->Menu();
		if (menu && menu->CountItems() > 0) {
			BMenuItem* first = menu->ItemAt(0);
			if (first) first->SetMarked(true);
		}
	}
}

void SettingsDialog::_RebuildModelPresets(int32 profile_index) {
	if (!fModelPresets) return;
	BMenu* menu = fModelPresets->Menu();
	if (!menu) return;
	while (BMenuItem* it = menu->RemoveItem((int32)0)) delete it;

	if (profile_index < kProfileCount) {
		// Built-in profile.
		const auto& p = kProfiles[profile_index];
		for (int i = 0; p.models[i].label != nullptr; ++i) {
			auto* msg = new BMessage(kMsgModelPreset);
			msg->AddString("model", p.models[i].id);
			msg->AddInt32("max_tokens", p.models[i].max_tokens);
			msg->AddInt32("context_window", p.models[i].context_window);
			menu->AddItem(new BMenuItem(p.models[i].label, msg));
		}
	} else {
		// User-defined profile.
		int ui = profile_index - kProfileCount;
		if (ui >= 0 && ui < (int)fUserProfiles.size()) {
			for (const auto& m : fUserProfiles[ui].models) {
				auto* msg = new BMessage(kMsgModelPreset);
				msg->AddString("model", m.id.c_str());
				msg->AddInt32("max_tokens", m.max_tokens);
				msg->AddInt32("context_window", m.context_window);
				menu->AddItem(new BMenuItem(m.label.c_str(), msg));
			}
		}
	}
}

void SettingsDialog::_UpdateKeyStatus() {
	std::string env_name = fApiKeyEnv->Text() ? fApiKeyEnv->Text() : "";
	if (env_name.empty()) {
		fApiKeyField->SetText("");
		fKeyStatus->SetText("");
		return;
	}

	// Check if a key exists for this specific provider only
	// (env var or per-provider key file — no legacy fallback).
	std::string key;
	const char* v = std::getenv(env_name.c_str());
	if (v && *v) {
		key = v;
	} else {
		pluto::Config tmp;
		tmp.api_key_env = env_name;
		// ResolveApiKey falls back to the legacy file which is shared;
		// check only the per-provider file instead.
		key = pluto::ResolveProviderKey(env_name);
	}

	if (!key.empty()) {
		fApiKeyField->SetText(kKeyPlaceholder);
		fKeyStatus->SetText("\xe2\x9c\x93 Key configured");
		fKeyStatus->SetHighColor(30, 140, 70, 255);
	} else {
		fApiKeyField->SetText("");
		fKeyStatus->SetText("No API key");
		fKeyStatus->SetHighColor(200, 50, 50, 255);
	}
}

void SettingsDialog::MessageReceived(BMessage* msg) {
	switch (msg->what) {
		case kMsgOk: {
			BMessage out(kMsgSettingsAccepted);

			// Determine provider_id from current profile.
			// All user-defined profiles use openai-compat.
			const char* prov = "openai-compat";
			if (fCurrentProfile >= 0 && fCurrentProfile < kProfileCount)
				prov = kProfiles[fCurrentProfile].provider_id;
			out.AddString("provider", prov);
			out.AddString("base_url",
				fBaseUrl->Text() ? fBaseUrl->Text() : "");
			{
				std::string env = fApiKeyEnv->Text() ? fApiKeyEnv->Text() : "";
				out.AddString("api_key_env", env.c_str());
				// If the user typed a new key (not the placeholder), save it.
				std::string entered_key = fApiKeyField->Text()
					? fApiKeyField->Text() : "";
				// Trim whitespace.
				while (!entered_key.empty()
				    && std::isspace(static_cast<unsigned char>(entered_key.front())))
					entered_key.erase(0, 1);
				while (!entered_key.empty()
				    && std::isspace(static_cast<unsigned char>(entered_key.back())))
					entered_key.pop_back();
				if (!entered_key.empty() && entered_key != kKeyPlaceholder
				    && !env.empty())
					pluto::SaveApiKey(entered_key, env);
			}
			out.AddString("model",
				fModel->Text() ? fModel->Text() : "");
			out.AddInt32("max_tokens",
				fMaxTokens->Text() ? std::atoi(fMaxTokens->Text()) : 0);
			out.AddInt32("context_window",
				fContextWin->Text() ? std::atoi(fContextWin->Text()) : 0);
			out.AddString("system",
				fSystem->Text() ? fSystem->Text() : "");
			out.AddBool("cache", fCache->Value() == B_CONTROL_ON);
			// Color scheme: find marked item in the menu.
			{
				BMenu* cm = fColorScheme->Menu();
				BMenuItem* marked = cm ? cm->FindMarked() : nullptr;
				const char* scheme = "auto";
				if (marked && marked->Message())
					marked->Message()->FindString("scheme", &scheme);
				out.AddString("color_scheme", scheme);
			}
			fTarget.SendMessage(&out);
			fAccepted = true;
			Quit();
			break;
		}

		case kMsgCancel:
			Quit();
			break;

		case kMsgProfilePick: {
			int32 index = -1;
			if (msg->FindInt32("index", &index) == B_OK)
				_SelectProfile(index);
			break;
		}

		case kMsgModelPreset: {
			const char* id = "";
			if (msg->FindString("model", &id) == B_OK && *id)
				fModel->SetText(id);
			int32 mt = 0, cw = 0;
			if (msg->FindInt32("max_tokens", &mt) == B_OK && mt > 0)
				fMaxTokens->SetText(std::to_string(mt).c_str());
			if (msg->FindInt32("context_window", &cw) == B_OK && cw > 0)
				fContextWin->SetText(std::to_string(cw).c_str());
			break;
		}

		case kMsgFetchModels:
			_FetchModels();
			break;

		case kMsgFetchDone: {
			fFetchBtn->SetEnabled(true);
			fFetchBtn->SetLabel("Fetch Models");

			const char* err = nullptr;
			if (msg->FindString("error", &err) == B_OK && err && *err) {
				BAlert* a = new BAlert("Pluto", err, "OK",
					nullptr, nullptr, B_WIDTH_AS_USUAL, B_STOP_ALERT);
				a->Go(nullptr);
				break;
			}

			// Populate the model presets menu with fetched models.
			BMenu* menu = fModelPresets->Menu();
			if (!menu) break;
			while (BMenuItem* it = menu->RemoveItem((int32)0)) delete it;

			int32 count = 0;
			const char* name;
			while (msg->FindString("model_name", count, &name) == B_OK) {
				const char* detail = "";
				msg->FindString("model_detail", count, &detail);

				BString label;
				if (detail && *detail)
					label.SetToFormat("%s  (%s)", name, detail);
				else
					label = name;

				auto* item_msg = new BMessage(kMsgModelPreset);
				item_msg->AddString("model", name);
				menu->AddItem(new BMenuItem(label.String(), item_msg));
				++count;
			}

			if (count > 0) {
				// Auto-select first model if the model field is empty or
				// has the placeholder.
				std::string cur = fModel->Text() ? fModel->Text() : "";
				if (cur.empty() || cur == "(enter model ID below)"
				    || cur == "local-model") {
					const char* first = "";
					msg->FindString("model_name", (int32)0, &first);
					if (first && *first) fModel->SetText(first);
				}
			}
			break;
		}

		case kMsgToggleAdvanced: {
			if (fAdvancedBox->IsHidden()) {
				fAdvancedBox->Show();
				fAdvancedToggle->SetLabel("Advanced \xe2\x96\xbc");
			} else {
				fAdvancedBox->Hide();
				fAdvancedToggle->SetLabel("Advanced \xe2\x96\xb6");
			}
			InvalidateLayout();
			break;
		}

		case 'plOL': {
			// Start OAuth login in background.
			fOAuthBtn->SetEnabled(false);
			fOAuthBtn->SetLabel("Waiting for browser\xe2\x80\xa6");
			fOAuthStatus->SetText("Check your browser to complete login.");
			fOAuthStatus->SetHighColor(100, 100, 100, 255);
			thread_id tid = spawn_thread(_OAuthThread, "oauth_login",
				B_NORMAL_PRIORITY, this);
			if (tid >= 0) resume_thread(tid);
			else {
				fOAuthBtn->SetEnabled(true);
				fOAuthBtn->SetLabel("Login with Claude account");
			}
			break;
		}

		case 'plOD': {
			bool ok = false;
			msg->FindBool("ok", &ok);
			fOAuthBtn->SetEnabled(true);
			if (ok) {
				fOAuthBtn->SetLabel("Logged in \xe2\x80\x94 Re-authenticate");
				fOAuthStatus->SetText("\xe2\x9c\x93 Login successful!");
				fOAuthStatus->SetHighColor(30, 140, 70, 255);
			} else {
				fOAuthBtn->SetLabel("Login with Claude account");
				fOAuthStatus->SetText("Login failed.");
				fOAuthStatus->SetHighColor(200, 50, 50, 255);
			}
			break;
		}

		default:
			BWindow::MessageReceived(msg);
	}
}

bool SettingsDialog::QuitRequested() {
	return true;
}

int32 SettingsDialog::_OAuthThread(void* data) {
	auto* self = static_cast<SettingsDialog*>(data);
	bool ok = pluto::OAuthLogin();
	BMessage done('plOD');
	done.AddBool("ok", ok);
	BMessenger(self).SendMessage(&done);
	return 0;
}

// ----------------------------------------------------------------
// Fetch models from a remote server (runs in a background thread)
// ----------------------------------------------------------------

namespace {

size_t settings_collect_cb(char* ptr, size_t size, size_t nmemb, void* ud) {
	static_cast<std::string*>(ud)->append(ptr, size * nmemb);
	return size * nmemb;
}

}  // namespace

void SettingsDialog::_FetchModels() {
	const char* url_text = fBaseUrl->Text();
	if (!url_text || !*url_text) {
		BAlert* a = new BAlert("Pluto",
			"Enter a Base URL first (e.g. http://192.168.1.100:11434).",
			"OK");
		a->Go(nullptr);
		return;
	}
	fFetchBtn->SetEnabled(false);
	fFetchBtn->SetLabel("Fetching…");
	thread_id tid = spawn_thread(_FetchThread, "fetch_models",
		B_NORMAL_PRIORITY, this);
	if (tid >= 0) resume_thread(tid);
	else {
		fFetchBtn->SetEnabled(true);
		fFetchBtn->SetLabel("Fetch Models");
	}
}

int32 SettingsDialog::_FetchThread(void* data) {
	auto* self = static_cast<SettingsDialog*>(data);

	std::string base_url = self->fBaseUrl->Text() ? self->fBaseUrl->Text() : "";
	while (!base_url.empty() && base_url.back() == '/') base_url.pop_back();

	BMessage done(kMsgFetchDone);

	CURL* curl = curl_easy_init();
	if (!curl) {
		done.AddString("error", "Failed to initialize HTTP client.");
		BMessenger(self).SendMessage(&done);
		return 0;
	}

	std::string body;
	// Try Ollama /api/tags first.
	std::string url = base_url + "/api/tags";
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, settings_collect_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "pluto/0.3.0 (Haiku)");
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

	CURLcode rc = curl_easy_perform(curl);
	long http_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

	if (rc == CURLE_OK && http_code == 200) {
		// Parse Ollama response.
		try {
			json::Value root = json::Value::parse(body);
			const json::Value* models = root.find("models");
			if (models && models->is_arr()) {
				for (const auto& m : models->as_arr()) {
					std::string name = m.str("name");
					if (name.empty()) continue;
					done.AddString("model_name", name.c_str());

					std::string detail;
					const json::Value* det = m.find("details");
					if (det && det->is_obj()) {
						std::string ps = det->str("parameter_size");
						std::string q  = det->str("quantization_level");
						if (!ps.empty()) detail = ps;
						if (!q.empty()) {
							if (!detail.empty()) detail += ", ";
							detail += q;
						}
					}
					const json::Value* sz = m.find("size");
					if (sz && sz->is_num()) {
						double gb = sz->as_num() / (1024.0 * 1024.0 * 1024.0);
						char buf[32];
						std::snprintf(buf, sizeof(buf), "%.1f GB", gb);
						if (!detail.empty()) detail += ", ";
						detail += buf;
					}
					done.AddString("model_detail", detail.c_str());
				}
			}
		} catch (const std::exception&) {
			done.AddString("error", "Failed to parse Ollama response.");
		}
		curl_easy_cleanup(curl);
		BMessenger(self).SendMessage(&done);
		return 0;
	}

	// Fallback: OpenAI /v1/models (with auth if api_key_env is set).
	body.clear();
	url = base_url + "/v1/models";
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

	struct curl_slist* auth_hdrs = nullptr;
	std::string env_name = self->fApiKeyEnv->Text() ? self->fApiKeyEnv->Text() : "";
	if (!env_name.empty()) {
		pluto::Config tmp;
		tmp.api_key_env = env_name;
		std::string key = pluto::ResolveApiKey(tmp);
		if (!key.empty()) {
			std::string hdr = "Authorization: Bearer " + key;
			auth_hdrs = curl_slist_append(auth_hdrs, hdr.c_str());
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, auth_hdrs);
		}
	}

	rc = curl_easy_perform(curl);
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
	curl_slist_free_all(auth_hdrs);
	curl_easy_cleanup(curl);

	if (rc != CURLE_OK) {
		BString err;
		err.SetToFormat("Connection failed: %s", curl_easy_strerror(rc));
		done.AddString("error", err.String());
		BMessenger(self).SendMessage(&done);
		return 0;
	}
	if (http_code != 200) {
		BString err;
		err.SetToFormat("Server returned HTTP %ld", http_code);
		done.AddString("error", err.String());
		BMessenger(self).SendMessage(&done);
		return 0;
	}

	try {
		json::Value root = json::Value::parse(body);
		const json::Value* data = root.find("data");
		if (data && data->is_arr()) {
			for (const auto& m : data->as_arr()) {
				std::string id = m.str("id");
				if (id.empty()) continue;
				// Strip "models/" prefix (Gemini returns "models/gemini-...")
				if (id.rfind("models/", 0) == 0)
					id = id.substr(7);
				done.AddString("model_name", id.c_str());
				done.AddString("model_detail", "");
			}
		}
	} catch (const std::exception&) {
		done.AddString("error", "Failed to parse /v1/models response.");
	}

	BMessenger(self).SendMessage(&done);
	return 0;
}

}  // namespace gui
}  // namespace pluto
