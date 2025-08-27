#pragma once





#include "ASYNCLOG.h"
#include "concurrentqueue.h"
#include "errorMessage.h"
#include "STLTimeWheel.h"

#include <openssl/sha.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>


#include<numeric>
#include<stdexcept>
#include<exception>
#include<atomic>
#include<memory>
#include<algorithm>
#include<functional>
#include "commonStruct.h"



//鉴于mysql 官方的异步api需要不断轮询状态，在大量操作时性能会较为低下
//现成实现中 很多都带大量拷贝操作的
//于是决定实现新的mysql自解析 真正意义上的零拷贝客户端

//该实现版本为无SSL 且密码认证插件为caching_sha2_password 方式
//使用环境主要是在内网或者本机场景


struct MULTISQLREAD
{
	

	MULTISQLREAD(const std::shared_ptr<boost::asio::io_context> &ioc,const std::shared_ptr<std::function<void()>> &unlockFun, 
		const std::shared_ptr<STLTimeWheel>& timeWheel,
		const std::shared_ptr<ASYNCLOG>& log,
		const std::string &SQLHOST, const std::string &SQLUSER,
		const std::string &SQLPASSWORD, const std::string &SQLDB, const unsigned int &SQLPORT, 
		const unsigned int commandMaxSize, bool& success, const unsigned int bufferSize = 67108864);


	






private:

	std::string m_host{};
	std::string m_user{};
	std::string m_passwd{};
	std::string m_db{};
	const std::string plugin_name{ "caching_sha2_password" };
	const unsigned int m_port{};
	

	bool* m_success{};
	

	const std::shared_ptr<boost::asio::io_context> m_ioc{};
	const std::shared_ptr<std::function<void()>> m_unlockFun{};
	const std::shared_ptr<STLTimeWheel> m_timeWheel{};
	const std::shared_ptr<ASYNCLOG> m_log{};



	boost::system::error_code m_err;
	std::unique_ptr<boost::asio::ip::tcp::endpoint>m_endPoint{};
	std::unique_ptr<boost::asio::ip::tcp::socket>m_sock{};


	
	

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	 
	//接收发送缓冲区
	std::unique_ptr<unsigned char[]>m_msgBuf{};

	//接收发送缓冲区最大值
	//每次发送数据量不要大于m_msgBufMaxSize - 4 （根据mysql协议）
	const unsigned int m_msgBufMaxSize{};

	// 接收发送缓冲区当前值
	unsigned int m_msgBufMaxNowSize{};

	//已接收字段长度
	int m_readLen{};

	//本次数据包消息长度
	unsigned int m_messageLen{};

	//本次数据包序列号
	unsigned int m_seqID{};

	///////////////////////   握手包解析参数

	//每次数据包首尾位置  排除前四位   
	const unsigned char* strBegin{}, * strEnd{};


	unsigned int serverCapabilityFlags{};


	std::string autuPluginData{};

	////////////////////////////////////////   构建认证握手包参数

	unsigned char* clientBegin{}, * clientEnd{}, * clientIter{};

	unsigned int handShakeLen{ };

	unsigned int clientFlag{};

	unsigned int clientSendLen{};


	

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	std::atomic<int> m_queryStatus{ 0 };         //0 未连接    1 已连接未处理状态    2   已连接正在处理状态


	bool m_jumpThisRes{ false };                     //是否跳过本次请求的标志






	const unsigned int m_commandMaxSize{};        // 命令个数最大大小（长度）

	unsigned int m_commandNowSize{};        // 命令个数实际大小（长度）

	unsigned int m_thisCommandToalSize{};       // 本条节点的总请求个数

	unsigned int m_currentCommandSize{};        // 当前命令的已处理个数


	///////////////////////////////////////////////////////////////////
	// 
	//待投递队列，未/等待拼凑消息的队列
	//使用开源无锁队列进一步提升qps ，这是github地址  https://github.com/cameron314/concurrentqueue
	//moodycamel::ConcurrentQueue<std::shared_ptr<resultTypeSW>>m_messageList;



	//待获取结果队列，已经发送请求的队列，处理时使用m_waitMessageList进行处理，减少使用m_messageList的锁的机会
	//std::unique_ptr<std::shared_ptr<resultTypeSW>[]>m_waitMessageList{};

	//unsigned int m_waitMessageListMaxSize{};

	//unsigned int m_waitMessageListNowSize{};


	//  根据m_waitMessageListBegin获取每次的RES
	//std::shared_ptr<resultTypeSW> *m_waitMessageListBegin{};

	//std::shared_ptr<resultTypeSW> *m_waitMessageListEnd{};

	///////////////////////////////////////////////////////////


	////////////////////////////////////////////////////////////////
	


	/////////////////////////////////////////////////////////


	// 能力标志位定义 (MySQL 8.0)
	static const unsigned int CLIENT_LONG_PASSWORD = 1;
	static const unsigned int CLIENT_FOUND_ROWS = 2;
	static const unsigned int CLIENT_LONG_FLAG = 4;
	static const unsigned int CLIENT_CONNECT_WITH_DB = 8;
	static const unsigned int CLIENT_PROTOCOL_41 = 512;
	static const unsigned int CLIENT_PLUGIN_AUTH = 0x80000;
	static const unsigned int CLIENT_SECURE_CONNECTION = 0x8000;
	static const unsigned int CLIENT_MULTI_STATEMENTS = (1UL << 16);
	static const unsigned int CLIENT_MULTI_RESULTS = (1UL << 17);
	static const unsigned int CLIENT_PS_MULTI_RESULTS = 0x00040000;
	static const unsigned int CLIENT_LOCAL_FILES = 0x00000080;
	static const unsigned int CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA = 0x00200000;
	static const unsigned int utf8mb4_unicode_ci = 224;


	////////////////////////////////////////////////////

	

private:
	void connectMysql();

	void recvHandShake();

	bool parseHandShake();
	

	bool makeHandshakeResponse41();

	void sendHandshakeResponse41();
};


