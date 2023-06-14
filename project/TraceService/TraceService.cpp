#include <windows.h>
#include <stdio.h>
#include <iostream>
#include <cstdio>

#define SERVICE_NAME L"Trace8021XLog"
using namespace std;

#pragma comment(lib,"Advapi32")

SERVICE_STATUS        g_serviceStatus = { 0 };
SERVICE_STATUS_HANDLE g_serviceStatusHandle = NULL;
HANDLE                g_stopEvent = NULL;


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

	std::wstring strOutput;
	std::string strRead;
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

int CreateFolder()
{
	int ret = -1;
	LPCWSTR folderName = L"C:\\MSLOG"; // 文件夹路径
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

#define WIRELESS
#ifdef WIRELESS
#define CMD_START_TRACE L"netsh trace start scenario=wlan,wlan_wpp,wlan_dbg,wireless_dbg globallevel=0xff capture=yes maxsize=1024 tracefile=C:\\MSLOG\\leagsoft_"
#else
#define CMD_START_TRACE L"netsh trace start scenario=lan globallevel=0xff capture=yes maxsize=1024 tracefile=C:\\MSLOG\\leagsoft_"
#endif
int runTrace()
{
	//AdjustPrivilege();
	CreateFolder();
	std::wstring cmd1(L"netsh ras set tracing * enabled");
	std::string strMsg = execAndResponse(cmd1);
	std::cout << "recv msg:" << std::endl;
	std::cout << strMsg.c_str();

	std::wstring cmd2(CMD_START_TRACE);
	cmd2 += stringToWstring(getTimeStamp()) + L"_wireless_cli.etl";
	strMsg = execAndResponse(cmd2);
	std::cout << "recv msg2:" << std::endl;
	std::cout << strMsg.c_str();
#ifndef WIRELESS
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
	std::wstring cmd3(L"netsh trace stop");
	std::string strMsg = execAndResponse(cmd3);

	cmd3 = (L"netsh ras set tracing * disabled");
	strMsg = execAndResponse(cmd3);
#ifndef WIRELESS
	//禁用并复制 CAPI2 日志
	cmd3 = (L"wevtutil sl Microsoft-Windows-CAPI2/Operational /e:false");
	strMsg = execAndResponse(cmd3);

	cmd3 = (L"wevtutil epl Microsoft-Windows-CAPI2/Operational C:\\MSLOG\\");
	cmd3 += stringToWstring(getTimeStamp()) + L"_CAPI2.evtx";
	strMsg = execAndResponse(cmd3);
#endif
}

void WINAPI ServiceMain(DWORD argc, LPTSTR* argv);

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
