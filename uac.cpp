#include <windows.h>
#include <iostream>
using namespace std;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, LPSTR lpcommand, int nShowCmd)
{
    int argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc < 2) {
        cout << "You must specify a command!" << endl;
        return 1;
    }
    LPWSTR args = NULL;
    if (argc >= 3) {
        wstring arguments = wstring(argv[2]);
        for (int i=3; i<argc; i++)
            arguments += wstring(argv[i]);
        args = (LPWSTR)arguments.c_str();
    }

    ShellExecuteW(
        NULL,
        L"runas",
        argv[1],
        args,
        NULL,
        SW_SHOWNORMAL
    );
    return 0;
}
