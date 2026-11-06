#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

static int check_tracking(HWND tooltip, TOOLINFOW *tool)
{
    if (!SendMessageW(tooltip, TTM_ADDTOOLW, 0, (LPARAM)tool))
    {
        fputs("Could not register the tool.\n", stderr);
        return 1;
    }

    SendMessageW(tooltip, TTM_TRACKPOSITION, 0, MAKELPARAM(100, 100));
    SendMessageW(tooltip, TTM_TRACKACTIVATE, TRUE, (LPARAM)tool);
    if (!IsWindowVisible(tooltip))
    {
        fputs("Tracking tooltip did not appear.\n", stderr);
        return 1;
    }

    SendMessageW(tooltip, TTM_DELTOOLW, 0, (LPARAM)tool);
    SendMessageW(tooltip, TTM_TRACKACTIVATE, FALSE, (LPARAM)tool);
    if (IsWindowVisible(tooltip))
    {
        fputs("Tracking tooltip stayed visible after deletion and deactivation.\n", stderr);
        return 1;
    }

    return 0;
}

int main(void)
{
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_WIN95_CLASSES};
    WCHAR text[] = L"Tracking tooltip";
    TOOLINFOW tool = {0};
    HWND owner, tooltip;
    int result;

    if (!InitCommonControlsEx(&controls))
        return 1;
    owner = CreateWindowExW(0, L"Static", L"Tooltip owner", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                            0, 0, 320, 240, NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (!owner)
        return 1;
    tooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, NULL, WS_POPUP | TTS_ALWAYSTIP,
                              0, 0, 0, 0, owner, NULL, GetModuleHandleW(NULL), NULL);
    if (!tooltip)
    {
        DestroyWindow(owner);
        return 1;
    }

    tool.cbSize = TTTOOLINFOW_V2_SIZE;
    tool.uFlags = TTF_TRACK | TTF_ABSOLUTE;
    tool.hwnd = owner;
    tool.uId = 1;
    tool.lpszText = text;
    result = check_tracking(tooltip, &tool);
    if (!result)
        result = check_tracking(tooltip, &tool);
    DestroyWindow(tooltip);
    DestroyWindow(owner);
    return result;
}
