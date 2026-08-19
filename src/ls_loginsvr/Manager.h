#pragma once

class Manager
{
public:
	Manager(void);
	virtual ~Manager(void);

	bool Init();
	bool Run(TCHAR* scriptName);

};

