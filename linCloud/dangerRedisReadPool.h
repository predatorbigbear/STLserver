#pragma once


#include "publicHeader.h"
#include "asyncRedisRead.h"
#include "IOcontextPool.h"


struct DANGERREDISREADPOOL
{
	DANGERREDISREADPOOL(std::shared_ptr<IOcontextPool> ioPool, std::shared_ptr<std::function<void()>>unlockFun, const std::string &redisIP, const unsigned int redisPort,
		const unsigned int memorySize, const unsigned int checkSize , const unsigned int bufferNum);

	std::shared_ptr<ASYNCREDISREAD> getRedisNext();


private:
	std::shared_ptr<std::function<void()>>m_unlockFun;
	std::shared_ptr<std::function<void()>>m_innerUnLock;


	std::shared_ptr<IOcontextPool> m_ioPool;
	std::mutex m_redisMutex;
	std::vector<std::shared_ptr<ASYNCREDISREAD>> m_redisList;
	std::vector<std::shared_ptr<ASYNCREDISREAD>>::iterator m_iterNow;
	std::string m_redisIP;
	unsigned int m_redisPort;
	unsigned int m_memorySize{};
	unsigned int m_checkSize{};

	size_t m_redisNum{};

	int m_bufferNum{};


private:
	void readyReadyList();

	void unlock();

	void innerUnlock();
};
