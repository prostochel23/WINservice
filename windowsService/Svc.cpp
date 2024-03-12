#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <string>
#include "sample.h"
#include "Header.h"

#include "wtsapi32.h"
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "wtsapi32.lib")



SERVICE_STATUS          gSvcStatus;
SERVICE_STATUS_HANDLE   gSvcStatusHandle;
HANDLE                  ghSvcStopEvent = NULL;
SERVICE_STATUS_PROCESS  ssp;
PROCESS_INFORMATION     pInfo;

int __cdecl _tmain(int argc, TCHAR* argv[]);

VOID SvcInstall(void);
VOID SvcUninstall(void);
VOID WINAPI SvcCtrlHandler(DWORD);
VOID WINAPI SvcMain(DWORD, LPTSTR*);

VOID ReportSvcStatus(DWORD, DWORD, DWORD);
VOID SvcInit(DWORD, LPTSTR*);
VOID SvcReportEvent(LPTSTR);
VOID StartSvc();
VOID QueryStatus(SC_HANDLE schService);
VOID StopSvc();
VOID RunUI();

VOID RunUI()
{
    PWTS_SESSION_INFOW wtsSessions = NULL;
    DWORD sessionCount;
    if (WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &wtsSessions, &sessionCount))
    {
        for (DWORD i = 0; i < sessionCount; i++)
        {
            auto wtsSession = wtsSessions[i].SessionId;
            if (wtsSession != 0) {
                HANDLE userToken;
                if (WTSQueryUserToken(wtsSession, &userToken))
                {
                    STARTUPINFO sInfo = { sizeof(sInfo) };
                    TCHAR desktop[] = TEXT("winsta0\\default");
                    sInfo.lpDesktop = desktop;
                    CreateProcessAsUser(userToken, L"C:\\Windows\\System32\\calc.exe", NULL, NULL, NULL, FALSE, 0,
                        NULL,
                        NULL,
                        &sInfo,
                        &pInfo);
                    return;
                }
            }
        }
    }
}


//
// Purpose: 
//   Entry point for the process
//
// Parameters:
//   None
// 
// Return value:
//   None, defaults to 0 (zero)
//
int __cdecl _tmain(int argc, TCHAR* argv[])
{
    // If command-line parameter is "install", install the service. 
    // Otherwise, the service is probably being started by the SCM.

    if (lstrcmpi(argv[1], TEXT("install")) == 0)
    {
        SvcInstall();
        return 0;
    }
    else if (lstrcmpi(argv[1], TEXT("uninstall")) == 0)
    {
        SvcUninstall();
        return 0;
    }
    else if (lstrcmpi(argv[1], TEXT("start")) == 0)
    {
        StartSvc();
        return 0;
    }
    else if (lstrcmpi(argv[1], TEXT("stop")) == 0)
    {
        StopSvc();
        return 0;
    }
    else {
        printf("Unknown choice\n");
    }

    // TO_DO: Add any additional services for the process to this table.
    SERVICE_TABLE_ENTRY DispatchTable[] =
    {
        { (wchar_t*)SVCNAME, (LPSERVICE_MAIN_FUNCTION)SvcMain },
        { NULL, NULL }
    };

    // This call returns when the service has stopped. 
    // The process should simply terminate when the call returns.

    if (!StartServiceCtrlDispatcher(DispatchTable))
    {
        printf("Dispatch error\n");
        SvcReportEvent((wchar_t*)L"(StartServiceCtrlDispatcher)");
    }
    printf("Success\n");
}

