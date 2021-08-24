#include "multiSQLWriteSW.h"

MULTISQLWRITESW::MULTISQLWRITESW(std::shared_ptr<io_context> ioc, std::shared_ptr<std::function<void()>> unlockFun, const std::string & SQLHOST, 
	const std::string & SQLUSER, const std::string & SQLPASSWORD, const std::string & SQLDB, const std::string & SQLPORT, const unsigned int commandMaxSize ,std::shared_ptr<LOG> log)
	:m_host(SQLHOST), m_user(SQLUSER), m_passwd(SQLPASSWORD), m_db(SQLDB), m_unlockFun(unlockFun), m_commandMaxSize(commandMaxSize), m_log(log)
{
	try
	{
		if (SQLHOST.empty())
			throw std::runtime_error("SQLHOST is empty");
		if (SQLUSER.empty())
			throw std::runtime_error("SQLUSER is empty");
		if (SQLPASSWORD.empty())
			throw std::runtime_error("SQLPASSWORD is empty");
		if (SQLDB.empty())
			throw std::runtime_error("SQLDB is empty");
		if (!ioc)
			throw std::runtime_error("io_context is empty");
		if (SQLPORT.empty())
			throw std::runtime_error("SQLPORT is empty");
		if (!unlockFun)
			throw std::runtime_error("unlockFun is nullptr");
		if (!std::all_of(SQLPORT.begin(), SQLPORT.end(), std::bind(std::logical_and<bool>(), std::bind(std::greater_equal<char>(), std::placeholders::_1, '0'),
			std::bind(std::less_equal<char>(), std::placeholders::_1, '9'))))
			throw std::runtime_error("SQLPORT is not number");
		if (!log)
			throw std::runtime_error("log is nullptr");
		m_port = std::stoi(SQLPORT);
		if (m_port < 1 || m_port>65535)
			throw std::runtime_error("SQLPORT is invaild number");
		if (!commandMaxSize)
			throw std::runtime_error("commandMaxSize is invaild number");
		m_commandNowSize = 0;



		m_timer.reset(new steady_timer(*ioc));
		ConnectDatabase();
	}
	catch (const std::exception &e)
	{
		cout << e.what() << "  ,please restart server\n";
		throw;
	}
}



bool MULTISQLWRITESW::insertSQLRequest(std::shared_ptr<SQLWriteTypeSW> sqlRequest)
{
	if (!sqlRequest || std::get<0>(*sqlRequest).get().empty() || !std::get<1>(*sqlRequest) || std::get<1>(*sqlRequest) > m_commandMaxSize)
	{
		return false;
	}

	m_mutex.lock();
	if (!m_hasConnect)
	{
		m_mutex.unlock();
		return false;
	}
	if (!m_queryStatus)
	{
		m_queryStatus = true;
		m_mutex.unlock();

		//////////////////////////////////////////////////////////////////////


		std::vector<std::string_view> &sourceVec{ std::get<0>(*sqlRequest).get() };
		std::vector<std::string_view>::const_iterator souceBegin{ sourceVec.cbegin() }, souceEnd{ sourceVec.cend() };
		char *messageIter{};

		//������Ҫ���ܳ���
		int totalLen{ std::accumulate(souceBegin,souceEnd,0,[](auto &sum,const std::string_view &sw)
		{
			return sum += sw.size();
		}) };


		//��ͼ������ʵ�buffer����
		if (totalLen > m_messageBufferMaxSize)
		{
			try
			{
				m_messageBuffer.reset(new char[totalLen]);
				m_messageBufferMaxSize = totalLen;
			}
			catch (const std::exception &e)
			{
				m_messageBufferMaxSize = 0;
				m_mutex.lock();
				m_queryStatus = false;
				m_mutex.unlock();
				return false;
			}
		}

		m_messageBufferNowSize = totalLen;



		//copy���ɵ������ַ���buffer��

		souceBegin = sourceVec.cbegin();

		messageIter = m_messageBuffer.get();


		do
		{
			if (!souceBegin->empty())
			{
				std::copy(souceBegin->cbegin(), souceBegin->cend(), messageIter);

				messageIter += souceBegin->size();
			}

		} while (++souceBegin != souceEnd);
		

	
		m_sendMessage = m_messageBuffer.get(), m_sendLen = m_messageBufferNowSize;

		///////////////////////////////////////////////////////////
		m_receiveBufferNowSize = m_lastPrasePos = 0;

		m_waitMessageListBegin = m_waitMessageList.get();

		*m_waitMessageListBegin++ = sqlRequest;

		m_waitMessageListEnd = m_waitMessageListBegin;

		m_waitMessageListBegin = m_waitMessageList.get();

		m_jumpNode = false;

		m_commandTotalSize = std::get<1>(*sqlRequest);

		m_commandCurrentSize = 0;
		

		//���뷢�ͺ���

		query();
//////////////////////////////////////////////////////////////////////////////////////////////////////
		return true;
	}
	else
	{
		m_mutex.unlock();

		std::vector<std::string_view> &sourceVec{ std::get<0>(*sqlRequest).get() };
		std::vector<std::string_view>::iterator souceBegin{ sourceVec.begin() }, souceEnd{ sourceVec.end() };

		unsigned int bufferLen{ std::accumulate(souceBegin,souceEnd,0,[](auto &sum,auto const sw)
		{
			return sum += sw.size();
		}) };

		if (bufferLen)
		{
			char *buffer{ m_charMemoryPool.getMemory(bufferLen) };

			if (!buffer)
				return false;

			while (souceBegin != souceEnd)
			{
				if (!souceBegin->empty())
				{
					std::copy(souceBegin->cbegin(), souceBegin->cend(), buffer);
					std::string_view sw(buffer, souceBegin->size());
					souceBegin->swap(sw);
					buffer += souceBegin->size();
					++souceBegin;
				}
			}
		}

		m_messageList.getMutex().lock();

		if (!m_messageList.unsafeInsert(sqlRequest))
		{
			m_messageList.getMutex().unlock();
			return false;
		}

		m_messageList.getMutex().unlock();

		return true;

		
	}
}



