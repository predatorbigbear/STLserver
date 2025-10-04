#include "verifyCode.h"



VERIFYCODE::VERIFYCODE(const std::shared_ptr<boost::asio::io_context>& ioc, const std::shared_ptr<ASYNCLOG>& log,
	const std::shared_ptr<STLTimeWheel>& timeWheel,
    const unsigned int listSize,
    const unsigned int codeLen, const unsigned int checkTime):
	m_ioc(ioc),m_log(log),m_timeWheel(timeWheel),m_codeLen(codeLen),m_checkTime(checkTime), m_messageList(listSize)
{
	m_ctx.set_default_verify_paths();      // 自动加载系统CA证书

    m_verifyCode.resize(m_codeLen + 11);   

    std::string str1{ "code=" }, str2{ "targets=" };

    m_codeIter = std::search(m_realCode.begin(), m_realCode.end(), str1.cbegin(), str1.cend());
    if (m_codeIter == m_realCode.end())
        throw std::runtime_error("realCode invaild");
    m_codeIter += str1.size();

    m_targetIter = std::search(m_codeIter, m_realCode.end(), str2.cbegin(), str2.cend());
    if (m_targetIter == m_realCode.end())
        throw std::runtime_error("target invaild");
    m_targetIter += str2.size();


    runVerifyCode();
    startCheck();
}








void VERIFYCODE::resetResolver()
{
	m_resolver.reset(new boost::asio::ip::tcp::resolver(*m_ioc));
}



void VERIFYCODE::resetSocket()
{
	m_socket.reset(new boost::asio::ssl::stream<boost::asio::ip::tcp::socket>(*m_ioc, m_ctx));
	m_socket->set_verify_mode(boost::asio::ssl::verify_peer);
	m_socket->set_verify_callback(
		std::bind(&VERIFYCODE::verify_certificate, this, std::placeholders::_1, std::placeholders::_2));

}



void VERIFYCODE::resetVerifyTime()
{
	m_verifyTime = 0;
}



bool VERIFYCODE::verify_certificate(bool preverified, boost::asio::ssl::verify_context& ctx)
{
    ++m_verifyTime;
    if (preverified)
    {
        char subject_name[256];
        static const std::string name1{ "/OU=GlobalSign ECC Root CA - R5/O=GlobalSign/CN=GlobalSign" }, cnName1{ "GlobalSign" };
        static const std::string name2{ "/C=BE/O=GlobalSign nv-sa/CN=GlobalSign ECC OV SSL CA 2018" }, cnName2{ "GlobalSign ECC OV SSL CA 2018" };
        X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
        X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);

        X509_NAME* subject = X509_get_subject_name(cert);
        char cn[256];
        X509_NAME_get_text_by_NID(subject, NID_commonName, cn, sizeof(cn));

        if (m_verifyTime == 1)
        {
            if (!std::equal(subject_name, subject_name + strlen(subject_name), name1.cbegin(), name1.cend()) ||
                !std::equal(cn, cn + strlen(cn), cnName1.cbegin(), cnName1.cend()))
            {
                preverified = false;
            }
        }
        else if (m_verifyTime == 2)
        {
            if (!std::equal(subject_name, subject_name + strlen(subject_name), name2.cbegin(), name2.cend()) ||
                !std::equal(cn, cn + strlen(cn), cnName2.cbegin(), cnName2.cend()))
            {
                preverified = false;
            }
        }
        //CN验证是旧式方法，现代证书应优先使用SAN扩展
        //对于IP地址验证，需额外检查GEN_IPADD类型的SAN或CN字段格式
        
    }


    return preverified;
}




void VERIFYCODE::resetTimer()
{
    m_timer.reset(new boost::asio::steady_timer(*m_ioc));

}




void VERIFYCODE::startCheck()
{
    m_timer->expires_after(std::chrono::seconds(m_checkTime));
    m_timer->async_wait([this](const boost::system::error_code& err)
    {
        if (err)
        {
            m_log->writeLog("VERIFYCODE::startCheck", err.value(), err.message());

        }
        else
        {
            //发送一次伪验证码
            insertVerifyCode<int>(nullptr, 0, {});

            startCheck();
        }
    });
}




void VERIFYCODE::startResolver()
{
    m_resolver->async_resolve("push.spug.cc", "443", [this](const boost::system::error_code& err,
        const boost::asio::ip::tcp::resolver::results_type &endpoints)
    {
        if (!err)
        {
            startConnection(endpoints);
        }
        else
        {
            if (m_reConnect)
                m_timeWheel->insert([this]() {startResolver(); }, 5);
            m_log->writeLog("VERIFYCODE::startResolver", err.value(), err.message());
        }
    });
}





void VERIFYCODE::startConnection(const boost::asio::ip::tcp::resolver::results_type& endpoints)
{
    boost::asio::async_connect(m_socket->lowest_layer(), endpoints,
        [this](const boost::system::error_code& err,
            const boost::asio::ip::tcp::endpoint& endpoint)
    {
        if (!err)
        {
            handshake();
        }
        else
        {
            if (m_reConnect)
                m_timeWheel->insert([this]() {startResolver(); }, 5);
            m_log->writeLog("VERIFYCODE::startConnection", err.value(), err.message());
        }
    });
}



