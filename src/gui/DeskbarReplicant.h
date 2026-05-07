#pragma once

#include <View.h>

class BBitmap;

namespace pluto {
namespace gui {

class DeskbarReplicant : public BView {
public:
	DeskbarReplicant();
	DeskbarReplicant(BMessage* archive);
	~DeskbarReplicant() override;

	static BArchivable* Instantiate(BMessage* archive);
	status_t Archive(BMessage* archive, bool deep = true) const override;

	void Draw(BRect updateRect) override;
	void MouseDown(BPoint where) override;
	void MouseMoved(BPoint where, uint32 transit, const BMessage*) override;
	void MessageReceived(BMessage* msg) override;
	void AttachedToWindow() override;

private:
	void _ShowPopup(BPoint screenWhere);
	void _UpdateToolTip();

	BBitmap* fIcon = nullptr;
};

bool InstallDeskbarReplicant();
bool RemoveDeskbarReplicant();

}  // namespace gui
}  // namespace pluto
