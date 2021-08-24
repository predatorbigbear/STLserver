#include "socketPool.h"


//  beginSize为本池的设定容量，如果nowSize小于它，那么在无法获取buffer的情况下会尝试新建以补充



SocketPool::SocketPool(std::shared_ptr<IOcontextPool> ioPool, std::shared_ptr<std::function<void()>> startFunction, std::shared_ptr<std::function<void()>> starthttpServicePool , 
	std::shared_ptr<LOG> log  , int beginSize, int newSize):
	m_ioPool(ioPool), m_beginSize(beginSize),m_newSize(newSize), m_startFunction(startFunction),m_starthttpServicePool(starthttpServicePool),m_log(log)
{
	try
	{
		if (!ioPool)
			throw std::runtime_error("ioPool is null");
		if (beginSize <= 0)
			throw std::runtime_error("beginSize is invaild");
		if (m_newSize < 0)
			throw std::runtime_error("newSize is invaild");
		if (!startFunction)
			throw std::runtime_error("startFunction is invaild");
		if (!starthttpServicePool)
			throw std::runtime_error("starthttpServicePool is invaild");
		m_timer.reset(new boost::asio::steady_timer(*(m_ioPool->getIoNext())));
		ready(m_beginSize);
	}
	catch (const std::exception &e)
	{
		cout << e.what() << "  , please restart server\n";
	}
}



void SocketPool::ready(int beginSize)
{
	try
	{
		if (m_bufferList.empty())
		{
			for (i = 0; i != beginSize; ++i)
			{
				m_bufferList.emplace_back(std::make_shared<ReadBuffer>(m_ioPool->getIoNext()));
			}
		}
		if (!m_bufferList.empty())
		{
			m_iterNow = m_bufferList.begin();
		}
		m_timer->expires_after(std::chrono::seconds(5));
		m_timer->async_wait([this](const boost::system::error_code &err)
		{
			if(!err)
				(*m_starthttpServicePool)();
		});
	}
	catch (const std::exception &e)
	{
		//m_log->writeLog(__FILE__, __LINE__, e.what());
		if (!m_bufferList.empty())
		{
			m_iterNow = m_bufferList.begin();
		}
	}
}



void SocketPool::getNextBuffer(std::shared_ptr<ReadBuffer> &outBuffer)
{
	//std::lock_guard<std::mutex>m1{ m_listMutex };
	m_listMutex.lock();
	try
	{
		if (!m_bufferList.empty())
		{
			if (m_iterNow == m_bufferList.end() && m_newSize)
			{
				--m_iterNow;
				for (i = 0; i != m_newSize; ++i)
				{
					m_bufferList.emplace_back(std::make_shared<ReadBuffer>(m_ioPool->getIoNext()));
				}
				++m_iterNow;
			}
			if (m_iterNow != m_bufferList.end())
			{
				outBuffer = *m_iterNow ;
				++m_iterNow;
				//m_listMutex.unlock();
				//return m_tempReadBuffer;
			}
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
				//return m_tempReadBuffer;
			}
			//m_listMutex.unlock();
			//return nullptr;
		}
		m_listMutex.unlock();
		//return nullptr;
	}
}




void SocketPool::getBackElem(const std::shared_ptr<ReadBuffer> &buffer)
{
	//std::lock_guard<std::mutex>l1{ m_listMutex };
	m_listMutex.lock();
	if (buffer)
	{
		buffer->getHasRead() = 0;
		boost::system::error_code err{};
		{
			do
			{
				buffer->getErrCode() = {};
				buffer->getSock()->cancel(buffer->getErrCode());
				if (buffer->getErrCode())
				{
					//m_log->writeLog(__FILE__, __LINE__, buffer->getErrCode().message());
				}
			} while (buffer->getErrCode());
			do
			{
				buffer->getErrCode() = {};
				buffer->getSock()->shutdown(boost::asio::socket_base::shutdown_send, buffer->getErrCode());
				if (buffer->getErrCode())
				{
					if (buffer->getErrCode().message() == "Transport endpoint is not connected")
					{
						break;
					}
					//m_log->writeLog(__FILE__, __LINE__, buffer->getErrCode().message());
				}
			} while (buffer->getErrCode());
			do
			{
				buffer->getErrCode() = {};
				buffer->getSock()->close(buffer->getErrCode());
				if (buffer->getErrCode())
				{
					//m_log->writeLog(__FILE__, __LINE__, buffer->getErrCode().message());
				}
			} while (buffer->getErrCode());
		}
		{
			if (!m_bufferList.empty() && m_iterNow != m_bufferList.begin())
			{
				--m_iterNow;
				*m_iterNow = buffer;
				if (!m_startAccept)
				{
					m_startAccept = true;
					//l1.~lock_guard();
					m_listMutex.unlock();
					(*m_startFunction)();

					/*      池元素数量少的时候需要使用这个，避免外层锁还没切换好出现通知不了的情况，超过2000以上这里可以去掉了
					m_timer->expires_after(std::chrono::seconds(3));
					m_timer->async_wait([this](const boost::system::error_code &err)
					{
						if (err)
						{


						}
						else
						{
							(*m_startFunction)();
						}
					});  */
					m_listMutex.unlock();
					return;
				}
			}
		}
	}
	m_listMutex.unlock();
}



void SocketPool::setNotifyAccept()
{
	m_timer->expires_after(std::chrono::microseconds(1));
	m_timer->async_wait([this](const boost::system::error_code &err)
	{
		if (!err)
		{
			//std::lock_guard<std::mutex>m1{ m_listMutex };
			m_listMutex.lock();
			m_startAccept = false;
			m_listMutex.unlock();
		}
	});
}

