#pragma once

#include <Messenger.h>
#include <Window.h>

#include <string>
#include <vector>

#include "../config.h"

class BBox;
class BButton;
class BCheckBox;
class BMenuField;
class BPopUpMenu;
class BScrollView;
class BStringView;
class BTextControl;
class BTextView;

namespace pluto {
namespace gui {

inline constexpr uint32 kMsgSettingsAccepted = 'plSa';

// User-defined provider profile loaded from .conf files in
// ~/config/settings/pluto/providers/
struct UserModel {
	std::string label;
	std::string id;
	int         max_tokens     = 4096;
	int         context_window = 128000;
};

struct UserProvider {
	std::string label;
	std::string base_url;
	std::string api_key_env;
	std::vector<UserModel> models;
};

class SettingsDialog : public BWindow {
public:
	SettingsDialog(BMessenger target, const Config& current);

	void MessageReceived(BMessage* msg) override;
	bool QuitRequested() override;

private:
	void _SelectProfile(int32 index);
	void _RebuildModelPresets(int32 profile_index);
	void _UpdateKeyStatus();
	void _FetchModels();
	static int32 _FetchThread(void* data);
	static int32 _OAuthThread(void* data);

	BMessenger    fTarget;
	BMenuField*   fProviderField = nullptr;
	BTextControl* fBaseUrl       = nullptr;
	BTextControl* fApiKeyEnv     = nullptr;
	BTextControl* fApiKeyField   = nullptr;  // direct key entry
	BStringView*  fKeyStatus     = nullptr;  // "✓ Key saved" / "No key"
	BButton*      fOAuthBtn      = nullptr;  // "Login with Claude account"
	BStringView*  fOAuthStatus   = nullptr;
	BTextControl* fModel         = nullptr;
	BMenuField*   fModelPresets  = nullptr;
	BButton*      fFetchBtn      = nullptr;
	BTextControl* fMaxTokens     = nullptr;
	BTextControl* fContextWin    = nullptr;
	BMenuField*   fColorScheme   = nullptr;
	BTextView*    fSystem        = nullptr;
	BScrollView*  fSysScroll     = nullptr;
	BCheckBox*    fCache         = nullptr;
	BButton*      fAdvancedToggle = nullptr;
	BBox*         fAdvancedBox   = nullptr;
	bool          fAccepted      = false;
	int32         fCurrentProfile = -1;

	// User-defined provider profiles loaded from
	// ~/config/settings/pluto/providers/*.conf
	std::vector<UserProvider> fUserProfiles;
};

}  // namespace gui
}  // namespace pluto
