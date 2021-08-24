#pragma once


#include "publicHeader.h"
#include "IOcontextPool.h"
#include "SQL.h"


struct SQLPOOL
{
	SQLPOOL(shared_ptr<IOcontextPool> ioPool, shared_ptr<function<void()>>unlockFun, const string &SQLHOST, const string &SQLUSER, const string &SQLPASSWORD, const string &SQLDB, const string &SQLPORT ,const int bufferNum = 4);

	shared_ptr<SQL> getSqlNext();


private:
	shared_ptr<IOcontextPool> m_ioPool;
	int m_bufferNum{};

	mutex m_sqlMutex;
	vector<shared_ptr<SQL>>m_sqlPool;
	shared_ptr<function<void()>>m_unlockFun;
	shared_ptr<function<void()>>m_loop;


	string m_SQLHOST;
	string m_SQLUSER;
	string m_SQLPASSWORD;
	string m_SQLDB;
	string m_SQLPORT;

	size_t m_sqlNum{};



private:
	void readyReadyList();

	void nextReady();

};