#pragma once



#include "publicHeader.h"




template<typename T>
struct TEMPLATESAFELIST
{
	TEMPLATESAFELIST(std::shared_ptr<io_context> ioc, std::shared_ptr<std::function<void()>>start , std::shared_ptr<std::function<void()>> startcheckTime ,  const int beginSize = 200,
		const int addSize = 50):
		m_addSize(addSize), m_start(start), m_startcheckTime(startcheckTime)
	{
		try
		{
			if (beginSize <= 0)
				throw std::runtime_error("beginSize invaild");
			if (addSize < 0)
				throw std::runtime_error("addSize invaild");
			if (!ioc)
				throw std::runtime_error("io_context is nullptr");
			if (!start)
				throw std::runtime_error("start is invaild");
			if (!startcheckTime)
				throw std::runtime_error("startcheckTime is invaild");
			m_timer.reset(new boost::asio::steady_timer(*ioc));
			if (!readyList(beginSize))
				throw std::runtime_error("readyList fail");
		}
		catch (const std::exception &e)
		{
			cout << e.what() << "  , please restart server\n";
		}
	}



	bool insert(T &buffer)
	{	
		//std::lock_guard<std::mutex>l1{ m_listMutex };
		m_listMutex.lock();
		if (m_emptyIter != m_emptyList.end())
		{
			*(*m_emptyIter) = buffer;
			buffer->getListIter() = *m_emptyIter;
			++m_emptyIter;
			m_listMutex.unlock();
			return true;
		}
		else
		{
			try
			{
				--m_emptyIter;
				m_valueIter = *m_emptyIter ;
				for (i = 0; i != m_addSize; ++i)
				{
					m_valueList.emplace_after(m_valueIter, nullptr);
					m_emptyList.emplace_back(++m_valueIter);
				}
				++m_emptyIter;
				*(*m_emptyIter) = buffer;
				buffer->getListIter() = *m_emptyIter;
				++m_emptyIter;
				m_listMutex.unlock();
				return true;
			}
			catch (const std::exception &e)
			{
				++m_emptyIter;
				if (m_emptyIter != m_emptyList.end())
				{
					*(*m_emptyIter) = buffer;
					buffer->getListIter() = *m_emptyIter;
					++m_emptyIter;
					m_listMutex.unlock();
					return true;
				}
				m_listMutex.unlock();
				return false;
			}
		}
	}



	bool pop(typename std::forward_list<T>::iterator &iter)
	{	
		//std::lock_guard<std::mutex>l1{ m_listMutex };
		m_listMutex.lock();
		if (m_emptyIter != m_emptyList.begin() && iter != m_valueList.end())
		{
			--m_emptyIter;
			*m_emptyIter = iter;
			if (std::distance(m_tempIter, m_emptyIter) > 1)
			{
				++m_tempIter;
				//l1.~lock_guard();
				m_listMutex.unlock();
				startCheckLoop();
			}
			else
			{
				m_listMutex.unlock();
				(*m_startcheckTime)();
			}
			return true;
		}
		else
		{
			m_listMutex.unlock();
			(*m_startcheckTime)();
		}
		return false;
	}



	void check()
	{
		//std::lock_guard<std::mutex>l1{ m_listMutex };
		m_listMutex.lock();
		if (m_emptyIter != m_emptyList.begin())
		{
			for (m_tempIter = m_emptyList.begin(); m_tempIter != m_emptyIter;)
			{
				if ((*(*m_tempIter))->checkTimeOut())
				{
					//l1.~lock_guard();
					m_listMutex.unlock();
					startClean();
					return;
				}
				++m_tempIter;
			}
			m_listMutex.unlock();
			(*m_startcheckTime)();
		}
		else
		{
			m_listMutex.unlock();
			(*m_startcheckTime)();
		}
	}



	void startCheckLoop()
	{
		//std::lock_guard<std::mutex>l1{ m_listMutex };
		m_listMutex.lock();
		for (; m_tempIter != m_emptyIter;)
		{
			if ((*(*m_tempIter))->checkTimeOut())
			{
				//l1.~lock_guard();
				m_listMutex.unlock();
				(*(*m_tempIter))->clean();
				break;
			}
			++m_tempIter;
		}
		m_listMutex.unlock();
		(*m_startcheckTime)();
	}



	void startClean()
	{
		(*(*m_tempIter))->clean();
	}





private:
	std::forward_list<T> m_valueList;                                  //  存储对象的单链表，记得用共享指针或指针
	typedef typename  std::forward_list<T>::iterator valueType;        
	std::list<valueType> m_emptyList;
	typename std::list<valueType>::iterator m_emptyIter;               //  存储单链表空位置的双链表
	typename std::list<valueType>::iterator m_tempIter;
	typename std::forward_list<T>::iterator m_valueIter;
	std::mutex m_listMutex;
	int m_addSize{};
	bool m_hasReady{ false };
	int i{};
	std::unique_ptr<boost::asio::steady_timer> m_timer{};
	std::shared_ptr<std::function<void()>> m_start{};
	std::shared_ptr<std::function<void()>>m_startcheckTime{};

private:
	bool readyList(const int beginSize)
	{
		try
		{
			if (!m_hasReady)
			{
				m_hasReady = true;

				m_valueList.emplace_after(m_valueList.before_begin(), nullptr);
				m_valueIter = m_valueList.begin() ;
				m_emptyList.emplace_back(m_valueIter);
				for (i = 1; i != beginSize; ++i)
				{
					m_valueList.emplace_after(m_valueIter, nullptr);
					m_emptyList.emplace_back(++m_valueIter);
				}
				m_emptyIter = m_emptyList.begin();
				m_timer->expires_after(std::chrono::seconds(5));
				m_timer->async_wait([this](const boost::system::error_code &err)
				{
					if (!err)
						(*m_start)();
				});
				return true;
			}
			return false;
		}
		catch (const std::exception &e)
		{
			return false;
		}
	}
};










