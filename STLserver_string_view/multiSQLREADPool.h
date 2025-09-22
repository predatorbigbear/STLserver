#pragma once


#include "IOcontextPool.h"
#include "multiSQLREAD.h"
#include "logPool.h"
#include "STLTimeWheel.h"

#include<functional>


struct MULTISQLREADPOOL
{

	MULTISQLREADPOOL(const std::shared_ptr<IOcontextPool> &ioPool,const std::shared_ptr<std::function<void()>> &unlockFun,
		const std::shared_ptr<STLTimeWheel>& timeWheel,
		const std::shared_ptr<LOGPOOL>& logPool,
		const std::string &SQLHOST, const std::string &SQLUSER,
		const std::string &SQLPASSWORD, const std::string &SQLDB, const unsigned int SQLPORT, 
		bool& success,
		const unsigned int commandMaxSize = 50, const unsigned int bufferSize = 67108864, const int bufferNum = 4);

	std::shared_ptr<MULTISQLREAD>& getSqlNext();


private:
	const std::shared_ptr<IOcontextPool> m_ioPool{};
	const std::shared_ptr<STLTimeWheel> m_timeWheel{};

	std::mutex m_sqlMutex;
	std::vector<std::shared_ptr<MULTISQLREAD>>m_sqlPool{};
	const std::shared_ptr<std::function<void()>>m_unlockFun{};
	std::shared_ptr<std::function<void()>>m_loop{};
	const std::shared_ptr<LOGPOOL> m_logPool{};

	std::shared_ptr<MULTISQLREAD> m_sql_temp{};

	const std::string m_SQLHOST;
	const std::string m_SQLUSER;
	const std::string m_SQLPASSWORD;
	const std::string m_SQLDB;
	const unsigned int m_SQLPORT;

	const unsigned int m_commandMaxSize{};
	const unsigned int m_bufferSize{};
	const int m_bufferNum{};

	size_t m_sqlNum{};

	bool* m_success{ };



private:
	void readyReadyList();

	void nextReady();

};