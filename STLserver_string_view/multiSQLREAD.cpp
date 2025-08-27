#include "multiSQLREAD.h"

MULTISQLREAD::MULTISQLREAD(const std::shared_ptr<boost::asio::io_context>& ioc, const std::shared_ptr<std::function<void()>>& unlockFun,
	const std::shared_ptr<STLTimeWheel>& timeWheel, const std::shared_ptr<ASYNCLOG>& log, 
	const std::string& SQLHOST, const std::string& SQLUSER, const std::string& SQLPASSWORD,
	const std::string& SQLDB, const unsigned int& SQLPORT, 
	const unsigned int commandMaxSize, bool& success, const unsigned int bufferSize):m_ioc(ioc),m_unlockFun(unlockFun),
	m_timeWheel(timeWheel),m_log(log), m_host(SQLHOST), m_user(SQLUSER), m_passwd(SQLPASSWORD), m_db(SQLDB),
	m_port(SQLPORT), m_commandMaxSize(commandMaxSize), m_msgBufMaxSize(bufferSize), m_success(&success)
{
	//在外层进行数据检查

	m_msgBuf.reset(new unsigned char[m_msgBufMaxSize]);

	m_endPoint.reset(new boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(m_host), m_port));

	m_sock.reset(new boost::asio::ip::tcp::socket(*m_ioc));

	connectMysql();
}



void MULTISQLREAD::connectMysql()
{
	m_sock->async_connect(*m_endPoint, [this](const boost::system::error_code& err)
	{
		if (err)
		{
			*m_success = false;
			(*m_unlockFun)();
		}
		else
		{
			//开始接收mysql 握手包
			m_readLen = 0;
			recvHandShake();
		}
	});

}



void MULTISQLREAD::recvHandShake()
{
	m_sock->async_read_some(boost::asio::buffer(m_msgBuf.get() + m_readLen, m_msgBufMaxSize - m_readLen),
		[this](const boost::system::error_code& err, const std::size_t bytes_transferred)
	{
		if (err)
		{
			*m_success = false;
			(*m_unlockFun)();
		}
		else
		{
			m_readLen += bytes_transferred;
			//解析握手包
			if (m_readLen < 4)
				return recvHandShake();

			//解析本次数据包消息长度
			m_messageLen = static_cast<unsigned int>(*(m_msgBuf.get())) + static_cast<unsigned int>(*(m_msgBuf.get() + 1)) * 256 +
				static_cast<unsigned int>(*(m_msgBuf.get() + 2)) * 65536;

			if (m_readLen < m_messageLen + 4)
				return recvHandShake();

			//解析本次数据包消息序列号
			m_seqID = static_cast<unsigned int>(*(m_msgBuf.get() + 3));

            if (!parseHandShake())
            {
                *m_success = false;
                (*m_unlockFun)();
            }
            else
            {
                //组装握手认证包
                //可以参考
                // https://dev.mysql.com/doc/dev/mysql-server/latest/page_protocol_connection_phase_packets_protocol_handshake_response.html
                if (!makeHandshakeResponse41())
                {
                    *m_success = false;
                    (*m_unlockFun)();
                }
                else
                {
                    sendHandshakeResponse41();
                }
            }
		}
	});
}



