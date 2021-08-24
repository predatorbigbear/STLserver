#pragma once

#include "publicHeader.h"
#include "mysql.h"
#include "mysqld_error.h"




//  插入一个处理函数


//  加锁投递进消息队列，立马返回投递结果    投递之后立即触发处理检查函数，判断处理标志位


//  设置一个处理标志位，每次sql进入读取处理时置为true，处理完成后置为false 


//  投递一个tuple装载数据，函数状态应该为投递失败成功与否   tuple里面内容应该为char* 执行体，长度，处理函数（char**装载返回结果，以达到极致性能）


//  处理完本次消息之后调用回调函数清理本次sql查询结果，然后加锁弹出头部，判断消息队列是否为空，如果为空则将处理标志位置为false，否则进入下一次处理




struct SQLREAD
{
	SQLREAD(std::shared_ptr<io_context> ioc , std::shared_ptr<std::function<void()>>unlockFun , const std::string &SQLHOST, const std::string &SQLUSER, 
		const std::string &SQLPASSWORD, const std::string &SQLDB, const std::string &SQLPORT);
	
	bool insertSqlRequest(std::shared_ptr<std::tuple<const char*, unsigned int, const char **, unsigned int, unsigned int, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>> sqlRequest);

	std::shared_ptr<std::function<void()>> getSqlUnlock();
	
	~SQLREAD();

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



	std::unique_ptr<const char*[]>m_sqlResult{};        //存储sql结果

	unsigned int m_sqlResultSize{ 0 };           //存储sql结果buffer大小



	std::atomic<bool>m_queryStatus{false};

	//query语句指针首位  长度  返回sql结果指针   结果行数  结果列数   回调处理函数 结果数量
	SAFELIST<std::shared_ptr<std::tuple<const char*, unsigned int, const char **, unsigned int, unsigned int, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>>>m_messageList;
	
	std::shared_ptr<std::tuple<const char*, unsigned int, const char **, unsigned int, unsigned int, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>> m_sqlRequest{};

	std::shared_ptr<std::tuple<const char*, unsigned int, const char **, unsigned int, unsigned int, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>> m_nextSqlRequest{};

	std::shared_ptr<std::function<void()>>m_freeSqlResultFun;

	
	MYSQL_FIELD *fd;    //字段列数组
	MYSQL_RES *m_result{nullptr};    //行的一个查询结果集
	MYSQL_ROW m_row;   //数据行的列
	
	unsigned int m_rowNum{};
	unsigned int m_rowCount{};
	unsigned int m_fieldNum{};
	const char **m_data{};
	
	unsigned int m_temp{};


private:

	void firstQuery();

	void query();

	void store();

	void fetch_row();

	void freeResult();

	void checkRowField();


	void FreeConnect();

	void ConnectDatabase();

	void ConnectDatabaseLostConnection();

	void ConnectDatabaseLoop();

	void ConnectDatabaseLoopLostConnection();

	void checkList();


	void handleLostConnection();

};