#pragma once


#include "publicHeader.h"
#include "IOcontextPool.h"
#include "multiSQLREAD.h"


struct MULTISQLREADPOOL
{
	MULTISQLREADPOOL(std::shared_ptr<IOcontextPool> ioPool, std::shared_ptr<function<void()>>unlockFun, const std::string &SQLHOST, const std::string &SQLUSER,
		const std::string &SQLPASSWORD, const std::string &SQLDB, const std::string &SQLPORT, const unsigned int commandMaxSize = 50 , const int bufferNum = 4);

	std::shared_ptr<MULTISQLREAD> getSqlNext();


private:
	std::shared_ptr<IOcontextPool> m_ioPool;
	int m_bufferNum{};
	unsigned int m_commandMaxSize{};


	std::mutex m_sqlMutex;
	std::vector<std::shared_ptr<MULTISQLREAD>>m_sqlPool;
	std::shared_ptr<std::function<void()>>m_unlockFun;
	std::shared_ptr<std::function<void()>>m_loop;


	std::shared_ptr<MULTISQLREAD> m_sql_temp;

	std::string m_SQLHOST;
	std::string m_SQLUSER;
	std::string m_SQLPASSWORD;
	std::string m_SQLDB;
	std::string m_SQLPORT;

	size_t m_sqlNum{};



private:
	void readyReadyList();

	void nextReady();

};