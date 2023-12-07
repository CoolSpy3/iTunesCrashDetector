#pragma clang diagnostic push
#pragma ide diagnostic ignored "modernize-raw-string-literal"
#pragma ide diagnostic ignored "modernize-use-nullptr"
#include <Windows.h>
#include <psapi.h>

const char* iTunesEXE = "C:\\Program Files\\iTunes\\iTunes.exe";

struct Process {
    HANDLE procHandle;
    HWND procWND;
};

BOOL CALLBACK FindITunesProc(HWND hwnd, LPARAM lParam) {
    // Find the process associated with the window
    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);
    // Modified from Windows Docs' example code
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE, FALSE, processId);
    if(process) {
        // Get the name of the process
        TCHAR processName[MAX_PATH] = TEXT("<unknown>");
        GetModuleFileNameEx(process, NULL, processName, sizeof(processName)/sizeof(TCHAR));

        // If it's "iTunes.exe", we've found iTunes
        if(strcmp(processName, iTunesEXE) == 0) {
            // Populate the process struct
            auto *iTunes = reinterpret_cast<Process*>(lParam);
            iTunes->procHandle = process;
            iTunes->procWND = hwnd;
            return FALSE; // We found iTunes :D Stop iterating!
        }
    }
    CloseHandle(process);
    return TRUE; // We haven't found iTunes :( Continue iterating!
}

bool FindITunes(Process *iTunes) {
    // Clear the process struct if it hasn't been already.
    if(iTunes->procHandle) {
        CloseHandle(iTunes->procHandle);
    }
    iTunes->procHandle = NULL;
    iTunes->procWND = NULL;

    EnumWindows(FindITunesProc, reinterpret_cast<LPARAM>(iTunes));

    return iTunes->procHandle; // If the process handle has been reinitialized, we found it!
}

int main() {
    HANDLE runningMutex = CreateMutex(NULL, TRUE, "iTunesCrashDetector/Running");
    if(!runningMutex) {
        // Program is already running. Terminate
        exit(2);
    }
    Process iTunes = {NULL, NULL};
    while(TRUE) { // FOREVER
        if(!(iTunes.procHandle || FindITunes(&iTunes))) { // Find iTunes
            // It isn't open. Wait 30 seconds before checking again
            Sleep(30000);
            continue;
        }
        // https://stackoverflow.com/questions/6493626/how-to-check-if-a-given-process-is-running-when-having-its-handle
        if(WaitForSingleObject(iTunes.procHandle, 0) == WAIT_OBJECT_0) {
            // iTunes was closed. Clear the process handle and wait 30 seconds before checking again
            CloseHandle(iTunes.procHandle);
            iTunes.procHandle = NULL;
            iTunes.procWND = NULL;
            Sleep(30000);
            continue;
        }
        if(!IsHungAppWindow(iTunes.procWND)) { // Is iTunes running!
            // Yep! Wait 1 second before checking again
            Sleep(1000);
            continue;
        }

        // ITunes is dead :(

        // Ask the user for permission to restart iTunes
        if(MessageBox(NULL, "iTunes is hanging. Terminate?", "iTunesCrashDetector", MB_YESNO) == IDYES) {
            // They said yes :D KILL ITUNES!!!!
            TerminateProcess(iTunes.procHandle, 1);
            Sleep(500); // Wait a second for stuff to happen
            // Restart iTunes
            ShellExecute(
                    NULL,
                    NULL,
                    iTunesEXE,
                    "",
                    "C:\\Windows\\System32\\",
                    SW_NORMAL
            );
            // iTunes just got restarted. Wait 30 seconds before checking again
            Sleep(30000);
        } else {
            // They said no :( Wait 5 seconds before asking again
            Sleep(5000);
        }
    }
    // Release Mutex and Terminate (If we ever get here)
    ReleaseMutex(runningMutex);
    CloseHandle(runningMutex);
    return 0;
}

#pragma clang diagnostic pop
