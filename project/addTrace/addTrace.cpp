// addTrace.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <cstdio>
#include <windows.h>

#pragma comment(lib,"Advapi32")

#define EXE_NAME L"startTrace"

std::wstring getPath()
{
	//2、得到本程序自身的全路径
	TCHAR strExeFullDir[MAX_PATH];
	GetModuleFileName(NULL, strExeFullDir, MAX_PATH);
	wchar_t *lastDelimiter = wcsrchr(strExeFullDir, '\\');
	if (lastDelimiter != NULL) {
		*lastDelimiter = '\0';
	}
	return std::wstring(strExeFullDir)+L"\\";
}

//设置当前程序开机启动
bool AutoPowerOn()
{
	bool bRet = false;
	HKEY hKey;
	//std::string strRegPath = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";

	//1、找到系统的启动项  
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) ///打开启动项       
	{
		//2、得到本程序自身的全路径
		TCHAR strExeFullDir[MAX_PATH];
		GetModuleFileName(NULL, strExeFullDir, MAX_PATH);
		wchar_t *lastDelimiter = wcsrchr(strExeFullDir, '\\');
		if (lastDelimiter != NULL) {
			*lastDelimiter = '\0';
		}
		wcscat_s(strExeFullDir, L"\\");
		wcscat_s(strExeFullDir, EXE_NAME);
		wcscat_s(strExeFullDir, L".exe");
		
		//3、判断注册表项是否已经存在
		TCHAR strDir[MAX_PATH] = {};
		DWORD nLength = MAX_PATH;
		long result = RegGetValue(hKey, nullptr, EXE_NAME, RRF_RT_REG_SZ, 0, strDir, &nLength);

		//4、已经存在
		if (result != ERROR_SUCCESS || wcscmp(strExeFullDir, strDir) != 0)
		{
			//5、添加一个子Key,并设置值，"GISRestart"是应用程序名字（不加后缀.exe） 
			RegSetValueEx(hKey, EXE_NAME, 0, REG_SZ, (LPBYTE)strExeFullDir, (lstrlen(strExeFullDir) + 1) * sizeof(TCHAR));
			//6、关闭注册表
			RegCloseKey(hKey);
			bRet = true;
		}
	}

	return bRet;
}

#define WIRELESS 0
#if WIRELESS
#define SERVICE_EXE_NAME  L"TraceService-wireless.exe"
#else 
#define SERVICE_EXE_NAME  L"TraceService-wired.exe"
#endif

int regSc()
{
	SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
	if (hSCManager == NULL)
	{
		std::cerr << "Failed to open SC Manager" << std::endl;
		return 1;
	}
	std::wstring strPath = getPath();
	strPath.append(SERVICE_EXE_NAME);
	const wchar_t* szServiceName = L"trace 802 service";

	SC_HANDLE hService = CreateService(
		hSCManager,
		szServiceName,
		L"log 802 auth",
		SERVICE_ALL_ACCESS,
		SERVICE_WIN32_OWN_PROCESS,
		SERVICE_AUTO_START,
		SERVICE_ERROR_NORMAL,
		strPath.c_str(),
		NULL,
		NULL,
		NULL,
		NULL,
		NULL
	);

	if (hService == NULL)
	{
		std::cerr << "Failed to create service" << std::endl;
		CloseServiceHandle(hSCManager);
		return 1;
	}

	std::cout << "Service created successfully" << std::endl;
	//启动服务
	if (!StartService(hService, 0, NULL))
	{
		// 处理启动服务失败的情况
		CloseServiceHandle(hService);
		CloseServiceHandle(hService);
		return -1;
	}
	CloseServiceHandle(hService);
	CloseServiceHandle(hSCManager);

	return 0;
}

int main()
{
	/*bool bOn = AutoPowerOn();
	std::string str = bOn ? "true" : "false";
	std::cout << "run result:" << str.c_str() << std::endl;*/
	int ret = regSc();
	std::cout << "run reg sc result:" << ret << std::endl;
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
