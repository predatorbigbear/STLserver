#pragma once

#include "publicHeader.h"
#include "mysql.h"
#include "mysqld_error.h"




// SQLWrite只负责执行例如插入  删除 update等动作的sql语句，立即返回插入消息队列是否成功




struct SQLWRITE
{
	SQLWRITE(std::shared_ptr<io_context> ioc, std::shared_ptr<std::function<void()>>unlockFun, const std::string &SQLHOST, const std::string &SQLUSER, const std::string &SQLPASSWORD,
		const std::string &SQLDB, const std::string &SQLPORT);

	bool insertSqlWrite(const std::string &sqlWrite);

	bool insertSqlWrite(const char *ch, const size_t len);

	~SQLWRITE();

	/*
	bool Mymysql_real_query(const string &source);
	bool MysqlResult(std::vector<std::vector<std::pair<std::string, std::string>>>& resultVec);
	const char* getSqlError();
	bool reconnect(const string &source);
	bool MYmysql_select_db(const char *DBname);
	*/


private:
	MYSQL *m_mysql;

	std::string m_host{};
	std::string m_user{};
	std::string m_passwd{};
	std::string m_db{};
	unsigned int m_port{};
	std::atomic<bool> m_hasConnect{ false };
	const char *m_unix_socket{};

	net_async_status m_status;
	std::unique_ptr<boost::asio::steady_timer> m_timer;

	std::shared_ptr<std::function<void()>>m_unlockFun;


	std::atomic<bool>m_queryStatus{ false };

	//执行语句
	SAFELIST<std::string>m_messageList;

	std::string m_sqlStr{};

	std::string m_nextSqlStr{};


private:

	void firstQuery();

	void query();

	void FreeConnect();

	void ConnectDatabase();

	void ConnectDatabaseLostConnection();

	void ConnectDatabaseLoop();

	void ConnectDatabaseLoopLostConnection();

	void checkList();

	void handleLostConnection();
};

