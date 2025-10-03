#pragma once





#include "ASYNCLOG.h"
#include "concurrentqueue.h"
#include "errorMessage.h"
#include "STLTimeWheel.h"
#include "memoryPool.h"

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
// mysql_native_password暂不支持，因为9.4版本已经移除了这种认证方式
//使用环境主要是在内网或者本机场景


struct MULTISQLREAD
{
	/*
	0执行命令string_view集
	1执行命令个数
	2每条命令的string_view个数（方便进行拼接）

	3 first 每条命令获取结果个数，second 是否为事务语句  （因为比如一些事务操作可能不一定有结果返回，利用second
	可以在事务中发生错误时快速跳转指针指向）、
	

	对于存储过程调用,使用CALL 存储过程名(参数列表)格式 传入，设置好获取结果次数，
	存储过程需预先在服务器中设置好，因为如果在这里调用命令组装还需要再增加一项参数  每个获取结果对应的执行命令个数
	简单的事务语句测试过每条命令也对应一个结果，复杂的需要进行测试验证



	4返回结果string_view
	5每个结果的string_view个数

	6回调函数
	*/
	// 
	using MYSQLResultTypeSW = std::tuple<std::reference_wrapper<std::vector<std::string_view>>, unsigned int,
		std::reference_wrapper<std::vector<unsigned int>>, 
		std::reference_wrapper<std::vector<std::pair<unsigned int, bool>>>,
		std::reference_wrapper<std::vector<std::string_view>>, std::reference_wrapper<std::vector<unsigned int>>,
		std::function<void(bool, enum ERRORMESSAGE)>, MEMORYPOOL<>&>;




	MULTISQLREAD(const std::shared_ptr<boost::asio::io_context> &ioc,const std::shared_ptr<std::function<void()>> &unlockFun, 
		const std::shared_ptr<STLTimeWheel>& timeWheel,
		const std::shared_ptr<ASYNCLOG>& log,
		const std::string &SQLHOST, const std::string &SQLUSER,
		const std::string &SQLPASSWORD, const std::string &SQLDB, const unsigned int SQLPORT, 
		bool& success,
		const unsigned int commandMaxSize, const unsigned int bufferSize = 67108864);


	//插入请求，首先判断是否连接mysql服务器成功，
	//如果没有连接，插入直接返回错误
	//连接成功的情况下，检查请求是否符合要求
	bool insertMysqlRequest(std::shared_ptr<MYSQLResultTypeSW>& mysqlRequest);






private:

	const std::string m_host{};
	const std::string m_user{};
	const std::string m_passwd{};
	const std::string m_db{};
	const std::string plugin_name{ "caching_sha2_password" };
	const unsigned int m_port{};

	//用于公钥加密密码时使用
	std::string m_encryptPass{};
	

	bool* m_success{};
	bool firstConnect{ true };

	const std::shared_ptr<boost::asio::io_context> m_ioc{};
	const std::shared_ptr<std::function<void()>> m_unlockFun{};
	const std::shared_ptr<STLTimeWheel> m_timeWheel{};
	const std::shared_ptr<ASYNCLOG> m_log{};



	boost::system::error_code m_err;
	std::unique_ptr<boost::asio::ip::tcp::endpoint>m_endPoint{};
	std::unique_ptr<boost::asio::ip::tcp::socket>m_sock{};


	//第一次发送命令时数据包序列号为0，不需要递增
	bool firstQuery{ true };


	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	 
	//存储每条命令的起始位置缓冲区
	std::unique_ptr<unsigned char*[]>m_commandBuf{};

	//存储命令起始位置
	unsigned char** m_commandBufBegin{};

	//存储命令结束位置
	unsigned char** m_commandBufEnd{};


	//接收发送缓冲区
	std::unique_ptr<unsigned char[]>m_msgBuf{};

	//发送缓冲区起始位置
	unsigned char* m_sendBuf{};

	//接收缓冲区起始位置
	unsigned char* m_recvBuf{};

	//接收发送缓冲区最大值
	//每次发送数据量不要大于m_msgBufMaxSize - 4 （根据mysql协议）
	unsigned int m_msgBufMaxSize{};

	//客户端接收消息单个数据包最大大小
	const unsigned int m_clientBufMaxSize{};

	// 接收发送缓冲区当前值
	unsigned int m_msgBufNowSize{};

	//已接收字段长度
	int m_readLen{};

	//本次数据包消息长度
	unsigned int m_messageLen{};

	//本次数据包序列号
	unsigned char m_seqID{};

	//指向MYSQLResultTypeSW 第3位vector的迭代器
	std::vector<std::pair<unsigned int, bool>>::const_iterator m_VecBegin{}, m_VecEnd{};

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



	const unsigned char OKPacket[7]{ 0x00,0x00,0x00,0x02,0x00,0x00,0x00 };
	const unsigned char reqPubkey[5]{ 0x01,0x00,0x00,0x00,0x02 };

	//////////////////////////////////////////////////////////  公钥相关参数

	const std::string publicKey1{ "-----BEGIN PUBLIC KEY-----" };
	const std::string publicKey2{ "-----END PUBLIC KEY-----" };

