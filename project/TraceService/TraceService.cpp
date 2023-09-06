#include <windows.h>
#include <WinUser.h>
#include <stdio.h>
#include <iostream>
#include <cstdio>
#include <tchar.h>
#include <thread>
#include <future>
#include <fstream>

//日志收集 ： https://learn.microsoft.com/zh-cn/troubleshoot/windows-client/networking/wireless-network-connectivity-issues-troubleshooting
// netsh trace convert c:\tmp\wireless.etl
#define SERVICE_NAME L"Trace8021XLog"
using namespace std;

#pragma comment(lib,"Advapi32")
#pragma comment(lib,"user32")

void writeFile(std::string strMsg);
void writeFile(std::wstring strMsg);


std::wstring stringToWstring(const std::string& str)
{
	int len = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);
	wchar_t* wstr = new wchar_t[len];
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, wstr, len);
	std::wstring result(wstr);
	delete[] wstr;
	return result;
}

std::string execAndResponse(std::wstring strCmd)
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	SECURITY_ATTRIBUTES sa;
	HANDLE pipe_out_read, pipe_out_write;
	HANDLE pipe_err_read = NULL, pipe_err_write = NULL;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
	ZeroMemory(&sa, sizeof(sa));
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;
	//writeFile(L"run cmd:" + strCmd);
	std::string strRead("recv msg:");
	do
	{
		// 创建标准输出管道
		if (!CreatePipe(&pipe_out_read, &pipe_out_write, &sa, 0))
		{
			cout << "Failed to create output pipe!" << endl;
			break;
		}

		// 确保管道读端口是非继承的
		if (!SetHandleInformation(pipe_out_read, HANDLE_FLAG_INHERIT, 0))
		{
			cout << "Failed to set output handle information!" << endl;
			break;
		}

		// 创建标准错误管道
		if (!CreatePipe(&pipe_err_read, &pipe_err_write, &sa, 0))
		{
			cout << "Failed to create error pipe!" << endl;
			break;
		}

		// 确保管道读端口是非继承的
		if (!SetHandleInformation(pipe_err_read, HANDLE_FLAG_INHERIT, 0))
		{
			cout << "Failed to set error handle information!" << endl;
			break;
		}

		// 设置启动信息
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdOutput = pipe_out_write;
		si.hStdError = pipe_err_write;

		// 命令行命令
		// 创建进程时禁用文件系统重定向
		Wow64DisableWow64FsRedirection(NULL);
		// 执行命令行命令
		if (!CreateProcess(NULL, (LPWSTR)strCmd.c_str(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
		{
			cout << "Failed to execute command!" << endl;
			break;
		}

		WaitForSingleObject(pi.hProcess, INFINITE);

		// 关闭无用的管道句柄
		CloseHandle(pipe_out_write);
		CloseHandle(pipe_err_write);

		// 从管道中读取输出内容
		char buffer[1024];
		DWORD bytes_read;
		memset(buffer, 0, 1024);

		while (PeekNamedPipe(pipe_out_read, NULL, 0, NULL, &bytes_read, NULL))
		{
			if (bytes_read == 0)
				break;
			if (ReadFile(pipe_out_read, buffer, min(bytes_read, sizeof(buffer)), &bytes_read, NULL))
			{
				strRead.append(buffer);
			}
			//cout << string(buffer, bytes_read);
		}

		while (PeekNamedPipe(pipe_err_read, NULL, 0, NULL, &bytes_read, NULL))
		{
			if (bytes_read == 0)
				break;
			if (ReadFile(pipe_err_read, buffer, min(bytes_read, sizeof(buffer)), &bytes_read, NULL))
			{

			}
			//cerr << string(buffer, bytes_read);
		}
	} while (0);

	if (pipe_out_read)
		CloseHandle(pipe_out_read);
	if (pipe_err_read)
		CloseHandle(pipe_err_read);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return (strRead);
}

std::string getTimeStamp()
{
	FILETIME fileTime;
	GetSystemTimeAsFileTime(&fileTime);

	ULARGE_INTEGER uli;
	uli.LowPart = fileTime.dwLowDateTime;
	uli.HighPart = fileTime.dwHighDateTime;

	ULONGLONG currentTimeStamp = uli.QuadPart / 10000000ULL - 11644473600ULL;
	char timestampStr[64];
	memset(timestampStr, 0, 64);

	sprintf_s(timestampStr, sizeof(timestampStr), "%llu", currentTimeStamp);
	return timestampStr;
}

int CreateFolder(bool btest = false)
{
	int ret = -1;
	LPCWSTR folderName = L"C:\\MSLOG"; // 文件夹路径
	if (btest)
	{
		folderName = L"C:\\MSLOG2";
	}
	DWORD result = GetFileAttributes(folderName);

	if (result != INVALID_FILE_ATTRIBUTES && (result & FILE_ATTRIBUTE_DIRECTORY))
	{
		wprintf(L"Folder exists.\n");
		ret = 0;
	}
	else
	{
		wprintf(L"Folder does not exist. Create the folder\n");
		BOOL result = CreateDirectory(folderName, NULL);
		if (result)
		{
			wprintf(L"Folder created successfully.\n");
			ret = 1;
		}
		else
		{
			DWORD error = GetLastError();
			if (error == ERROR_ALREADY_EXISTS)
			{
				wprintf(L"Folder already exists.\n");
			}
			else
			{
				wprintf(L"Folder creation failed with error code %d\n", error);
				ret = -2;
			}
		}
	}
	return ret;
}

#define NETSH	L"C:\\Windows\\System32\\netsh.exe"

#define LOG_CAPI2 0
//#define WIRELESS
#ifdef WIRELESS
#define CMD_START_TRACE L"C:\\Windows\\System32\\netsh.exe trace start scenario=wlan,wlan_wpp,wlan_dbg,wireless_dbg globallevel=0xff capture=yes maxsize=1024 tracefile=C:\\MSLOG\\leagsoft_"
#else
#define CMD_START_TRACE L"C:\\Windows\\System32\\netsh.exe trace start scenario=lan globallevel=0xff capture=yes maxsize=1024 tracefile=\"C:\\MSLOG\\leagsoft_"
#endif

void writeFile(std::wstring strMsg)
{
	std::wofstream ofs;
	ofs.open("C:\\MSLOG\\log.txt", std::ios_base::app);
	if (ofs.is_open()) {
		ofs << strMsg.c_str() << std::endl;
		ofs.close();

		std::cout << "Successfully wrote to file." << std::endl;
	}
}

void writeFile(std::string strMsg)
{
	std::ofstream ofs;
	ofs.open("C:\\MSLOG\\log.txt", std::ios_base::app);
	if (ofs.is_open()) {  
		ofs << strMsg.c_str() << std::endl;
		ofs.close();    

		std::cout << "Successfully wrote to file." << std::endl;
	}
}

void writeError(std::string strMsg)
{
	std::ofstream ofs;
	ofs.open("C:\\MSLOG\\error.txt", std::ios_base::app);
	if (ofs.is_open()) {
		ofs << strMsg.c_str() << std::endl;
		ofs.close();

		std::cout << "Successfully wrote to file." << std::endl;
	}
	else
	{
		std::ofstream ofs("C:\\MSLOG\\failed open error.txt");
	}
}

int g_cout = 0;
int runTrace()
{
	//AdjustPrivilege();
	CreateFolder();
	auto runCmd = [](std::wstring strNetsh)->std::string {
		std::wstring cmd1(NETSH + std::wstring(L" ras set tracing * enabled"));
		std::string strMsg = execAndResponse(cmd1);
		std::cout << "recv msg:" << std::endl;
		std::cout << strMsg.c_str();

		std::wstring cmd2(CMD_START_TRACE);
		cmd2 += stringToWstring(getTimeStamp()) + L"_wireless_cli.etl";
		strMsg.clear();
		strMsg = execAndResponse(cmd2);
		writeFile(strMsg);
		return strMsg;
	};
	std::wstring strCurNetsh = NETSH;
	std::string strRet = runCmd(strCurNetsh);
	if (strRet.find("找不到下列命令") != -1)
	{
		HMODULE hModule = NULL;
		wchar_t buffer[MAX_PATH];
		if (GetModuleFileNameW(hModule, buffer, MAX_PATH) != 0) {
			std::wstring currentDir = buffer;
			size_t lastDelimiter = currentDir.find_last_of(L"\\/");
			if (lastDelimiter != std::wstring::npos) {
				currentDir = currentDir.substr(0, lastDelimiter);
			}
			strCurNetsh = currentDir + L"\\netsh.exe";
			strRet = runCmd(strCurNetsh);
		}
	}
	

#if LOG_CAPI2
	std::wstring cmd3(L"wevtutil.exe sl Microsoft-Windows-CAPI2/Operational /e:true");
	strMsg = execAndResponse(cmd3);
	std::cout << "recv msg2:" << std::endl;
	std::cout << strMsg.c_str();

	std::wstring cmd4(L"wevtutil sl Microsoft-Windows-CAPI2/Operational /ms:104857600");
	strMsg = execAndResponse(cmd4);
	std::cout << "recv msg2:" << std::endl;
	std::cout << strMsg.c_str();
#endif
	return 0;
}

void stopTrace()
{
	std::wstring cmd3(std::wstring(NETSH) + L" trace stop");
	std::string strMsg = execAndResponse(cmd3);

	cmd3 = (std::wstring(NETSH) + L" ras set tracing * disabled");
	strMsg = execAndResponse(cmd3);
#if LOG_CAPI2
	//禁用并复制 CAPI2 日志
	cmd3 = (L"wevtutil sl Microsoft-Windows-CAPI2/Operational /e:false");
	strMsg = execAndResponse(cmd3);

	cmd3 = (L"wevtutil epl Microsoft-Windows-CAPI2/Operational C:\\MSLOG\\");
	cmd3 += stringToWstring(getTimeStamp()) + L"_CAPI2.evtx";
	strMsg = execAndResponse(cmd3);
#endif
}

SERVICE_STATUS        g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE                g_ServiceStopEvent = INVALID_HANDLE_VALUE;

VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
VOID WINAPI ServiceCtrlHandler(DWORD);
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam);

#define SERVICE_NAME  _T("Trace8021XLog")

int _tmain(int argc, TCHAR *argv[])
{
	OutputDebugString(_T("Trace8021XLog Service: Main: Entry"));

	SERVICE_TABLE_ENTRY ServiceTable[] =
	{
		{(LPWSTR)SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain},
		{NULL, NULL}
	};

	if (StartServiceCtrlDispatcher(ServiceTable) == FALSE)
	{
		OutputDebugString(_T("Trace8021XLog: Main: StartServiceCtrlDispatcher returned error"));
		return GetLastError();
	}

	OutputDebugString(_T("Trace8021XLog: Main: Exit"));
	return 0;
}


VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv)
{
	DWORD Status = E_FAIL;
	HANDLE hThread = NULL;
	OutputDebugString(_T("Trace8021XLog: ServiceMain: Entry"));

	g_StatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);

	if (g_StatusHandle == NULL)
	{
		OutputDebugString(_T("Trace8021XLog: ServiceMain: RegisterServiceCtrlHandler returned error"));
		goto EXIT;
	}

	// Tell the service controller we are starting
	ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
	g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	g_ServiceStatus.dwControlsAccepted = 0;
	g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwServiceSpecificExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 0;

	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		OutputDebugString(_T("Trace8021XLog: ServiceMain: SetServiceStatus returned error"));
	}

	/*
	 * Perform tasks neccesary to start the service here
	 */
	OutputDebugString(_T("Trace8021XLog: ServiceMain: Performing Service Start Operations"));

	// Create stop event to wait on later.
	g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (g_ServiceStopEvent == NULL)
	{
		OutputDebugString(_T("Trace8021XLog: ServiceMain: CreateEvent(g_ServiceStopEvent) returned error"));

		g_ServiceStatus.dwControlsAccepted = 0;
		g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		g_ServiceStatus.dwWin32ExitCode = GetLastError();
		g_ServiceStatus.dwCheckPoint = 1;

		if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
		{
			OutputDebugString(_T("Trace8021XLog: ServiceMain: SetServiceStatus returned error"));
		}
		goto EXIT;
	}

	// Tell the service controller we are started
	g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 0;

	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		OutputDebugString(_T("Trace8021XLog: ServiceMain: SetServiceStatus returned error"));
	}

	// Start the thread that will perform the main task of the service
	hThread = CreateThread(NULL, 0, ServiceWorkerThread, NULL, 0, NULL);

	OutputDebugString(_T("Trace8021XLog: ServiceMain: Waiting for Worker Thread to complete"));

	// Wait until our worker thread exits effectively signaling that the service needs to stop
	WaitForSingleObject(hThread, INFINITE);

	OutputDebugString(_T("Trace8021XLog: ServiceMain: Worker Thread Stop Event signaled"));


	/*
	 * Perform any cleanup tasks
	 */
	OutputDebugString(_T("Trace8021XLog: ServiceMain: Performing Cleanup Operations"));

	CloseHandle(g_ServiceStopEvent);

	g_ServiceStatus.dwControlsAccepted = 0;
	g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 3;

	if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
	{
		OutputDebugString(_T("Trace8021XLog: ServiceMain: SetServiceStatus returned error"));
	}

