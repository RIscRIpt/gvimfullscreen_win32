#include <Windows.h>

const char *szgVimWndStatus = "gVimWndStatus";

HWND hTop = NULL;
HWND hTextArea = NULL;

typedef struct {
	RECT win_rect;
	LONG_PTR win_style;
	LONG_PTR win_exstyle;
	LONG_PTR txt_exstyle;
	char isZoomed;
} gvim_wnd_status_t;

size_t dex2bin(char *dest, const char *src, size_t size) {
	while(size-- && src[0] && src[1]) {
		*(dest++) = ((src[0] & 0x0F) << 4) | (src[1] & 0x0F);
		src += 2;
	}
	return size;
}

void bin2dex(char *dest, const char *src, size_t size) {
	while(size--) {
		*(dest++) = '0' | (*src >> 4);
		*(dest++) = '0' | (*src & 0x0F);
		src++;
	}
	*dest = '\0';
}

BOOL CALLBACK FindWindowProc(HWND hWnd, LPARAM lParam) {
	if(GetParent(hWnd))
		return TRUE;
	hTop = hWnd;
	return FALSE;
}

int Init() {
	EnumThreadWindows(GetCurrentThreadId(), FindWindowProc, NULL);
	hTextArea = FindWindowEx(hTop, NULL, "VimTextArea", "Vim text area");
	return hTop != NULL && hTextArea != NULL;
}

