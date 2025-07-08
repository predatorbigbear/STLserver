#pragma once


#include "LOG.h"
#include "concurrentqueue.h"
#include "regexFunction.h"
#include "httpResponse.h"
#include "errorMessage.h"
#include "staticString.h"
#include "STLTimeWheel.h"


#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>

#include<numeric>
#include<forward_list>
#include<atomic>


struct MULTIREDISREAD
{
	/*
	ִ������string_view��
	ִ���������
	ÿ������Ĵ���������������redis RESP����ƴ�ӣ�
	��ȡ�������  ����Ϊ����һЩ����������ܲ�һ���н�����أ�

	���ؽ��string_view
	ÿ������Ĵ������

	�ص�����
	*/
	// 
	using redisResultTypeSW = std::tuple<std::reference_wrapper<std::vector<std::string_view>>, unsigned int, std::reference_wrapper<std::vector<unsigned int>>, unsigned int,
		std::reference_wrapper<std::vector<std::string_view>>, std::reference_wrapper<std::vector<unsigned int>>,
		std::function<void(bool, enum ERRORMESSAGE)>>;


	MULTIREDISREAD(std::shared_ptr<boost::asio::io_context> ioc, std::shared_ptr<LOG> log, std::shared_ptr<std::function<void()>>unlockFun, 
		std::shared_ptr<STLTimeWheel> timeWheel,
		const std::string &redisIP, const unsigned int redisPort,
		const unsigned int memorySize, const unsigned int outRangeMaxSize, const unsigned int commandSize);


	//�������������ж��Ƿ�����redis�������ɹ���
	//���û�����ӣ�����ֱ�ӷ��ش���
	//���ӳɹ�������£���������Ƿ����Ҫ��
	bool insertRedisRequest(std::shared_ptr<redisResultTypeSW> &redisRequest);









private:
	std::shared_ptr<boost::asio::io_context> m_ioc{};
	boost::system::error_code m_err;
	std::unique_ptr<boost::asio::ip::tcp::endpoint>m_endPoint{};
	std::unique_ptr<boost::asio::ip::tcp::socket>m_sock{};

	std::shared_ptr<LOG> m_log{};
	std::shared_ptr<STLTimeWheel> m_timeWheel{};

	std::shared_ptr<std::function<void()>> m_unlockFun{};
	std::string m_redisIP;
	unsigned int m_redisPort{};
	const char **m_data{};


	int m_connectStatus{};

	std::unique_ptr<char[]>m_receiveBuffer{};               //������Ϣ�Ļ���
	unsigned int m_receiveBufferMaxSize{};                //������Ϣ�ڴ���ܴ�С
	unsigned int m_receiveBufferNowSize{};                //ÿ�μ��Ŀ��С


	unsigned int m_thisReceivePos{};              //����ÿ��receive pos��ʼ��
	unsigned int m_thisReceiveLen{};              //����ÿ��receive����


	unsigned int m_commandTotalSize{};                //���νڵ������ܼ�
	unsigned int m_commandCurrentSize{};              //���νڵ�������ǰ����

	bool m_jumpNode{ false };
	unsigned int m_lastPrasePos{  };     //  m_lastReceivePos��ÿ�ν������ȥ�������¼��ǰ��������λ��
	//ÿ�ν�����ʼʱ��⵱ǰ�ڵ�λ�ã���ǰ�ڵ��������Ѿ���ȡ�������������Լ��������Ƿ��Ѿ����ر�������������������־��
	// $-1\r\n$11\r\nhello_world\r\n               REDISPARSETYPE::LOT_SIZE_STRING
	// :11\r\n:0\r\n                               REDISPARSETYPE::INTERGER
	// -ERR unknown command `getx`, with args beginning with: `foo`, \r\n-ERR unknown command `getg`, with args beginning with: `dan`, \r\n                REDISPARSETYPE::ERROR
	// +OK\r\n+OK\r\n                              REDISPARSETYPE::SIMPLE_STRING
	// *2\r\n$3\r\nfoo\r\n$3\r\nfoo\r\n*2\r\n$3\r\nfoo\r\n$-1\r\n         REDISPARSETYPE::ARRAY


	std::unique_ptr<char[]>m_outRangeBuffer{};              //����߽�λ�õ��ַ��������Խ����ַ������ȳ���m_outRangeMaxSize��ֱ�ӷ���0�����������ʹ��ʱ���ֿ�ָ�����⣬���ݴ����������ͨ������buffer��С���
	unsigned int m_outRangeMaxSize{};                     //
	unsigned int m_outRangeNowSize{};


	std::atomic<bool>m_connect{ false };                  //�ж��Ƿ��Ѿ�������redis�˵�����
	std::atomic<bool>m_queryStatus{ false };
	


	//��Ͷ�ݶ��У�δ/�ȴ�ƴ����Ϣ�Ķ���
	//ʹ�ÿ�Դ�������н�һ������qps ������github��ַ  https://github.com/cameron314/concurrentqueue
	moodycamel::ConcurrentQueue<std::shared_ptr<redisResultTypeSW>>m_messageList;



/////////////////////////////////////////////////////////
	std::unique_ptr<std::shared_ptr<redisResultTypeSW>[]>m_waitMessageList{};

	unsigned int m_waitMessageListMaxSize{};

	unsigned int m_waitMessageListNowSize{};

	std::shared_ptr<redisResultTypeSW> *m_waitMessageListBegin{};

	std::shared_ptr<redisResultTypeSW> *m_waitMessageListEnd{};

	std::shared_ptr<redisResultTypeSW> *m_waitMessageListBeforeParse{};      //�����жϽ�����λ����û�з����仯

	std::shared_ptr<redisResultTypeSW> *m_waitMessageListAfterParse{};       //�����жϽ�����λ����û�з����仯

	std::shared_ptr<redisResultTypeSW> redisRequest;

	//һ���Է��������������
	unsigned int m_commandMaxSize{};

	//���η��͵��������
	unsigned int m_commandNowSize{};


	////////////////////////////////////////////////////////
	//std::string m_message;                     //���Ϸ��������ַ���

	std::unique_ptr<char[]>m_messageBuffer{};

	unsigned int m_messageBufferMaxSize{};

	unsigned int m_messageBufferNowSize{};


	const char *m_sendMessage{};

	unsigned int m_sendLen{};

	std::vector<std::string_view>m_arrayResult;             //��ʱ�洢��������vector
	//////////////////////////////////////////////////////////////////////////////

	boost::system::error_code ec;

private:
	bool readyEndPoint();


	bool readySocket();


	void firstConnect();

	//redis�ͻ��˺ͷ�����ʧȥ���Ӻ���������
	void reconnect();


	void setConnectSuccess();


	void setConnectFail();


	void query();
	

	void receive();


	void resetReceiveBuffer();

	//redis������Ϣ�����㷨
	bool parse(const int size);


	void handlelRead(const boost::system::error_code &err, const std::size_t size);

	//���͸�redis-server������ƴװ����
	void readyMessage();


	void shutdownLoop();


	void cancelLoop();


	void closeLoop();


	void resetSocket();
};
