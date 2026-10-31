/*
 * RandomSaver - a screen saver that draws nothing itself. Every time it is
 * started it picks another installed screen saver at random and simply runs
 * that one instead.
 */

#include "RandomSaver.h"

#include <image.h>
#include <OS.h>

#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <Font.h>
#include <InterfaceDefs.h>
#include <ListView.h>
#include <Path.h>
#include <ScrollView.h>
#include <StringItem.h>
#include <StringView.h>
#include <View.h>

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const char* kBlacklistedKey = "blacklisted";
static const char* kExcludedName = "Blackness";
static const char* kSelfName = "RandomSaver";
static const char* kSettingsFileName = "ScreenSaver_settings";

// geometry of the little check box drawn in front of every row
static const float kBoxSize = 14.0f;
static const float kBoxLeft = 4.0f;
static const float kBoxGap = 4.0f;

// the rectangle of the check box inside the given row, used both for
// drawing the box and for hit testing clicks on it
static BRect
check_box_rect(const BRect& row)
{
	float rowHeight = row.bottom - row.top + 1.0f;
	float size = kBoxSize;
	if (rowHeight - 2.0f < size)
		size = rowHeight - 2.0f;
	if (size < 6.0f)
		size = 6.0f;

	BRect box;
	box.left = row.left + kBoxLeft;
	box.right = box.left + size;
	box.top = row.top + floorf((rowHeight - size) / 2.0f);
	box.bottom = box.top + size;
	return box;
}


class CheckItem : public BStringItem {
public:
	CheckItem(const char* text, bool checked)
		:
		BStringItem(text),
		fChecked(checked)
	{
	}

	bool IsChecked() const { return fChecked; }
	void SetChecked(bool checked) { fChecked = checked; }

	virtual void DrawItem(BView* owner, BRect frame, bool complete)
	{
		// repaint the row background the way BStringItem does
		rgb_color lowColor = owner->LowColor();
		if (IsSelected() || complete) {
			rgb_color color = IsSelected()
				? ui_color(B_LIST_SELECTED_BACKGROUND_COLOR)
				: owner->ViewColor();
			owner->SetLowColor(color);
			owner->FillRect(frame, B_SOLID_LOW);
		} else
			owner->SetLowColor(owner->ViewColor());

		rgb_color background = owner->LowColor();
		BRect box = check_box_rect(frame);

		// the box: a filled rectangle with a contrasting border
		rgb_color fill = IsSelected()
			? tint_color(background, B_LIGHTEN_1_TINT)
			: background;
		owner->SetHighColor(fill);
		owner->FillRect(box);
		owner->SetHighColor(tint_color(background, B_DARKEN_3_TINT));
		owner->StrokeRect(box);

		if (fChecked) {
			// a check mark, drawn dark on light backgrounds,
			// light on dark ones
			rgb_color mark
				= background.red + background.green + background.blue
					> 128 * 3
					? tint_color(background, B_DARKEN_4_TINT)
					: tint_color(background, B_LIGHTEN_2_TINT);
			owner->SetHighColor(mark);
			owner->SetPenSize(2.0f);
			float size = box.Width();
			owner->StrokeLine(
				BPoint(box.left + 0.20f * size, box.top + 0.50f * size),
				BPoint(box.left + 0.42f * size, box.top + 0.75f * size));
			owner->StrokeLine(
				BPoint(box.left + 0.42f * size, box.top + 0.75f * size),
				BPoint(box.left + 0.82f * size, box.top + 0.25f * size));
			owner->SetPenSize(1.0f);
		}

		// and the label itself, drawn by BStringItem next to the box
		BRect textFrame = frame;
		textFrame.left = box.right + kBoxGap;
		BStringItem::DrawItem(owner, textFrame, complete);

		owner->SetLowColor(lowColor);
	}

private:
	bool fChecked;
};