	const unsigned char* publicKeyBegin{}, * publicKeyEnd{};

	///////////////////////////////////////////////////////////////////////////////////////////////////////////

	std::atomic<int> m_queryStatus{ 0 };         //0 未连接    1 已连接未处理状态    2   已连接正在处理状态


	bool m_jumpNode{ false };                     //是否跳过本次请求的标志

	//是否开始执行检查
	bool m_checkTime{};

	//是否开始获取结果
	bool m_getResult{ false };


	//每条查询结果总string_view个数
	int m_everyCommandResultSum{};


	const unsigned int m_commandMaxSize{};        // 命令个数最大大小（长度）


	unsigned int m_commandTotalSize{};                //本次节点结果数总计


	unsigned int m_commandCurrentSize{};              //本次节点结果数当前计数

	//column_length 数组，直接分配足够大空间   
	// 每四位  第一位表示 column_length   第二位表示enumType   第三位表示flags    第四位表示decimals 
	std::unique_ptr<unsigned int[]>m_colLenArr{};

	const unsigned int m_colLenMax{ 1024 };

	///////////////////////////////////////////////////////////////////
	// 
	//待投递队列，未/等待拼凑消息的队列
	//使用开源无锁队列进一步提升qps ，这是github地址  https://github.com/cameron314/concurrentqueue
	moodycamel::ConcurrentQueue<std::shared_ptr<MYSQLResultTypeSW>>m_messageList;



	//待获取结果队列，已经发送请求的队列，处理时使用m_waitMessageList进行处理，减少使用m_messageList的锁的机会
	std::unique_ptr<std::shared_ptr<MYSQLResultTypeSW>[]>m_waitMessageList{};

	unsigned int m_waitMessageListMaxSize{};

	unsigned int m_waitMessageListNowSize{};


	//  根据m_waitMessageListBegin获取每次的RES
	std::shared_ptr<MYSQLResultTypeSW> *m_waitMessageListBegin{};

	std::shared_ptr<MYSQLResultTypeSW> *m_waitMessageListEnd{};

	//每次m_waitMessageList开始检查节点
	std::shared_ptr<MYSQLResultTypeSW>* m_waitMessageListStart{};

	///////////////////////////////////////////////////////////


	////////////////////////////////////////////////////////////////
	


	/////////////////////////////////////////////////////////


	// 能力标志位定义 (MySQL 8.0)
	static const unsigned int MYSQL_CLIENT_LONG_PASSWORD{ 1 };
	static const unsigned int MYSQL_CLIENT_FOUND_ROWS{ 2 };
	static const unsigned int MYSQL_CLIENT_LONG_FLAG{ 4 };
	static const unsigned int MYSQL_CLIENT_CONNECT_WITH_DB{ 8 };
	static const unsigned int MYSQL_CLIENT_PROTOCOL_41{ 512 };
	static const unsigned int MYSQL_CLIENT_PLUGIN_AUTH{ 0x80000 };
	static const unsigned int MYSQL_CLIENT_SECURE_CONNECTION{ 0x8000 };
	static const unsigned int MYSQL_CLIENT_MULTI_STATEMENTS{ (1UL << 16) };
	static const unsigned int MYSQL_CLIENT_MULTI_RESULTS{ (1UL << 17) };
	static const unsigned int MYSQL_CLIENT_PS_MULTI_RESULTS{ 0x00040000 };
	static const unsigned int MYSQL_CLIENT_LOCAL_FILES{ 0x00000080 };
	static const unsigned int MYSQL_CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA{ 0x00200000 };
	static const unsigned int utf8mb4_unicode_ci{ 224 };


	////////////////////////////////////////////////////

	

private:
	void connectMysql();

	//接收服务器握手包
	void recvHandShake();

	//解析握手包
	bool parseHandShake();
	
	//生成客户端登录握手包
	bool makeHandshakeResponse41();

	//发送客户端登录握手包
	void sendHandshakeResponse41();

	//解析每隔数据包前三位  获取数据长度
	unsigned int parseMessageLen(const unsigned char* messageIter);

	//获取认证结果接口1
	void recvAuthResult1();

	//解析认证结果接口1     0  解析出错    1 快速认证成功    2 需要完全认证
	int parseAuthResult1();

	//接收快速认证成功后跟随的OK包
	void recvAuthOKPacket();

	//请求获取服务器公钥
	void sendReqPubkey();

	// 接收服务器公钥数据包
	void recvPubkey();

	//提取公钥
	bool parsePubkey();

	//使用公钥加密密码生成认证包
	bool makePubkeyAuth();


	//发送公钥加密密码认证包
	void sendPubkeyAuth();

	//接收公钥加密密码认证结果
	void recvPubkeyAuth();

	//发送mysql查询命令
	void mysqlQuery();

	//接收mysql查询结果
	void recvMysqlResult();


	//解析mysql查询结果   0 解析协议出错    1  成功(全部解析处理完毕)   2  执行命令出错     3结果集未完毕，需要继续接收   
	int parseMysqlResult(const std::size_t bytes_transferred);


	//发送给mysql-server的命令拼装函数
	void readyMysqlMessage();
};


