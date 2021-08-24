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



struct MULTISQLREAD
{
	/*
	ִ�����ָ��

	�������const char*����string װ��ʵ���ַ�����Ϣ��������Ϊ�����������Կɶ���ȷʵ�ܺã����ǻ����һ�����ܣ�ԭ����������ƴ�ӹ����в��ɱ����������ݿ�������������ʹ�ö���ָ�룬��ȻҲ�����ݿ�����
	���ǿ��Խ�����������Ľ������ͣ����������ַ�������ֻ�ڷ�������׼�������н���һ��


	ָ����� ��ʵ�ʸ���=ָ�����/2  ��Ϊ������βλ�ã�����Ƕ���м���Ҫ����;��
	ִ���������
	�����ָ��
	������� ����int*    ÿ2���洢һ�����������������
	������� ����buffer��С
	���ָ�룬�洢ÿһ���Ľ��ָ��
	���ָ��buffer��С
	�ص�����
	*/

	// 

	using resultType = std::tuple<const char**, unsigned int, unsigned int, std::shared_ptr<MYSQL_RES*>, unsigned int, std::shared_ptr<unsigned int[]>, unsigned int, 
		std::shared_ptr<const char*[]>, unsigned int,std::function<void(bool, enum ERRORMESSAGE)>>;


	using resultTypeRW = std::tuple<std::reference_wrapper<std::vector<std::string_view>>, unsigned int, std::reference_wrapper<std::vector<MYSQL_RES*>> ,
		std::reference_wrapper<std::vector<unsigned int>> , std::reference_wrapper<std::vector<std::string_view>> , std::function<void(bool, enum ERRORMESSAGE)> >;


	MULTISQLREAD(std::shared_ptr<io_context> ioc, std::shared_ptr<std::function<void()>>unlockFun, const std::string &SQLHOST, const std::string &SQLUSER,
		const std::string &SQLPASSWORD, const std::string &SQLDB, const std::string &SQLPORT,const unsigned int commandMaxSize);

	
	std::shared_ptr<std::function<void(MYSQL_RES **, const unsigned int)>> sqlRelease();



	bool insertSqlRequest(std::shared_ptr<resultType> sqlRequest);


	// �ͷ����ݿ�����
	void FreeResult(MYSQL_RES ** resultPtr, const unsigned int resultLen);


	~MULTISQLREAD();





private:
	


	MYSQL *m_mysql;

	std::string m_host{};
	std::string m_user{};
	std::string m_passwd{};
	std::string m_db{};
	unsigned int m_port{};
	
	const char *m_unix_socket{};


	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//std::string m_message;                     //���Ϸ��������ַ���
	
	std::unique_ptr<char[]>m_messageBuffer{};

	unsigned int m_messageBufferMaxSize{};
	
	unsigned int m_messageBufferNowSize{};



	unsigned int m_commandMaxSize{};        //�����������С�����ȣ�

	unsigned int m_commandNowSize{};        //�������ʵ�ʴ�С�����ȣ�
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	const char *m_sendMessage{};

	unsigned int m_sendLen{};


	bool resetConnect{ false };

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	std::atomic<bool> m_hasConnect{ false };         //�Ƿ��Ѿ���������״̬
	std::atomic<bool>m_queryStatus{ false };         //�Ƿ����ڽ��в�ѯ����
	




	net_async_status m_status;
	std::unique_ptr<boost::asio::steady_timer> m_timer;
	std::shared_ptr<std::function<void()>>m_unlockFun;


	//�ȴ��ͷŽ������ָ�룬���������
	std::shared_ptr<std::function<void(MYSQL_RES **,const unsigned int)>>m_freeSqlResultFun;


	

	
	///////////////////////////////////////////////////////////////////
	//��Ͷ�ݶ��У�δ/�ȴ�ƴ����Ϣ�Ķ���
	SAFELIST<std::shared_ptr<resultType>>m_messageList;



	//����ȡ������У��Ѿ���������Ķ��У�����ʱʹ��m_waitMessageList���д�������ʹ��m_messageList�����Ļ���
	std::unique_ptr<std::shared_ptr<resultType>[]>m_waitMessageList;

	unsigned int m_waitMessageListMaxSize{};

	unsigned int m_waitMessageListNowSize{};



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

	std::shared_ptr<resultType> m_sqlRequest{};


	//////////////////////////////////////////////////////



	std::unique_ptr<MYSQL_RES*[]>m_resultArray{};        //  �洢MYSQL RES �����
	unsigned int m_resultMaxSize{};                         //  buffer�ռ��С
	unsigned int m_resultNowSize{};





	std::unique_ptr<unsigned int[]>m_rowField{};         //  �洢ÿһ����Ӧ�������row��field
	unsigned int m_rowFieldMaxSize{};
	unsigned int m_rowFieldNowSize{};                  //����row field�洢ռ�ݵ���Ŀ


	std::vector<const char *>m_dataVec;       //   �洢ÿһ����Ӧ��������ַ�������

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


	void handleAsyncError();


	void store();


	void next_result();


	void checkRowField();


	void fetch_row();


	void multiGetResult();

	//��m_dataArray���
	void clearDataArray();


	//���m_messageList�Ƿ�����Ҫƴװ�������Ϣ
	void makeMessage();


	void clearWaitMessageList();


	void clearMessageList(const ERRORMESSAGE value);

	//  ÿ��������װ��Ϣʱ�����ø��õı���  
	void readyMakeMessage();
};