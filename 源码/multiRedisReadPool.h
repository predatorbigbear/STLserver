#pragma once


#include "logPool.h"
#include "multiRedisRead.h"
#include "IOcontextPool.h"


struct MULTIREDISREADPOOL
{
	MULTIREDISREADPOOL(std::shared_ptr<IOcontextPool> ioPool, std::shared_ptr<LOGPOOL> logPool, std::shared_ptr<std::function<void()>>unlockFun,
		std::shared_ptr<STLTimeWheel> timeWheel,
		const std::string &redisIP, const unsigned int redisPort,
		const unsigned int bufferNum, 
		const unsigned int memorySize, const unsigned int outRangeMaxSize, const unsigned int commandSize);

	std::shared_ptr<MULTIREDISREAD> getRedisNext();


private:
	std::shared_ptr<std::function<void()>>m_unlockFun{};
	std::shared_ptr<std::function<void()>>m_innerUnLock{};
	std::shared_ptr<STLTimeWheel> m_timeWheel{};

	std::shared_ptr<LOGPOOL> m_logPool{};

	std::shared_ptr<IOcontextPool> m_ioPool{};
	std::mutex m_redisMutex;
	std::vector<std::shared_ptr<MULTIREDISREAD>> m_redisList;
	std::vector<std::shared_ptr<MULTIREDISREAD>>::iterator m_iterNow;
	std::string m_redisIP;
	unsigned int m_redisPort{};
	unsigned int m_memorySize{};
	unsigned int m_outRangeMaxSize{};
	unsigned int m_commandSize{};


	std::shared_ptr<MULTIREDISREAD> m_redis_temp{};

	size_t m_redisNum{};

	int m_bufferNum{};


private:
	void readyReadyList();

	void unlock();

	void innerUnlock();
};

