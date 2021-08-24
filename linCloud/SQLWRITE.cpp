#include "SQLWRITE.h"

SQLWRITE::SQLWRITE(std::shared_ptr<io_context> ioc, std::shared_ptr<std::function<void()>> unlockFun, const std::string & SQLHOST, const std::string & SQLUSER,
	const std::string & SQLPASSWORD, const std::string & SQLDB, const std::string & SQLPORT)
	:m_host(SQLHOST), m_user(SQLUSER), m_passwd(SQLPASSWORD), m_db(SQLDB), m_unlockFun(unlockFun)
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
		m_timer.reset(new steady_timer(*ioc));
		ConnectDatabase();
	}
	catch (const std::exception &e)
	{
		cout << e.what() << "  ,please restart server\n";
	}
}



bool SQLWRITE::insertSqlWrite(const std::string & sqlWrite)
{
	if (m_hasConnect.load())
	{
		if (sqlWrite.empty())
			return false;
		if (!m_queryStatus.load())
		{
			m_queryStatus.store(true);       //直接进入处理函数

			m_sqlStr.assign(sqlWrite);

			query();

			return true;
		}
		return m_messageList.insert(std::move(sqlWrite));
	}
	return false;
}



bool SQLWRITE::insertSqlWrite(const char * ch, const size_t len)
{
	if(!ch || !len)
		return false;
	if (!m_queryStatus.load())
	{
		m_queryStatus.store(true);       //直接进入处理函数

		m_sqlStr.assign(ch, len);

		firstQuery();

		return true;
	}
	return m_messageList.insert(std::string(ch, len));
}



SQLWRITE::~SQLWRITE()
{
	FreeConnect();
}



void SQLWRITE::firstQuery()
{
	m_status = mysql_real_query_nonblocking(m_mysql, &m_sqlStr[0], m_sqlStr.size());
	if (m_messageList.popTop(m_nextSqlStr))
	{

	}
	else
	{
		m_nextSqlStr.clear();
	}
	switch (m_status)
	{
	case NET_ASYNC_NOT_READY:
		query();
		break;
	case NET_ASYNC_ERROR:
		handleLostConnection();
		break;
	default:
		checkList();
		break;
	}
}



void SQLWRITE::query()
{
	m_status = mysql_real_query_nonblocking(m_mysql, &m_sqlStr[0], m_sqlStr.size());
	m_timer->expires_after(std::chrono::milliseconds(1));
	m_timer->async_wait([this](const boost::system::error_code &err)
	{
		if (err)   //记录下错误
		{

			
		}
		else
		{
			switch (m_status)
			{
			case NET_ASYNC_NOT_READY:
				query();
				break;
			case NET_ASYNC_ERROR:
				handleLostConnection();
				break;
			default:
				checkList();
				break;
			}
		}
	});
}



void SQLWRITE::FreeConnect()
{
	if (m_hasConnect.load())
	{
		mysql_close(m_mysql);
		m_hasConnect.store(false);
	}
}


void SQLWRITE::ConnectDatabase()
{
	if (!m_hasConnect.load())
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



void SQLWRITE::ConnectDatabaseLostConnection()
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
					ConnectDatabaseLostConnection();
				}
			});
			return;
		}

		ConnectDatabaseLoopLostConnection();
	}
}



void SQLWRITE::ConnectDatabaseLoop()
{
	m_status = mysql_real_connect_nonblocking(m_mysql, m_host.data(), m_user.data(), m_passwd.data(), m_db.data(), m_port, m_unix_socket, CLIENT_MULTI_STATEMENTS | CLIENT_MULTI_RESULTS);
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
				cout << "connect database error,please restart server\n";
				ConnectDatabaseLoop();
				break;

			case NET_ASYNC_COMPLETE:
				cout << "connect mysqlWrite success\n";
				m_hasConnect.store(true);
				(*m_unlockFun)();
				break;

			default:
				cout << "connect database error,please restart server\n";
				ConnectDatabaseLoop();
				break;
			}
		}
	});
}



void SQLWRITE::ConnectDatabaseLoopLostConnection()
{
	m_status = mysql_real_connect_nonblocking(m_mysql, m_host.data(), m_user.data(), m_passwd.data(), m_db.data(), m_port, m_unix_socket, CLIENT_MULTI_STATEMENTS | CLIENT_MULTI_RESULTS);
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
				ConnectDatabaseLoopLostConnection();
				break;

			case NET_ASYNC_ERROR:
				cout << "connect database error,please restart server\n";
				ConnectDatabaseLostConnection();
				break;

			case NET_ASYNC_COMPLETE:
				cout << "connect mysqlRead success\n";
				m_hasConnect.store(true);
				checkList();
				break;

			default:
				cout << "connect database error,please restart server\n";
				ConnectDatabaseLoopLostConnection();
				break;
			}
		}
	});
}



void SQLWRITE::checkList()
{
	if (m_hasConnect.load())
	{
		if (!m_nextSqlStr.empty())
		{
			m_sqlStr.swap(m_nextSqlStr);
			m_nextSqlStr.clear();
		}
		else
		{
			if (m_messageList.popTop(m_sqlStr))
				query();
			else
				m_queryStatus.store(false);
		}
	}
}



void SQLWRITE::handleLostConnection()
{
	FreeConnect();
	ConnectDatabaseLostConnection();
}
