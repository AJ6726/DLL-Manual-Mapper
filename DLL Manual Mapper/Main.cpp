#include "CManualMapper.h"
#include <iostream>

int main()
{
	std::string processName; 
	std::string fullDllPath; 

	std::cout << "Process name: "; 
	std::getline(std::cin, processName); 
	
	std::cout << "Full dll path: "; 
	std::getline(std::cin, fullDllPath); 

	std::string fixedDllPath; 

	for (char c : fullDllPath)
	{
		if (c == '\\')
			fixedDllPath.push_back(c); 

		fixedDllPath.push_back(c);
	}

	CManualMapper ManualMapper; 
	ManualMapper.GetProcessId(processName); 
	ManualMapper.GetProcessHandle(); 
	ManualMapper.LoadDll(fixedDllPath); 

	std::cout << "L to free memory, module must have finished execution" << std::endl; 
	while (true)
	{
		if (GetAsyncKeyState('L') & 1)
		{
			ManualMapper.FreeMemoryOnProcess();
			break; 
		}
	}

}