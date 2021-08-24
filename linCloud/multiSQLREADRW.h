#pragma once



#pragma once


#include "publicHeader.h"
#include "mysql.h"
#include "mysqld_error.h"



//ҵ���ʹ��vector<const char**>��װ�ش洢ָ�룬ʡȴ�Զ�������

//��ܲ�������Σ���ʹ��string���������ݣ��ؼ������ָ�뷵�ز���


//  ���ñ�־λ����¼sqlִ��״̬


//  �����Ͷ����Ϣ����ʱ���ж�sqlִ��״̬�����sqlû��ִ�У�����λִ�У��������ִ�е����������޶���������ֱ�ӱ����أ�����������Ͷ���ַ����У��ۼ��������ǵü���ۼ��������ܳ����޶���������


//  ����ۼӺ�����������޶������򲻽����ۼӲ���


//  



//



struct MULTISQLREADRW
{
	/*
	ִ�����


	ִ���������
	�����
	������� ����int
	�������
	�ص�����
	*/

	// 

	using resultTypeRW = std::tuple<std::reference_wrapper<std::vector<std::string_view>>, unsigned int, std::reference_wrapper<std::vector<MYSQL_RES*>>,
		std::reference_wrapper<std::vector<unsigned int>>, std::reference_wrapper<std::vector<std::string_view>>, std::function<void(bool, enum ERRORMESSAGE)> >;


	MULTISQLREADRW(std::shared_ptr<io_context> ioc, std::shared_ptr<std::function<void()>>unlockFun, const std::string &SQLHOST, const std::string &SQLUSER,
		const std::string &SQLPASSWORD, const std::string &SQLDB, const std::string &SQLPORT, const unsigned int nodeMaxSize);


	std::shared_ptr<std::function<void(std::reference_wrapper<std::vector<MYSQL_RES*>>)>> sqlRelease();



	bool insertSqlRequest(std::shared_ptr<resultTypeRW> sqlRequest);


	// �ͷ����ݿ�����
	void FreeResult(std::reference_wrapper<std::vector<MYSQL_RES*>> vecwrapper);


	~MULTISQLREADRW();





private:



	MYSQL *m_mysql;

	std::string m_host{};
	std::string m_user{};
	std::string m_passwd{};
	std::string m_db{};
	unsigned int m_port{};

	const char *m_unix_socket{};


	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	 //���Ϸ��������ַ���

	std::unique_ptr<char[]>m_messageBuffer{};

	unsigned int m_messageBufferMaxSize{};

	unsigned int m_messageBufferNowSize{};



	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	const char *m_sendMessage{};

	unsigned int m_sendLen{};


	bool resetConnect{ false };

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	std::atomic<bool> m_hasConnect{ false };         //�Ƿ��Ѿ���������״̬
	std::atomic<bool>m_queryStatus{ false };         //�Ƿ����ڽ��в�ѯ����



	bool m_jumpThisRes{ false };                     //�Ƿ�������������ı�־


	net_async_status m_status;
	std::unique_ptr<boost::asio::steady_timer> m_timer;
	std::shared_ptr<std::function<void()>>m_unlockFun;


	//�ȴ��ͷŽ������ָ�룬���������
	std::shared_ptr<std::function<void(std::reference_wrapper<std::vector<MYSQL_RES*>>)>>m_freeSqlResultFun;




	unsigned int m_commandMaxSize{};        // �����������С�����ȣ�

	unsigned int m_commandNowSize{};        // �������ʵ�ʴ�С�����ȣ�

	unsigned int m_thisCommandToalSize{};       // �����ڵ�����������

	unsigned int m_currentCommandSize{};        // ��ǰ������Ѵ������


	///////////////////////////////////////////////////////////////////
	//��Ͷ�ݶ��У�δ/�ȴ�ƴ����Ϣ�Ķ���
	SAFELIST<std::shared_ptr<resultTypeRW>>m_messageList;



	//����ȡ������У��Ѿ���������Ķ��У�����ʱʹ��m_waitMessageList���д�������ʹ��m_messageList�����Ļ���
	std::unique_ptr<std::shared_ptr<resultTypeRW>>m_waitMessageList;

	unsigned int m_waitMessageListMaxSize{};

	unsigned int m_waitMessageListNowSize{};


	//  ����m_waitMessageListBegin��ȡÿ�ε�RES
	std::shared_ptr<resultTypeRW> *m_waitMessageListBegin{};

	std::shared_ptr<resultTypeRW> *m_waitMessageListEnd{};



	///////////////////////////////////////////////////////////

	/*
	ִ�����ָ��
	ָ����� ��ʵ�ʸ���=ָ�����/2  ��Ϊ������βλ�ã�����Ƕ���м���Ҫ����;��
	ִ���������
	�����ָ��
	������� ����int*    ÿ2���洢һ�����������������
	������� ����buffer��С
	���ָ�룬�洢ÿһ���Ľ��ָ��
	���ָ��buffer��С
	�ص�����
	*/

	std::shared_ptr<resultTypeRW> m_sqlRequest{};


	/////////////////////////////////////////////////////////

	MYSQL_FIELD *fd;    //�ֶ�������
	MYSQL_RES *m_result{ nullptr };    //�е�һ����ѯ�����
	MYSQL_ROW m_row;   //�����е���

	unsigned int m_rowNum{};
	unsigned int m_rowCount{};
	unsigned int m_fieldNum{};
	const char **m_data{};
	unsigned long *m_lengths{};


	unsigned int m_temp{};


private:
	void FreeConnect();


	void ConnectDatabase();


	void ConnectDatabaseLoop();


	void firstQuery();


//	void handleAsyncError();


	void store();


	void readyQuery();


	bool readyNextQuery();


	void next_result();


//	void checkRowField();


//	void fetch_row();


//	void multiGetResult();

	//��m_dataArray���
//	void clearDataArray();


	//���m_messageList�Ƿ�����Ҫƴװ�������Ϣ
	void makeMessage();


//	void clearWaitMessageList();


//	void clearMessageList(const ERRORMESSAGE value);

	//  ÿ��������װ��Ϣʱ�����ø��õı���  
	void readyMakeMessage();


	//��������������Ƿ�����Ϣ����
	void checkMakeMessage();
};
