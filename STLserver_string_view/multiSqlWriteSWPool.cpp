#include "multiSqlWriteSWPool.h"

MULTISQLWRITESWPOOL::MULTISQLWRITESWPOOL(std::shared_ptr<IOcontextPool> ioPool, std::shared_ptr<std::function<void()>> unlockFun, const std::string & SQLHOST,
	const std::string & SQLUSER, const std::string & SQLPASSWORD, const std::string & SQLDB, const std::string & SQLPORT, std::shared_ptr<LOGPOOL> logPool,
	const unsigned int commandMaxSize, const int bufferNum) :
	m_bufferNum(bufferNum), m_unlockFun(unlockFun), m_SQLHOST(SQLHOST), m_SQLUSER(SQLUSER), m_SQLPASSWORD(SQLPASSWORD),
	m_SQLDB(SQLDB), m_SQLPORT(SQLPORT), m_ioPool(ioPool), m_commandMaxSize(commandMaxSize), m_logPool(logPool)
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
	if (SQLPORT.empty())
		throw std::runtime_error("SQLPORT is empty");
	if (!unlockFun)
		throw std::runtime_error("unlockFun is nullptr");
	if (!logPool)
		throw std::runtime_error("logPool is nullptr");
	if (!all_of(SQLPORT.begin(), SQLPORT.end(), std::bind(std::logical_and<bool>(), std::bind(std::greater_equal<char>(), std::placeholders::_1, '0'),
		std::bind(std::less_equal<char>(), std::placeholders::_1, '9'))))
		throw std::runtime_error("SQLPORT is not number");
	if (bufferNum <= 0)
		throw std::runtime_error("bufferNum is invaild");
	if (!commandMaxSize)
		throw std::runtime_error("commandMaxSize is invaild");

	m_loop.reset(new std::function<void()>(std::bind(&MULTISQLWRITESWPOOL::nextReady, this)));
	readyReadyList();
}



std::shared_ptr<MULTISQLWRITESW> MULTISQLWRITESWPOOL::getSqlNext()
{
	m_sqlMutex.lock();
	m_sql_temp = m_sqlPool[m_sqlNum];
	++m_sqlNum;
	if (m_sqlNum == m_sqlPool.size())
		m_sqlNum = 0;
	m_sqlMutex.unlock();
	return m_sql_temp;
}


void MULTISQLWRITESWPOOL::readyReadyList()
{
	for (unsigned int i = 0; i != m_bufferNum; ++i)
	{
		m_sqlMutex.lock();
		m_sqlPool.emplace_back(std::make_shared<MULTISQLWRITESW>(m_ioPool->getIoNext(), m_loop, m_SQLHOST, m_SQLUSER, m_SQLPASSWORD, m_SQLDB, m_SQLPORT, m_commandMaxSize, m_logPool->getLogNext()));
	}
	m_sqlMutex.lock();
	(*m_unlockFun)();
	m_sqlMutex.unlock();

}


void MULTISQLWRITESWPOOL::nextReady()
{
	m_sqlMutex.unlock();
}



