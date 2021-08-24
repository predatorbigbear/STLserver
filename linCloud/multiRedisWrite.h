#pragma once

#include "publicHeader.h"
#include "LOG.h"
#include "STLtreeFast.h"


struct MULTIREDISWRITE
{
	/*
	ִ������string_view��
	ִ���������
	ÿ������Ĵ���������������redis RESP����ƴ�ӣ�
	����ֱ��ƴ���ַ�������string�У�����ǰ����ȡ�����ݽ��з���

	*/
	// 
	using redisWriteTypeSW = std::tuple<std::reference_wrapper<std::vector<std::string_view>>, unsigned int, std::reference_wrapper<std::vector<unsigned int>>>;


	MULTIREDISWRITE(std::shared_ptr<io_context> ioc, std::shared_ptr<std::function<void()>>unlockFun, const std::string &redisIP, const unsigned int redisPort,
		const unsigned int memorySize, const unsigned int outRangeMaxSize, const unsigned int commandSize);


	/*
	//�������������ж��Ƿ�����redis�������ɹ���
	//���û�����ӣ�����ֱ�ӷ��ش���
	//���ӳɹ�������£���������Ƿ����Ҫ��
	*/
	bool insertRedisRequest(std::shared_ptr<redisWriteTypeSW> redisRequest);







private:
	std::shared_ptr<io_context> m_ioc{};
	std::unique_ptr<boost::asio::steady_timer>m_timer{};
	boost::system::error_code m_err;
	std::unique_ptr<boost::asio::ip::tcp::endpoint>m_endPoint{};
	std::unique_ptr<boost::asio::ip::tcp::socket>m_sock{};



	std::shared_ptr<std::function<void()>> m_unlockFun{};
	std::string m_redisIP;
	unsigned int m_redisPort{};
	const char **m_data{};



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


	



	std::mutex m_mutex;
	bool m_connect{ false };                  //�ж��Ƿ��Ѿ�������redis�˵�����
	bool m_queryStatus{ false };                         //�ж��Ƿ��Ѿ������������̣������queryStatus��������������ô�򵥣����ǰ������󣬽�����Ϣ����Ŀǰ����ȫ��������ϵı�־


	//��Ͷ�ݶ��У�δ/�ȴ�ƴ����Ϣ�Ķ���
	SAFELIST<std::shared_ptr<redisWriteTypeSW>>m_messageList;



	/////////////////////////////////////////////////////////
	std::unique_ptr<std::shared_ptr<redisWriteTypeSW>[]>m_waitMessageList{};

	unsigned int m_waitMessageListMaxSize{};

	unsigned int m_waitMessageListNowSize{};

	std::shared_ptr<redisWriteTypeSW> *m_waitMessageListBegin{};

	std::shared_ptr<redisWriteTypeSW> *m_waitMessageListEnd{};

	std::shared_ptr<redisWriteTypeSW> *m_waitMessageListBeforeParse{};      //�����жϽ�����λ����û�з����仯

	std::shared_ptr<redisWriteTypeSW> *m_waitMessageListAfterParse{};       //�����жϽ�����λ����û�з����仯



	//���Ȼ�ȡ�ж�����Ҫ�Ŀռ䣬���Խ���һ���Է���ռ䣬���Ƿ�ɹ�
	//�ɹ��Ļ����ɣ�Ȼ����з���д������

	//�������ʱ�������������string_viewָ��ҵ�������ݽ������룬��ƴװǰ����һ��copy���ɡ�
	//������ǹ�֮�󣬻���ѡ������write������һ��copy�������ǣ�
	//ʹ��redis�����£�����д�٣����Դ������ҵ��㣬��ҵ���ÿ��ʹ�ø�buffer����Ҫ���м���������д���ܺ��ٺ��ٷ�������Զ�������۲�ֵ�ã�
	//����ǣ�Դ���ݿ��ܴ����ڶ���ط���buffer�У����ʹ���������buffer������Ҫ����ʹ��
	MEMORYPOOL<char> m_charMemoryPool;

	//


	



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


private:


	
	bool readyEndPoint();


	bool readySocket();

	
	void firstConnect();


	void setConnectSuccess();


	void setConnectFail();


	
	void query();

	
	void receive();


	void resetReceiveBuffer();


	void handlelRead(const boost::system::error_code &err, const std::size_t size);


	bool prase(const int size);


	void readyMessage();
	


};