bool MULTISQLREAD::parseHandShake()
{
	strBegin = m_msgBuf.get() + 4;
	strEnd = strBegin + m_messageLen;

    // 握手包参数参考 https://dev.mysql.com/doc/dev/mysql-server/latest/page_protocol_connection_phase_packets_protocol_handshake_v10.html
    unsigned int protocolVersion{};

    const unsigned char* serverVersionBegin{}, * serverVersionEnd{};

    unsigned int threadID{};

    const unsigned char* autuPluginDataPart1Begin{}, * autuPluginDataPart1End{};

    int filler{};

    const unsigned char* capabilityFlags1Begin{}, * capabilityFlags1End{};

    unsigned int characterSet{};

    const unsigned char* statusFlagsBegin{}, * statusFlagsEnd{};

    const unsigned char* capabilityFlags2Begin{}, * capabilityFlags2End{};

    unsigned int capabilityFlags1{};
    unsigned int capabilityFlags2{};
    unsigned int capabilityFlags{};

    unsigned int authPluginDataLen{};
    unsigned int authPluginDataLeftLen{};

    const unsigned char* autuPluginDataPart2Begin{}, * autuPluginDataPart2End{};

    const unsigned char* authPluginNameBegin{}, * authPluginNameEnd{};


    authPluginDataLen = 0;
    protocolVersion = *strBegin;
    if (protocolVersion != 10)
        return false;


    serverVersionBegin = strBegin + 1;
    serverVersionEnd = std::find(serverVersionBegin, strEnd, 0);
    if (serverVersionEnd == strEnd)
        return false;

    if (std::distance(serverVersionEnd + 1, strEnd) < 4)
        return false;


    threadID = static_cast<unsigned int>(*(serverVersionEnd + 1)) + static_cast<unsigned int>(*(serverVersionEnd + 2)) * 256 +
        static_cast<unsigned int>(*(serverVersionEnd + 3)) * 65536 + static_cast<unsigned int>(*(serverVersionEnd + 4)) * 16777216;

    if (std::distance(serverVersionEnd + 5, strEnd) < 8)
        return false;

    autuPluginDataPart1Begin = serverVersionEnd + 5;
    autuPluginDataPart1End = autuPluginDataPart1Begin + 8;


    if (autuPluginDataPart1End == strEnd)
        return false;

    filler = *(autuPluginDataPart1End);

    if (filler)
        return false;

    if (std::distance(autuPluginDataPart1End + 1, strEnd) < 2)
        return false;

    capabilityFlags1Begin = autuPluginDataPart1End + 1;
    capabilityFlags1End = capabilityFlags1Begin + 2;


    if (capabilityFlags1End == strEnd)
        return false;

    characterSet = static_cast<unsigned int>(*capabilityFlags1End);


    if (std::distance(capabilityFlags1End + 1, strEnd) < 2)
        return false;

    statusFlagsBegin = capabilityFlags1End + 1;
    statusFlagsEnd = statusFlagsBegin + 2;


    if (std::distance(statusFlagsEnd, strEnd) < 2)
        return false;

    capabilityFlags2Begin = statusFlagsEnd;
    capabilityFlags2End = capabilityFlags2Begin + 2;


    capabilityFlags1 = static_cast<unsigned int>(*(capabilityFlags1Begin)) +
        static_cast<unsigned int>(*(capabilityFlags1Begin + 1)) * 256;

    capabilityFlags2 = static_cast<unsigned int>(*(capabilityFlags2Begin)) * 65536 +
        static_cast<unsigned int>(*(capabilityFlags2Begin + 1)) * 16777216;

    capabilityFlags = capabilityFlags1 + capabilityFlags2;


    if (capabilityFlags2End == strEnd)
        return false;

    if (capabilityFlags & CLIENT_PLUGIN_AUTH)
    {
        authPluginDataLen = static_cast<unsigned int>(*capabilityFlags2End);
    }
    else
    {
        if (*capabilityFlags2End)
            return false;
    }

    if (std::distance(capabilityFlags2End + 1, strEnd) < 10)
        return false;

    if (!std::all_of(capabilityFlags2End + 1, capabilityFlags2End + 11, std::bind(std::equal_to<>(), std::placeholders::_1, 0)))
        return false;

    if (!authPluginDataLen || authPluginDataLen < 8)
        authPluginDataLeftLen = 13;
    else
        authPluginDataLeftLen = std::max(static_cast<unsigned int>(13), authPluginDataLen - 8);

    if (std::distance(capabilityFlags2End + 11, strEnd) < authPluginDataLeftLen)
        return false;

    autuPluginDataPart2Begin = capabilityFlags2End + 11;
    autuPluginDataPart2End = autuPluginDataPart2Begin + authPluginDataLeftLen;


    if (capabilityFlags & CLIENT_PLUGIN_AUTH)
    {
        if (autuPluginDataPart2End == strEnd)
        {
            authPluginNameBegin = nullptr;
            authPluginNameEnd = nullptr;
            return false;
        }
        else
        {
            authPluginNameBegin = autuPluginDataPart2End;
            authPluginNameEnd = std::find(authPluginNameBegin, strEnd, 0);
            if(std::equal(authPluginNameBegin, authPluginNameEnd, plugin_name.cbegin(), plugin_name.cend()))
                return false;
        }
    }
    else
    {
        authPluginNameBegin = nullptr;
        authPluginNameEnd = nullptr;
        return false;
    }

    if (std::distance(autuPluginDataPart1Begin, autuPluginDataPart1End) +
        std::distance(autuPluginDataPart2Begin, autuPluginDataPart2End) != 21)
        return false;

	//保存服务器能力标志位
    serverCapabilityFlags = capabilityFlags;

	//保存认证插件数据前20位
    try
    {
        autuPluginData.assign(autuPluginDataPart1Begin, autuPluginDataPart1End);
        autuPluginData.append(autuPluginDataPart2Begin, autuPluginDataPart2End - 1);
    }
    catch (const std::exception& e)
    {
        return false;
	}


	return true;
}



