#include "STLTimeWheel.h"

STLTimeWheel::STLTimeWheel(std::shared_ptr<boost::asio::io_context> ioc, const unsigned int checkSecond, const unsigned int wheelNum, const unsigned int everySecondFunctionNumber)
	: m_checkSecond(checkSecond), m_wheelNum(wheelNum), m_everySecondFunctionNumber(everySecondFunctionNumber)
{
	if (m_hasInit)
		return;
	m_hasInit = true;

	try
	{
		if (!ioc)
			throw std::runtime_error("ioc is nullptr");
		if (!checkSecond)
			throw std::runtime_error("checkSecond is invaild");
		if(!m_wheelNum)
			throw std::runtime_error("wheelSecond is invaild");
		if(!m_everySecondFunctionNumber)
			throw std::runtime_error("everySecondFunctionNumber is invaild");
		m_timer.reset(new boost::asio::steady_timer(*ioc));

		int totalFunctionNumber{ m_wheelNum * m_everySecondFunctionNumber };


		//初始化装载存储轮盘信息的数组

		m_WheelInformationPtr.reset(new WheelInformation[m_wheelNum]);

		m_WheelInfoBegin = m_WheelInformationPtr.get();

		m_WheelInfoEnd = m_WheelInformationPtr.get() + m_wheelNum;

		m_WheelInfoIter = m_WheelInfoBegin;


		while (m_WheelInfoIter != m_WheelInfoEnd)
		{
			m_WheelInfoIter->listAppendIter = m_WheelInfoIter->functionList.emplace_after(m_WheelInfoIter->listAppendIter, new std::function<void()>[everySecondFunctionNumber]);

			m_WheelInfoIter->listIter = m_WheelInfoIter->functionList.cbegin();

			m_WheelInfoIter->bufferIter = m_WheelInfoIter->listIter->get();

			++m_WheelInfoIter;
		}


		m_WheelInfoIter = m_WheelInfoBegin;

		std::cout << "STLTimeWhell init success\n";

		start();
	}
	catch (const std::exception &e)
	{
		std::cout << e.what() << '\n';
		throw;
	}
}



//每次对当前轮盘内的回调函数进行检查，当前指针不等于首指针则进行处理，然后复位，
//随后将轮盘指针向前+1
void STLTimeWheel::start()
{
	//插入函数永远不能插入当前轮盘内的回调函数格子，则这里处理过程可以不进行加锁处理
	std::function<void()> *begin{ }, *end{ }, *iter{ };

	struct WheelInformation &refWheelInfo{ *m_WheelInfoIter };

	std::forward_list<std::unique_ptr<std::function<void()>[]>>::const_iterator listNow{ refWheelInfo.listIter }, listBegin{ refWheelInfo.functionList.cbegin() };

	if (listNow == listBegin)
	{
		if (refWheelInfo.bufferIter != listNow->get())
		{
			begin = listNow->get(), end = refWheelInfo.bufferIter;

			do
			{
				(*begin)();
			} while (++begin != end);

			refWheelInfo.bufferIter = listNow->get();
		}
	}
	else
	{
		do
		{
			begin = listBegin->get(), end = listBegin->get() + m_everySecondFunctionNumber;

			do
			{
				(*begin)();
			} while (++begin != end);

		} while (++listBegin != listNow);

		if (listBegin != refWheelInfo.functionList.cend())
		{
			if (refWheelInfo.bufferIter != listBegin->get())
			{
				begin = listBegin->get(), end = refWheelInfo.bufferIter;

				do
				{
					(*begin)();
				} while (++begin != end);
			}
		}
		
		refWheelInfo.listIter = refWheelInfo.functionList.cbegin();

		refWheelInfo.bufferIter = refWheelInfo.listIter->get();
	}

	m_mutex.lock();

	if (++m_WheelInfoIter == m_WheelInfoEnd)
		m_WheelInfoIter = m_WheelInfoBegin;

	m_mutex.unlock();

	m_timer->expires_after(std::chrono::seconds(m_checkSecond));

	m_timer->async_wait([this](const boost::system::error_code &err)
	{
		if (!err)
			start();
	});
}