//
// Purpose: 
//   Installs a service in the SCM database
//
// Parameters:
//   None
// 
// Return value:
//   None
//
/*VOID SendMessageToLog()
{

    HANDLE hEventSource = RegisterEventSource(NULL, SVCNAME);
    //LPWSTR message = 
    //StringCchPrintf(message, 180, (const wchar_t*)L"(I'm in an event log %ul)", GetTickCount());
    LPCWSTR prepared = L"I'm in an event log" + GetTickCount64();
    ReportEvent(hEventSource, EVENTLOG_ERROR_TYPE, 0, NULL, NULL, 1, 0, &prepared, NULL);
    DeregisterEventSource(hEventSource);
}*/
//HANDLE hEventSource;
//LPCTSTR lpszStrings[2];
//LPCTSTR Buffer = *szFunction + L"( failed with )" + GetLastError();
//
//hEventSource = RegisterEventSource(NULL, SVCNAME);
//
//if (NULL != hEventSource)
//{
//    //StringCchPrintf(Buffer, 320, (const wchar_t*)L"(%p failed with %d)", *szFunction, GetLastError());
//
//    lpszStrings[0] = SVCNAME;
//    lpszStrings[1] = Buffer;
//
//    ReportEvent(hEventSource,        // event log handle
//        EVENTLOG_ERROR_TYPE, // event type
//        0,                   // event category
//        NULL,           // event identifier - deleted
//        NULL,                // no security identifier
//        2,                   // size of lpszStrings array
//        0,                   // no binary data
//        lpszStrings,         // array of strings
//        NULL);               // no binary data
//
//    DeregisterEventSource(hEventSource);
//}
VOID SvcUninstall()
{
    SC_HANDLE manager = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!manager)
    {
        printf("Cannot open SCM (%d)\n", GetLastError());
        return;
    }
    SC_HANDLE service = OpenServiceW(manager, SVCNAME, SC_MANAGER_ALL_ACCESS);
    if (!service)
    {
        printf("Cannot open service (%d)\n", GetLastError());
        return;
    }
    if (deleteRegisteryKeys())
    {
        printf("Cannot delete Registtery Keys (%d)\n", GetLastError());
        return;
    }
    if (DeleteService(service))
        printf("Service uninstalled successfully\n");
    else
        printf("Cannot uninstall service (%d)\n", GetLastError());
    CloseServiceHandle(manager);
    CloseServiceHandle(service);
}
VOID SvcInstall()
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    TCHAR szUnquotedPath[MAX_PATH];
    createRegistryKeys();
    if (!GetModuleFileName(NULL, szUnquotedPath, MAX_PATH))
    {
        printf("Cannot install service (%d)\n", GetLastError());
        return;
    }

    // In case the path contains a space, it must be quoted so that
    // it is correctly interpreted. For example,
    // "d:\my share\myservice.exe" should be specified as
    // ""d:\my share\myservice.exe"".
    TCHAR szPath[MAX_PATH];
    StringCbPrintf(szPath, MAX_PATH, TEXT("\"%s\""), szUnquotedPath);

    // Get a handle to the SCM database. 

    schSCManager = OpenSCManager(
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 

    if (NULL == schSCManager)
    {
        printf("OpenSCManager failed (%d)\n", GetLastError());
        return;
    }

    // Create the service

    schService = CreateService(
        schSCManager,              // SCM database 
        SVCNAME,                   // name of service 
        SVCNAME,                   // service name to display 
        SERVICE_ALL_ACCESS,        // desired access 
        SERVICE_WIN32_OWN_PROCESS, // service type 
        SERVICE_DEMAND_START,      // start type 
        SERVICE_ERROR_NORMAL,      // error control type 
        szPath,                    // path to service's binary 
        NULL,                      // no load ordering group 
        NULL,                      // no tag identifier 
        NULL,                      // no dependencies 
        NULL,                      // LocalSystem account 
        NULL);                     // no password 

    if (schService == NULL)
    {
        printf("CreateService failed (%d)\n", GetLastError());
        CloseServiceHandle(schSCManager);
        return;
    }
    else printf("Service installed successfully\n");

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
}
VOID QueryStatus(SC_HANDLE schService) {
    DWORD dwBytesNeeded;
    if (!QueryServiceStatusEx(
        schService,
        SC_STATUS_PROCESS_INFO,
        (LPBYTE)&ssp,
        sizeof(SERVICE_STATUS_PROCESS),
        &dwBytesNeeded))
    {
        printf("QueryServiceStatusEx failed (%d)\n", GetLastError());
    }
}
VOID StopSvc() {
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    schSCManager = OpenSCManager(
        NULL,                    // local computer
        NULL,                    // servicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 
    schService = OpenService(
        schSCManager,         // SCM database 
        SVCNAME,            // name of service 
        SERVICE_ALL_ACCESS);
    if (!ControlService(
        schService,
        SERVICE_CONTROL_STOP,
        (LPSERVICE_STATUS)&ssp))
    {
        printf("ControlService failed (%d)\n", GetLastError());
    }
    SvcReportEvent((wchar_t*)L"(STOP)");
    ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
}
VOID StartSvc()
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    schSCManager = OpenSCManager(
        NULL,                    // local computer
        NULL,                    // servicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 
    schService = OpenService(
        schSCManager,         // SCM database 
        SVCNAME,            // name of service 
        SERVICE_ALL_ACCESS);  // full access 
    printf("Start is coming\n");
    if (schService == NULL)
    {
        printf("OpenService failed (%d)\n", GetLastError());
        CloseServiceHandle(schSCManager);
        return;
    }

    if (NULL == schSCManager)
    {
        printf("OpenSCManager failed (%d)\n", GetLastError());
        return;
    }
    if (!StartService(
        schService,  // handle to service 
        0,           // number of arguments 
        NULL))      // no arguments 
    {
        printf("StartService failed (%d)\n", GetLastError());
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return;
    }
    else printf("Service start pending...\n");
}

//
// Purpose: 
//   Entry point for the service
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None.
//
VOID WINAPI SvcMain(DWORD dwArgc, LPTSTR* lpszArgv)
{
    // Register the handler function for the service

    gSvcStatusHandle = RegisterServiceCtrlHandler(
        SVCNAME,
        SvcCtrlHandler);

    if (!gSvcStatusHandle)
    {
        SvcReportEvent((TCHAR*)(L"RegisterServiceCtrlHandler"));
        return;
    }
    //SendMessageToLog();

    // These SERVICE_STATUS members remain as set here

    gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    gSvcStatus.dwServiceSpecificExitCode = 0;

    // Report initial status to the SCM

    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    // Perform service-specific initialization and work.

    SvcInit(dwArgc, lpszArgv);
}

//
// Purpose: 
//   The service code
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None
//
VOID SvcInit(DWORD dwArgc, LPTSTR* lpszArgv)
{
    // TO_DO: Declare and set any required variables.
    //   Be sure to periodically call ReportSvcStatus() with 
    //   SERVICE_START_PENDING. If initialization fails, call
    //   ReportSvcStatus with SERVICE_STOPPED.

    // Create an event. The control handler function, SvcCtrlHandler,
    // signals this event when it receives the stop control code.


    ghSvcStopEvent = CreateEvent(
        NULL,    // default security attributes
        TRUE,    // manual reset event
        FALSE,   // not signaled
        NULL);   // no name

    if (ghSvcStopEvent == NULL)
    {
        ReportSvcStatus(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }

    // Report running status when initialization is complete.

    ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

    // TO_DO: Perform work until service stops.

    while (1)// != WAIT_OBJECT_0)
    {
        //;
        RunUI();
        WaitForSingleObject(ghSvcStopEvent, INFINITE);
        ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
        break;
    }
    return;
}

//
// Purpose: 
//   Sets the current service status and reports it to the SCM.
//
// Parameters:
//   dwCurrentState - The current state (see SERVICE_STATUS)
//   dwWin32ExitCode - The system error code
//   dwWaitHint - Estimated time for pending operation, 
//     in milliseconds
// 
// Return value:
//   None
//
VOID ReportSvcStatus(DWORD dwCurrentState,
    DWORD dwWin32ExitCode,
    DWORD dwWaitHint)
{
    static DWORD dwCheckPoint = 1;

    // Fill in the SERVICE_STATUS structure.

    gSvcStatus.dwCurrentState = dwCurrentState;
    gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
    gSvcStatus.dwWaitHint = dwWaitHint;

    if (dwCurrentState == SERVICE_START_PENDING)
        gSvcStatus.dwControlsAccepted = 0;
    else gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    if ((dwCurrentState == SERVICE_RUNNING) ||
        (dwCurrentState == SERVICE_STOPPED))
        gSvcStatus.dwCheckPoint = 0;
    else gSvcStatus.dwCheckPoint = dwCheckPoint++;

    // Report the status of the service to the SCM.
    SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
}

//
// Purpose: 
//   Called by SCM whenever a control code is sent to the service
//   using the ControlService function.
//
// Parameters:
//   dwCtrl - control code
// 
// Return value:
//   None
//
VOID WINAPI SvcCtrlHandler(DWORD dwCtrl)
{
    // Handle the requested control code. 

    switch (dwCtrl)
    {
    case SERVICE_CONTROL_STOP:
        ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

        // Signal the service to stop.

        SetEvent(ghSvcStopEvent);
        ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);

        return;

    case SERVICE_CONTROL_INTERROGATE:
        break;

    default:
        break;
    }

}

//
// Purpose: 
//   Logs messages to the event log
//
// Parameters:
//   szFunction - name of function that failed
// 
// Return value:
//   None
//
// Remarks:
//   The service must have an entry in the Application event log.
//
VOID SvcReportEvent(LPTSTR szFunction)
{
    HANDLE hEventSource;
    LPCTSTR lpszStrings[2];
    LPCTSTR Buffer = *szFunction + L"( failed with )" + GetLastError();

    hEventSource = RegisterEventSource(NULL, SVCNAME);

    if (NULL != hEventSource)
    {
        //StringCchPrintf(Buffer, 320, (const wchar_t*)L"(%p failed with %d)", *szFunction, GetLastError());

        lpszStrings[0] = SVCNAME;
        lpszStrings[1] = Buffer;

        ReportEvent(hEventSource,        // event log handle
            EVENTLOG_ERROR_TYPE, // event type
            0,                   // event category
            NULL,           // event identifier - deleted
            NULL,                // no security identifier
            2,                   // size of lpszStrings array
            0,                   // no binary data
            lpszStrings,         // array of strings
            NULL);               // no binary data

        DeregisterEventSource(hEventSource);
    }
}