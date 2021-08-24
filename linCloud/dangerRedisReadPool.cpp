#include "dangerRedisReadPool.h"

DANGERREDISREADPOOL::DANGERREDISREADPOOL(std::shared_ptr<IOcontextPool> ioPool, std::shared_ptr<std::function<void()>> unlockFun, const std::string & redisIP,
	const unsigned int redisPort, const unsigned int memorySize, const unsigned int checkSize, const unsigned int bufferNum)
	:m_unlockFun(unlockFun), m_redisIP(redisIP), m_redisPort(redisPort), m_ioPool(ioPool), m_bufferNum(bufferNum), m_memorySize(memorySize), m_checkSize(checkSize)
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
		if (!m_memorySize)
			throw std::runtime_error("memorySize is invaild");
		if (!checkSize || checkSize >= m_memorySize)
			throw std::runtime_error("checkSize is too small");
		m_innerUnLock.reset(new std::function<void()>(std::bind(&DANGERREDISREADPOOL::innerUnlock, this)));
	}
	catch (const std::exception &e)
	{
		cout << e.what() << "   ,please restart server\n";
	}
	readyReadyList();
}


shared_ptr<ASYNCREDISREAD> DANGERREDISREADPOOL::getRedisNext()
{
	m_redisMutex.lock();
	std::shared_ptr<ASYNCREDISREAD> redis_temp{ m_redisList[m_redisNum] };
	++m_redisNum;
	if (m_redisNum == m_redisList.size())
		m_redisNum = 0;
	m_redisMutex.unlock();
	return redis_temp;
}


void DANGERREDISREADPOOL::readyReadyList()
{
	try
	{
		for (unsigned int i = 0; i != m_bufferNum; ++i)
		{
			m_redisMutex.lock();
			//std::shared_ptr<ASYNCREDISREAD>tempRedisRead{ std::make_shared<ASYNCREDISREAD>(m_ioPool->getIoNext() ,m_innerUnLock ,m_redisIP ,m_redisPort ,m_memorySize ,m_checkSize) };
			m_redisList.emplace_back(std::make_shared<ASYNCREDISREAD>(m_ioPool->getIoNext(), m_innerUnLock, m_redisIP, m_redisPort, m_memorySize, m_checkSize));
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



void DANGERREDISREADPOOL::unlock()
{
	(*m_unlockFun)();
}



void DANGERREDISREADPOOL::innerUnlock()
{
	m_redisMutex.unlock();
}

