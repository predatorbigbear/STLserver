#include "multiSQLREADSW.h"

MULTISQLREADSW::MULTISQLREADSW(std::shared_ptr<io_context> ioc, std::shared_ptr<std::function<void()>> unlockFun, const std::string & SQLHOST, 
	const std::string & SQLUSER, const std::string & SQLPASSWORD, const std::string & SQLDB, const std::string & SQLPORT, const unsigned int commandMaxSize, 
	std::shared_ptr<LOG> log)
	:m_host(SQLHOST), m_user(SQLUSER), m_passwd(SQLPASSWORD), m_db(SQLDB), m_unlockFun(unlockFun), m_commandMaxSize(commandMaxSize),m_log(log)
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
		if (!log)
			throw std::runtime_error("log is nullptr");
		if (!std::all_of(SQLPORT.begin(), SQLPORT.end(), std::bind(std::logical_and<bool>(), std::bind(std::greater_equal<char>(), std::placeholders::_1, '0'),
			std::bind(std::less_equal<char>(), std::placeholders::_1, '9'))))
			throw std::runtime_error("SQLPORT is not number");
		m_port = std::stoi(SQLPORT);
		if (m_port < 1 || m_port>65535)
			throw std::runtime_error("SQLPORT is invaild number");
		if (!commandMaxSize)
			throw std::runtime_error("commandMaxSize is invaild number");
		m_commandNowSize = 0;


		m_waitMessageList.reset(new std::shared_ptr<resultTypeSW>[m_commandMaxSize]);
		m_waitMessageListMaxSize = m_commandMaxSize;


		m_timer.reset(new steady_timer(*ioc));
		m_freeSqlResultFun.reset(new std::function<void(std::reference_wrapper<std::vector<MYSQL_RES*>>)>(std::bind(&MULTISQLREADSW::FreeResult, this, std::placeholders::_1)));


		if (mysql_library_init(0, NULL, NULL)) //�������̶߳�MYSQL C API����֮ǰ����
		{
			std::cout << "mysql_library_init error\n";
			return;
		}
		

		if (!(m_mysql = mysql_init(m_mysql)))
		{
			std::cout << "mysql init error\n";
			return;
		}


		ConnectDatabase();
	}
	catch (const std::exception &e)
	{
		std::cout << e.what() << "  ,please restart server\n";
		throw;
	}
}



std::shared_ptr<std::function<void(std::reference_wrapper<std::vector<MYSQL_RES*>>)>> MULTISQLREADSW::sqlRelease()
{
	return m_freeSqlResultFun;
}




bool MULTISQLREADSW::insertSqlRequest(std::shared_ptr<resultTypeSW> sqlRequest)
{
	if(std::get<0>(*sqlRequest).get().empty() || 
		std::any_of(std::get<0>(*sqlRequest).get().cbegin(), std::get<0>(*sqlRequest).get().cend(), [](auto const sw)
	{
		return sw.empty();
	}) || !std::get<1>(*sqlRequest) || std::get<1>(*sqlRequest) > m_commandMaxSize)
		return false;

	m_mutex.lock();
	if (!m_hasConnect)
	{
		m_mutex.unlock();
		return false;
	}
	if (!m_queryStatus)
	{
		m_queryStatus=true;
		m_mutex.unlock();

		m_sendLen = std::accumulate(std::get<0>(*sqlRequest).get().cbegin(),std::get<0>(*sqlRequest).get().cend(),0,[](auto &sum,auto const sw)
		{
			return sum += sw.size();
		});

		try
		{
			if (m_messageBufferMaxSize < m_sendLen)
			{
				m_messageBuffer.reset(new char[m_sendLen]);
				m_messageBufferMaxSize = m_sendLen;
			}
		}
		catch (const std::exception &e)
		{
			m_mutex.lock();
			m_queryStatus=false;
			m_mutex.unlock();
			return false;
		}
		

		char *buffer{ m_messageBuffer.get() };
		for (auto sw : std::get<0>(*sqlRequest).get())
		{
			std::copy(sw.cbegin(), sw.cend(), buffer);
			buffer += sw.size();
		}

		m_messageBufferNowSize = m_sendLen;
		m_waitMessageListNowSize = 1;
		*(m_waitMessageList.get()) = sqlRequest;
		m_commandNowSize = std::get<1>(*sqlRequest);
		m_sendMessage = m_messageBuffer.get();

		
		readyQuery();

		//���봦����
		firstQuery();

		return true;
	}
	else
	{
		m_mutex.unlock();
		//�������ִ��״̬
		//���Բ�����������֮��

		m_messageList.getMutex().lock();
		
		if (!m_messageList.unsafeInsert(sqlRequest))
		{
			m_messageList.getMutex().unlock();
			return false;
		}
		m_messageList.getMutex().unlock();

		return true;
	}

	return false;


	////////////////////////////////////////////////////////////////////////////////////////////

	//�����ж��Ƿ��Ѿ����ӣ�
	//δ����ֱ���ж�ʧ�ܣ���Ϊδ������Ŀǰ�������ղ���
	//
	//�ж�ִ��״̬��
	//���û��ִ�У����Խ��ô�����ƴ��������Ĭ�ϵ������м����;���Ѿ������
	//�����ִ�У���ֱ�Ӳ���ȴ��������֮��

	//���û����ִ��״̬
	//����ͳ������Ҫ���ַ����ܳ��ȣ�������������з��䣬���ɱ���������ַ���
	//Ȼ�󽫱��ε������Բ������ȡ�����������

	//�������ִ��״̬
	//���Բ�����������֮��
}




