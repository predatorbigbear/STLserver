#include "multiRedisReadPool.h"

MULTIREDISREADPOOL::MULTIREDISREADPOOL(std::shared_ptr<IOcontextPool> ioPool, std::shared_ptr<std::function<void()>> unlockFun, const std::string & redisIP, 
	const unsigned int redisPort,
	const unsigned int bufferNum,
	const unsigned int memorySize, const unsigned int outRangeMaxSize, const unsigned int commandSize)
	:m_unlockFun(unlockFun), m_redisIP(redisIP), m_redisPort(redisPort), m_ioPool(ioPool), m_bufferNum(bufferNum),
	m_memorySize(memorySize),m_outRangeMaxSize(outRangeMaxSize),m_commandSize(commandSize)
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
		if (!memorySize)
			throw std::runtime_error("memorySize is invaild");
		if (!outRangeMaxSize)
			throw std::runtime_error("outRangeMaxSize is invaild");
		if (!commandSize)
			throw std::runtime_error("commandSize is invaild");


		m_innerUnLock.reset(new std::function<void()>(std::bind(&MULTIREDISREADPOOL::innerUnlock, this)));

		readyReadyList();
	}
	catch (const std::exception &e)
	{
		cout << e.what() << "   ,please restart server\n";
		throw;
	}
}



std::shared_ptr<MULTIREDISREAD> MULTIREDISREADPOOL::getRedisNext()
{
	m_redisMutex.lock();
	m_redis_temp = m_redisList[m_redisNum];
	++m_redisNum;
	if (m_redisNum == m_redisList.size())
		m_redisNum = 0;
	m_redisMutex.unlock();
	return m_redis_temp;
}



void MULTIREDISREADPOOL::readyReadyList()
{
	try
	{
		for (unsigned int i = 0; i != m_bufferNum; ++i)
		{
			m_redisMutex.lock();
			//std::shared_ptr<REDISREAD>tempRedisRead{ std::make_shared<REDISREAD>(m_ioPool->getIoNext() ,m_innerUnLock ,m_redisIP ,m_redisPort) };
			m_redisList.emplace_back(std::make_shared<MULTIREDISREAD>(m_ioPool->getIoNext(), m_innerUnLock, m_redisIP, m_redisPort, m_memorySize, m_outRangeMaxSize, m_commandSize));
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


void MULTIREDISREADPOOL::unlock()
{
	(*m_unlockFun)();
}


void MULTIREDISREADPOOL::innerUnlock()
{
	m_redisMutex.unlock();
}