LONG _declspec(dllexport) ToggleFullScreen(COLORREF border_color) {
	if(Init()) {
		gvim_wnd_status_t status;
		char str_hex_status[sizeof(gvim_wnd_status_t) * 2 + 1];

		GetEnvironmentVariableA(szgVimWndStatus, str_hex_status, sizeof(str_hex_status));

		//
		// Check if gVim Window Status environment variable was set
		//
		if(GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
			//
			// No window status was found, try to go to fullscreen
			// Save current gVim window status
			//
			status.isZoomed    = IsZoomed(hTop) ? 1 : 0;
			status.win_style   = GetWindowLongPtr(hTop,      GWL_STYLE);
			status.win_exstyle = GetWindowLongPtr(hTop,      GWL_EXSTYLE);
			status.txt_exstyle = GetWindowLongPtr(hTextArea, GWL_EXSTYLE);
			GetWindowRect(hTop, &status.win_rect);

			//
			// Encode current window status,
			// and try to set it up
			//
			bin2dex(str_hex_status, &status, sizeof(status));
			if(SetEnvironmentVariableA(szgVimWndStatus, str_hex_status, sizeof(str_hex_status))) {
				//
				// Environment variable of window status was successfully set up.
				// Enable fullscreen mode
				//
				HDC hDC;
				HMONITOR hMonitor;
				MONITORINFO mi;

				if((hDC = GetDC(hTextArea)) != NULL) {
					if(border_color == -1) {
						//
						// Try to get precise color of background, if it wasn't specified
						//
						RECT rc;
						if(GetClientRect(hTextArea, &rc))
							border_color = GetPixel(hDC, rc.right - 2, rc.bottom - 2);
					}
					SetDCBrushColor(hDC, border_color);
					SetClassLongPtr(hTextArea, GCLP_HBRBACKGROUND, GetStockObject(DC_BRUSH));
					ReleaseDC(hTextArea, hDC);
				}

				//
				// Get the work area or entire monitor rect.
				//
				hMonitor = MonitorFromRect(&status.win_rect, MONITOR_DEFAULTTONEAREST);
				mi.cbSize = sizeof(mi);
				GetMonitorInfo(hMonitor, &mi);

				//
				// Restore window if it is maximized
				//
				if(status.isZoomed)
					SendMessage(hTop, WM_SYSCOMMAND, SC_RESTORE, 0);

				//
				// Remove borders
				//
				SetWindowLongPtr(hTop,      GWL_STYLE,   status.win_style   & ~(
							WS_OVERLAPPEDWINDOW |
							WS_BORDER));
				SetWindowLongPtr(hTop,      GWL_EXSTYLE, status.win_exstyle & ~(
							WS_EX_OVERLAPPEDWINDOW |
							WS_EX_DLGMODALFRAME |
							WS_EX_STATICEDGE));
				SetWindowLongPtr(hTextArea, GWL_EXSTYLE, status.txt_exstyle & ~(
							WS_EX_OVERLAPPEDWINDOW |
							WS_EX_DLGMODALFRAME |
							WS_EX_STATICEDGE));

				//
				// Set position to fill entire screen
				//
				SetWindowPos(hTop,
						HWND_TOP,
						mi.rcMonitor.left,
						mi.rcMonitor.top,
						mi.rcMonitor.right - mi.rcMonitor.left,
						mi.rcMonitor.bottom - mi.rcMonitor.top,
						SWP_SHOWWINDOW
						);
				SetWindowPos(
						hTextArea,
						HWND_TOP,
						0,
						0,
						mi.rcMonitor.right - mi.rcMonitor.left,
						mi.rcMonitor.bottom - mi.rcMonitor.top,
						SWP_SHOWWINDOW);
			}
		} else {
			//
			// Try to decode previous gVim window status
			//
			if(dex2bin(&status, str_hex_status, sizeof(status)) != 0) {
				SetWindowLongPtr(hTop,      GWL_STYLE,   status.win_style);
				SetWindowLongPtr(hTop,      GWL_EXSTYLE, status.win_exstyle);
				SetWindowLongPtr(hTextArea, GWL_EXSTYLE, status.txt_exstyle);
				SetWindowPos(
						hTop,
						HWND_TOP,
						status.win_rect.left,
						status.win_rect.top,
						status.win_rect.right - status.win_rect.left,
						status.win_rect.bottom - status.win_rect.top,
						SWP_SHOWWINDOW
						);
				if(status.isZoomed)
					SendMessage(hTop, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
			}
			//
			// Remove previous window status,
			// as full screen mode must has been disabled
			//
			SetEnvironmentVariable(szgVimWndStatus, NULL, 0);
		}
	}
	return GetLastError();
}

LONG _declspec(dllexport) SetAlpha(LONG nTrans) {
	if(Init()) {
		LONG_PTR exs = GetWindowLongPtr(hTop, GWL_EXSTYLE);
		if(nTrans == 255) {
			if(exs & WS_EX_LAYERED)
				SetWindowLongPtr(hTop, GWL_EXSTYLE, exs & ~WS_EX_LAYERED);
		} else {
			if(!(exs & WS_EX_LAYERED))
				SetWindowLongPtr(hTop, GWL_EXSTYLE, exs |  WS_EX_LAYERED);
			SetLayeredWindowAttributes(hTop, 0, (BYTE)nTrans, LWA_ALPHA);
		}
	}
	return GetLastError();
}

LONG _declspec(dllexport) AddAlpha(LONG nAlpha) {
	if(Init()) {
		LONG nCurAlpha;
		LONG_PTR exs;

		exs = GetWindowLongPtr(hTop, GWL_EXSTYLE);

		if(!(exs & WS_EX_LAYERED)) {
			nCurAlpha = 255;
			SetWindowLongPtr(hTop, GWL_EXSTYLE, exs | WS_EX_LAYERED);
		} else {
			nCurAlpha = 0;
			GetLayeredWindowAttributes(hTop, NULL, &nCurAlpha, LWA_ALPHA);
		}

		nCurAlpha += nAlpha;
		if(nCurAlpha > 255)
			nCurAlpha = 255;
		else if(nCurAlpha < 0)
			nCurAlpha = 0;

		SetLayeredWindowAttributes(hTop, 0, nCurAlpha, LWA_ALPHA);
		if(nCurAlpha == 255)
			SetWindowLongPtr(hTop, GWL_EXSTYLE, exs & ~WS_EX_LAYERED);
	}
	return GetLastError();
}

LONG _declspec(dllexport) EnableTopMost(LONG bEnable) {
	if(Init())
		SetWindowPos(
				hTop,
				bEnable ? HWND_TOPMOST : HWND_NOTOPMOST,
				0, 0, 0, 0,
				SWP_NOSIZE | SWP_NOMOVE
				);
	return GetLastError();
}

