#ifndef renderer_h__
#define renderer_h__

#include <windows.h>
#include <GdiPlus.h>
#include <vector>
#include <string>
#include "gen_win7shell.h"
#include "metadata.h"
#include "lines.h"

class renderer
{
public:
	renderer(sSettings& settings, MetaData &metadata);
	~renderer();

	bool getAlbumArt(const std::wstring &fname, int& iconheight, int& iconwidth);
	HBITMAP GetThumbnail(const bool get_bmp = true);
	void ClearAlbumart();
	void ClearBackground();
	void ClearCustomBackground();
	void ThumbnailPopup();
	void SetDimensions(const int new_w, const int new_h);

private:
	ULONG_PTR gdiplusToken;
	Gdiplus::Image *custom_img;
	Gdiplus::Bitmap *background;
	Gdiplus::Bitmap *albumart;

	sSettings &m_settings;
	MetaData &m_metadata;

	int m_width, m_height, m_iconwidth, m_iconheight, m_textpause;
	std::vector<int> m_textpositions;
	bool no_icon, fail, scroll_block, no_text;

	bool render();
};

#endif // renderer_h__