#include "stdafx.h"
#include "ScreenToolWnd.h"

#include <vector>
#include <ranges>
#include <algorithm>
#include <functional>
#include <map>
#include <deque>
#include <limits>
#include <numeric>
#include <windowsx.h>

using namespace std::placeholders;

namespace
{
	const std::wstring CLASS_NAME = L"ScreenToolWnd";
	WNDCLASSW* wndClass = nullptr;

	static const FLOAT F = 0.1f; // factor
}

struct Screen
{
	size_t scr; // screen number
	int x, y, w, h; // left, top, width, height in pixels
};

struct WinPos {
	int scr; // screen 0 = all, 1 = first, 2 = second, ...
	wstring n; // name
	int x, y, w, h; // left, top, width, height in % from screen
};


struct ScreenToolWnd::Impl
{
	Impl(HINSTANCE hInst, HWND hParent, UINT message, WPARAM wParam, LPARAM lParam);
	~Impl();

	HHOOK _hhkCallKeyboardMsg = 0;
	HWND _hWnd = 0;
	POINT _clickPoint = { 0 };
	RECT _currentPreviewScr = {0}, _currentPreviewPos = {0};
	std::vector<RECT> _realScrRects, _previwScrRects, _realPosRects, _previewPosRects;
	LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	
	template<typename It, typename Ct>
	inline It NextPos(It it, Ct& posRects);
	template<typename It, typename Ct>
	inline It PreviousPos(It it, Ct& posRects);

	RECT Calculate(WinPos& wp, Screen& s);

	static std::map<HWND, ScreenToolWnd::Impl*> hWndToImplMap;
	static LRESULT CALLBACK ToolWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
};

std::map<HWND, ScreenToolWnd::Impl*> ScreenToolWnd::Impl::hWndToImplMap;

ScreenToolWnd::ScreenToolWnd() = default;
ScreenToolWnd::~ScreenToolWnd() = default;

HWND ScreenToolWnd::GetHwnd()
{
	return this->_pImpl->_hWnd;
}

LRESULT ScreenToolWnd::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	return _pImpl->WndProc(hWnd, message, wParam,lParam);
}

ScreenToolWnd::Ptr ScreenToolWnd::ShowWindow(HINSTANCE hInst, HWND hParent, UINT message, WPARAM wParam, LPARAM lParam)
{
	Ptr ptr = std::make_unique<ScreenToolWnd>();
	ptr->_pImpl = std::make_unique<Impl>(hInst, hParent, message, wParam, lParam);
	return ptr;
}

ScreenToolWnd::Ptr ScreenToolWnd::pWnd;

BOOL ScreenToolWnd::IsScreenToolWnd(HWND hWnd)
{
	static wchar_t className[100] = { L'\0' };
	GetClassName(hWnd, className, 99);
	return CLASS_NAME.compare(className) == 0;
} 

LRESULT ScreenToolWnd::Impl::ToolWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	ScreenToolWnd::Impl* pImpl = hWndToImplMap[hWnd];
	if (pImpl)
		return pImpl->WndProc(hWnd, message, wParam, lParam);

	return DefWindowProc(hWnd, message, wParam, lParam);
}

BOOL Monitorenumproc(HMONITOR hMon, HDC hDC, LPRECT pRECT, LPARAM lParam)
{
	std::vector<RECT>& mInfos = *(std::vector<RECT>*)lParam;
	MONITORINFOEX mi = { 0 };
	mi.cbSize = sizeof(MONITORINFOEX);
	GetMonitorInfo(hMon, &mi);
	mInfos.push_back(mi.rcWork);
	return TRUE;
}

RECT ScaleRect(RECT& in, FLOAT f)
{
	RECT r = { 0 };

	r.right = static_cast<LONG>(((FLOAT)((in.right - in.left) + in.left)) * f);
	r.bottom = static_cast<LONG>(((FLOAT)((in.bottom - in.top) + in.top)) * f);
	r.left = static_cast<LONG>(((FLOAT)in.left) * f);
	r.top = static_cast<LONG>(((FLOAT)in.top) * f);

	return r;
}


