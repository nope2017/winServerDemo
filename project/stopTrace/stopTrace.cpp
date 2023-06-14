// stopTrace.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <cstdio>
#include <windows.h>

#pragma comment(lib,"Advapi32")

using namespace std;

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

#define EXE_NAME L"startTrace"

//取消程序开机启动
void CanclePowerOn()
{
	HKEY hKey;
	//std::string strRegPath = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
	//1、找到系统的启动项  
	if (RegOpenKeyEx(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
	{
		//2、删除值
		RegDeleteValue(hKey, EXE_NAME);
		//3、关闭注册表
		RegCloseKey(hKey);
	}
}


#include <Dbt.h>
#include <ShellAPI.h>

#define SERVICE_NAME L"trace 802 service"

void stopAndRemoveTraceService()
{
	BOOL bRet = FALSE;
	printf("into func...\n");
	SERVICE_STATUS			m_ssStatus;			// current status of the service
	SERVICE_STATUS_HANDLE	m_sshStatusHandle;
	DWORD					m_dwControlsAccepted;	// bit-field of what control requests the

	// Real NT services go here.
	SC_HANDLE schSCManager = OpenSCManager(
		0,						// machine (NULL == local)
		0,						// database (NULL == default)
		SC_MANAGER_ALL_ACCESS	// access required
	);
	if (schSCManager) {
		SC_HANDLE schService = OpenService(
			schSCManager,
			SERVICE_NAME,
			SERVICE_ALL_ACCESS
		);

		if (schService) {
			// try to stop the service
			if (ControlService(schService, SERVICE_CONTROL_STOP, &m_ssStatus)) {
				printf(("Stopping trace 802 service."));
				Sleep(1000);

				while (QueryServiceStatus(schService, &m_ssStatus)) {
					if (m_ssStatus.dwCurrentState == SERVICE_STOP_PENDING) {
						printf(("."));
						Sleep(10);
					}
					else
						break;
				}

				if (m_ssStatus.dwCurrentState == SERVICE_STOPPED)
				{
					printf(("\n stopped....\n"));
				}	
				else
					printf("\n failed to stop.\n");
			}

			// now remove the service
			if (DeleteService(schService)) {
				//_tprintf(TEXT("%s removed.\n"), m_lpDisplayName);
				printf("removed service...\n");
				bRet = TRUE;
			}
			else {
				TCHAR szErr[256];
				printf("DeleteService failed - \n");
			}

			CloseServiceHandle(schService);
		}
		else {
			TCHAR szErr[256];
			printf(("OpenService failed -\n"));
		}

		CloseServiceHandle(schSCManager);
	}
	else {
		printf("OpenSCManager failed \n");
	}
}

int main()
{
	/*std::wstring cmd3(L"netsh trace stop");
	std::string strMsg = execAndResponse(cmd3);
	std::cout << "recv msg:" << std::endl;
	std::cout << strMsg.c_str();

	cmd3 = (L"netsh ras set tracing * disabled");
	strMsg = execAndResponse(cmd3);
	std::cout << "recv msg:" << std::endl;
	std::cout << strMsg.c_str();
	CanclePowerOn();*/
	stopAndRemoveTraceService();
	system("pause");
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