bool STLTimeWheel::insert(const std::function<void()>& callBack, const unsigned int turnNum)
{
	if (!callBack || !turnNum || turnNum >= m_wheelNum)
		return false;

	
	WheelInformation *thisWheelInfo{};

	m_mutex.lock();

	//计算跳转到适当的轮盘位置
	thisWheelInfo = m_WheelInfoIter + turnNum;

	if (std::distance(m_WheelInfoBegin, thisWheelInfo) >= m_wheelNum)
		thisWheelInfo = m_WheelInfoBegin + (thisWheelInfo - m_WheelInfoEnd);

	struct WheelInformation &refWheelInfo{ *thisWheelInfo };

	//尝试在当前轮盘格子内插入
	//如果当前格子内不能插入，
	//则尝试在当前轮盘内开辟新节点保存
	//如果失败，则从当前跳转到的轮盘位置到 m_WheelInfoIter之间寻找空位置插入，
	//如果还是没有，返回失败
	if (refWheelInfo.listIter != refWheelInfo.functionList.cend())
	{
		//如果function指针位置指向的不是当前节点内function指针位置的尾部
		if (refWheelInfo.bufferIter != (refWheelInfo.listIter->get() + m_everySecondFunctionNumber))
		{
			*(refWheelInfo.bufferIter)++ = callBack;
			m_mutex.unlock();
			return true;
		}
		else
		{
			++refWheelInfo.listIter;
			if (refWheelInfo.listIter != refWheelInfo.functionList.cend())
			{
				refWheelInfo.bufferIter = refWheelInfo.listIter->get();
				*(refWheelInfo.bufferIter)++ = callBack;
				m_mutex.unlock();
				return true;
			}
		}
	}

	//尝试进行开辟新节点，然后保存

	try
	{
		refWheelInfo.listAppendIter= refWheelInfo.functionList.emplace_after(refWheelInfo.listAppendIter, new std::function<void()>[m_everySecondFunctionNumber]);

		refWheelInfo.listIter = refWheelInfo.listAppendIter;

		refWheelInfo.bufferIter = refWheelInfo.listIter->get();

		*(refWheelInfo.bufferIter)++ = callBack;

		m_mutex.unlock();

		return true;
	}
	catch (const std::exception &e)
	{
		//分配内存失败，则尝试跳到下一个节点进行处理，直至跳到m_WheelInfoIter
		//thisWheelInfo可能在m_WheelInfoIter之后，需要判断尾部情况
		if (thisWheelInfo > m_WheelInfoIter)
		{
			while (++thisWheelInfo != m_WheelInfoEnd)
			{
				struct WheelInformation &everyWheelInfo{ *thisWheelInfo };

				if (everyWheelInfo.listIter != everyWheelInfo.functionList.cend())
				{
					//如果function指针位置指向的不是当前节点内function指针位置的尾部
					if (everyWheelInfo.bufferIter != (everyWheelInfo.listIter->get() + m_everySecondFunctionNumber))
					{
						*(everyWheelInfo.bufferIter)++ = callBack;
						m_mutex.unlock();
						return true;
					}
					else
					{
						++everyWheelInfo.listIter;
						if (everyWheelInfo.listIter != everyWheelInfo.functionList.cend())
						{
							everyWheelInfo.bufferIter = everyWheelInfo.listIter->get();
							*(everyWheelInfo.bufferIter)++ = callBack;
							m_mutex.unlock();
							return true;
						}
					}
				}
			}

			thisWheelInfo = m_WheelInfoBegin;

			//对m_WheelInfoBegin情况特殊处理
			struct WheelInformation &everyWheelInfo{ *thisWheelInfo };

			if (everyWheelInfo.listIter != everyWheelInfo.functionList.cend())
			{
				//如果function指针位置指向的不是当前节点内function指针位置的尾部
				if (everyWheelInfo.bufferIter != (everyWheelInfo.listIter->get() + m_everySecondFunctionNumber))
				{
					*(everyWheelInfo.bufferIter)++ = callBack;
					m_mutex.unlock();
					return true;
				}
				else
				{
					++everyWheelInfo.listIter;
					if (everyWheelInfo.listIter != everyWheelInfo.functionList.cend())
					{
						everyWheelInfo.bufferIter = everyWheelInfo.listIter->get();
						*(everyWheelInfo.bufferIter)++ = callBack;
						m_mutex.unlock();
						return true;
					}
				}
			}
		}

		while (++thisWheelInfo != m_WheelInfoIter)
		{
			struct WheelInformation &everyWheelInfo{ *thisWheelInfo };

			if (everyWheelInfo.listIter != everyWheelInfo.functionList.cend())
			{
				//如果function指针位置指向的不是当前节点内function指针位置的尾部
				if (everyWheelInfo.bufferIter != (everyWheelInfo.listIter->get() + m_everySecondFunctionNumber))
				{
					*(everyWheelInfo.bufferIter)++ = callBack;
					m_mutex.unlock();
					return true;
				}
				else
				{
					++everyWheelInfo.listIter;
					if (everyWheelInfo.listIter != everyWheelInfo.functionList.cend())
					{
						everyWheelInfo.bufferIter = everyWheelInfo.listIter->get();
						*(everyWheelInfo.bufferIter)++ = callBack;
						m_mutex.unlock();
						return true;
					}
				}
			}
		}

		m_mutex.unlock();
		return false;

	}

	return false;
}


