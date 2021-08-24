#include "redisReadPool.h"








REDISREADPOOL::REDISREADPOOL(std::shared_ptr<IOcontextPool> ioPool, std::shared_ptr<std::function<void()>> unlockFun, const std::string & redisIP, const unsigned int redisPort, 
	const unsigned int bufferNum)
	:m_unlockFun(unlockFun), m_redisIP(redisIP),m_redisPort(redisPort),m_ioPool(ioPool), m_bufferNum(bufferNum)
{
	try
	{
		if (!ioPool)
			throw std::runtime_error("ioPool is nullptr");
		if (!unlockFun)
			throw std::runtime_error("unlockFun is nullptr");
		if (!REGEXFUNCTION::isVaildIpv4(redisIP))
			throw std::runtime_error("redisIP is invaild");
		if (redisPort > 65535)
			throw std::runtime_error("redisPort is invaild");
		if (!bufferNum)
			throw std::runtime_error("bufferNum is invaild");
		m_innerUnLock.reset(new std::function<void()>(std::bind(&REDISREADPOOL::innerUnlock, this)));
	}
	catch (const std::exception &e)
	{
		cout << e.what() << "   ,please restart server\n";
	}
	readyReadyList();
}



std::shared_ptr<REDISREAD> REDISREADPOOL::getRedisNext()
{
	//std::lock_guard<mutex>l1{ m_redisMutex };
	m_redisMutex.lock();
	m_redis_temp = m_redisList[m_redisNum] ;
	++m_redisNum;
	if (m_redisNum == m_redisList.size())
		m_redisNum = 0;
	m_redisMutex.unlock();
	return m_redis_temp;
}



void REDISREADPOOL::readyReadyList()
{
	try
	{
		for (unsigned int i = 0; i != m_bufferNum; ++i)
		{
			m_redisMutex.lock();
			//std::shared_ptr<REDISREAD>tempRedisRead{ std::make_shared<REDISREAD>(m_ioPool->getIoNext() ,m_innerUnLock ,m_redisIP ,m_redisPort) };
			m_redisList.emplace_back(std::make_shared<REDISREAD>(m_ioPool->getIoNext(), m_innerUnLock, m_redisIP, m_redisPort));
		}
		m_redisMutex.lock();
		unlock();
		m_redisMutex.unlock();
	}
	catch (const std::exception &e)
	{
		cout << e.what() << "   ,please restart server\n";
	}
}



void REDISREADPOOL::unlock()
{
	(*m_unlockFun)();
}


void REDISREADPOOL::innerUnlock()
{
	m_redisMutex.unlock();
}
