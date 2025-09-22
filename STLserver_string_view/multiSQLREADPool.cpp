#include "multiSQLREADPool.h"

MULTISQLREADPOOL::MULTISQLREADPOOL(const std::shared_ptr<IOcontextPool>& ioPool, const std::shared_ptr<std::function<void()>>& unlockFun, 
	const std::shared_ptr<STLTimeWheel>& timeWheel, const std::shared_ptr<LOGPOOL>& logPool,
	const std::string& SQLHOST, const std::string& SQLUSER, const std::string& SQLPASSWORD,
	const std::string& SQLDB, const unsigned int SQLPORT,
	bool& success, const unsigned int commandMaxSize, const unsigned int bufferSize, const int bufferNum):
	m_ioPool(ioPool),m_unlockFun(unlockFun),m_timeWheel(timeWheel),m_logPool(logPool),
	m_SQLHOST(SQLHOST),m_SQLUSER(SQLUSER),m_SQLPASSWORD(SQLPASSWORD),m_SQLDB(SQLDB),m_SQLPORT(SQLPORT),
	m_commandMaxSize(commandMaxSize),m_bufferSize(bufferSize),m_bufferNum(bufferNum)
{
	if (!ioPool)
		throw std::runtime_error("ioPool is nullptr");
	if (!unlockFun)
		throw std::runtime_error("unlockFun is nullptr");
	if (SQLHOST.empty())
		throw std::runtime_error("SQLHOST is empty");
	if (SQLUSER.empty())
		throw std::runtime_error("SQLUSER is empty");
	if (SQLDB.empty())
		throw std::runtime_error("SQLDB is empty");
	if (!SQLPORT)
		throw std::runtime_error("SQLPORT is invaild");
	if (!unlockFun)
		throw std::runtime_error("unlockFun is nullptr");
	if (!logPool)
		throw std::runtime_error("logPool is nullptr");
	if (!bufferNum)
		throw std::runtime_error("bufferNum is invaild");
	if (!commandMaxSize)
		throw std::runtime_error("commandMaxSize is invaild");
	if (!bufferSize)
		throw std::runtime_error("bufferSize is invaild");

	m_success = &success;
	m_loop.reset(new std::function<void()>(std::bind(&MULTISQLREADPOOL::nextReady, this)));
	readyReadyList();

}



void MULTISQLREADPOOL::nextReady()
{
	m_sqlMutex.unlock();
}


std::shared_ptr<MULTISQLREAD>& MULTISQLREADPOOL::getSqlNext()
{
	m_sqlMutex.lock();
	m_sql_temp = m_sqlPool[m_sqlNum];
	++m_sqlNum;
	if (m_sqlNum == m_sqlPool.size())
		m_sqlNum = 0;
	m_sqlMutex.unlock();
	return m_sql_temp;
}


void MULTISQLREADPOOL::readyReadyList()
{
	bool success{true};
	for (unsigned int i = 0; i != m_bufferNum; ++i)
	{
		m_sqlMutex.lock();
		if (!success)
		{
			*m_success = false;
			break;
		}
		m_sqlPool.emplace_back(std::make_shared<MULTISQLREAD>(m_ioPool->getIoNext(), m_loop, m_timeWheel, m_logPool->getLogNext(),
			m_SQLHOST, m_SQLUSER, m_SQLPASSWORD, m_SQLDB, m_SQLPORT, success,
			m_commandMaxSize, m_bufferSize));
	}
	m_sqlMutex.lock();
	if (!success)
	{
		*m_success = false;
	}
	else
	{
		*m_success = true;
	}
	(*m_unlockFun)();
	m_sqlMutex.unlock();

}