EXIT:
	OutputDebugString(_T("Trace8021XLog: ServiceMain: Exit"));

	return;
}


VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode)
{
	OutputDebugString(_T("Trace8021XLog: ServiceCtrlHandler: Entry"));
	bool bNeedStopTrace = false;
	switch (CtrlCode)
	{
	case SERVICE_CONTROL_STOP:

		OutputDebugString(_T("Trace8021XLog: ServiceCtrlHandler: SERVICE_CONTROL_STOP Request"));

		if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
			break;

		/*
		 * Perform tasks neccesary to stop the service here
		 */

		g_ServiceStatus.dwControlsAccepted = 0;
		g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
		g_ServiceStatus.dwWin32ExitCode = 0;
		g_ServiceStatus.dwCheckPoint = 4;

		if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
		{
			OutputDebugString(_T("Trace8021XLog: ServiceCtrlHandler: SetServiceStatus returned error"));
		}

		// This will signal the worker thread to start shutting down
		SetEvent(g_ServiceStopEvent);

		break;
	case SERVICE_CONTROL_CONTINUE:
		OutputDebugString(_T("Trace8021XLog: ServiceCtrlHandler: continue..."));
		break;
	default:
		break;
	}
	OutputDebugString(_T("Trace8021XLog: ServiceCtrlHandler: Exit"));
}