ScreenToolWnd::Impl::Impl(HINSTANCE hInst, HWND hParent, UINT message, WPARAM wParam, LPARAM lParam)
	:_hWnd(nullptr)
{
	if (wndClass == nullptr)
	{
		static WNDCLASSW wcl = { 0 };
		wcl.hbrBackground = (HBRUSH)COLOR_WINDOW;
		wcl.hCursor = LoadCursor(NULL, IDC_ARROW);
		wcl.hInstance = hInst;
		wcl.lpszClassName = CLASS_NAME.c_str();
		wcl.lpfnWndProc = ToolWndProc;

		wndClass = &wcl;
		RegisterClassW(wndClass);
	}
	_clickPoint = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
	std::vector<RECT> mInfos;
#ifdef _WIN64
	EnumDisplayMonitors(NULL, NULL, Monitorenumproc, (LPARAM)&mInfos);
#endif

	_realScrRects = mInfos;	

	LONG ox = 0, oy = 0; // offset

	for (RECT& sr : _realScrRects) // screen rect
	{
		ox = std::min(sr.left, ox);
		oy = std::min(sr.top, oy);
	}


	RECT wr = { 0 }; // window rect
	for (RECT& sr : _realScrRects)
	{
		OffsetRect(&sr, -ox, -oy);

		wr.left = std::min( sr.left, wr.left );
		wr.top = std::min( sr.top, wr.top );
		wr.right = std::max( sr.right, wr.right );
		wr.bottom = std::max( sr.bottom, wr.bottom );
	}

	WinPos winPositions[] = {
		{ 0, L"Big Center", 7, 7, 90, 90 },

		{ 1, L"Left Half", 3, 3, 46, 94 },
		{ 1, L"Right Half", 52, 3, 46, 94 },

		{ 1, L"Left TwoThirds", 3, 3, 62, 94 },
		{ 1, L"Right Third", 68, 3, 30, 94 },

		{ 1, L"Left Third", 3, 3, 30, 94 },
		{ 1, L"Right TwoThirds", 36, 3, 62, 94 },

		{ 2, L"Top Half", 3, 3, 94, 46 },
		{ 2, L"Bottom Half", 3, 52, 94, 46 },

		{ 2, L"Top TwoThirds", 3, 3, 94, 62 },
		{ 2, L"Bottom Third", 3, 68, 94, 30 },

		{ 2, L"Top Third", 3, 3, 94, 30 },
		{ 2, L"Bottom TwoThirds", 3, 36, 94, 62 },

		{ 0, L"Small Center", 20, 20, 60, 60 },

		{ 2, L"Right 1", 5, 03, 90, 15 },
		{ 2, L"Right 2", 5, 19, 90, 15 },
		{ 2, L"Right 3", 5, 35, 90, 15 },
		{ 2, L"Right 4", 5, 51, 90, 15 },
		{ 2, L"Right 5", 5, 67, 90, 15 },
		{ 2, L"Right 6", 5, 83, 90, 15 },

		{ 1, L"Top Left", 3, 3, 42, 42 },
		{ 1, L"Top Right", 56, 3, 42, 42 },
		{ 1, L"Bottom Left", 3, 56, 42, 42 },
		{ 1, L"Bottom Right", 56, 56, 42, 42 }
	};

	for (size_t i = 0; i < _realScrRects.size(); ++i)
	{
		RECT& sr = _realScrRects[i];
		Screen s = {
			i + 1,
			sr.left,
			sr.top,
			sr.right - sr.left,
			sr.bottom - sr.top
		};

		for (WinPos& wp : winPositions) {
			if (wp.scr > 0 && wp.scr != s.scr)
				continue;

			_realPosRects.push_back(Calculate(wp, s));
		}
	}

	_previwScrRects.clear();
	_previewPosRects.clear();
	std::ranges::transform(_realScrRects, std::back_inserter(_previwScrRects),
		[](RECT& r) {return ScaleRect(r, F); });

	std::ranges::transform(_realPosRects, std::back_inserter(_previewPosRects),
		[](RECT& r) {return ScaleRect(r, F); });

	// after all calculations, reset the offset in case of more than one screen with different resolution 
	for (RECT& bpr : _realPosRects)
	{
		OffsetRect(&bpr, ox, oy);
	}

	wr = ScaleRect(wr, F);

	LONG w = (wr.right - wr.left) + GetSystemMetrics(SM_CXBORDER) * 2;
	LONG h = (wr.bottom - wr.top) + GetSystemMetrics(SM_CYBORDER) * 2;

	POINT pt = _clickPoint;

	OffsetRect(&wr, pt.x - (w / 2), pt.y + 10);

	{
		using std::ranges::find_if;
		RECT wr;
		GetWindowRect(hParent, &wr);
		if (auto it = find_if(_realPosRects, [&wr](RECT& r) {return EqualRect(&r, &wr); }); it != _realPosRects.end())
		{
			size_t i = std::distance(_realPosRects.begin(), it);
			_currentPreviewPos = _previewPosRects[i];
		}
	}

	_hWnd = CreateWindowEx(
		WS_EX_TOPMOST, // | WS_EX_TOOLWINDOW,// Optional window styles.
		CLASS_NAME.c_str(),                     // Window class
		L"Pick a rectangle",    // Window text
		WS_POPUP | WS_VISIBLE | WS_BORDER,           
		//WS_OVERLAPPEDWINDOW, // Window style

		// Size and position
		//CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		wr.left, wr.top, w, h,

		hParent,       // Parent window    
		NULL,       // Menu
		hInst,     // Instance handle
		NULL        // Additional application data
	);
	if (_hWnd)
	{
		hWndToImplMap[_hWnd] = this;
		::ShowWindow(_hWnd, SW_SHOW);
		log_debug(L"ShowWindow, ScreenToolWnd::pWnd: {}", (void*)this);

		HMODULE hModDll = GetModuleHandle(BUILD(MT_DLL_NAME));
		if (hModDll != NULL)
		{
			HOOKPROC hkCallKeyboardMsg = (HOOKPROC)GetProcAddress(hModDll, MT_HOOK_PROC_KYB);

			DWORD dwThreadId = ::GetCurrentThreadId();
			_hhkCallKeyboardMsg = SetWindowsHookEx(WH_KEYBOARD, hkCallKeyboardMsg, NULL, dwThreadId);
		}
	}
}

