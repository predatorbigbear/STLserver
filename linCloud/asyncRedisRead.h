#pragma once


#include "publicHeader.h"


struct ASYNCREDISREAD
{
	ASYNCREDISREAD(std::shared_ptr<io_context> ioc, std::shared_ptr<std::function<void()>>unlockFun, const std::string &redisIP, const unsigned int redisPort,
		const unsigned int memorySize, const unsigned int checkSize);



	//ÿ�β���������ʱ�������´���
	//1 �ж����������Ƿ�Ϸ������Ϸ�ֱ�Ӳ���
	//2 �ж��Ƿ����ӳɹ������ɹ���ͼ����ȴ���������Ķ��У�����ʧ�ܷ��ز���ʧ�ܴ�����Ϣ
	//3 ���ӳɹ�������£������´���
	//��1�� �жϵ�ǰ����״̬�Ƿ�Ϊtrue������ǣ���˵����֮ǰ�Ѿ���ʼ����������ͼ����ȴ���������Ķ��У�����ʧ�ܷ��ز���ʧ�ܴ�����Ϣ
	//��2�� ��ǰ����״̬Ϊfalse�����Ƚ�����״̬��Ϊtrue����ռ������Ȼ��ʼ��ͼ����redis������Ϣ��������ɹ�������ȴ���������Ķ����Ƿ����Ԫ�أ�
	//      �����������ͼ����redis������Ϣ���ڼ�Ƿ����ݲ����Ѿ���ȡ��Ϣ���׼�����첽�ȴ��ڼ䴦��Ķ��У����ȫ��Ϊ�Ƿ���������״̬��Ϊfalse�����������Ѿ���ȡ��Ϣ���׼�����첽�ȴ��ڼ䴦��Ķ���
	//��3�� ������ɳɹ��������startQuery���������״�startQuery�ڼ�û��������Ҫ���д���
	bool insertRedisRequest(std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>> redisRequest);




private:
	std::shared_ptr<io_context> m_ioc{};
	std::unique_ptr<boost::asio::steady_timer>m_timer;
	boost::system::error_code m_err;
	std::unique_ptr<boost::asio::ip::tcp::endpoint>m_endPoint;
	std::unique_ptr<boost::asio::ip::tcp::socket>m_sock;
	



	std::shared_ptr<std::function<void()>> m_unlockFun{};
	std::string m_redisIP;
	unsigned int m_redisPort{};
	const char **m_data{};





	std::unique_ptr<char[]>m_receiveBuffer;                    //������Ϣ�Ļ���
	unsigned int m_memorySize{};                          //������Ϣ�ڴ���ܴ�С
	unsigned int m_checkSize{};                           //ÿ�μ��Ŀ��С
	unsigned int m_thisReceiveLen{};
	unsigned int m_receiveSize{};
	unsigned int m_receivePos{  };
	unsigned int m_lastReceivePos{  };     //  m_lastReceivePos��ÿ�ν������ȥ���



	std::unique_ptr<char[]>m_outRangeBuffer;              //����߽�λ�õ��ַ���
	unsigned int m_outRangeSize{};



	std::atomic<bool>m_connect{ false };                  //�ж��Ƿ��Ѿ�������redis�˵�����
	std::atomic<bool>m_queryStatus{ false };              //�ж��Ƿ��Ѿ������������̣������queryStatus��������������ô�򵥣����ǰ������󣬽�����Ϣ����Ŀǰ����ȫ��������ϵı�־
	std::atomic<bool>m_writeStatus{ false };              //����첽д��״̬
	std::atomic<bool>m_readStatus{ false };               //����첽��ȡ״̬
	std::atomic<bool>m_readyRequest{ false };             //׼�������־״̬
	std::atomic<bool>m_readyParse{ false };
	std::atomic<bool>m_messageEmpty{ true };             //���m_messageList�Ƿ�Ϊ��


	bool parseLoop{ false };
	bool parseStatus;


	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//query���ָ����λ  ����  ����redis���ָ��    �ص�������  �������
	SAFELIST<std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>>>m_messageList;       //  �ȴ���������Ķ���
	

	std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>> m_tempRequest;


	SAFELIST<std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>>>m_waitMessageList;   //  �ȴ���ȡ�������Ķ���
	



	struct SINGLEMESSAGE;
	SAFELIST<std::shared_ptr<SINGLEMESSAGE>>m_messageResponse;                          //�Ѿ���ȡ��Ϣ���׼�����첽�ȴ��ڼ䴦��Ķ���
	
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////





	struct SINGLEMESSAGE
	{
		SINGLEMESSAGE(std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>> tempRequest, bool tempVaild,
			ERRORMESSAGE tempEM) :m_tempRequest(tempRequest), m_tempVaild(tempVaild), m_tempEM(tempEM) {}

		std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>> m_tempRequest;
		bool m_tempVaild{};
		ERRORMESSAGE m_tempEM{};
	};


	////////////////////////////////////////////////////////////////////////////
	std::vector<const char*>m_vec;            //readyQueryʹ��



	std::unique_ptr<char[]>m_ptr;             //�洢ת��������
	unsigned int m_ptrLen{};
	unsigned int m_totalLen{};







	/////////////////////////////////////////////////////////////////////////////////

	std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>>m_redisRequest{};

	std::shared_ptr<std::tuple<const char*, unsigned int, const char **, std::function<void(bool, enum ERRORMESSAGE)>, unsigned int>>m_nextRedisRequest{};


	bool m_insertRequest{ false };              //  ��������״̬


private:
	bool readyEndPoint();

	bool readySocket();

	void firstConnect();

	void setConnectSuccess();

	void setConnectFail();


	////////////////////////////////////////////////////

	bool readyQuery(const char *source, const unsigned int sourceLen);

	bool tryCheckList();                 // ���ȴ���������Ķ��У��鿴�Ƿ��п��Է��͵��������


	///////////////////////////////////////////////////
	void startQuery();




	////////////////////////////////////////////////////
	//��������н��д������Ѿ�������Ϣ����������
	void resetData();

	void tryReadyEndPoint();

	void tryReadySocket();

	void tryConnect();

	void tryResetTimer();

	

	void startReadyRequest();

	void startReadyParse();



	void startReceive();

	//���յ��Ľ���н�������
	bool startParse();

	//�����������ְ汾�����ڽ����ȴ���ȡ������е����
	bool StartParseWaitMessage();   


	//�����������ְ汾�����ڽ���������������
	bool StartParseThisMessage();




	//������Ҫʱ����һ���Ѿ��н������Ϣ���м��ɣ������ظ�����
	void handleMessageResponse();     //�����Ѿ��н���Ķ���


	void queryLoop();             //�첽query

	void checkLoop();              //startReceive  startParse�ɹ�֮����������

	void simpleReceive();          //ֻ�Ǽ򵥵Ķ�ȡ�������κ����� 


	void simpleReceiveWaitMessage();          //ֻ�Ǽ򵥵Ķ�ȡ���ְ汾�������κ����� ���ڽ����ȴ���ȡ������е����

	void simpleReceiveThisMessage();          //ֻ�Ǽ򵥵Ķ�ȡ���ְ汾�������κ����� ���ڽ���������������
};
