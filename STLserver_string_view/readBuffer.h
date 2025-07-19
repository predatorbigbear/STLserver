#pragma once



#include <boost/asio.hpp>



#include "MyReqView.h"
#include<memory>
#include<iostream>


struct ReadBuffer
{
	//初始化时从外部传入buffer分配空间大小
	ReadBuffer(std::shared_ptr<boost::asio::io_context> ioc, const unsigned int bufNum);

	char* getBuffer();

	size_t& getHasRead();

	MYREQVIEW& getView();

	std::shared_ptr<boost::asio::ip::tcp::socket>& getSock();

	boost::system::error_code& getErrCode();

	void setBodyLen(const int len);

	int getBodyLen();

	void setBodyFrontLen(const int len);

	int getBodyFrontLen();

	const char **bodyPara();

	size_t getBodyParaLen();

	void setFileName(const char *ch, const size_t len);

	std::string_view fileName();

private:
	std::shared_ptr<boost::asio::ip::tcp::socket> m_sock{};
	std::unique_ptr<char[]> m_readBuffer{};
	size_t m_bodyLen{};
	size_t m_bodyFrontLen{};
	size_t m_hasReadLen{};
	size_t m_bodyParaLen{100};
	std::unique_ptr<const char*[]>m_bodyPara{ new const char*[m_bodyParaLen] };

	std::string_view m_fileName;
	
	boost::system::error_code m_err{};

	MYREQVIEW reqView;
};


