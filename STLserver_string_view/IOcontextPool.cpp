#include "IOcontextPool.h"

IOcontextPool::IOcontextPool()
{
}


void IOcontextPool::setThreadNum(bool& success, const size_t threadNum)
{
	try
	{
		if (!m_hasSetThreadNum)
		{
			m_hasSetThreadNum = true;
			m_ioc.reset(new std::vector<std::shared_ptr<boost::asio::io_context>>());
			size_t num{ threadNum };
			if (!num)
			{
				num = std::thread::hardware_concurrency();
			}
			m_threadNum = num;

			for (size_t i = 0; i != num; ++i)
			{
				std::shared_ptr<boost::asio::io_context> io{ new boost::asio::io_context() };
				m_ioc->emplace_back(io);
				m_work.emplace_back(io->get_executor());
				m_tg.create_thread([this, io]
				{
					io->run();
				});
			}
		}
		success = true;

	}
	catch (const std::exception &e)
	{
		m_threadNum = 0;
		success = false;
	}
}


std::shared_ptr<boost::asio::io_context> IOcontextPool::getIoNext()
{
	m_ioMutex.lock();
	//std::lock_guard<mutex>l1{ m_ioMutex };
	m_io_temp = (*m_ioc)[m_ioNum] ;
	++m_ioNum;
	if (m_ioNum == m_ioc->size())
		m_ioNum = 0;
	m_ioMutex.unlock();
	return m_io_temp;
}


void IOcontextPool::run()
{
	if (!m_hasRun)
	{
		m_hasRun = true;
		if (!m_hasSetThreadNum)
		{
			bool success;
			setThreadNum(success);
		}

		m_tg.join_all();
	}
}


void IOcontextPool::stop()
{
	if (m_hasRun)
	{
		for (int i = 0; i != m_ioc->size(); ++i)
		{
			if (!(*m_ioc)[i]->stopped())
				(*m_ioc)[i]->stop();
		}
		m_hasRun = false;
	}
}


const size_t IOcontextPool::getThreadNum()
{
	return m_threadNum;
}