void MULTISQLREADSW::FreeResult(std::reference_wrapper<std::vector<MYSQL_RES*>> vecwrapper)
{
	if (vecwrapper.get().empty())
		return;

	for (auto sqlPtr : vecwrapper.get())
	{
		if(sqlPtr)
			mysql_free_result(sqlPtr);
	}
	vecwrapper.get().clear();
}



MULTISQLREADSW::~MULTISQLREADSW()
{
	FreeConnect();
}



void MULTISQLREADSW::FreeConnect()
{
	m_mutex.lock();
	if (m_hasConnect)
	{
		mysql_close(m_mysql);
		m_hasConnect = false;
	}
	m_mutex.unlock();
}



void MULTISQLREADSW::ConnectDatabase()
{
	m_mutex.lock();
	if (!m_hasConnect)
	{
		m_mutex.unlock();

		ConnectDatabaseLoop();
	}
	else
		m_mutex.unlock();
}



void MULTISQLREADSW::ConnectDatabaseLoop()
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
					cout << "connect multi mysqlRead string_view success\n";
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




void MULTISQLREADSW::firstQuery()
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
				firstQuery();
				break;
			case NET_ASYNC_ERROR:              // �ص���������
				//������ȡ����Լ���������н��д������ش���Ȼ����գ�����   ���Ƚ���������λfalse 
				//����Ǵ���Ĳ�ѯ�ᱨ��
				//std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_NET_ASYNC_ERROR);
				handleAsyncError();
				break;
			case NET_ASYNC_COMPLETE_NO_MORE_RESULTS:
				break;
			case NET_ASYNC_COMPLETE:
				store();
				break;
			default:
				store();                       // store_result
				break;
			}
		}
	});
}



void MULTISQLREADSW::handleAsyncError()
{
	m_mutex.lock();
	m_hasConnect = false;
	m_mutex.unlock();

	m_messageList.getMutex().lock();
	if (!m_messageList.listSelf().empty())
	{
		for (auto const &request : m_messageList.listSelf())
			std::get<5>(*request)(false, ERRORMESSAGE::SQL_NET_ASYNC_ERROR);
		m_messageList.listSelf().clear();
	}
	m_messageList.getMutex().unlock();


	if (m_waitMessageListNowSize)
	{
		std::shared_ptr<resultTypeSW> *begin{ m_waitMessageList.get() }, *end{ m_waitMessageList.get() + m_waitMessageListNowSize };
		do
		{
			std::get<5>(**begin)(false, ERRORMESSAGE::SQL_NET_ASYNC_ERROR);
		} while (++begin != end);
		m_waitMessageListNowSize = 0;
	}

	resetConnect = true;

	m_mutex.lock();
	m_queryStatus = false;
	m_mutex.unlock();

	ConnectDatabase();
}



