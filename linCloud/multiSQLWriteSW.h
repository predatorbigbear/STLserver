#pragma once




#include "publicHeader.h"
#include "mysql.h"
#include "mysqld_error.h"
#include "LOG.h"
#include "STLtreeFast.h"


struct MULTISQLWRITESW
{
	/*
	ִ������string_view��
	ִ���������
	����ֱ��ƴ���ַ�������string�У�����ǰ����ȡ�����ݽ��з���

	*/
	// 

	using SQLWriteTypeSW = std::tuple<std::reference_wrapper<std::vector<std::string_view>>, unsigned int>;


	MULTISQLWRITESW(std::shared_ptr<io_context> ioc, std::shared_ptr<std::function<void()>>unlockFun, const std::string &SQLHOST, const std::string &SQLUSER,
		const std::string &SQLPASSWORD, const std::string &SQLDB, const std::string &SQLPORT, const unsigned int nodeMaxSize ,std::shared_ptr<LOG> log);


	/*
	//�������������ж��Ƿ�����redis�������ɹ���
	//���û�����ӣ�����ֱ�ӷ��ش���
	//���ӳɹ�������£���������Ƿ����Ҫ��
	*/
	bool insertSQLRequest(std::shared_ptr<SQLWriteTypeSW> sqlRequest);



private:



	MYSQL *m_mysql;

	std::string m_host{};
	std::string m_user{};
	std::string m_passwd{};
	std::string m_db{};
	unsigned int m_port{};

	const char *m_unix_socket{};




	std::shared_ptr<std::function<void()>>m_unlockFun{};




	unsigned int m_commandMaxSize{};        // �����������С�����ȣ�

	unsigned int m_commandNowSize{};        // �������ʵ�ʴ�С�����ȣ�



	std::unique_ptr<boost::asio::steady_timer> m_timer{};


	std::mutex m_mutex;
	bool m_hasConnect{ false };         //�Ƿ��Ѿ���������״̬


	net_async_status m_status;


	bool resetConnect{ false };



	////////////////////////////////////////////////////////
	//std::string m_message;                     //���Ϸ��������ַ���

	std::unique_ptr<char[]>m_messageBuffer{};

	unsigned int m_messageBufferMaxSize{};

	unsigned int m_messageBufferNowSize{};


	const char *m_sendMessage{};

	unsigned int m_sendLen{};

	std::vector<std::string_view>m_arrayResult;             //��ʱ�洢��������vector
	//////////////////////////////////////////////////////////////////////////////

	bool m_queryStatus{ false };                         //�ж��Ƿ��Ѿ������������̣������queryStatus��������������ô�򵥣����ǰ������󣬽�����Ϣ����Ŀǰ����ȫ��������ϵı�־



	std::unique_ptr<char[]>m_receiveBuffer{};               //������Ϣ�Ļ���
	unsigned int m_receiveBufferMaxSize{};                //������Ϣ�ڴ���ܴ�С
	unsigned int m_receiveBufferNowSize{};                //ÿ�μ��Ŀ��С


	unsigned int m_thisReceivePos{};              //����ÿ��receive pos��ʼ��
	unsigned int m_thisReceiveLen{};              //����ÿ��receive����


	unsigned int m_commandTotalSize{};                //���νڵ������ܼ�
	unsigned int m_commandCurrentSize{};              //���νڵ�������ǰ����

	bool m_jumpNode{ false };
	unsigned int m_lastPrasePos{  };     //  m_lastReceivePos��ÿ�ν������ȥ�������¼��ǰ��������λ��


	//����ȡ������У��Ѿ���������Ķ��У�����ʱʹ��m_waitMessageList���д�������ʹ��m_messageList�����Ļ���
	std::unique_ptr<std::shared_ptr<SQLWriteTypeSW>[]>m_waitMessageList{};

	unsigned int m_waitMessageListMaxSize{};

	unsigned int m_waitMessageListNowSize{};


	//  ����m_waitMessageListBegin��ȡÿ�ε�RES
	std::shared_ptr<SQLWriteTypeSW> *m_waitMessageListBegin{};

	std::shared_ptr<SQLWriteTypeSW> *m_waitMessageListEnd{};


	MEMORYPOOL<char> m_charMemoryPool;


	//��Ͷ�ݶ��У�δ/�ȴ�ƴ����Ϣ�Ķ���  C++17    ��ʱsql buffer  buffer����   �������
	//���m_messageList�Ƿ�Ϊ�գ�
	//Ϊ�յ��ñ�־λ�˳�
	//��Ϊ�յĳ��Ի�ȡcomnmandMaxSize �Ľڵ����ݣ�ͳ����Ҫ��buffer�ռ䣬
	//���Ը��������buffer�ռ���з���
	//����ɹ��Ļ�����copy��Ȼ�������һ��query
	//��Ͷ�ݶ��У�δ/�ȴ�ƴ����Ϣ�Ķ���
	SAFELIST<std::shared_ptr<SQLWriteTypeSW>>m_messageList;

	std::shared_ptr<LOG> m_log{};

	 



private:

	void ConnectDatabase();


	void ConnectDatabaseLoop();


	void setConnectSuccess();


	void setConnectFail();


	//������������
	void query();


	void readyMessage();
	
};
