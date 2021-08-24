#pragma once

#include "publicHeader.h"
#include "mysql.h"
#include "mysqld_error.h"




//  ����һ��������


//  ����Ͷ�ݽ���Ϣ���У�������Ͷ�ݽ��    Ͷ��֮���������������麯�����жϴ����־λ


//  ����һ�������־λ��ÿ��sql�����ȡ����ʱ��Ϊtrue��������ɺ���Ϊfalse 


//  Ͷ��һ��tupleװ�����ݣ�����״̬Ӧ��ΪͶ��ʧ�ܳɹ����   tuple��������Ӧ��Ϊchar* ִ���壬���ȣ���������char**װ�ط��ؽ�����Դﵽ�������ܣ�


//  �����걾����Ϣ֮����ûص�����������sql��ѯ�����Ȼ���������ͷ�����ж���Ϣ�����Ƿ�Ϊ�գ����Ϊ���򽫴����־λ��Ϊfalse�����������һ�δ���




struct SQL
{
	SQL(shared_ptr<io_context> ioc , shared_ptr<function<void()>>unlockFun , const string &SQLHOST, const string &SQLUSER, const string &SQLPASSWORD, const string &SQLDB, const string &SQLPORT);
	
	bool insertSqlRequest(std::shared_ptr<std::tuple<const char*, unsigned int, const char **, unsigned int, unsigned int, function<void(bool, enum ERRORMESSAGE)>, unsigned int>> sqlRequest);

	std::shared_ptr<function<void()>> getSqlUnlock();
	
	~SQL();

	/*
	bool Mymysql_real_query(const string &source);
	bool MysqlResult(std::vector<std::vector<std::pair<std::string, std::string>>>& resultVec);
	const char* getSqlError();
	bool reconnect(const string &source);
	bool MYmysql_select_db(const char *DBname);
	*/

	
private:
	MYSQL *m_mysql;

	string m_host{};
	string m_user{};
	string m_passwd{};
	string m_db{};
	unsigned int m_port{};
	bool m_hasConnect{ false };
	const char *m_unix_socket{};

	net_async_status m_status;
	unique_ptr<boost::asio::steady_timer> m_timer;

	shared_ptr<function<void()>>m_unlockFun;


	atomic<bool>m_queryStatus{false};

	//query���ָ����λ  ����  ����sql���ָ��   �������  �������   �ص������� �������
	SAFELIST<std::shared_ptr<std::tuple<const char*, unsigned int, const char **, unsigned int, unsigned int, function<void(bool, enum ERRORMESSAGE)>, unsigned int>>>m_messageList;
	
	std::shared_ptr<std::tuple<const char*, unsigned int, const char **, unsigned int, unsigned int, function<void(bool, enum ERRORMESSAGE)>, unsigned int>> m_sqlRequest;

	std::shared_ptr<function<void()>>m_freeSqlResultFun;

	
	MYSQL_FIELD *fd;    //�ֶ�������
	MYSQL_RES *m_result{nullptr};    //�е�һ����ѯ�����
	MYSQL_ROW m_row;   //�����е���
	
	unsigned int m_rowNum{};
	unsigned int m_rowCount{};
	unsigned int m_fieldNum{};
	const char **m_data{};
	
	unsigned int m_temp{};


private:

	void query();

	void store();

	void fetch_row();

	void freeResult();

	void freeResultStore();

	void checkRowField();


	void FreeConnect();

	void ConnectDatabase();

	void ConnectDatabaseLoop();

	void checkList();
};