#include "multiSQLREADRW.h"

MULTISQLREADRW::MULTISQLREADRW(std::shared_ptr<io_context> ioc, std::shared_ptr<std::function<void()>> unlockFun, const std::string & SQLHOST, 
	const std::string & SQLUSER, const std::string & SQLPASSWORD, const std::string & SQLDB, const std::string & SQLPORT, const unsigned int commandMaxSize)
	:m_host(SQLHOST), m_user(SQLUSER), m_passwd(SQLPASSWORD), m_db(SQLDB), m_unlockFun(unlockFun), m_commandMaxSize(commandMaxSize)
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
		m_port = stoi(SQLPORT);
		if (m_port < 1 || m_port>65535)
			throw std::runtime_error("SQLPORT is invaild number");
		if (!commandMaxSize)
			throw std::runtime_error("commandMaxSize is invaild number");
		m_commandNowSize = 0;


		m_waitMessageList.reset(new std::shared_ptr<resultTypeRW>[m_commandMaxSize]);
		m_waitMessageListMaxSize = m_commandMaxSize;


		m_timer.reset(new steady_timer(*ioc));
		m_freeSqlResultFun.reset(new std::function<void(std::reference_wrapper<std::vector<MYSQL_RES*>>)>(std::bind(&MULTISQLREADRW::FreeResult, this, std::placeholders::_1)));
		ConnectDatabase();
	}
	catch (const std::exception &e)
	{
		cout << e.what() << "  ,please restart server\n";
	}
}



std::shared_ptr<std::function<void(std::reference_wrapper<std::vector<MYSQL_RES*>>)>> MULTISQLREADRW::sqlRelease()
{
	return m_freeSqlResultFun;
}




bool MULTISQLREADRW::insertSqlRequest(std::shared_ptr<resultTypeRW> sqlRequest)
{
	if(std::get<0>(*sqlRequest).get().empty() || 
		std::any_of(std::get<0>(*sqlRequest).get().cbegin(), std::get<0>(*sqlRequest).get().cend(), [](auto const sw)
	{
		return sw.empty();
	}) || !std::get<1>(*sqlRequest) || std::get<1>(*sqlRequest) > m_commandMaxSize)
		return false;


	if (!m_queryStatus.load())
	{
		m_queryStatus.store(true);

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
			m_queryStatus.store(false);
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

		//进入处理函数
		firstQuery();

		return true;
	}
	else
	{
		//如果正在执行状态
		//则尝试插入待插入队列之中

		m_messageList.getMutex().lock();
		try
		{
			m_messageList.unsafeInsert(sqlRequest);
		}
		catch (const std::exception &e)
		{
			m_messageList.getMutex().unlock();
			return false;
		}
		m_messageList.getMutex().unlock();

		return true;
	}

	return false;


	////////////////////////////////////////////////////////////////////////////////////////////

	//首先判断是否已经连接，
	//未连接直接判断失败，因为未连接下目前会进行清空操作
	//
	//判断执行状态，
	//如果没有执行，则尝试将该次请求拼凑起来，默认调用者中间带有;。已经处理好
	//如果在执行，则直接插入等待插入队列之中

	//如果没有在执行状态
	//首先统计所需要的字符串总长度，如果不够，进行分配，生成本次请求的字符串
	//然后将本次的请求尝试插入待获取结果队列里面

	//如果正在执行状态
	//则尝试插入待插入队列之中
}




void MULTISQLREADRW::FreeResult(std::reference_wrapper<std::vector<MYSQL_RES*>> vecwrapper)
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



MULTISQLREADRW::~MULTISQLREADRW()
{
	FreeConnect();
}



void MULTISQLREADRW::FreeConnect()
{
	if (m_hasConnect.load())
	{
		mysql_close(m_mysql);
		m_hasConnect.store(false);
	}
}