void MULTISQLWRITESW::ConnectDatabase()
{
	m_mutex.lock();
	if (!m_hasConnect)
	{
		m_mutex.unlock();
		if (!(m_mysql = mysql_init(nullptr)))
		{
			m_timer->expires_after(std::chrono::seconds(1));
			m_timer->async_wait([this](const boost::system::error_code &err)
			{
				if (err)
				{
					//tryResetTimer();
				}
				else
				{
					ConnectDatabase();
				}
			});
			return;
		}

		ConnectDatabaseLoop();
	}
	else
		m_mutex.unlock();

}




void MULTISQLWRITESW::ConnectDatabaseLoop()
{
	m_status = mysql_real_connect_nonblocking(m_mysql, m_host.data(), m_user.data(), m_passwd.data(), m_db.data(), m_port, m_unix_socket,
		CLIENT_MULTI_STATEMENTS | CLIENT_MULTI_RESULTS | CLIENT_OPTIONAL_RESULTSET_METADATA);
	m_timer->expires_after(std::chrono::milliseconds(100));
	m_timer->async_wait([this](const boost::system::error_code &err)
	{
		if (err)
		{
			cout << "sql error,please restart server\n";
		}
		else
		{
			switch (m_status)
			{
			case NET_ASYNC_NOT_READY:
				ConnectDatabaseLoop();
				break;

			case NET_ASYNC_ERROR:
				if (!resetConnect)
					cout << "connect database error,please restart server\n";
				ConnectDatabaseLoop();
				break;

			case NET_ASYNC_COMPLETE:
				if (!resetConnect)
					cout << "connect multi mysqlWrite string_view success\n";
				m_mutex.lock();
				m_hasConnect = true;
				m_mutex.unlock();
				if (!resetConnect)
					(*m_unlockFun)();
				else
					resetConnect = false;
				break;

			default:
				if (!resetConnect)
					cout << "connect database error,please restart server\n";
				ConnectDatabaseLoop();
				break;
			}
		}
	});
}



void MULTISQLWRITESW::setConnectSuccess()
{
	m_mutex.lock();
	m_hasConnect = true;
	m_mutex.unlock();
}



void MULTISQLWRITESW::setConnectFail()
{
	m_mutex.lock();
	m_hasConnect = false;
	m_mutex.unlock();
}



// �ɹ��� ֮����m_messageList�Ƿ�Ϊ�գ����Խ���ƴ��
void MULTISQLWRITESW::query()
{
	m_status = mysql_real_query_nonblocking(m_mysql, m_sendMessage, m_sendLen);
	m_timer->expires_after(std::chrono::microseconds(100));
	m_timer->async_wait([this](const boost::system::error_code &err)
	{
		if (err)   //�ص���������
		{

		}
		else
		{
			switch (m_status)
			{
			case NET_ASYNC_NOT_READY:
				query();
				break;
			case NET_ASYNC_ERROR:              // �ص���������
				//������ȡ����Լ���������н��д������ش���Ȼ����գ�����   ���Ƚ���������λfalse 
				//����Ǵ���Ĳ�ѯ�ᱨ��
				//std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_NET_ASYNC_ERROR);
				//handleAsyncError();
				break;
			case NET_ASYNC_COMPLETE_NO_MORE_RESULTS:
				break;

				//�ɹ��� ��������
			case NET_ASYNC_COMPLETE:
				readyMessage();
				//store();
				break;
			default:
				//store();                       // store_result
				break;
			}
		}
	});

}




