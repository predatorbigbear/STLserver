#include "sqlPool.h"

SQLPOOL::SQLPOOL(shared_ptr<IOcontextPool> ioPool, shared_ptr<function<void()>> unlockFun, const string & SQLHOST, const string & SQLUSER, const string & SQLPASSWORD, 
	const string & SQLDB, const string & SQLPORT, const int bufferNum):
	m_bufferNum(bufferNum), m_unlockFun(unlockFun),m_SQLHOST(SQLHOST),m_SQLUSER(SQLUSER),m_SQLPASSWORD(SQLPASSWORD),
	m_SQLDB(SQLDB),m_SQLPORT(SQLPORT), m_ioPool(ioPool)
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
		if (!all_of(SQLPORT.begin(), SQLPORT.end(), std::bind(std::logical_and<bool>(), std::bind(greater_equal<char>(), std::placeholders::_1, '0'), std::bind(less_equal<char>(), std::placeholders::_1, '9'))))
			throw std::runtime_error("SQLPORT is not number");
		if(bufferNum<=0)
			throw std::runtime_error("bufferNum is invaild");
		m_loop.reset(new std::function<void()>(std::bind(&SQLPOOL::nextReady, this)));
		readyReadyList();
	}
	catch (const std::exception &e)
	{
		cout << e.what() << "   ,please restart server\n";
	}
}



shared_ptr<SQL> SQLPOOL::getSqlNext()
{
	std::lock_guard<mutex>l1{ m_sqlMutex };
	shared_ptr<SQL> sql_temp{ m_sqlPool[m_sqlNum] };
	++m_sqlNum;
	if (m_sqlNum == m_sqlPool.size())
		m_sqlNum = 0;
	return sql_temp;
}




void SQLPOOL::readyReadyList()
{
	if (m_bufferNum)
	{
		shared_ptr<SQL>tempSql{ std::make_shared<SQL>(m_ioPool->getIoNext(),m_loop , m_SQLHOST, m_SQLUSER, m_SQLPASSWORD, m_SQLDB, m_SQLPORT) };
		m_sqlPool.emplace_back(tempSql);
	}
	else
	{
		(*m_unlockFun)();
	}
}



void SQLPOOL::nextReady()
{
	--m_bufferNum;
	readyReadyList();
}