void MULTISQLREADRW::ConnectDatabase()
{
	if (!m_hasConnect)
	{
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
}



void MULTISQLREADRW::ConnectDatabaseLoop()
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
					cout << "connect multi mysqlRead success\n";
				m_hasConnect.store(true);
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




void MULTISQLREADRW::firstQuery()
{
	m_status = mysql_real_query_nonblocking(m_mysql, m_sendMessage, m_sendLen);
	m_timer->expires_after(std::chrono::microseconds(100));
	m_timer->async_wait([this](const boost::system::error_code &err)
	{
		if (err)   //回调函数处理
		{

		}
		else
		{
			switch (m_status)
			{
			case NET_ASYNC_NOT_READY:
				firstQuery();
				break;
			case NET_ASYNC_ERROR:              // 回调函数处理
				//将待获取结果以及待插入队列进行处理，返回错误，然后清空，重连   首先将已连接置位false 
				//如果是错误的查询会报错
				//std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_NET_ASYNC_ERROR);
				//handleLostConnection();
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



void MULTISQLREADRW::store()
{
	m_status = mysql_store_result_nonblocking(m_mysql, &m_result);
	m_timer->expires_after(std::chrono::microseconds(10));
	m_timer->async_wait([this](const boost::system::error_code &err)
	{
		if (err)   //回调函数处理
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
			case NET_ASYNC_ERROR:        // 回调函数处理
				//std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_NET_ASYNC_ERROR);
				//handleAsyncError();
				break;
			case NET_ASYNC_COMPLETE_NO_MORE_RESULTS:


				break;
			case NET_ASYNC_COMPLETE:
				if (!m_result)
				{
					//清理
					//std::get<5>(*m_sqlRequest)(true, ERRORMESSAGE::SQL_MYSQL_RES_NULL);
				}
				else
				{
					//首先检查行和列，存储起来
					//根据行列不断地获取每行数据存储，一旦失败，释放本次MYSQL_RES指针，判断本节点是否还有请求，有的话置位跳过
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
								//检查待请求队列中是否有消息存在
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
						//获取行数,列数
						unsigned int rowNum{ mysql_num_rows(m_result) }, fieldNum{ mysql_num_fields(m_result) };
						unsigned int rowCount{ -1 }, fieldCount{ -1 };

						std::shared_ptr<resultTypeRW> sqlRequest = *m_waitMessageListBegin;
						std::vector<std::string_view> &vec{ std::get<4>(*sqlRequest).get() };

						try
						{
							while (++rowCount != rowNum)
							{
								m_row = mysql_fetch_row(m_result);

								//信任mysql的函数，这里不判空                                                 
								m_lengths = mysql_fetch_lengths(m_result);

								for (fieldCount = 0; fieldCount != fieldNum; ++fieldCount)
								{
									if (m_row[fieldCount])
									{
										vec.emplace_back(std::string_view(m_row[fieldCount], m_lengths[fieldCount]));
									}
									else
									{
										vec.emplace_back(std::string_view(m_row[fieldCount], 0));
									}
								}
							}
							std::get<3>(*sqlRequest).get().emplace_back(rowNum);
							std::get<3>(*sqlRequest).get().emplace_back(fieldNum);
							std::get<2>(*sqlRequest).get().emplace_back(m_result);
							if (++m_currentCommandSize == m_thisCommandToalSize)
							{
								std::get<5>(*sqlRequest)(true, ERRORMESSAGE::OK);
								if (readyNextQuery())
								{
									next_result();
								}
								else
								{
									//检查待请求队列中是否有消息存在
									checkMakeMessage();
								}
							}
							else
							{
								next_result();
							}
						}
						catch (const std::exception &e)
						{
							mysql_free_result(m_result);
							std::get<5>(*sqlRequest)(false, ERRORMESSAGE::SQL_QUERY_ERROR);
							if (++m_currentCommandSize == m_thisCommandToalSize)
							{
								if (readyNextQuery())
								{
									next_result();
								}
								else
								{
									//检查待请求队列中是否有消息存在
									checkMakeMessage();
								}
							}
							else
							{
								m_jumpThisRes = true;
								next_result();
							}
						}
					}
				}
				break;
			default:

				break;
			}
		}
	});
}



void MULTISQLREADRW::readyQuery()
{
	//根据m_waitMessageListBegin获取每次的RES
	m_waitMessageListBegin = m_waitMessageList.get();
	m_waitMessageListEnd = m_waitMessageListBegin + m_waitMessageListNowSize;

	std::shared_ptr<resultTypeRW> sqlRequest = *m_waitMessageListBegin;
	std::get<2>(*sqlRequest).get().clear();
	std::get<3>(*sqlRequest).get().clear();
	std::get<4>(*sqlRequest).get().clear();


	m_thisCommandToalSize = std::get<1>(*sqlRequest);

	m_currentCommandSize = 0;

	m_jumpThisRes = false;
}



bool MULTISQLREADRW::readyNextQuery()
{
	if (++m_waitMessageListBegin != m_waitMessageListEnd)
	{
		std::shared_ptr<resultTypeRW> sqlRequest = *m_waitMessageListBegin;
		std::get<2>(*sqlRequest).get().clear();
		std::get<3>(*sqlRequest).get().clear();
		std::get<4>(*sqlRequest).get().clear();


		m_thisCommandToalSize = std::get<1>(*sqlRequest);

		m_currentCommandSize = 0;

		m_jumpThisRes = false;

		return true;
	}
	return false;
}


void MULTISQLREADRW::next_result()
{
	m_status = mysql_next_result_nonblocking(m_mysql);
	m_timer->expires_after(std::chrono::microseconds(10));
	m_timer->async_wait([this](const boost::system::error_code &err)
	{
		if (err)   //回调函数处理
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
			case NET_ASYNC_ERROR:        // 回调函数处理
				//std::get<5>(*m_sqlRequest)(false, ERRORMESSAGE::SQL_NET_ASYNC_ERROR);

				//handleAsyncError();
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




void MULTISQLREADRW::makeMessage()
{
	//简单实现：先遍历一次获取需要的空间大小一次分配，分配错误的情况下全部解除
	//后续实现：在插入时计算好
	//          或使用容器


	int index{ -1 };
	unsigned int resultNum{};
	std::forward_list<std::shared_ptr<resultTypeRW>>::const_iterator m_messageBegin, m_messageEnd, m_messageIter;
	std::shared_ptr<resultTypeRW> result;

	std::shared_ptr<resultTypeRW> *waitBegin, *waitEnd;
	unsigned int totalLen{}, everyResult{};

	const char **PointerBegin{}, **pointEnd{}, **pointer{};
	char *messageIter{ };

	m_messageBufferNowSize = 0;
	bool openLock{ false };

	while (true)
	{
		totalLen  = m_commandNowSize = m_waitMessageListNowSize = 0;

		//一般情况下，并不会频繁遭遇此处的while循环情况，只有在内存缺乏的情况下才会进入第二次以上的while循环
		m_messageList.getMutex().lock();
		std::forward_list<std::shared_ptr<resultTypeRW>> &flist{ m_messageList.listSelf() };
		if (flist.empty())
		{
			m_messageList.getMutex().unlock();
			m_queryStatus.store(false);
			break;
		}

		index = -1;
		waitBegin = m_waitMessageList.get();
		while (!flist.empty())
		{
			result = *flist.cbegin();

			//请求命令数总数不要超过最大限定值
			everyResult = std::get<1>(*result);;
			if (m_commandNowSize + everyResult > m_commandMaxSize)
				break;

			m_commandNowSize += everyResult;

			//计算需要的字符空间大小
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
		//如果至少有一个则尝试分配空间，反之开始新的检查
		
		/*
		
		if (m_waitMessageListNowSize)
		{
			if (m_messageBufferMaxSize < totalLen)
			{
				try
				{
					m_messageBuffer.reset(new char[totalLen]);
					m_messageBufferMaxSize = totalLen;
				}
				catch (const std::exception &e)
				{
					continue;
				}
			}

			waitBegin = m_waitMessageList.get(), waitEnd = m_waitMessageList.get() + m_waitMessageListNowSize;

			index = -1;
			messageIter = m_messageBuffer.get();
			do
			{
				result = *waitBegin;

				pointer = std::get<0>(*result);

				pointerNum = std::get<1>(*result);

				PointerBegin = pointer, pointEnd = pointer + pointerNum;

				if (++index)
				{
					*messageIter++ = ';';
				}
				while (PointerBegin != pointEnd)
				{
					std::copy(*PointerBegin, *(PointerBegin + 1), messageIter);
					messageIter += *(PointerBegin + 1) - *PointerBegin;
					PointerBegin += 2;
				}
			} while (++waitBegin != waitEnd);

			m_messageBufferNowSize = totalLen;

			m_sendMessage = m_messageBuffer.get(), m_sendLen = m_messageBufferNowSize;

			firstQuery();

			break;
		}
		else
		{
			continue;
		}

		*/
	}
}




void MULTISQLREADRW::readyMakeMessage()
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



void MULTISQLREADRW::checkMakeMessage()
{
	readyMakeMessage();


}