extern HINSTANCE hInst;  // current instance

ScreenToolWnd::Impl::~Impl()
{
	if (hInst) {
		UnhookWindowsHookEx(_hhkCallKeyboardMsg);

		hWndToImplMap.erase(_hWnd);
		//SendMessage(_hWnd, WM_CLOSE, 0, 0);
		if(IsWindow(_hWnd))
			DestroyWindow(_hWnd);
	}
}

LRESULT ScreenToolWnd::Impl::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	using std::ranges::find_if;

	//wstring msg = wm_to_wstring(message);
	//if(!msg.empty())
	//	log_debug(L"Message: {}\n", msg);
	LRESULT res = 0;

	POINT pt = { 0 };
	GetCursorPos(&pt);
	ScreenToClient(hWnd, &pt);

	LONG ix = GetSystemMetrics(SM_CXBORDER) * 2;
	LONG iy = GetSystemMetrics(SM_CXBORDER) * 2;

	if (auto it = find_if(_previwScrRects, [&pt](RECT& r) {return PtInRect(&r, pt); }); it != _previwScrRects.end())
	{
		if (!EqualRect(&*it, &_currentPreviewScr))
		_currentPreviewScr = *it;
		InvalidateRect(hWnd, &_currentPreviewScr, FALSE);
	}
	else
	{
		for(RECT& r : _previwScrRects)
			InvalidateRect(hWnd, &r, FALSE);
		if (!_previwScrRects.empty())
			_currentPreviewScr = _previwScrRects.front();
	}

	auto posRects = std::ranges::filter_view(_previewPosRects, [pt](RECT r) { return PtInRect(&r, pt); });

	switch (message)
	{	
	case WM_MOUSEWHEEL:
	{
		//POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		//ScreenToClient(hWnd, &pt);

		for (RECT r : posRects)
		{
			log_debug(L"PosRects: Left {}, Top {}, Right {}, Bottom {}\n", r.left, r.top, r.right, r.bottom);
		}
		int delta = GET_WHEEL_DELTA_WPARAM(wParam);
		if (auto it = std::ranges::find_if(posRects, [this](RECT& r) {
				bool res = EqualRect(&_currentPreviewPos, &r);
				return res;
			}); it != posRects.end())
		{
			if (delta < 0)
				it = NextPos(it, posRects);
			else if (delta > 0)
				it = PreviousPos(it, posRects);
			_currentPreviewPos = *it;
			auto r = _currentPreviewPos;
			log_debug(L"MouseWheel, CurPos: Left {}, Top {}, Right {}, Bottom {}", r.left, r.top, r.right, r.bottom);
		}
		log_debug(L"MouseWheel, Delta: {}, X: {}, Y: {}\n", delta, pt.x, pt.y);
		break;
	}

	case WM_LBUTTONDOWN:
	{
		if (auto scr_it = std::ranges::find_if(_previwScrRects, [this](RECT& r) { return EqualRect(&_currentPreviewScr, &r); }); scr_it != _previwScrRects.end())
		{
			if (auto pos_it = std::ranges::find_if(_previewPosRects, [this](RECT& r) {return EqualRect(&r, &_currentPreviewPos); }); pos_it != _previewPosRects.end())
			{
				size_t i1 = std::distance(_previewPosRects.begin(), pos_it);
				size_t i2 = std::distance(_previwScrRects.begin(), scr_it);
				if (i1 < _realPosRects.size() && i2 < _realScrRects.size()) 
				{
					RECT r = _realPosRects[i1];
					RECT scr = _realScrRects[i2];

					HWND hParent = GetParent(hWnd);
					SetWindowPos(hParent, HWND_NOTOPMOST, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_SHOWWINDOW);
				}
			}
		}
		ScreenToolWnd::pWnd.reset();
		return  DefWindowProc(hWnd, message, wParam, lParam);
	}

	case WM_MOUSEMOVE:
	{
		static std::deque<POINT> last_pts;
		last_pts.push_back(pt);
		while(last_pts.size() > 10)
			last_pts.pop_front();

		using std::ranges::minmax;
		using std::views::transform;
		using std::distance;
		using std::abs;

		//auto [x_min, x_max] = minmax(transform(last_pts, [](POINT pt) { return pt.x; }));
		auto [y_min, y_max] = minmax(transform(last_pts, [](POINT pt) { return pt.y; }));
		log_debug(L"Min,Max: {}, {} = {}", y_min, y_max, abs(y_max - y_min));
		//if (/*std::abs(x_max - x_min) > 10 ||*/  abs(y_max - y_min) > 10)
		//{
		//	if (auto it = std::ranges::find_if(posRects, [this](RECT& r) {
		//		bool res = EqualRect(&_currentPreviewPos, &r);
		//		return res;
		//		}); it != posRects.end())
		//	{
		//		//if (delta < 0)
		//		it = NextPos(it, posRects);
		//		//else if (delta > 0)
		//		//	it = PreviousPos(it, posRects);
		//		_currentPreviewPos = *it;
		//		auto r = _currentPreviewPos;
		//		//log_debug(L"MouseMove, CurPos: Left {}, Top {}, Right {}, Bottom {}", r.left, r.top, r.right, r.bottom);
		//	}
		//}
		//else
		{
			if (auto it = std::ranges::find_if(
				_previewPosRects.rbegin(),
				_previewPosRects.rend(),
				[pt](RECT& r) {
					bool res = PtInRect(&r, pt);
					return res;
				}); it != _previewPosRects.rend())
			{
				if (last_pts.size() > 10 && EqualRect(&_currentPreviewPos, &*it))
				{

				}
				_currentPreviewPos = *it;
				auto cp = _currentPreviewPos;
				//log_debug(L"MouseMove, CurPos: Left {}, Top {}, Right {}, Bottom {}\n", cp.left, cp.top, cp.right, cp.bottom);
			}
		}
		break;
	}

	case WM_KEYDOWN: 
	{
		if (wParam == VK_ESCAPE)
		{
			ScreenToolWnd::pWnd.reset();
			return  DefWindowProc(hWnd, message, wParam, lParam);
		}
		else if (is_one_of(wParam, VK_DOWN, VK_RIGHT, VK_UP, VK_LEFT))
		{
			if (auto it = std::ranges::find_if(_previewPosRects, [this](RECT& r) {
				bool res = EqualRect(&_currentPreviewPos, &r);
				return res;
				}); it != _previewPosRects.end())
			{
				if (is_one_of(wParam, VK_DOWN, VK_RIGHT))
					it = NextPos(it, _previewPosRects);
				else //if (is_one_of(wParam, VK_UP, VK_LEFT))
					it = PreviousPos(it, _previewPosRects);
				_currentPreviewPos = *it;
			}
			else
			{
				if (!_previewPosRects.empty())
					_currentPreviewPos = _previewPosRects.front();
			}
		}
		else if (wParam == VK_RETURN)
		{
			if (auto scr_it = std::ranges::find_if(_previwScrRects, [this](RECT& r) { return EqualRect(&_currentPreviewScr, &r); }); scr_it != _previwScrRects.end())
			{
				if (auto pos_it = std::ranges::find_if(_previewPosRects, [this](RECT& r) {return EqualRect(&r, &_currentPreviewPos); }); pos_it != _previewPosRects.end())
				{
					size_t i1 = std::distance(_previewPosRects.begin(), pos_it);
					size_t i2 = std::distance(_previwScrRects.begin(), scr_it);
					if (i1 < _realPosRects.size() && i2 < _realScrRects.size())
					{
						RECT r = _realPosRects[i1];
						RECT scr = _realScrRects[i2];

						HWND hParent = GetParent(hWnd);
						SetWindowPos(hParent, HWND_NOTOPMOST, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_SHOWWINDOW);
					}
				}
			}

			ScreenToolWnd::pWnd.reset();
			return  DefWindowProc(hWnd, message, wParam, lParam);
		}
		//else
		//{
		//	wchar_t strDataToSend[32];
		//	wsprintf(strDataToSend, L"%c", wParam);
		//	MessageBox(NULL, strDataToSend, L"keyselected", MB_OK);
		//}
		break;

	} // end switch

	case WM_PAINT:
	{
		//InflateRect(&currentPart, -ix * 4, -iy * 4);

		PAINTSTRUCT ps;
		HDC hDC = BeginPaint(hWnd, &ps);

		for (RECT sr : _previwScrRects) // screen rect		
		{
			InflateRect(&sr, -ix, -iy);
			FillRect(hDC, &sr, GetSysColorBrush(COLOR_ACTIVEBORDER));
			FrameRect(hDC, &sr, GetSysColorBrush(COLOR_HOTLIGHT));			
		}

		for (RECT pr : _previewPosRects) // part rect	
		{
			//InflateRect(&pr, -ix * 4, -iy * 4);
			FillRect(hDC, &pr, GetSysColorBrush(COLOR_INACTIVECAPTION));
			FrameRect(hDC, &pr, GetSysColorBrush(COLOR_HOTLIGHT));
			//std::wostringstream os;
			//os << L"x: " << pr.left << L" y: " << pr.top << L" w: " << pr.right - pr.left << L" h: " << pr.bottom - pr.top;
			//DrawText(hDC, os.str().c_str(), -1, &pr, DT_LEFT | DT_TOP | DT_WORDBREAK);
		}

		FillRect(hDC, &_currentPreviewPos, GetSysColorBrush(COLOR_ACTIVECAPTION));
		FrameRect(hDC, &_currentPreviewPos, GetSysColorBrush(COLOR_HOTLIGHT));

		EndPaint(hWnd, &ps);
		break;
	}

	} // end switch

	return  DefWindowProc(hWnd, message, wParam, lParam);
}



template<typename It, typename Ct>
inline It ScreenToolWnd::Impl::NextPos(It it, Ct& posRects)
{
	if (it == posRects.end())
		it = posRects.begin();
	else if (++it == posRects.end())
		it = posRects.begin();
	return it;
}

template<typename It, typename Ct>
inline It ScreenToolWnd::Impl::PreviousPos(It it, Ct& posRects)
{
	if (it == posRects.begin())
		it = (--posRects.end());
	else if (--it == posRects.begin())
		it = (--posRects.end());
	return it;
}


RECT ScreenToolWnd::Impl::Calculate(WinPos& wp, Screen& s)
{
	RECT r = { 
		static_cast<LONG>(s.x + wp.x * (s.w / 100.0)),
		static_cast<LONG>(s.y + wp.y * (s.h / 100.0)),
	};
	r.right = static_cast<LONG>(r.left + wp.w * (s.w / 100.0));
	r.bottom = static_cast<LONG>(r.top + wp.h * (s.h / 100.0));
	return r;
}
