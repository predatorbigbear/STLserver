#include "multiRedisWritePool.h"

MULTIREDISWRITEPOOL::MULTIREDISWRITEPOOL(std::shared_ptr<IOcontextPool> ioPool, std::shared_ptr<LOGPOOL> logPool, std::shared_ptr<std::function<void()>> unlockFun,
	std::shared_ptr<STLTimeWheel> timeWheel,
	const std::string& redisIP,
	const unsigned int redisPort,
	const unsigned int bufferNum,
	const unsigned int memorySize, const unsigned int outRangeMaxSize, const unsigned int commandSize)
	:m_unlockFun(unlockFun), m_redisIP(redisIP), m_redisPort(redisPort), m_ioPool(ioPool), m_bufferNum(bufferNum),
	m_memorySize(memorySize), m_outRangeMaxSize(outRangeMaxSize), m_commandSize(commandSize), m_logPool(logPool), m_timeWheel(timeWheel)
{
	if (!ioPool)
		throw std::runtime_error("ioPool is nullptr");
	if (!logPool)
		throw std::runtime_error("log is nullptr");
	if (!timeWheel)
		throw std::runtime_error("timeWheel is nullptr");
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


	m_innerUnLock.reset(new std::function<void()>(std::bind(&MULTIREDISWRITEPOOL::innerUnlock, this)));

	readyReadyList();
}



std::shared_ptr<MULTIREDISWRITE> MULTIREDISWRITEPOOL::getRedisNext()
{
	m_redisMutex.lock();
	m_redis_temp = m_redisList[m_redisNum];
	++m_redisNum;
	if (m_redisNum == m_redisList.size())
		m_redisNum = 0;
	m_redisMutex.unlock();
	return m_redis_temp;
}



void MULTIREDISWRITEPOOL::readyReadyList()
{
	for (unsigned int i = 0; i != m_bufferNum; ++i)
	{
		m_redisMutex.lock();
		m_redisList.emplace_back(std::make_shared<MULTIREDISWRITE>(m_ioPool->getIoNext(), m_logPool->getLogNext(), m_innerUnLock,
			m_timeWheel,
			m_redisIP, m_redisPort, m_memorySize, m_outRangeMaxSize, m_commandSize));
	}
	m_redisMutex.lock();
	unlock();
	m_redisMutex.unlock();
}


void MULTIREDISWRITEPOOL::unlock()
{
	(*m_unlockFun)();
}


void MULTIREDISWRITEPOOL::innerUnlock()
{
	m_redisMutex.unlock();
}
