#pragma once


#include<memory>
#include<functional>




template<typename HTTPCLASS>
struct FIXEDTEMPLATESAFELIST
{
	FIXEDTEMPLATESAFELIST(std::shared_ptr<std::function<void()>> startcheckTime, 
		const int beginSize = 200) :
		m_startcheckTime(startcheckTime), m_beginSize(beginSize)
	{
		try
		{
			if (beginSize <= 0)
				throw std::runtime_error("beginSize invaild");
			if (!startcheckTime)
				throw std::runtime_error("startcheckTime is invaild");
		}
		catch (const std::exception &e)
		{
			cout << e.what() << "  , please restart server\n";
		}
	}


	bool ready()
	{
		try
		{
			if (!m_hasReady)
			{
				m_emptyListEnd = nullptr;
				m_valueListEnd = nullptr;

				m_valueList.reset(new HTTPCLASS[m_beginSize]);
				m_valueIter = m_valueList.get();

				m_emptyList.reset(new HTTPCLASS*[m_beginSize]);
				m_emptyIter = m_emptyList.get();


				for (i = 0; i != m_beginSize; ++i)
				{
					*m_valueIter = nullptr;
					*m_emptyIter++ = m_valueIter;
					++m_valueIter;
					m_valueListEnd = m_valueIter;
					m_emptyListEnd = m_emptyIter;
				}
				m_emptyIter = m_emptyList.get();
				m_hasReady = true;
				return true;
			}
		}
		catch (const std::exception &e)
		{
			return false;
		}
		return false;
	}


	
	bool insert(HTTPCLASS &buffer)
	{
		//std::lock_guard<std::mutex>l1{ m_listMutex };
		m_listMutex.lock();
		if (m_emptyList &&  m_emptyIter != m_emptyListEnd)
		{
			*(*m_emptyIter) = buffer;
			buffer->getListIter() = *m_emptyIter;
			++m_emptyIter;
			m_listMutex.unlock();
			return true;
		}
		return false;
	}


	
	bool pop(HTTPCLASS *iter)
	{
		//std::lock_guard<std::mutex>l1{ m_listMutex };
		m_listMutex.lock();
		if (m_emptyList &&  m_emptyIter != m_emptyList.get() && iter != m_valueListEnd)
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
		if (m_emptyList && m_emptyIter != m_emptyList.get())
		{
			for (m_tempIter = m_emptyList.get(); m_tempIter != m_emptyIter;)
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
	std::unique_ptr<HTTPCLASS[]> m_valueList{};                                  //  存储对象的单链表，记得用共享指针或指针

	std::unique_ptr<HTTPCLASS*[]> m_emptyList{};
	HTTPCLASS** m_emptyIter{};               //  存储单链表空位置的双链表
	HTTPCLASS** m_emptyListEnd{};
	HTTPCLASS** m_tempIter{};

	HTTPCLASS* m_valueIter{};
	HTTPCLASS* m_valueListEnd{};

	std::mutex m_listMutex;
	unsigned int m_beginSize{};

	bool m_hasReady{ false };
	int i{};
	std::shared_ptr<std::function<void()>>m_startcheckTime{};

};










