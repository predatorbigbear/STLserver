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
				m_checkList.reset(new HTTPCLASS[m_beginSize]);
				m_checkBegin = m_checkList.get();
				m_checkEnd = m_checkBegin;
				m_checkMax = m_checkBegin + m_beginSize;
	
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
		if (m_checkList)
		{
			//如果开始位置不是空间最开始位置
			if (m_checkBegin != m_checkList.get())
			{
				--m_checkBegin;
				*m_checkBegin = buffer;
				buffer->getListIter() = m_checkBegin;
			}
			else if (m_checkEnd != m_checkMax)   
			{
				//如果末尾位置不是空间最大值位置
				*m_checkEnd = buffer;
				buffer->getListIter() = m_checkEnd;
				++m_checkEnd;
			}
			else
			{
				m_listMutex.unlock();
				return false;
			}

			m_listMutex.unlock();
			return true;
		}
		m_listMutex.unlock();
		return false;
	}


	
	bool pop(HTTPCLASS *iter)
	{
		//std::lock_guard<std::mutex>l1{ m_listMutex };
		m_listMutex.lock();
		if (m_checkList && m_checkEnd != m_checkList.get())
		{
			//判断要pop出的指针位置在空间内什么位置
			//分别判断在最开始位置
			//最末尾位置
			//中间位置
			//位于中间位置时计算该位置距离前后的距离
			//进行智能copy
			if (iter == m_checkBegin)
				++m_checkBegin;
			else if (iter == (m_checkEnd - 1))
				--m_checkEnd;
			else
			{
				int len1{ std::distance(m_checkBegin,iter) }, len2{ std::distance(iter,m_checkEnd) };
				if (len1 < len2)
				{
					std::copy(m_checkBegin, iter, (m_checkBegin + 1));
					++m_checkBegin;
				}
				else
				{
					std::copy(iter + 1, m_checkEnd, iter);
					--m_checkEnd;
				}
			}

			if (m_checkBegin == m_checkEnd)
				m_checkBegin = m_checkEnd = m_checkList.get();
			

			m_listMutex.unlock();
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
		if (m_checkList && m_checkEnd != m_checkList.get())
		{
			for (m_tempIter = m_checkBegin; m_tempIter != m_checkEnd; ++m_tempIter)
			{
				if ((*m_tempIter)->checkTimeOut())
					(*m_tempIter)->clean();
			}
		}
		m_listMutex.unlock();
		(*m_startcheckTime)();
	}







private:

	std::unique_ptr<HTTPCLASS[]> m_checkList{};            //检查是否超时的空间
	HTTPCLASS* m_checkBegin{};                              //指向检查空间开始位置的指针
	HTTPCLASS* m_checkEnd{};                              //指向超时的空间末尾位置的指针
	HTTPCLASS* m_checkMax{};                                //指向超时的空间最大值的指针
	HTTPCLASS* m_tempIter{};                               //临时遍历检查超时的空间用的指针

	std::mutex m_listMutex;
	unsigned int m_beginSize{};

	bool m_hasReady{ false };
	int i{};
	std::shared_ptr<std::function<void()>>m_startcheckTime{};

};