void MULTISQLREADSW::store()
{
	m_status = mysql_store_result_nonblocking(m_mysql, &m_result);
	m_timer->expires_after(std::chrono::microseconds(10));
	m_timer->async_wait([this](const boost::system::error_code &err)
	{
		if (err)   //�ص���������
		{
			//std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_QUERY_ERROR);
		}
		else
		{
			switch (m_status)
			{
			case NET_ASYNC_NOT_READY:
				store();
				break;
			case NET_ASYNC_ERROR:        // �ص���������
				//std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_NET_ASYNC_ERROR);
				handleAsyncError();
				break;
			case NET_ASYNC_COMPLETE_NO_MORE_RESULTS:


				break;
			case NET_ASYNC_COMPLETE:
				if (!m_result)
				{

	                //����
					//std::get<5>(*m_sqlRequest)(true, ERRORMESSAGE::SQL_MYSQL_RES_NULL);
				}
				else
				{
					//���ȼ���к��У��洢����
					//�������в��ϵػ�ȡÿ�����ݴ洢��һ��ʧ�ܣ��ͷű���MYSQL_RESָ�룬�жϱ��ڵ��Ƿ��������еĻ���λ����
					if (m_jumpThisRes)
					{
						mysql_free_result(m_result);
						if (++m_currentCommandSize == m_thisCommandToalSize)
						{
							m_jumpThisRes = false;
							if (readyNextQuery())
							{
								next_result();
							}
							else
							{
								//��������������Ƿ�����Ϣ����
								checkMakeMessage();
							}
						}
						else
						{
							next_result();
						}
					}
					else
					{
						//��ȡ����,����
						m_rowNum = mysql_num_rows(m_result), m_fieldNum = mysql_num_fields(m_result);
						m_rowCount = m_fieldCount = -1;


						m_sqlRequest = *m_waitMessageListBegin;
						try
						{
							std::vector<unsigned int> &rowFieldVec = std::get<3>(*m_sqlRequest);
							rowFieldVec.emplace_back(m_rowNum);
							rowFieldVec.emplace_back(m_fieldNum);
						}
						catch (const std::exception &e)
						{
							handleSqlMemoryError();
							return;
						}

						fetch_row();
					}
				}
				break;
			default:

				break;
			}
		}
	});
}



void MULTISQLREADSW::readyQuery()
{
	//����m_waitMessageListBegin��ȡÿ�ε�RES
	m_waitMessageListBegin = m_waitMessageList.get();
	m_waitMessageListEnd = m_waitMessageListBegin + m_waitMessageListNowSize;

	freeResult(m_waitMessageListBegin, m_waitMessageListEnd);

	m_sqlRequest = *m_waitMessageListBegin;

	m_thisCommandToalSize = std::get<1>(*m_sqlRequest);

	m_currentCommandSize = 0;

	m_jumpThisRes = false;
}



bool MULTISQLREADSW::readyNextQuery()
{
	if (++m_waitMessageListBegin != m_waitMessageListEnd)
	{
		m_sqlRequest = *m_waitMessageListBegin;
		
		m_thisCommandToalSize = std::get<1>(*m_sqlRequest);

		m_currentCommandSize = 0;

		m_jumpThisRes = false;

		return true;
	}
	return false;
}



void MULTISQLREADSW::next_result()
{
	m_status = mysql_next_result_nonblocking(m_mysql);
	m_timer->expires_after(std::chrono::microseconds(10));
	m_timer->async_wait([this](const boost::system::error_code &err)
	{
		if (err)   //�ص���������
		{
			//std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_QUERY_ERROR);
		}
		else
		{
			switch (m_status)
			{
			case NET_ASYNC_NOT_READY:
				next_result();
				break;
			case NET_ASYNC_ERROR:        // �ص���������
				//std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_NET_ASYNC_ERROR);

				handleAsyncError();
				break;
			case NET_ASYNC_COMPLETE_NO_MORE_RESULTS:

				break;
			case NET_ASYNC_COMPLETE:
				store();
				break;
			default:

				break;
			}
		}
	});

}




