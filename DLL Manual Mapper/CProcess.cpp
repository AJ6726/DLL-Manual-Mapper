#include "CProcess.h"
#include <TlHelp32.h>

void CProcess::GetProcessId(std::string processName)
{
	HANDLE hProcessSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); 

	PROCESSENTRY32 ProcessEntry; 
	ProcessEntry.dwSize = sizeof(PROCESSENTRY32); 

	if (Process32First(hProcessSnapshot, &ProcessEntry))
	{
		do
		{
			if (!processName.compare(ProcessEntry.szExeFile))
			{
				m_id = ProcessEntry.th32ProcessID; 
				CloseHandle(hProcessSnapshot); 
				break; 
			}
		} while (Process32Next(hProcessSnapshot, &ProcessEntry)); 
	}
}
void CProcess::GetProcessHandle()
{
	m_handle = OpenProcess(PROCESS_ALL_ACCESS, false, m_id); 
}