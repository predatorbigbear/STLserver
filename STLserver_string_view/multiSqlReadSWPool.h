#pragma once


#include "IOcontextPool.h"
#include "multiSQLREADSW.h"
#include "logPool.h"

#include<functional>


struct MULTISQLREADSWPOOL
{
	MULTISQLREADSWPOOL(const std::shared_ptr<IOcontextPool> &ioPool,const std::shared_ptr<std::function<void()>> &unlockFun, 
		const std::string &SQLHOST, const std::string &SQLUSER,
		const std::string &SQLPASSWORD, const std::string &SQLDB, const std::string &SQLPORT, const std::shared_ptr<LOGPOOL> &logPool, 
		const unsigned int commandMaxSize = 50, const int bufferNum = 4, const unsigned int bufferSize = 4096000);

	std::shared_ptr<MULTISQLREADSW> getSqlNext();


private:
	const std::shared_ptr<IOcontextPool> m_ioPool{};
	int m_bufferNum{};
	unsigned int m_commandMaxSize{};
	unsigned int m_bufferSize{};

	std::mutex m_sqlMutex;
	std::vector<std::shared_ptr<MULTISQLREADSW>>m_sqlPool{};
	const std::shared_ptr<std::function<void()>>m_unlockFun{};
	std::shared_ptr<std::function<void()>>m_loop{};
	const std::shared_ptr<LOGPOOL> m_logPool{};

	std::shared_ptr<MULTISQLREADSW> m_sql_temp{};

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