void MULTISQLREADSW::makeMessage()
{
	//��ʵ�֣��ȱ���һ�λ�ȡ��Ҫ�Ŀռ��Сһ�η��䣬�������������ȫ�����
	//����ʵ�֣��ڲ���ʱ�����
	//          ��ʹ������
	int index{ -1 };
	unsigned int resultNum{};
	// messageBufferʹ��
	std::forward_list<std::shared_ptr<resultTypeSW>>::const_iterator messageBegin{}, messageEnd{}, messageIter{};
	std::shared_ptr<resultTypeSW> result{};

	//waitMessage����ʹ��
	std::shared_ptr<resultTypeSW> *waitBegin{}, *waitEnd{}, *waitIter{};
	//ͳ���ַ����ܳ��ȣ�ÿ���ڵ����ݵ��������
	unsigned int totalLen{}, everyResult{};

	char *bufferIter{};


	while (true)
	{
		waitBegin = nullptr;
		totalLen  = m_commandNowSize = m_waitMessageListNowSize = 0;

		//һ������£�������Ƶ�������˴���whileѭ�������ֻ�����ڴ�ȱ��������²Ż����ڶ������ϵ�whileѭ��
		m_messageList.getMutex().lock();
		std::forward_list<std::shared_ptr<resultTypeSW>> &flist{ m_messageList.listSelf() };
		if (flist.empty())
		{
			m_messageList.getMutex().unlock();
			m_mutex.lock();
			m_queryStatus=false;
			m_mutex.unlock();
			break;
		}

		index = -1;
		waitBegin = m_waitMessageList.get();
		while (!flist.empty())
		{
			result = *flist.cbegin();

			//����������������Ҫ��������޶�ֵ
			everyResult = std::get<1>(*result);;
			if (m_commandNowSize + everyResult > m_commandMaxSize)
				break;

			m_commandNowSize += everyResult;

			//������Ҫ���ַ��ռ��С
			for (auto const sw : std::get<0>(*result).get())
			{
				if (++index)
				{
					++totalLen;
				}
				totalLen += sw.size();
			}

			*waitBegin++ = result;
			flist.pop_front();
		}
		m_messageList.getMutex().unlock();

		m_waitMessageListNowSize = waitBegin - m_waitMessageList.get();

		////////////////////////////////////////////////////////////////////////////////////////////////////////
		//���������һ�����Է���ռ䣬��֮��ʼ�µļ��
		
		
		//���������һ����Ϣ�ڵ�������
		if (m_waitMessageListNowSize)
		{
			//����ʧ��ʱ�����Ѿ��洢�Ľڵ㷵�ش�������
			if (m_messageBufferMaxSize < totalLen)
			{
				try
				{
					m_messageBuffer.reset(new char[totalLen]);
					m_messageBufferMaxSize = totalLen;
				}
				catch (const std::exception &e)
				{
					m_messageBufferMaxSize = 0;
					waitIter = m_waitMessageList.get();
					do
					{
						std::get<5>(**waitIter)(false, ERRORMESSAGE::SQL_QUERY_ERROR);

					} while (++waitIter != waitBegin);
					continue;
				}
			}

			
		    //��װ��Ϣ��buffer����
			waitBegin = m_waitMessageList.get(), waitEnd = m_waitMessageList.get() + m_waitMessageListNowSize;

			index = -1;
			bufferIter = m_messageBuffer.get();
			do
			{
				result = *waitBegin;

				for (auto const sw : std::get<0>(*result).get())
				{
					if (++index)
					{
						*bufferIter++ = ';';
					}
					std::copy(sw.cbegin(), sw.cend(), bufferIter);
					bufferIter += sw.size();
				}
			} while (++waitBegin != waitEnd);


			m_messageBufferNowSize = totalLen;

			readyQuery();

			m_sendMessage = m_messageBuffer.get(), m_sendLen = m_messageBufferNowSize;

			firstQuery();

			break;
		}
		else
		{
			continue;
		}		
	}
}




void MULTISQLREADSW::readyMakeMessage()
{
	m_messageBufferNowSize = 0;

	m_sendMessage = nullptr;

	m_sendLen = 0;

	m_jumpThisRes = false;

	m_commandNowSize = 0;

	m_thisCommandToalSize = 0;

	m_currentCommandSize = 0;

	m_waitMessageListNowSize = 0;

	m_waitMessageListBegin = nullptr;

	m_waitMessageListEnd = nullptr;

	m_sqlRequest = nullptr;
}



