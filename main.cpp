#include <windows.h>
#include <D3dkmthk.h>
#include <chrono>

#include "strobe-api/strobe/strobe-core.h"

using namespace std::chrono;

const char g_szClassName[] = "desktopBFIwindowClass";
bool quitProgram = false;
bool frameVisible;

StrobeAPI* strobe = nullptr;
char* strobeInfo = (char*)malloc(sizeof(char) * 4096);
bool debugMode = false;
int frameSnapshot = 0;

// Window event handling
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);
		if (strobe && debugMode && strobeInfo && strobe->frameCount(StrobeAPI::TotalFrame) - frameSnapshot >= 10)
		{
			RECT rec;
			SetRect(&rec, 10, 10, 500, 600);
			HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
			FillRect(hdc, &rec, brush);
			snprintf(strobeInfo, sizeof(char) * 4096, "%s", strobe->getDebugInformation());
			DrawText(hdc, strobeInfo, strlen(strobeInfo), &rec, DT_TOP | DT_LEFT);
			frameSnapshot = strobe->frameCount(StrobeAPI::TotalFrame);
		}
		EndPaint(hwnd, &ps);
		ReleaseDC(hwnd, hdc);
		break;
	}
	case WM_CLOSE:
		quitProgram = true;
		DestroyWindow(hwnd);
		break;
	case WM_DESTROY:
		quitProgram = true;
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

	WNDCLASSEX wc;
	HWND hwnd;
	MSG Msg;

	// Registering the Window Class
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = g_szClassName;
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	if (!RegisterClassEx(&wc))
	{
		MessageBox(NULL, "Window Registration Failed!", "Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	// Window ignores mouseclicks thanks to this advice:
	// https://stackoverflow.com/questions/31313624/click-through-transparent-window-no-dragging-allowed-c

	// Creating the Window
	hwnd = CreateWindowEx(
		WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
		g_szClassName,
		"DesktopBFI",
		WS_POPUP,
		0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
		NULL, NULL, hInstance, NULL);

	if (hwnd == NULL)
	{
		MessageBox(NULL, "Window Creation Failed!", "Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	LPWSTR* szArglist;
	int argCount;
	szArglist = CommandLineToArgvW(GetCommandLineW(), &argCount);
	if (argCount == 4)
	{
		int sMode = _wtoi(szArglist[1]);
		int pSInterval = _wtoi(szArglist[2]);
		debugMode = _wtoi(szArglist[3]);

		strobe = new StrobeCore(sMode, pSInterval);
	}
	else
	{
		strobe = new StrobeCore();
	}


	// Setting up to do V-sync. Guidance came from:
	// https://www.vsynctester.com/firefoxisbroken.html
	// https://gist.github.com/anonymous/4397e4909c524c939bee#file-gistfile1-txt-L3
	D3DKMT_WAITFORVERTICALBLANKEVENT we;
	D3DKMT_OPENADAPTERFROMHDC oa;
	oa.hDc = GetDC(hwnd);
	NTSTATUS result = D3DKMTOpenAdapterFromHdc(&oa);
	if (result == STATUS_INVALID_PARAMETER) {
		MessageBox(NULL, "D3DKMTOpenAdapterFromHdc function received an invalid parameter.", "Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}
	else if (result == STATUS_NO_MEMORY) {
		MessageBox(NULL, "D3DKMTOpenAdapterFromHdc function, kernel ran out of memory.", "Error!",
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}
	we.hAdapter = oa.hAdapter;
	we.hDevice = 0;
	we.VidPnSourceId = oa.VidPnSourceId;

	// Sets up for D3DKTGetScanLine(), to poll for VBlank exit
	D3DKMT_GETSCANLINE gsl;
	gsl.hAdapter = oa.hAdapter;
	gsl.VidPnSourceId = oa.VidPnSourceId;

	while (!quitProgram)
	{
		result = D3DKMTWaitForVerticalBlankEvent(&we);
		do {
			high_resolution_clock::time_point pollTime = high_resolution_clock::now() + microseconds(100);
			while (pollTime < high_resolution_clock::now())
			{
			}
			result = D3DKMTGetScanLine(&gsl);
		} while (gsl.InVerticalBlank == TRUE);

		if (strobe)
			frameVisible = strobe->strobe();
		else
			frameVisible = true;

		// Window transparency: 0 is invisible, 255 is opaque
		SetLayeredWindowAttributes(hwnd, 0, (frameVisible ? 0 : 1) * 255, LWA_ALPHA);
		RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE);

		while (PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE) > 0) {
			TranslateMessage(&Msg);
			DispatchMessage(&Msg);
		}
	}

	delete strobeInfo;
	delete strobe;

	return Msg.wParam;
}
