// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "stdafx.h"
#include <sstream>
#include <tchar.h>
#include <stdio.h>
#include <regex>
#include "detours.h"

#pragma comment(lib, "detours.lib")

using namespace std::regex_constants;

static std::vector<std::pair<std::string, bool> > strings;
static std::vector<std::string> subprocs;


static int (WINAPI *oldMultiByteToWideChar)(UINT CodePage, DWORD dwFlags, LPCCH lpMultiByteStr, int cbMultiByte,
    LPWSTR lpWideCharStr, int cchWideChar) = MultiByteToWideChar;


int WINAPI newMultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCCH lpMultiByteStr, int cbMultiByte,
    LPWSTR lpWideCharStr, int cchWideChar)
{
    int ret = oldMultiByteToWideChar(CodePage, dwFlags, lpMultiByteStr, cbMultiByte, lpWideCharStr, cchWideChar);
    if (cbMultiByte > 0) {
        std::string str(lpMultiByteStr, cbMultiByte);
        char *pattern = "^\\s*(("
            "_END|_ENDFILE|_SUB|POS|\\{|\\}|\\[|\\]|LAMBDA|ADSL|BASE|BROW|CALC|CALL|CHEK|CODE|COME|NOTE|CMPS|DATE|"
            "DEVI|DFMT|DIR|DISK|DISP|DTIM|EDIT|EJEC|ENVI|SET|EXEC|EXIT|FBWF|FDIR|FDRV|FEXT|FILE|FIND|FLNK|FONT|"
            "FORM|FORX|GETF|GROU|HASH|HELP|HIDE|HIVE|HKEY|HOME|HOTK|IFEX|IMAG|IMPORT|INIT|IPAD|ITEM|KILL|LABE|"
            "LINK|LIST|LOAD|LOCK|LOGO|LOGS|LOOP|LPOS|LSTR|MAIN|MDIR|MEMO|MENU|MESS|MOUN|MSTR|FNAM|NAME|NTPC|NUMK|"
            "PAGE|PART|PATH|PBAR|PCIP|PINT|PUTF|RADI|RAMD|RAND|READ|RECY|REGI|RPOS|RSTR|RUNS|SCRN|SED|SEND|SERV|"
            "SHEL|SHOW|SHUT|SITE|SIZE|SLID|SOCK|SPIN|SSTR|STRL|SUBJ|SWIN|TABL|TABS|TEAM|TEMP|TEXT|THREAD|THRD|"
            "TIME|TIPS|UPNP|USER|WALL|WAIT|WRIT|DLL|\\\\|//|;|`|-|@|%|#(!|code)"
        ").*(\r|\n|\r\n)?|(\r|\n|\r\n))";
        std::smatch group;
        bool match = std::regex_match(str, group, std::regex(pattern, ECMAScript | icase));
        pattern = "^\\s*(Disk error).*(\r|\n|\r\n)?";
        match = match && !std::regex_match(str, std::regex(pattern, ECMAScript | icase));
        strings.push_back(std::make_pair(str, match));

        if (match) {
            for (int i = 0; i < cchWideChar; i++) {
                lpWideCharStr[i] = L'\0';
            }
            if (group[2] == "_SUB") {
                pattern = "^\\s*_SUB\\s+([0-9a-z_]+).*(\r|\n|\r\n)?";
                match = std::regex_match(str, group, std::regex(pattern, ECMAScript | icase));
                if (match) {
                    subprocs.push_back(group[1]);
                }
            }
        }
    }
    return ret;
}


void compose(std::string &script)
{
    std::string pattern;
    if (!subprocs.empty()) {
        std::ostringstream oss;
        auto it1 = subprocs.begin();
        oss << "^\\s*(" << *it1++;
        for (; it1 != subprocs.end(); ++it1) {
            oss << '|' << *it1;
        }
        oss << ").*(\r|\n|\r\n)?";
        pattern = oss.str();
    }

    for (auto it2 = strings.begin(); it2 != strings.end(); ++it2) {
        if (it2->second || (!pattern.empty() &&
            std::regex_match(it2->first, std::regex(pattern, ECMAScript | icase)))) {
            script += it2->first;
			script += "\n";
        }
    }
}


bool save(const std::string &script)
{
    FILE *logFile = NULL;
    TCHAR logPath[MAX_PATH] = {0};
    GetCurrentDirectory(MAX_PATH, logPath);
    _tcscat_s(logPath, MAX_PATH, _T("\\original.ini"));
    _tfopen_s(&logFile, logPath, _T("w"));
    if (!logFile) {
        MessageBoxA(NULL, "failed to save ini file", "Error", MB_OK);
        return false;
    }
    fwrite(script.c_str(), 1, script.size(), logFile);
    fclose(logFile);
    return true;
}

