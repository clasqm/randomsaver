#ifndef RANDOM_SAVER_H
#define RANDOM_SAVER_H

#include <ScreenSaver.h>

#include <Rect.h>
#include <String.h>

#include <vector>

class BView;

struct SaverEntry {
	BString name;	// file name, as listed by the preferences app
	BString path;	// full path to the add-on
	bool enabled = true;	// false when the user blacklisted it
};

class RandomSaver : public BScreenSaver {
public:
	RandomSaver(BMessage* archive, image_id thisImage);
	virtual ~RandomSaver();

	virtual status_t StartSaver(BView* view, bool preview);
	virtual void StopSaver();
	virtual void Draw(BView* view, int32 frame);
	virtual void DirectDraw(int32 frame);
	virtual void DirectConnected(direct_buffer_info* info);

	virtual void StartConfig(BView* configView);

	virtual status_t SaveState(BMessage* into) const;

	// used by the configuration list
	const std::vector<SaverEntry>& Entries() const { return fEntries; }
	void SetSaverEnabled(const char* name, bool enabled);

private:
	void _ScanInstalled();
	bool _IsExcluded(const char* name, const char* path) const;
	BMessage _ModuleState(const char* name) const;
	void _FillConfigView(BView* view);
	status_t _StartEntry(const SaverEntry& entry, BView* view, bool preview);
	void _StopInner();

	std::vector<SaverEntry> fEntries;
	BScreenSaver* fInner;
	image_id fInnerImage;
	BString fSelfPath;
};

#endif // RANDOM_SAVER_H
