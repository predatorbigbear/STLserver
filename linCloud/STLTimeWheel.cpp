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


		//��ʼ��װ�ش洢������Ϣ������

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



//ÿ�ζԵ�ǰ�����ڵĻص��������м�飬��ǰָ�벻������ָ������д���Ȼ��λ��
//�������ָ����ǰ+1
void STLTimeWheel::start()
{
	//���뺯����Զ���ܲ��뵱ǰ�����ڵĻص��������ӣ������ﴦ����̿��Բ����м�������
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

	//������ת���ʵ�������λ��
	thisWheelInfo = m_WheelInfoIter + turnNum;

	if (std::distance(m_WheelInfoBegin, thisWheelInfo) >= m_wheelNum)
		thisWheelInfo = m_WheelInfoBegin + (thisWheelInfo - m_WheelInfoEnd);

	struct WheelInformation &refWheelInfo{ *thisWheelInfo };

	//�����ڵ�ǰ���̸����ڲ���
	//�����ǰ�����ڲ��ܲ��룬
	//�����ڵ�ǰ�����ڿ����½ڵ㱣��
	//���ʧ�ܣ���ӵ�ǰ��ת��������λ�õ� m_WheelInfoIter֮��Ѱ�ҿ�λ�ò��룬
	//�������û�У�����ʧ��
	if (refWheelInfo.listIter != refWheelInfo.functionList.cend())
	{
		//���functionָ��λ��ָ��Ĳ��ǵ�ǰ�ڵ���functionָ��λ�õ�β��
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

	//���Խ��п����½ڵ㣬Ȼ�󱣴�

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
		//�����ڴ�ʧ�ܣ�����������һ���ڵ���д���ֱ������m_WheelInfoIter
		//thisWheelInfo������m_WheelInfoIter֮����Ҫ�ж�β�����
		if (thisWheelInfo > m_WheelInfoIter)
		{
			while (++thisWheelInfo != m_WheelInfoEnd)
			{
				struct WheelInformation &everyWheelInfo{ *thisWheelInfo };

				if (everyWheelInfo.listIter != everyWheelInfo.functionList.cend())
				{
					//���functionָ��λ��ָ��Ĳ��ǵ�ǰ�ڵ���functionָ��λ�õ�β��
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

			//��m_WheelInfoBegin������⴦��
			struct WheelInformation &everyWheelInfo{ *thisWheelInfo };

			if (everyWheelInfo.listIter != everyWheelInfo.functionList.cend())
			{
				//���functionָ��λ��ָ��Ĳ��ǵ�ǰ�ڵ���functionָ��λ�õ�β��
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
				//���functionָ��λ��ָ��Ĳ��ǵ�ǰ�ڵ���functionָ��λ�õ�β��
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


