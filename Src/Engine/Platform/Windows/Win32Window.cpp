#include "Win32Window.h"
#include "Engine/Platform/Window.h"

struct Window
{
    HWND Handle;
    String8 Title;
    uint32 Width, Height;
    bool Minimized;
    uint32 Flags;
};
static Window WindowData = {};

static LRESULT CALLBACK Win32WindowProcedure(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_CLOSE:
        {
            DestroyWindow(windowHandle);
        } break;

        case WM_DESTROY:
        {
            PostQuitMessage(0);
        } break;

        case WM_SIZE:
        {
            if(wParam == SIZE_MINIMIZED)
            {
                WindowData.Minimized = true;
                break;
            }

            WindowData.Width = LOWORD(lParam);
            WindowData.Height = HIWORD(lParam);
            WindowData.Minimized = false;
        } break;

        default:
        {
            return(DefWindowProcW(windowHandle, message, wParam, lParam));
        }
    }

    return(0);
}

bool Win32WindowCreate(StackAllocator* allocator, StringView8 title, uint32 width, uint32 height)
{
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(WNDCLASSEXW);
    windowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = Win32WindowProcedure;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(IDC_ARROW));
    windowClass.lpszClassName = L"SunbirdWin32WindowClass";

    if(!RegisterClassExW(&windowClass))
    {
        return(false);
    }

    uint32 windowStyle = WS_OVERLAPPEDWINDOW;
    uint32 windowWidth = width;
    uint32 windowHeight = height;
    uint32 windowX = CW_USEDEFAULT;
    uint32 windowY= CW_USEDEFAULT;

    if(WindowData.Flags & WindowFlags_Fullscreen)
    {
        windowStyle = WS_POPUP;
        windowWidth = GetSystemMetrics(SM_CXSCREEN);
        windowHeight = GetSystemMetrics(SM_CYSCREEN);
        windowX = 0;
        windowY = 0;
    }
    else
    {
        RECT windowClientArea = { 0, 0, (LONG)width, (LONG)height };
        AdjustWindowRectExForDpi(&windowClientArea, (DWORD)windowStyle, FALSE, 0, GetDpiForSystem());

        windowWidth = (uint32)(windowClientArea.right - windowClientArea.left);
        windowHeight = (uint32)(windowClientArea.bottom - windowClientArea.top);
    }

    Frame frameScratch = GetFrame(allocator, Heap::Upper);
    HWND windowHandle = CreateWindowExW(0,
                                        L"SunbirdWin32WindowClass",
                                        (LPCWSTR)((SV8ToSV16(allocator, title)).Data),
                                        (DWORD)windowStyle,
                                        (int)windowX, (int)windowY,
                                        (int)windowWidth, (int)windowHeight,
                                        nullptr,
                                        nullptr,
                                        GetModuleHandleW(nullptr),
                                        nullptr);
    ReleaseFrame(allocator, frameScratch);

    if(!windowHandle)
    {
        UnregisterClassW(L"SunbirdWin32WindowClass", GetModuleHandleW(nullptr));
        return(false);
    }

    WindowData.Handle = windowHandle;
    WindowData.Title = String8FromView(allocator, title);

    RECT windowClientArea = {};
    GetClientRect(windowHandle, &windowClientArea);
    WindowData.Width = (uint32)(windowClientArea.right - windowClientArea.left);
    WindowData.Height = (uint32)(windowClientArea.bottom - windowClientArea.top);

    WindowData.Minimized = false;

    return(true);
}

bool Win32WindowPumpEvents()
{
    MSG message;
    while(PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
    {
        if(message.message == WM_QUIT)
        {
            return(false);
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return(true);
}

void Win32WindowShutdown()
{
    UnregisterClassW(L"SunbirdWin32WindowClass", GetModuleHandleW(nullptr));
}

HWND Win32WindowGetHandle()
{
    return WindowData.Handle;
}

void WindowSetFlags(uint32 windowFlags)
{
    WindowData.Flags = windowFlags;
}

void WindowGetDimensions(uint32* width, uint32* height)
{
    *width = WindowData.Width;
    *height = WindowData.Height;
}

bool WindowGetMinimized()
{
    return WindowData.Minimized;
}