//����һ��whileѭ�����ж�
//��������ֱ�ӳ���ȡcommandSize��Ԫ�أ��������Ϊ�գ����˳������ô���״̬Ϊ��
//�����ȡ��Ԫ�أ������������Ҫ�Ŀռ�Ϊ����
//���Է���ռ䣬����ʧ����continue����
//�ɹ���copy��ϣ����뷢������׶�ѭ��
void MULTISQLWRITESW::readyMessage()
{
	//�Ƚ���Ҫ�õ��Ĺؼ�������λ���ٳ��������µ�������Ϣ
	std::shared_ptr<SQLWriteTypeSW> sqlRequest;
	std::vector<std::string_view>::const_iterator souceBegin, souceEnd;
	char *messageIter{};

	//���㱾������Ҫ�Ŀռ��С
	int totalLen{}, everyLen{}, index{}, divisor{ 10 }, temp{}, thisStrLen{};

	while (true)
	{
		m_waitMessageListBegin = m_waitMessageList.get();
		m_waitMessageListEnd = m_waitMessageList.get() + m_commandMaxSize;


		//��m_messageList�л�ȡԪ�ص�m_waitMessageList��
		m_messageList.getMutex().lock();
		std::forward_list<std::shared_ptr<SQLWriteTypeSW>> &flist{ m_messageList.listSelf() };
		if (flist.empty())
		{
			m_messageList.getMutex().unlock();
			m_mutex.lock();
			m_queryStatus = false;
			m_mutex.unlock();
			m_charMemoryPool.prepare();
			break;
		}



		do
		{
			*m_waitMessageListBegin++ = *flist.begin();
			flist.pop_front();
		} while (!flist.empty() && m_waitMessageListBegin != m_waitMessageListEnd);
		m_messageList.getMutex().unlock();


		//���Լ������������  �����ַ�������Ҫ�ܳ���,���ü��ǿ�����
		m_waitMessageListEnd = m_waitMessageListBegin;
		m_waitMessageListBegin = m_waitMessageList.get();
		totalLen = 0;

		do
		{
			sqlRequest = *m_waitMessageListBegin;

			std::vector<std::string_view> &sourceVec = std::get<0>(*sqlRequest).get();
			souceBegin = sourceVec.cbegin(), souceEnd = sourceVec.cend();
			

			totalLen+= std::accumulate(souceBegin, souceEnd, 0, [](auto &sum, const std::string_view &sw)
			{
				return sum += sw.size();
			}) + 1;
		} while (++m_waitMessageListBegin != m_waitMessageListEnd);



		//////////////////////////////////////////////////////////////////

		
		//�����ܳ�����ϣ����Խ��з���ռ�
		//����ʧ��ʱ��ǵñ����������ش���
		if (totalLen > m_messageBufferMaxSize)
		{
			try
			{
				m_messageBuffer.reset(new char[totalLen]);
				m_messageBufferMaxSize = totalLen;
			}
			catch (const std::exception &e)
			{
				m_messageBufferMaxSize = 0;

				m_waitMessageListBegin = m_waitMessageList.get();
				do
				{
					sqlRequest = *m_waitMessageListBegin;

				} while (++m_waitMessageListBegin != m_waitMessageListEnd);
				continue;
			}
		}

		m_messageBufferNowSize = totalLen;


		
		/////////�������������ַ���

		m_waitMessageListBegin = m_waitMessageList.get();
		messageIter = m_messageBuffer.get();
		do
		{
			sqlRequest = *m_waitMessageListBegin;

			std::vector<std::string_view> &sourceVec = std::get<0>(*sqlRequest).get();
			souceBegin = sourceVec.cbegin(), souceEnd = sourceVec.cend();



			do
			{
				if (!souceBegin->empty())
				{
					std::copy(souceBegin->cbegin(), souceBegin->cend(), messageIter);
					messageIter += souceBegin->size();
				}
			} while (++souceBegin != souceEnd);
			*messageIter++ = ';';


			//�������֮�����֪ͨ���գ����ټ���
		} while (++m_waitMessageListBegin != m_waitMessageListEnd);



		////   ׼������

		m_sendMessage = m_messageBuffer.get(), m_sendLen = m_messageBufferNowSize;

		///////////////////////////////////////////////////////////
		m_receiveBufferNowSize = m_lastPrasePos = 0;

		m_waitMessageListBegin = m_waitMessageList.get();

		sqlRequest = *m_waitMessageListBegin;

		m_jumpNode = false;

		m_commandTotalSize = std::get<1>(*sqlRequest);

		m_commandCurrentSize = 0;
		//////////////////////////////////////////////////////
		//���뷢�ͺ���

		query();

		break;
	}
}
