#pragma once


class Manager
{
public:
	Manager(void);
	virtual ~Manager(void);

public:
	bool Init();

public:
	bool Run(TCHAR* scriptName);
	void SetTcpLog();
};