class SaverListView : public BListView {
public:
	SaverListView(const BRect& frame, RandomSaver* saver)
		:
		BListView(frame, "Savers", B_SINGLE_SELECTION_LIST, B_FOLLOW_ALL,
			B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE),
		fSaver(saver)
	{
	}

	virtual void MouseDown(BPoint where)
	{
		int32 index = IndexOf(where);
		if (index >= 0) {
			BRect row = ItemFrame(index);
			if (check_box_rect(row).Contains(where)) {
				CheckItem* item = static_cast<CheckItem*>(ItemAt(index));
				if (item != NULL)
					_Toggle(item);
				return;
			}
		}
		BListView::MouseDown(where);
	}

	virtual void KeyDown(const char* bytes, int32 numBytes)
	{
		if (numBytes == 1 && bytes[0] == ' ') {
			int32 index = CurrentSelection();
			CheckItem* item = index >= 0
				? static_cast<CheckItem*>(ItemAt(index))
				: NULL;
			if (item != NULL) {
				_Toggle(item);
				return;
			}
		}
		BListView::KeyDown(bytes, numBytes);
	}

private:
	void _Toggle(CheckItem* item)
	{
		bool checked = !item->IsChecked();
		fSaver->SetSaverEnabled(item->Text(), checked);

		// the same screen saver can be installed in several places,
		// all of its rows share one state
		int32 count = CountItems();
		for (int32 i = 0; i < count; i++) {
			CheckItem* other = static_cast<CheckItem*>(ItemAt(i));
			if (other != NULL && strcmp(other->Text(), item->Text()) == 0) {
				other->SetChecked(checked);
				InvalidateItem(i);
			}
		}
	}

	RandomSaver* fSaver;
};


RandomSaver::RandomSaver(BMessage* archive, image_id thisImage)
	:
	BScreenSaver(archive, thisImage),
	fInner(NULL),
	fInnerImage(B_ERROR)
{
	// remember where we live, so we never pick ourselves
	image_info info;
	if (get_image_info(thisImage, &info) == B_OK)
		fSelfPath = info.name;

	_ScanInstalled();

	// no saved configuration at all (first run) leaves everything enabled
	if (archive != NULL) {
		const char* name;
		for (int32 i = 0; archive->FindString(kBlacklistedKey, i, &name)
				== B_OK; i++) {
			for (size_t j = 0; j < fEntries.size(); j++) {
				if (fEntries[j].name == name)
					fEntries[j].enabled = false;
			}
		}
	}
}


RandomSaver::~RandomSaver()
{
	_StopInner();
}


void
RandomSaver::_ScanInstalled()
{
	directory_which which[] = {
		B_USER_NONPACKAGED_ADDONS_DIRECTORY,
		B_USER_ADDONS_DIRECTORY,
		B_SYSTEM_NONPACKAGED_ADDONS_DIRECTORY,
		B_SYSTEM_ADDONS_DIRECTORY,
	};

	for (uint32 i = 0; i < sizeof(which) / sizeof(which[0]); i++) {
		BPath basePath;
		if (find_directory(which[i], &basePath) != B_OK)
			continue;
		if (basePath.Append("Screen Savers", true) != B_OK)
			continue;

		BDirectory dir(basePath.Path());
		if (dir.InitCheck() != B_OK)
			continue;

		BEntry entry;
		while (dir.GetNextEntry(&entry, true) == B_OK) {
			char name[B_FILE_NAME_LENGTH];
			if (entry.GetName(name) != B_OK)
				continue;

			BPath path(basePath);
			if (path.Append(name) != B_OK)
				continue;

			if (_IsExcluded(name, path.Path()))
				continue;

			SaverEntry saver;
			saver.name = name;
			saver.path = path.Path();
			fEntries.push_back(saver);
		}
	}
}


bool
RandomSaver::_IsExcluded(const char* name, const char* path) const
{
	// Blackness normally has no file of its own (the preferences app
	// adds it synthetically), but filter it out just in case
	if (strcmp(name, kExcludedName) == 0)
		return true;

	// and we never run ourselves either
	if (strcmp(name, kSelfName) == 0)
		return true;

	if (fSelfPath.Length() > 0 && strcmp(path, fSelfPath.String()) == 0)
		return true;

	return false;
}