void VERIFYCODE::handshake()
{
    m_socket->async_handshake(boost::asio::ssl::stream_base::client,
        [this](const boost::system::error_code& err)
    {
        if (!err)
        {
            sendFirstVerifyCode();
        }
        else
        {
            if (m_reConnect)
                sslShutdown();
            m_log->writeLog("VERIFYCODE::handshake", err.value(), err.message());
        }
    });
}



void VERIFYCODE::sendVerifyCode(const std::string& request, bool isRealCode)
{
    //记录下本次发送内容是否为真验证码
    //如果发送失败，则启动回收socket重连
    //重连完毕后，检查该标志
    //如果为true，则进行补发并检查无锁队列进行补发
    //如果为false，则检查无锁队列进行补发
    m_isRealCode = isRealCode;
    boost::asio::async_write(*m_socket,
        boost::asio::buffer(request),
        [this](const boost::system::error_code& error, std::size_t length)
    {
        if (!error)
        {
            checkList();
        }
        else
        {
            
            m_queryStatus.store(0, std::memory_order_release);
            //进入回收socket 并重连阶段
            sslShutdown();
   
            m_timeWheel->insert([this]()
            {
                ec = {};
                m_socket->lowest_layer().shutdown(boost::asio::socket_base::shutdown_both, ec);
                m_timeWheel->insert([this]() {shutdownLoop(); }, 5);
            },10);
        }
    });
}



void VERIFYCODE::sendFirstVerifyCode()
{
    boost::asio::async_write(*m_socket,
        boost::asio::buffer(m_forgedCode),
        [this](const boost::system::error_code& err, std::size_t length)
    {
        if (!err)
        {
            if (!m_reConnect)
            {
                std::cout << "verifyCode start\n";
                checkList();
            }
            else
            {
                m_reConnect = false;

                //重连后，检查m_isRealCode判断是否需要重发上次发送失败的真验证码
                if (m_isRealCode)
                    sendVerifyCode(m_realCode, true);
                else
                    checkList();
            }
            m_log->writeLog("verifyCode start success");
        }
        else
        {
            if (m_reConnect)
                sslShutdown();
            m_log->writeLog("VERIFYCODE::sendFirstVerifyCode", err.value(), err.message());
        }
    });
}




void VERIFYCODE::makeVerifyCode(const char* codeBegin, const char* codeEnd, const char* phoneBegin, const char* phoneEnd)
{
    std::copy(codeBegin, codeEnd, m_codeIter);
    std::copy(phoneBegin, phoneEnd, m_targetIter);
}



void VERIFYCODE::makeVerifyCode(const std::string& str)
{
    std::copy(str.cbegin(), str.cbegin() + m_codeLen, m_codeIter);
    std::copy(str.cbegin() + m_codeLen, str.cend(), m_targetIter);
}



void VERIFYCODE::checkList()
{
    if (!m_messageList.try_dequeue(m_message))
    {
        m_queryStatus.store(1, std::memory_order_release);
        return;
    }

    if (m_message->second)
    {
        makeVerifyCode(m_message->first);

        sendVerifyCode(m_realCode, true);
    }
    else
    {
        sendVerifyCode(m_forgedCode, false);
    }
}


void VERIFYCODE::runVerifyCode()
{
    resetQueryStatus();
    resetVerifyTime();
    resetResolver();
    resetSocket();
    resetTimer();
    startResolver();
}



void VERIFYCODE::resetQueryStatus()
{
    m_queryStatus.store(0, std::memory_order_release);
}




void VERIFYCODE::resetReConnect()
{
    m_reConnect = true;
}



void VERIFYCODE::sslShutdown()
{
    m_socket->async_shutdown([this](const boost::system::error_code& err)
    {
        //async_shutdown调用之后，无论成功与否，都可以进行下面的处理
        m_log->writeLog("VERIFYCODE::sslShutdown",err.value(),err.message());

       
    });

}




void VERIFYCODE::shutdownLoop()
{
    if (ec.value() != 107 && ec.value())
    {
        m_log->writeLog("VERIFYCODE::shutdownLoop", ec.value(), ec.message());
        m_socket->lowest_layer().shutdown(boost::asio::socket_base::shutdown_both, ec);
        m_timeWheel->insert([this]() {shutdownLoop(); }, 5);
    }
    else
    {
        m_socket->lowest_layer().cancel(ec);
        //等待异步cancel完成
        m_timeWheel->insert([this]() {cancelLoop(); }, 5);
    }
}




void VERIFYCODE::cancelLoop()
{
    if (ec.value() != 107 && ec.value())
    {
        m_log->writeLog("VERIFYCODE::cancelLoop", ec.value(), ec.message());
        m_socket->lowest_layer().cancel(ec);
        m_timeWheel->insert([this]() {cancelLoop(); }, 5);
    }
    else
    {
        m_socket->lowest_layer().close(ec);
        //等待异步cancel完成
        m_timeWheel->insert([this]() {closeLoop(); }, 5);
    }
}



void VERIFYCODE::closeLoop()
{
    if (ec.value() != 107 && ec.value())
    {
        m_log->writeLog("VERIFYCODE::closeLoop", ec.value(), ec.message());
        m_socket->lowest_layer().close(ec);
        m_timeWheel->insert([this]() {closeLoop(); }, 5);
    }
    else
    {
        tryConnect();
    }
}




void VERIFYCODE::tryConnect()
{
    resetQueryStatus();
    resetReConnect();
    resetVerifyTime();
    resetResolver();
    resetSocket();
    startResolver();

}
