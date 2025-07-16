#include "logPool.h"

LOGPOOL::LOGPOOL(const char * logFileName, std::shared_ptr<IOcontextPool> ioPool, bool& success, const int overTime, const int bufferSize, const int bufferNum):
	m_logFileName(logFileName),m_ioPool(ioPool),m_overTime(overTime),m_bufferSize(bufferSize),m_bufferNum(bufferNum)
{
	if (!logFileName)
		throw std::runtime_error("logFileName is invaild");
	if (!ioPool)
		throw std::runtime_error("ioPool is invaild");
	if (overTime <= 0)
		throw std::runtime_error("overTime is invaild");
	if (bufferSize <= 0)
		throw std::runtime_error("bufferSize is invaild");
	if (bufferNum <= 0)
		throw std::runtime_error("bufferNum is invaild");
	success = readyReadyList();
}




std::shared_ptr<LOG> LOGPOOL::getLogNext()
{
	//std::lock_guard<std::mutex>l1{ m_logMutex };
	m_logMutex.lock();
	m_log_temp =  m_logPool[m_logNum] ;
	++m_logNum;
	if (m_logNum == m_logPool.size())
		m_logNum = 0;
	m_logMutex.unlock();
	return m_log_temp;
}




bool LOGPOOL::readyReadyList()
{
	try
	{
		bool result{};
		for (int i = 0; i != m_bufferNum; ++i)
		{
			m_logPool.emplace_back(std::make_shared<LOG>((m_logFileName + std::to_string(i)).c_str(), m_ioPool, result , m_overTime + i, m_bufferSize));
			if (!result)
				return false;
		}
		return true;
	}
	catch (const std::exception &e)
	{
		std::cout << e.what() << "  ,please restart server\n";
		return false;
	}
}