void
RandomSaver::SetSaverEnabled(const char* name, bool enabled)
{
	if (name == NULL)
		return;

	for (size_t i = 0; i < fEntries.size(); i++) {
		if (fEntries[i].name == name)
			fEntries[i].enabled = enabled;
	}
}


status_t
RandomSaver::SaveState(BMessage* into) const
{
	if (into == NULL)
		return B_BAD_VALUE;

	for (size_t i = 0; i < fEntries.size(); i++) {
		const SaverEntry& entry = fEntries[i];
		if (entry.enabled)
			continue;

		// only store each blacklisted name once, even when the same
		// screen saver is installed in several places
		bool alreadyStored = false;
		const char* storedName;
		for (int32 j = 0; into->FindString(kBlacklistedKey, j, &storedName)
				== B_OK; j++) {
			if (entry.name == storedName) {
				alreadyStored = true;
				break;
			}
		}

		if (!alreadyStored)
			into->AddString(kBlacklistedKey, entry.name.String());
	}

	return B_OK;
}


status_t
RandomSaver::StartSaver(BView* view, bool preview)
{
	_StopInner();

	// gather the screensavers the user allows us to run...
	std::vector<size_t> candidates;
	for (size_t i = 0; i < fEntries.size(); i++) {
		if (fEntries[i].enabled)
			candidates.push_back(i);
	}
	if (candidates.empty())
		return B_ERROR;

	// ...shuffle them...
	srand((unsigned int)(system_time() ^ (bigtime_t)(uintptr_t)this));
	for (size_t i = candidates.size(); i > 1; i--) {
		size_t j = (size_t)(rand() % (int)i);
		size_t swap = candidates[i - 1];
		candidates[i - 1] = candidates[j];
		candidates[j] = swap;
	}

	// ...and run them in that order until one of them starts
	for (size_t i = 0; i < candidates.size(); i++) {
		if (_StartEntry(fEntries[candidates[i]], view, preview) == B_OK)
			return B_OK;
	}

	return B_ERROR;
}


status_t
RandomSaver::_StartEntry(const SaverEntry& entry, BView* view, bool preview)
{
	image_id image = load_add_on(entry.path.String());
	if (image < B_OK)
		return image;

	typedef BScreenSaver* (*instantiate_hook)(BMessage*, image_id);
	instantiate_hook instantiate = NULL;
	if (get_image_symbol(image, "instantiate_screen_saver", B_SYMBOL_TYPE_TEXT,
			(void**)&instantiate) != B_OK
		|| instantiate == NULL) {
		unload_add_on(image);
		return B_ERROR;
	}

	// hand the screensaver its own saved settings, if it has any
	BMessage state = _ModuleState(entry.name.String());
	BScreenSaver* inner = instantiate(&state, image);
	if (inner == NULL || inner->InitCheck() != B_OK) {
		delete inner;
		unload_add_on(image);
		return B_ERROR;
	}

	if (inner->StartSaver(view, preview) != B_OK) {
		delete inner;
		unload_add_on(image);
		return B_ERROR;
	}

	fInner = inner;
	fInnerImage = image;

	// adopt whatever pacing it asked for in its own StartSaver()
	SetTickSize(fInner->TickSize());
	SetLoop(fInner->LoopOnCount(), fInner->LoopOffCount());

	return B_OK;
}


void
RandomSaver::_StopInner()
{
	if (fInner != NULL) {
		fInner->StopSaver();
		// the object has to go before its add-on image does
		delete fInner;
		fInner = NULL;
	}

	if (fInnerImage != B_ERROR) {
		unload_add_on(fInnerImage);
		fInnerImage = B_ERROR;
	}
}


void
RandomSaver::StopSaver()
{
	_StopInner();
}


