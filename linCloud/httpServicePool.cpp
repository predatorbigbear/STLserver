#include "httpServicePool.h"

HTTPSERVICEPOOL::HTTPSERVICEPOOL(std::shared_ptr<IOcontextPool> ioPool, std::shared_ptr<SocketPool> sockPool,
	std::shared_ptr<MULTISQLREADSWPOOL>multiSqlReadSWPool, std::shared_ptr<MULTIREDISREADPOOL>multiRedisReadPool, std::shared_ptr<MULTIREDISWRITEPOOL>multiRedisWritePool,
	std::shared_ptr<std::function<void()>>start , std::shared_ptr<LOGPOOL> logPool, const unsigned int timeOut , int beginSize, int newSize):
	m_ioPool(ioPool), m_sockPool(sockPool), m_newSize(newSize),m_start(start), m_logPool(logPool), m_multiSqlReadSWPool(multiSqlReadSWPool), 
	m_multiRedisReadPool(multiRedisReadPool), m_multiRedisWritePool(multiRedisWritePool), m_timeOut(timeOut)
{
	try
	{
		if (!ioPool)
			throw std::runtime_error("ioPool is null");
		if (!sockPool)
			throw std::runtime_error("sockPool is null");
		if (beginSize <= 0)
			throw std::runtime_error("beginSize is invaild");
		if (newSize < 0)
			throw std::runtime_error("newSize is invaild");
		if(!start)
			throw std::runtime_error("start is invaild");
		if (!logPool)
			throw std::runtime_error("logPool is null");
		if(!timeOut)
			throw std::runtime_error("timeOut is invaild");
		if(!multiSqlReadSWPool)
			throw std::runtime_error("multiSqlReadSWPool is null");
		if (!multiRedisReadPool)
			throw std::runtime_error("multiRedisReadPool is null");
		if (!multiRedisWritePool)
			throw std::runtime_error("multiRedisWritePool is null");


		m_log = m_logPool->getLogNext();
		m_timer.reset(new boost::asio::steady_timer(*(m_ioPool->getIoNext())));
		ready(beginSize);
	}
	catch (const std::exception &e)
	{
		cout << e.what() << '\n';
		throw;
	}
}


void HTTPSERVICEPOOL::ready(int beginSize)
{
	try
	{
		if (m_bufferList.empty())
		{
			for (i = 0; i != beginSize; ++i)
			{
				m_success = true;
				m_bufferList.emplace_back(std::make_shared<HTTPSERVICE>(m_ioPool->getIoNext(), m_sockPool, m_logPool->getLogNext(), m_multiSqlReadSWPool->getSqlNext(), 
					m_multiRedisReadPool->getRedisNext(), m_multiRedisWritePool->getRedisNext(),
					m_timeOut, m_success));
				if (!m_success)
					break;
			}
		}
		if(!m_bufferList.empty())
			m_iterNow = m_bufferList.begin();
		m_timer->expires_after(std::chrono::seconds(5));
		m_timer->async_wait([this](const boost::system::error_code &err)
		{
			if (!err)
				(*m_start)();
		});
	}
	catch (const std::exception &e)
	{
		if (!m_bufferList.empty())
		{
			m_iterNow = m_bufferList.begin();
		}
	}
}



void HTTPSERVICEPOOL::getNextBuffer(std::shared_ptr<HTTPSERVICE> &outBuffer)
{
	//std::lock_guard<mutex>m1{ m_listMutex };
	m_listMutex.lock();
	try
	{
		if (m_iterNow == m_bufferList.end() && m_newSize)
		{
			--m_iterNow;
			for (i = 0; i != m_newSize; ++i)
			{
				m_success = true;
				m_bufferList.emplace_back(std::make_shared<HTTPSERVICE>(m_ioPool->getIoNext(), m_sockPool, m_logPool->getLogNext(), m_multiSqlReadSWPool->getSqlNext(),
					m_multiRedisReadPool->getRedisNext(), m_multiRedisWritePool->getRedisNext(),
					m_timeOut, m_success));
				if (!m_success)
					break;
			}
			++m_iterNow;
		}
		if (m_iterNow != m_bufferList.end())
		{
			outBuffer = *m_iterNow;
			++m_iterNow;
			m_listMutex.unlock();
			return;
			//return m_temp;
		}
		m_listMutex.unlock();
		//return nullptr;
	}
	catch (const std::exception &e)
	{
		if (!m_bufferList.empty())
		{
			if (m_iterNow != m_bufferList.end())
			{
				++m_iterNow;
				outBuffer = *m_iterNow ;
				++m_iterNow;
				//m_listMutex.unlock();
				//return;
				//return m_temp;
			}
			//m_listMutex.unlock();
			//return nullptr;
		}
		m_listMutex.unlock();
		//return nullptr;
	}
}


void HTTPSERVICEPOOL::getBackElem(std::shared_ptr<HTTPSERVICE>& buffer)
{
	//std::lock_guard<mutex>m1{ m_listMutex };
	m_listMutex.lock();
	if (buffer)
	{
		if (!m_bufferList.empty() && m_iterNow != m_bufferList.begin())
		{
			--m_iterNow;
			*m_iterNow = buffer;
		}
	}
	m_listMutex.unlock();
}