bool MULTISQLREAD::makeHandshakeResponse41()
{
    handShakeLen = 4 + 4 + 1 + 23 + m_user.size() + 1 + 32 + 1 + m_db.size() + 1 + plugin_name.size() + 1 + 4;

    if (m_msgBufMaxSize < handShakeLen)
        return false;


    //状态标志参考
    //https://dev.mysql.com/doc/dev/mysql-server/latest/group__group__cs__capabilities__flags.html
    clientFlag = CLIENT_PROTOCOL_41 | CLIENT_LONG_PASSWORD | CLIENT_FOUND_ROWS |
        CLIENT_SECURE_CONNECTION | CLIENT_LOCAL_FILES |
        CLIENT_CONNECT_WITH_DB | CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA |
        CLIENT_MULTI_RESULTS |
        CLIENT_MULTI_STATEMENTS |
        CLIENT_PS_MULTI_RESULTS |
        CLIENT_PLUGIN_AUTH;

    clientBegin = m_msgBuf.get();
    clientIter = clientBegin + 4;

    *clientIter++ = clientFlag % 256;
    *clientIter++ = clientFlag / 256 % 256;
    *clientIter++ = clientFlag / 65536 % 256;
    *clientIter++ = clientFlag / 16777216;


    *clientIter++ = m_msgBufMaxSize % 256;
    *clientIter++ = m_msgBufMaxSize / 256 % 256;
    *clientIter++ = m_msgBufMaxSize / 65536 % 256;
    *clientIter++ = m_msgBufMaxSize / 16777216;

    //utf8mb4_unicode_ci 
    *clientIter++ = utf8mb4_unicode_ci;

    std::fill(clientIter, clientIter + 23, 0);
    clientIter += 23;

    std::copy(m_user.cbegin(), m_user.cend(), clientIter);
    clientIter += m_user.size();
    *clientIter++ = 0;


    unsigned char buf1[32]{};
    unsigned char buf2[52]{};
   
    //https://dev.mysql.com/doc/dev/mysql-server/latest/page_caching_sha2_authentication_exchanges.html
    // 第一轮：对原始密码进行SHA256哈希
    SHA256((unsigned char*)m_passwd.data(), m_passwd.size(), buf1);

    // 第二轮：对hash1再次进行SHA256哈希
    SHA256(buf1, 32, buf2);

    // 第三轮：SHA256(hash2 + challenge)
    std::copy(autuPluginData.cbegin(), autuPluginData.cend(), buf2 + 32);
    SHA256(buf2, 52, buf2);
    for (int i = 0; i < 32; i++)
        buf1[i] ^= buf2[i];


    *clientIter++ = 32;
    std::copy(buf1, buf1 + 32, clientIter);
    clientIter += 32;

    std::copy(m_db.cbegin(), m_db.cend(), clientIter);
    clientIter += m_db.size();
    *clientIter++ = 0;

    std::copy(plugin_name.cbegin(), plugin_name.cend(), clientIter);
    clientIter += plugin_name.size();
    *clientIter++ = 0;

    clientSendLen = std::distance(clientBegin, clientIter) - 4;

    *clientBegin = clientSendLen % 256;
    *(clientBegin + 1) = clientSendLen / 256 % 256;
    *(clientBegin + 2) = clientSendLen / 65536 % 256;
    *(clientBegin + 3) = ++m_seqID;

    clientSendLen += 4;

    return true;

}



void MULTISQLREAD::sendHandshakeResponse41()
{
    boost::asio::async_write(*m_sock, boost::asio::buffer(m_msgBuf.get(), clientSendLen), [this](const boost::system::error_code& err, const std::size_t size)
    {
        if (err)  
        {
            *m_success = false;
            (*m_unlockFun)();
        }
        else
        {
			//开始接收认证结果  判断快速认证成功或者需要进行完整认证
            //当重启服务器或者重启mysql或者没有登陆过mysql时，触发完整认证
            //当服务器运行期间登陆过mysql，则触发快速认证
        }
    });

}




