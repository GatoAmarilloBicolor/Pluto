#include "../config.h"
#include "../systemone/SystemOneConfig.h"

#include <Messenger.h>
#include <Window.h>

#include <string>

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

// Aliases for backward compatibility — types now live in config.h.
using UserModel    = pluto::ProviderConfModel;
using UserProvider = pluto::ProviderConf;

class SettingsDialog : public BWindow {
public:
	SettingsDialog(BMessenger target, const Config& current);

	void MessageReceived(BMessage* msg) override;
	bool QuitRequested() override;

	// Returns the modified config (only valid if fAccepted is true).
	Config GetResult() const { return fResult; }
	bool   Accepted() const { return fAccepted; }

private:
	void _SelectProfile(int32 index);
	void _RebuildModelPresets(int32 profile_index);
	void _UpdateKeyStatus();
	void _FetchModels();
	static int32 _FetchThread(void* data);
	static int32 _OAuthThread(void* data);

	// System One
	void _BuildSystemOneBox();
	void _SaveSystemOne();
	void _LoadSystemOne(const Config& cfg);

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

	// System One controls
	BCheckBox*    fSystemOneEnabled  = nullptr;
	BMenuField*   fSystemOneModel    = nullptr;
	BTextControl* fSystemOneApiKey   = nullptr;
	BTextControl* fSystemOneKevUrl   = nullptr;
	BTextControl* fSystemOneLayaUrl  = nullptr;
	BTextControl* fSystemOneJevUrl   = nullptr;
	BTextControl* fSystemOneBaseUrl  = nullptr;
	BTextControl* fSystemOneThreshold = nullptr;
	BTextControl* fSystemOneTimeout   = nullptr;
	BCheckBox*    fSystemOnePreferLocal = nullptr;
	BCheckBox*    fSystemOneFallbackLocal = nullptr;

	bool          fAccepted      = false;
	int32         fCurrentProfile = -1;
	Config        fResult;

	// User-defined provider profiles loaded from
	// ~/config/settings/pluto/providers/*.conf
	std::vector<UserProvider> fUserProfiles;
};

}  // namespace gui
}  // namespace pluto
