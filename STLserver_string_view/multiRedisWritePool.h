#pragma once


#include "multiRedisWrite.h"
#include "IOcontextPool.h"


struct MULTIREDISWRITEPOOL
{
	MULTIREDISWRITEPOOL(std::shared_ptr<IOcontextPool> ioPool, std::shared_ptr<std::function<void()>>unlockFun, const std::string &redisIP, const unsigned int redisPort,
		const unsigned int bufferNum,
		const unsigned int memorySize, const unsigned int outRangeMaxSize, const unsigned int commandSize);

	std::shared_ptr<MULTIREDISWRITE> getRedisNext();


private:
	std::shared_ptr<std::function<void()>>m_unlockFun{};
	std::shared_ptr<std::function<void()>>m_innerUnLock{};


	std::shared_ptr<IOcontextPool> m_ioPool{};
	std::mutex m_redisMutex;
	std::vector<std::shared_ptr<MULTIREDISWRITE>> m_redisList;
	std::vector<std::shared_ptr<MULTIREDISWRITE>>::iterator m_iterNow;
	std::string m_redisIP;
	unsigned int m_redisPort{};
	unsigned int m_memorySize{};
	unsigned int m_outRangeMaxSize{};
	unsigned int m_commandSize{};


	std::shared_ptr<MULTIREDISWRITE> m_redis_temp{};

	size_t m_redisNum{};

	int m_bufferNum{};


private:
	void readyReadyList();

	void unlock();

	void innerUnlock();
};

