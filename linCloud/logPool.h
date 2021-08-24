#pragma once


#include "publicHeader.h"
#include "LOG.h"


struct LOGPOOL
{
	LOGPOOL(const char *logFileName, std::shared_ptr<IOcontextPool> ioPool, const int overTime = 60, const int bufferSize = 20480, const int bufferNum = 16);

	std::shared_ptr<LOG> getLogNext();
	

private:
	std::shared_ptr<IOcontextPool> m_ioPool{};
	int m_overTime{};
	int m_bufferSize{};
	int m_bufferNum{};

	std::mutex m_logMutex;
	std::string m_logFileName;
	std::vector<std::shared_ptr<LOG>>m_logPool;
	
	std::mutex logMutex;


	std::shared_ptr<LOG> m_log_temp{};

	size_t m_logNum{};



private:
	bool readyReadyList();

};