void
RandomSaver::Draw(BView* view, int32 frame)
{
	if (fInner == NULL) {
		view->SetHighColor(0, 0, 0);
		view->FillRect(view->Bounds());
		return;
	}

	fInner->Draw(view, frame);

	// the runner re-reads these from us on every tick, so keep them in
	// sync with whatever the delegated screensaver is doing
	SetTickSize(fInner->TickSize());
	SetLoop(fInner->LoopOnCount(), fInner->LoopOffCount());
}


void
RandomSaver::DirectDraw(int32 frame)
{
	if (fInner != NULL)
		fInner->DirectDraw(frame);
}


void
RandomSaver::DirectConnected(direct_buffer_info* info)
{
	if (fInner != NULL)
		fInner->DirectConnected(info);
	else
		BScreenSaver::DirectConnected(info);
}


BMessage
RandomSaver::_ModuleState(const char* name) const
{
	BMessage state;
	if (name == NULL || name[0] == '\0')
		return state;

	// the preferences app keeps the state of every module in one file
	BPath settingsPath;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &settingsPath) != B_OK)
		return state;
	if (settingsPath.Append(kSettingsFileName, true) != B_OK)
		return state;

	BFile file(settingsPath.Path(), B_READ_ONLY);
	if (file.InitCheck() != B_OK)
		return state;

	BMessage settings;
	if (settings.Unflatten(&file) != B_OK)
		return state;

	BString key("modulesettings_");
	key << name;

	BMessage moduleState;
	if (settings.FindMessage(key.String(), &moduleState) == B_OK)
		state = moduleState;

	return state;
}


void
RandomSaver::StartConfig(BView* configView)
{
	if (configView == NULL)
		return;

	BView* view = new BView(configView->Bounds(), "RandomSaverConfig",
		B_FOLLOW_ALL, B_WILL_DRAW);
	view->SetViewUIColor(B_PANEL_BACKGROUND_COLOR);

	// attach it first, so that the list view can set up its scroll bar
	// against a live window. The preferences app owns this view and
	// deletes it itself, we keep no pointer to it.
	configView->AddChild(view);

	_FillConfigView(view);
}


void
RandomSaver::_FillConfigView(BView* view)
{
	BRect bounds = view->Bounds();

	font_height fontHeight;
	be_plain_font->GetHeight(&fontHeight);
	float lineHeight = ceilf(fontHeight.ascent + fontHeight.descent
		+ fontHeight.leading);

	if (fEntries.empty()) {
		view->AddChild(new BStringView(BRect(0, 0, bounds.right, lineHeight),
			"Empty", "No other screensavers are installed.",
			B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP, B_WILL_DRAW));
		return;
	}

	view->AddChild(new BStringView(BRect(0, 0, bounds.right, lineHeight),
		"Hint", "Uncheck a screensaver to blacklist it.",
		B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP, B_WILL_DRAW));

	BRect listFrame(0, lineHeight + 4.0f, bounds.right, bounds.bottom);
	if (listFrame.bottom < listFrame.top)
		listFrame.bottom = listFrame.top;

	SaverListView* list = new SaverListView(listFrame, this);
	BScrollView* scroller = new BScrollView("Scroller", list, B_FOLLOW_ALL,
		B_WILL_DRAW, false, true, B_FANCY_BORDER);
	view->AddChild(scroller);

	// the scroll view sizes itself around its target, so move it exactly
	// where we want it afterwards
	scroller->MoveTo(listFrame.LeftTop());
	scroller->ResizeTo(listFrame.Width(), listFrame.Height());

	// add the items last: the list view then fixes up the scroll bar
	// range for the final item count
	for (size_t i = 0; i < fEntries.size(); i++) {
		list->AddItem(
			new CheckItem(fEntries[i].name.String(), fEntries[i].enabled));
	}
}


extern "C" _EXPORT BScreenSaver*
instantiate_screen_saver(BMessage* message, image_id image)
{
	return new RandomSaver(message, image);
}
