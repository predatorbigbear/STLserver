#include "multiSQLREADPOOL.h"

MULTISQLREADPOOL::MULTISQLREADPOOL(std::shared_ptr<IOcontextPool> ioPool, std::shared_ptr<function<void()>> unlockFun, const std::string & SQLHOST, 
	const std::string & SQLUSER, const std::string & SQLPASSWORD, const std::string & SQLDB, const std::string & SQLPORT,
	const unsigned int commandMaxSize, const int bufferNum):
	m_bufferNum(bufferNum), m_unlockFun(unlockFun), m_SQLHOST(SQLHOST), m_SQLUSER(SQLUSER), m_SQLPASSWORD(SQLPASSWORD),
	m_SQLDB(SQLDB), m_SQLPORT(SQLPORT), m_ioPool(ioPool),m_commandMaxSize(commandMaxSize)
{
	try
	{
		if (!ioPool)
			throw std::runtime_error("ioPool is nullptr");
		if (!unlockFun)
			throw std::runtime_error("unlockFun is nullptr");
		if (SQLHOST.empty())
			throw std::runtime_error("SQLHOST is empty");
		if (SQLUSER.empty())
			throw std::runtime_error("SQLUSER is empty");
		if (SQLPASSWORD.empty())
			throw std::runtime_error("SQLPASSWORD is empty");
		if (SQLDB.empty())
			throw std::runtime_error("SQLDB is empty");
		if (SQLPORT.empty())
			throw std::runtime_error("SQLPORT is empty");
		if (!unlockFun)
			throw std::runtime_error("unlockFun is nullptr");
		if (!all_of(SQLPORT.begin(), SQLPORT.end(), std::bind(std::logical_and<bool>(), std::bind(std::greater_equal<char>(), std::placeholders::_1, '0'),
			std::bind(std::less_equal<char>(), std::placeholders::_1, '9'))))
			throw std::runtime_error("SQLPORT is not number");
		if (bufferNum <= 0)
			throw std::runtime_error("bufferNum is invaild");
		if(!commandMaxSize)
			throw std::runtime_error("commandMaxSize is invaild");

		m_loop.reset(new std::function<void()>(std::bind(&MULTISQLREADPOOL::nextReady, this)));
		readyReadyList();
	}
	catch (const std::exception &e)
	{
		cout << e.what() << "   ,please restart server\n";
	}
}



std::shared_ptr<MULTISQLREAD> MULTISQLREADPOOL::getSqlNext()
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
	try
	{
		if (m_bufferNum)
		{
			m_sqlPool.emplace_back(std::make_shared<MULTISQLREAD>(m_ioPool->getIoNext(), m_loop, m_SQLHOST, m_SQLUSER, m_SQLPASSWORD, m_SQLDB, m_SQLPORT, m_commandMaxSize));
		}
		else
		{
			(*m_unlockFun)();
		}
	}
	catch (const std::exception &e)
	{
		cout << e.what() << "   ,please restart server\n";
	}
}


void MULTISQLREADPOOL::nextReady()
{
	--m_bufferNum;
	readyReadyList();
}




