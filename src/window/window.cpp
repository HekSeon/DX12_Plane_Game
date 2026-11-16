#include "window.h"

Window::~Window()
{
    if (m_hwnd)
        DestroyWindow(m_hwnd);
}

bool Window::Create(const wchar_t *title, uint32_t width, uint32_t height)
{
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"DX12PlaneGameWnd";
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return false;

    RECT rect{0, 0, LONG(width), LONG(height)};
    if (!AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE))
        return false;
    m_width = width;
    m_height = height;
    m_hwnd = CreateWindowExW(0, wc.lpszClassName, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                             rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, instance, this);
    if (!m_hwnd)
        return false;
    ShowWindow(m_hwnd, SW_SHOW);
    return true;
}

bool Window::PumpMessages()
{
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
    {
        if (message.message == WM_QUIT)
            return false;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return true;
}

bool Window::ConsumeResize(uint32_t &width, uint32_t &height)
{
    if (!m_resized)
        return false;
    m_resized = false;
    width = m_width;
    height = m_height;
    return true;
}

LRESULT CALLBACK Window::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Window *self = reinterpret_cast<Window *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        auto create = reinterpret_cast<CREATESTRUCTW *>(lParam);
        self = static_cast<Window *>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self ? self->HandleMessage(hwnd, message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT Window::HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            m_width = LOWORD(lParam);
            m_height = HIWORD(lParam);
            m_resized = true;
        }
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_NCDESTROY:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        m_hwnd = nullptr;
        break;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}