void MULTISQLREADSW::checkMakeMessage()
{
	readyMakeMessage();

	makeMessage();
}



void MULTISQLREADSW::fetch_row()
{
	m_status = mysql_fetch_row_nonblocking(m_result,&m_row);
	m_timer->expires_after(std::chrono::microseconds(10));
	m_timer->async_wait([this](const boost::system::error_code &err)
	{
		if (err)   //�ص���������
		{
			//std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_QUERY_ERROR);
		}
		else
		{
			m_sqlRequest = *m_waitMessageListBegin;
			std::vector<std::string_view> &vec{ std::get<4>(*m_sqlRequest).get() };
			switch (m_status)
			{
			case NET_ASYNC_NOT_READY:
				fetch_row();
				break;
			case NET_ASYNC_ERROR:        // �ص���������
				//std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_NET_ASYNC_ERROR);

				handleAsyncError();
				break;
			case NET_ASYNC_COMPLETE_NO_MORE_RESULTS:

				break;
			case NET_ASYNC_COMPLETE:
				++m_rowCount;


				try
				{
					if (m_row)
					{
						m_lengths = mysql_fetch_lengths(m_result);

						for (m_fieldCount = 0; m_fieldCount != m_fieldNum; ++m_fieldCount)
						{
							if (m_row[m_fieldCount])
							{
								vec.emplace_back(std::string_view(m_row[m_fieldCount], m_lengths[m_fieldCount]));
							}
							else
							{
								vec.emplace_back(std::string_view(m_row[m_fieldCount], 0));
							}
						}
					}
					else
					{

					}
				}
				catch (const std::exception &e)
				{
					handleSqlMemoryError();
					return;
				}



				if (m_rowCount != m_rowNum)
					fetch_row();
				else
				{

					try
					{
						std::get<2>(*m_sqlRequest).get().emplace_back(m_result);
					}
					catch (const std::exception &e)
					{
						handleSqlMemoryError();
						return;
					}


					if (++m_currentCommandSize == m_thisCommandToalSize)
					{
						std::get<5>(*m_sqlRequest)(true, ERRORMESSAGE::OK);
						if (readyNextQuery())
						{
							next_result();
						}
						else
						{
							//��������������Ƿ�����Ϣ����
							checkMakeMessage();
						}
					}
					else
					{
						next_result();
					}
				}
				break;
			default:

				break;
			}
		}
	});

}



void MULTISQLREADSW::freeResult(std::shared_ptr<resultTypeSW>* m_waitMessageListBegin, std::shared_ptr<resultTypeSW>* m_waitMessageListEnd)
{
	if (m_waitMessageListBegin && m_waitMessageListEnd && m_waitMessageListBegin != m_waitMessageListEnd && m_waitMessageListEnd > m_waitMessageListBegin)
	{
		do
		{
			m_sqlRequest = *m_waitMessageListBegin;
			std::vector<MYSQL_RES*> &vec = std::get<2>(*m_sqlRequest).get();

			if (!vec.empty())
			{
				for (auto res : vec)
				{
					mysql_free_result(res);
				}
				vec.clear();
			}
		} while (++m_waitMessageListBegin != m_waitMessageListEnd);
	}

}




//��ȡÿ��result�Ľ����������ʱ�����ȷ��ش�����Ϣ���д���Ȼ���鱾��ָ���Ƿ�����������Ҫȡ�أ�
//����У����ȡ��Ȼ���ͷţ�û�еĻ����Ի�ȡ��һ������ڵ��result
void MULTISQLREADSW::handleSqlMemoryError()
{
	m_sqlRequest = *m_waitMessageListBegin;
	mysql_free_result(m_result);
	std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_QUERY_ERROR);
	if (++m_currentCommandSize == m_thisCommandToalSize)
	{
		if (readyNextQuery())
		{
			next_result();
		}
		else
		{
			//��������������Ƿ�����Ϣ����
			checkMakeMessage();
		}
	}
	else
	{
		m_jumpThisRes = true;
		next_result();
	}
}



