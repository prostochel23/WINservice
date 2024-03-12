
#include <windows.h>
#include <iostream>
#include <strsafe.h>
#include "Header.h"
//#include <stdafx.h>

LSTATUS __cdecl deleteRegisteryKeys()
{
    TCHAR szBuf[MAX_PATH];
    size_t cchSize = MAX_PATH;
    HRESULT hr = StringCchPrintf(szBuf, cchSize,
        L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\%s", SVCNAME);
    RegDeleteKeyEx(HKEY_LOCAL_MACHINE, szBuf, KEY_WOW64_32KEY, 0);
    return ERROR_SUCCESS;
}
int __cdecl createRegistryKeys()
{
    // Name of the event log.
    wchar_t logName[] = L"Application";
    // Event Source name.
    wchar_t sourceName[] = L"SampleEventSourceName";
    // DLL that contains the event messages (descriptions).
    wchar_t dllName[] = L"C:\\Users\\vlady\\source\\repos\\windowsService\\windowsService\\sample.dll";
    // This number of categories for the event source.
    DWORD dwCategoryNum = 1;

    HKEY hk;
    DWORD dwData, dwDisp;
    TCHAR szBuf[MAX_PATH];
    size_t cchSize = MAX_PATH;

    // HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\EventLog\Application\SvcName

    // Create the event source as a subkey of the log. 
    HRESULT hr = StringCchPrintf(szBuf, cchSize,
        L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\%s", SVCNAME);

    if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, szBuf,
        0, NULL, REG_OPTION_NON_VOLATILE,
        KEY_WRITE, NULL, &hk, &dwDisp))
    {
        printf("Could not create the registry key.");
        return 0;
    }

    // Set the name of the message file. 

    if (RegSetValueEx(hk,             // subkey handle 
        L"EventMessageFile",        // value name 
        0,                         // must be zero 
        REG_EXPAND_SZ,             // value type 
        (LPBYTE)dllName,          // pointer to value data 
        (DWORD)(lstrlen(dllName) + 1) * sizeof(TCHAR))) // data size
    {
        printf("Could not set the event message file.");
        RegCloseKey(hk);
        return 0;
    }

    // Set the supported event types. 

    dwData = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE |
        EVENTLOG_INFORMATION_TYPE;

    if (RegSetValueEx(hk,      // subkey handle 
        L"TypesSupported",  // value name 
        0,                 // must be zero 
        REG_DWORD,         // value type 
        (LPBYTE)&dwData,  // pointer to value data 
        sizeof(DWORD)))    // length of value data 
    {
        printf("Could not set the supported types.");
        RegCloseKey(hk);
        return 0;
    }

    // Set the category message file and number of categories.

    //if (RegSetValueEx(hk,              // subkey handle 
    //    L"CategoryMessageFile",     // value name 
    //    0,                         // must be zero 
    //    REG_EXPAND_SZ,             // value type 
    //    (LPBYTE)dllName,          // pointer to value data 
    //    (DWORD)(lstrlen(dllName) + 1) * sizeof(TCHAR))) // data size
    //{
    //    printf("Could not set the category message file.");
    //    RegCloseKey(hk);
    //    return 0;
    //}

    //if (RegSetValueEx(hk,            // subkey handle 
    //    L"CategoryCount",         // value name 
    //    0,                       // must be zero 
    //    REG_DWORD,               // value type 
    //    (LPBYTE)&dwCategoryNum, // pointer to value data 
    //    sizeof(DWORD)))          // length of value data 
    //{
    //    printf("Could not set the category count.");
    //    RegCloseKey(hk);
    //    return 0;
    //}

    RegCloseKey(hk);
    return 1;
}