bool appendLogFile(const std::string& content)
{
	FILE *logFile = NULL;
	TCHAR logPath[MAX_PATH] = { 0 };
	GetCurrentDirectory(MAX_PATH, logPath);
	_tcscat_s(logPath, MAX_PATH, _T("\\original.ini"));
	_tfopen_s(&logFile, logPath, _T("a+"));
	if (!logFile) {
		MessageBoxA(NULL, "failed to save ini file", "Error", MB_OK);
		return false;
	}
	fwrite(content.c_str(), 1, content.size(), logFile);
	fclose(logFile);
	return true;
}

typedef int(*oldUnknowFunTypeExecACmd)(DWORD Unknow1, LPCWSTR Str, LPVOID Unknow2, LPVOID Unknow3, LPVOID Unknow4);
oldUnknowFunTypeExecACmd pFunOldUnknow = (oldUnknowFunTypeExecACmd)(0x1400363D8);
int newUnknowFun_ExecACmd(DWORD Unknow1, LPCWSTR Str, LPVOID Unknow2, LPVOID Unknow3, LPVOID Unknow4)
{
	int ret = pFunOldUnknow(Unknow1,Str, Unknow2, Unknow3, Unknow4);
	if (Str)
	{
		char recordstr[1024] = { 0 };
		WideCharToMultiByte(CP_ACP, 0, Str, -1, recordstr, sizeof(recordstr), NULL, NULL);
		strings.push_back(std::make_pair(recordstr,true));
	}
	return ret;
}

typedef int(*oldUnknowFunAllContents)(DWORD Unknow1, LPCWSTR Str, LPVOID Unknow2, LPVOID Unknow3);
oldUnknowFunAllContents pFunOldAllContents = (oldUnknowFunAllContents)(0x1400992A0);
int newUnknowFun_AllContents(DWORD Unknow1, LPCWSTR Str, LPVOID Unknow2, LPVOID Unknow3)
{
	int ret = pFunOldAllContents(Unknow1, Str, Unknow2, Unknow3);
	if (Str)
	{
		char recordstr[1024] = { 0 };
		WideCharToMultiByte(CP_ACP, 0, Str, -1, recordstr, sizeof(recordstr), NULL, NULL);
		strings.push_back(std::make_pair(recordstr, true));
	}
	return ret;
}

typedef PWCHAR(*oldUnknow3Ft)(DWORD unknow1, DWORD unknow2, DWORD unknow3, DWORD unknow4);
oldUnknow3Ft pFunOldUnknow3Ft = (oldUnknow3Ft)(0x14001515C);
PWCHAR newUnknow3(DWORD unknow1, DWORD unknow2, DWORD unknow3, DWORD unknow4)
{
	PWCHAR ret = pFunOldUnknow3Ft(unknow1, unknow2, unknow3, unknow4);
	if (ret && ret[0])
	{
		char recordstr[1024] = { 0 };
		WideCharToMultiByte(CP_ACP, 0, ret, -1, recordstr, sizeof(recordstr), NULL, NULL);
		strings.push_back(std::make_pair(recordstr, true));
		
		strcat(recordstr, "\r\n");
		appendLogFile(recordstr);
	}
	return ret;
}


void hook()
{
    DetourRestoreAfterWith();
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)pFunOldUnknow3Ft, newUnknow3);
	// DetourAttach(&(PVOID&)pFunOldUnknow, newUnknowFun_ExecACmd);
	//DetourAttach(&(PVOID&)pFunOldAllContents, newUnknowFun_AllContents);
	// DetourAttach(&(PVOID&)oldMultiByteToWideChar, newMultiByteToWideChar);
    DetourTransactionCommit();
}

void unhook()
{
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
	DetourDetach(&(PVOID&)pFunOldUnknow3Ft, newUnknow3);
	//DetourDetach(&(PVOID&)pFunOldAllContents, newUnknowFun_AllContents);
	//DetourDetach(&(PVOID&)pFunOldUnknow, newUnknowFun_ExecACmd);
   // DetourDetach(&(PVOID&)oldMultiByteToWideChar, newMultiByteToWideChar);
    DetourTransactionCommit();
}

extern "C" __declspec(dllexport) void dummy()
{
    return;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        {
			MessageBox(NULL, L"1", NULL, MB_OK);
            hook();
            break;
        }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        {
            unhook();
            try {
                std::string script;
                compose(script);
                save(script);
            } catch (const std::exception &exc) {
                MessageBoxA(NULL, exc.what(), "Error", MB_OK);
            }
            break;
        }
    }
    return TRUE;
}
