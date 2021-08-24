#pragma once


#include "publicHeader.h"
#include "redisRead.h"
#include "IOcontextPool.h"


struct REDISREADPOOL
{
	REDISREADPOOL(std::shared_ptr<IOcontextPool> ioPool, std::shared_ptr<std::function<void()>>unlockFun, const std::string &redisIP, const unsigned int redisPort, const unsigned int bufferNum);

	std::shared_ptr<REDISREAD> getRedisNext();


private:
	std::shared_ptr<std::function<void()>>m_unlockFun;
	std::shared_ptr<std::function<void()>>m_innerUnLock;


	std::shared_ptr<IOcontextPool> m_ioPool;
	std::mutex m_redisMutex;
	std::vector<std::shared_ptr<REDISREAD>> m_redisList;
	std::vector<std::shared_ptr<REDISREAD>>::iterator m_iterNow;
	std::string m_redisIP;
	unsigned int m_redisPort;

	std::shared_ptr<REDISREAD> m_redis_temp;

	size_t m_redisNum{};

	int m_bufferNum{};


private:
	void readyReadyList();

	void unlock();

	void innerUnlock();
};
