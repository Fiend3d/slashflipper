#include <iostream>
#include <thread>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_pixels.h>
#include <windows.h>

#include "resources/resource.h"

class ScopedMutex {
public:
    ScopedMutex(const char* name) 
    {
        hMutex_ = CreateMutex(NULL, TRUE, name);
        isOwner_ = (GetLastError() != ERROR_ALREADY_EXISTS);
    }

    ~ScopedMutex() 
    {
        if (hMutex_) 
        {
            if (isOwner_) 
            {
                ReleaseMutex(hMutex_);
            }
            CloseHandle(hMutex_);
        }
    }

    bool isOwner() const { return isOwner_; }
    operator bool() const { return hMutex_ != NULL; }

private:
    HANDLE hMutex_ = NULL;
    bool isOwner_ = false;
};

HHOOK g_keyboardHook = NULL;
bool g_winPressed = false;
bool g_enabled = true;
bool g_reverse = false;

void simulate_button(BYTE bVk, DWORD dWFlag)
{
    keybd_event(bVk, 0, dWFlag, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
};

void simulate_ctrl_v()
{
    simulate_button('V', KEYEVENTF_KEYUP); // v up
    simulate_button(VK_LWIN, KEYEVENTF_KEYUP); // win up
    simulate_button(VK_CONTROL, 0); // ctrl down
    simulate_button('V', 0); // v down
    simulate_button(VK_CONTROL, KEYEVENTF_KEYUP); // ctrl up
    simulate_button(VK_LWIN, 0); // win down, so it feels good
}

void on_shortcut_pressed()
{
    if (g_enabled)
    {
        char* clipboard = SDL_GetClipboardText();
        std::string text = clipboard;
        SDL_free(clipboard);

        if (g_reverse)
        {
            for (char& c : text)
            {
                if (c == '/')
                {
                    c = '\\';
                }
            }
        }
        else
        {
            for (char& c : text)
            {
                if (c == '\\')
                {
                    c = '/';
                }
            }
        }

        std::cout << text << std::endl;

        SDL_SetClipboardText(text.c_str());
        simulate_ctrl_v();
    }
}

// Keyboard hook procedure
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0)
    {
        KBDLLHOOKSTRUCT* kb = (KBDLLHOOKSTRUCT*)lParam;

        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
        {
            // Check for modifier keys
            if (kb->vkCode == VK_LWIN)
                g_winPressed = true;

            // Check for our hotkey combination
            else if (kb->vkCode == 'V' && g_winPressed)
            {
                on_shortcut_pressed();
                return 1;
            }
        }
        else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
        {
            if (kb->vkCode == VK_LWIN)
                g_winPressed = false;
        }
    }

    return CallNextHookEx(g_keyboardHook, nCode, wParam, lParam);
}

void callback_quit(void* userdata, SDL_TrayEntry* invoker)
{
    SDL_Event e;
    e.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&e);
}

void callback_reverse(void* userdata, SDL_TrayEntry* invoker)
{
    g_reverse = SDL_GetTrayEntryChecked(invoker);
    std::cout << "callback_reverse " << g_enabled << std::endl;
}

void callback_enable(void* userdata, SDL_TrayEntry* invoker)
{
    g_enabled = SDL_GetTrayEntryChecked(invoker);
    std::cout << "callback_enable " << g_enabled << std::endl;
}

SDL_Surface* LoadIconFromResource(HINSTANCE hInstance, int resourceID)
{
    HICON hIcon = (HICON)LoadImage(
        hInstance,
        MAKEINTRESOURCE(resourceID),
        IMAGE_ICON,
        0, 0,
        LR_DEFAULTSIZE | LR_SHARED
    );

    if (!hIcon) 
    {
        return NULL;
    }

    ICONINFO iconInfo;
    if (!GetIconInfo(hIcon, &iconInfo)) 
    {
        DestroyIcon(hIcon);
        return NULL;
    }

    BITMAP bmp;
    GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bmp);

    // Create SDL surface
    SDL_Surface* surface = SDL_CreateSurface(
        bmp.bmWidth,
        bmp.bmHeight,
        SDL_PIXELFORMAT_ARGB8888
    );

    if (!surface) 
    {
        DeleteObject(iconInfo.hbmColor);
        DeleteObject(iconInfo.hbmMask);
        DestroyIcon(hIcon);
        return NULL;
    }

    // Copy pixels from HICON into SDL surface
    HDC hdc = GetDC(NULL);
    HDC hMemDC = CreateCompatibleDC(hdc);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, iconInfo.hbmColor);

    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bmp.bmWidth;
    bmi.bmiHeader.biHeight = -bmp.bmHeight; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    GetDIBits(hMemDC, iconInfo.hbmColor, 0, bmp.bmHeight,
        surface->pixels, &bmi, DIB_RGB_COLORS);

    // Cleanup
    SelectObject(hMemDC, hOldBmp);
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hdc);
    DeleteObject(iconInfo.hbmColor);
    DeleteObject(iconInfo.hbmMask);
    DestroyIcon(hIcon);

    return surface;
}

int main(int argc, char* argv[])
{
    ScopedMutex mutex("Global.SlashFlipper");

    if (!mutex.isOwner())
    {
        std::cout << "Another instance is running!" << std::endl;
        return 0;
    }

    // Set low-level keyboard hook
    g_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(NULL), 0);

    if (!g_keyboardHook)
    {
        std::cerr << "Failed to set keyboard hook!" << std::endl;
        return 1;
    }

    SDL_Event e;
    SDL_Init(SDL_INIT_VIDEO);

    HINSTANCE hInstance = GetModuleHandle(NULL);
    SDL_Surface* icon = LoadIconFromResource(hInstance, IDI_ICON1);

    SDL_Tray* tray = SDL_CreateTray(icon, "SlashFlipper");
    SDL_TrayMenu* menu = SDL_CreateTrayMenu(tray);

    SDL_TrayEntry* menu_enable = SDL_InsertTrayEntryAt(menu, -1, "Enable", SDL_TRAYENTRY_CHECKBOX);
    SDL_SetTrayEntryChecked(menu_enable, g_enabled);
    SDL_TrayEntry* menu_reverse = SDL_InsertTrayEntryAt(menu, -1, "Reverse", SDL_TRAYENTRY_CHECKBOX);
    SDL_SetTrayEntryChecked(menu_reverse, g_reverse);

    SDL_TrayEntry* menu_quit = SDL_InsertTrayEntryAt(menu, -1, "Quit", SDL_TRAYENTRY_BUTTON);

    SDL_SetTrayEntryCallback(menu_reverse, callback_reverse, NULL);
    SDL_SetTrayEntryCallback(menu_enable, callback_enable, NULL);
    SDL_SetTrayEntryCallback(menu_quit, callback_quit, NULL);

    while (true)
    {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                goto quit;
            }
        }

        SDL_Delay(10);
    }
quit:

    SDL_DestroyTray(tray);
    UnhookWindowsHookEx(g_keyboardHook);

    SDL_Quit();

    return 0;
}