DWORD WINAPI ServiceWorkerThread(LPVOID lpParam)
{
	OutputDebugString(_T("Trace8021XLog: ServiceWorkerThread: Entry"));
	runTrace();
	//  Periodically check if the service has been requested to stop
	while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0)
	{
		/*
		 * Perform main service function here
		 */

		 //  Simulate some work by sleeping
		Sleep(3000);
	}
	stopTrace();
	OutputDebugString(_T("Trace8021XLog: ServiceWorkerThread: Exit"));

	return ERROR_SUCCESS;
}

/*

SERVICE_STATUS        g_serviceStatus = { 0 };
SERVICE_STATUS_HANDLE g_serviceStatusHandle = NULL;
HANDLE                g_stopEvent = NULL;

// 服务控制函数
void WINAPI ServiceControlHandler(DWORD controlCode)
{
	switch (controlCode)
	{
	case SERVICE_CONTROL_SHUTDOWN:
	case SERVICE_CONTROL_STOP:
		// 收到停止控制事件，设置服务状态为停止中
		g_serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
		SetServiceStatus(g_serviceStatusHandle, &g_serviceStatus);
		stopTrace();
		// 发送停止事件，在主函数中等待
		SetEvent(g_stopEvent);
		break;
	case SERVICE_CONTROL_CONTINUE:
		OutputDebugString(_T("Trace8021XLog: SERVICE_CONTROL_CONTINUE: continue..."));
		break;
	default:
		// 其他未处理的控制码
		break;
	}
}

// 主函数
int main(int argc, char* argv[])
{
	// 注册服务控制函数
	SERVICE_TABLE_ENTRY serviceTable[] =
	{
		{ (LPWSTR)SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain },
		{ NULL, NULL }
	};

	if (!StartServiceCtrlDispatcher(serviceTable)) {
		printf("Failed to register service control function. Error %d.\n", GetLastError());
		return 1;
	}

	return 0;
}

// 服务主函数
void WINAPI ServiceMain(DWORD argc, LPTSTR* argv)
{
	// 注册服务控制处理函数
	g_serviceStatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceControlHandler);
	if (g_serviceStatusHandle == NULL) {
		printf("Failed to register service control handler. Error %d.\n", GetLastError());
		return;
	}

	// 设置服务状态为正在启动
	g_serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	g_serviceStatus.dwCurrentState = SERVICE_START_PENDING;
	g_serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	g_serviceStatus.dwWin32ExitCode = NO_ERROR;
	g_serviceStatus.dwServiceSpecificExitCode = 0;
	g_serviceStatus.dwCheckPoint = 0;
	SetServiceStatus(g_serviceStatusHandle, &g_serviceStatus);

	// 创建事件并重置停止状态
	g_stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (g_stopEvent == NULL) {
		printf("Failed to create stop event. Error %d.\n", GetLastError());
		g_serviceStatus.dwCurrentState = SERVICE_STOPPED;
		SetServiceStatus(g_serviceStatusHandle, &g_serviceStatus);
		return;
	}

	// 执行命令
	//system("netsh ras set tracing * enabled");
	runTrace();

	// 设置服务状态为已启动
	g_serviceStatus.dwCurrentState = SERVICE_RUNNING;
	SetServiceStatus(g_serviceStatusHandle, &g_serviceStatus);

	// 主线程进入等待状态，直到收到停止事件
	WaitForSingleObject(g_stopEvent, INFINITE);

	// 设置服务状态为已停止
	g_serviceStatus.dwCurrentState = SERVICE_STOPPED;
	SetServiceStatus(g_serviceStatusHandle, &g_serviceStatus);

	// 关闭事件句柄
	CloseHandle(g_stopEvent);
}
*/