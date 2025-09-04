#pragma once



#include "ASYNCLOG.h"
#include "STLTimeWheel.h"
#include "concurrentqueue.h"
#include "commonStruct.h"

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/ssl.hpp>

#include<string>
#include<algorithm>
#include<functional>
#include<numeric>
#include<memory>
#include<atomic>
#include<string_view>
#include<type_traits>





//短信验证码发送模块
//基于spug推送对手
//在设置keep-alive状态下，超时有效期为2分钟
//2025年6月起，‌任何发送验证码的企业均需通过实名认证‌，且需严格按政策要求提交资质文件并确保信息真实有效
//该模块仅供参考原理，真正企业级开发需要使用企业认证过的短信api接口
struct VERIFYCODE
{
	//无锁队列容量大小
	//验证码有效长度
	//伪验证码发送时间间隔
	VERIFYCODE(const std::shared_ptr<boost::asio::io_context> &ioc,
		const std::shared_ptr<ASYNCLOG> &log,
		const std::shared_ptr<STLTimeWheel> &timeWheel,
		const unsigned int listSize = 1024,
		const unsigned int codeLen = 6,
		const unsigned int checkTime = 118);      //构造函数


	//phone在应用层调用REGEXFUNCTION::isVaildPhone进行校验
	template<typename T = REALVERIFYCODE>
	bool insertVerifyCode(const char* verifyCode, const unsigned int verifyCodeLen, const std::string_view phone);
	


private:
	std::string m_realCode{"POST /send/nbONk8gyB6j34gXG HTTP/1.1\r\nHost:push.spug.cc\r\nContent-Type:application/x-www-form-urlencoded\r\nConnection:keep-alive\r\nContent-Length:41\r\n\r\ncode=BWR2aG&number=60&targets=18612345678"};
	//真验证码模板

	std::string::iterator m_codeIter{};

	std::string::iterator m_targetIter{};

	std::string m_forgedCode{"POST /send/nbONk8gyB6j34gXG HTTP/1.1\r\nHost:push.spug.cc\r\nContent-Type:application/x-www-form-urlencoded\r\nConnection:keep-alive\r\nContent-Length:43\r\n\r\ncode=BWR2aG12&number=60&targets=11012345678"};                        
	//伪验证码模板

	std::string m_verifyCode{};

	const std::shared_ptr<ASYNCLOG>m_log{};                                            //日志模块

	const unsigned int m_checkTime{};                                                 //伪验证码发送时间间隔

	const unsigned int m_codeLen{};                                                   //验证码有效长度

	std::unique_ptr<boost::asio::steady_timer>m_timer{};                               //定时器

	const std::shared_ptr<boost::asio::io_context>m_ioc{};

	std::unique_ptr<boost::asio::ip::tcp::resolver>m_resolver{};

	std::unique_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>m_socket{};         

	boost::asio::ssl::context m_ctx{ boost::asio::ssl::context::tlsv12_client };

	const std::shared_ptr<STLTimeWheel> m_timeWheel{};

	boost::system::error_code m_ec{};

	std::atomic<int> m_queryStatus{ 0 };                   //是否在发送状态中    0 未连接   1  已连接不在发送状态   2  已连接在发送状态

	unsigned int m_verifyTime{};

	//待投递队列，未/等待拼凑消息的队列
	//使用开源无锁队列进一步提升qps ，这是github地址  https://github.com/cameron314/concurrentqueue

	//X位验证码+11位手机号  是否是真验证码  true 真验证码   false   伪验证码
	//伪验证码用于保活http连接
	moodycamel::ConcurrentQueue<std::shared_ptr<std::pair<std::string, bool>>>m_messageList;


	std::shared_ptr<std::pair<std::string, bool>> m_message{};

	bool m_isRealCode{};

	bool m_reConnect{false};

	boost::system::error_code ec;

private:

	void resetResolver();


	void resetSocket();


	void resetVerifyTime();


	bool verify_certificate(bool preverified,
		boost::asio::ssl::verify_context& ctx);


	void resetTimer();


	void startCheck();


	void startResolver();


	void startConnection(const boost::asio::ip::tcp::resolver::results_type& endpoints);


	void handshake();


	void sendVerifyCode(const std::string& request, bool isRealCode);


	void sendFirstVerifyCode();


	//将insertVerifyCode的消息copy到真验证码模板中使用
	void makeVerifyCode(const char* codeBegin, const char* codeEnd, const char* phoneBegin, const char* phoneEnd);


	//将从无锁队列中取出的消息copy到真验证码模板中使用
	void makeVerifyCode(const std::string &str);


	//检查无锁队列是否为空
	void checkList();


	//启动服务
	void runVerifyCode();


	void resetQueryStatus();


	void resetReConnect();


	////////////////////////////////

	void sslShutdown();


	void shutdownLoop();


	void cancelLoop();


	void closeLoop();


	void tryConnect();
};



template<typename T>
inline bool VERIFYCODE::insertVerifyCode(const char* verifyCode, const unsigned int verifyCodeLen, const std::string_view phone)
{
	if constexpr (std::is_same_v<T, REALVERIFYCODE>)
	{
		if (!verifyCode || verifyCodeLen != m_codeLen)
			return false;
	}

	int status = m_queryStatus.load();
	if (!status)
		return false;

	else if (status == 1)
	{
		m_queryStatus.store(2);

		if constexpr (std::is_same_v<T, REALVERIFYCODE>)
		{
			makeVerifyCode(verifyCode, verifyCode + verifyCodeLen, phone.cbegin(), phone.cend());

			sendVerifyCode(m_realCode, true);
		}
		else
		{
			sendVerifyCode(m_forgedCode, false);
		}

		return true;
	}
	else
	{
		try
		{
			if constexpr (std::is_same_v<T, REALVERIFYCODE>)
			{

				if (!m_messageList.try_enqueue(std::make_shared<std::pair<std::string, bool>>(std::string(verifyCode, verifyCode + verifyCodeLen).append(phone), true)))
					return false;
			}
			else
			{
				if (!m_messageList.try_enqueue(std::make_shared<std::pair<std::string, bool>>(m_verifyCode, false)))
					return false;
			}
		}
		catch (std::exception& e)
		{
			return false;
		}

		return true;
	}

	return false;
}
