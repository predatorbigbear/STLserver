#pragma once


#include "memoryPool.h"


#include<memory>
#include<algorithm>
#include<iterator>
#include <charconv>
#include <system_error>
#include <type_traits>


//json拼装函数中
// putString  putBoolean  putNull  putNumber  putObject  putArray现已可用
//  void HTTPSERVICE::testMakeJson()  展示如何使用STLtreeFast拼凑json

namespace MAKEJSON
{
	static const char *comma{ "," };
	static const unsigned int commaLen{ strlen(comma) };


	static const char *doubleQuotation{ "\"" };
	static const unsigned int doubleQuotationLen{ strlen(doubleQuotation) };


	static const char *putStrMiddle{ "\":\"" };
	static const unsigned int putStrMiddleLen{ strlen(putStrMiddle) };


	static const char *pushBackMiddle1{ "\":" };
	static const unsigned int pushBackMiddle1Len{ strlen(pushBackMiddle1) };

	static const char *pushBackObject{ "\":{" };
	static const unsigned int pushBackObjectLen{ strlen(pushBackObject) };

	static const char *pushBackArray{ "\":[" };
	static const unsigned int pushBackArrayLen{ strlen(pushBackArray) };


	static const char *leftBracket{ "{" };
	static const unsigned int leftBracketLen{ strlen(leftBracket) };


	static const char *rightBracket{ "}" };
	static const unsigned int rightBracketLen{ strlen(rightBracket) };


	static const char *leftRight{ "[]" };
	static const unsigned int leftRightLen{ strlen(leftRight) };


	static const char *leftAngle{ "[" };
	static const unsigned int leftAngleLen{ strlen(leftAngle) };


	static const char *rightAngle{ "]" };
	static const unsigned int rightAngleLen{ strlen(rightAngle) };


	static const char *httpFront{ "HTTP/" };
	static size_t httpFrontLen{ strlen(httpFront) };


	static const char *newLine{ "\r\n\r\n" };
	static size_t newLineLen{ strlen(newLine) };


	static const char *result{ "result" };
	static size_t resultLen{ strlen(result) };

	static const char *halfNewLine{ "\r\n" };
	static size_t halfNewLineLen{ strlen(halfNewLine) };


	static const char *space{ " " };
	static unsigned int spaceLen{ strlen(space) };


	static const char *colon{ ":" };
	static unsigned int colonLen{ strlen(colon) };

	static const char *ContentLength{ "Content-length" };
	static unsigned int ContentLengthLen{ strlen(ContentLength) };


	static const char *ContentType{ "Content-type" };
	static unsigned int ContentTypeLen{ strlen(ContentType) };

	static const char* applicationJson{ "application/json" };
	static unsigned int applicationJsonLen{ strlen(applicationJson) };


	static const char *httpOneZero{ "1.0" };
	static unsigned int httpOneZerolen{ strlen(httpOneZero) };

	static const char *httpOneOne{ "1.1" };
	static unsigned int httpOneOneLen{ strlen(httpOneOne) };

	static const char *http200{ "200" };
	static unsigned int http200Len{ strlen(http200) };


	


	static const char *httpOK{ "OK" };
	static unsigned int httpOKLen{ strlen(httpOK) };

	static const char *AccessControlAllowOrigin{ "Access-Control-Allow-Origin" };
	static unsigned int AccessControlAllowOriginLen{ strlen(AccessControlAllowOrigin) };


	static const char *CacheControl{ "Cache-Control" };
	static unsigned int CacheControlLen{ strlen(CacheControl) };


	static const char *maxAge{ "max_age" };
	static unsigned int maxAgeLen{ strlen(maxAge) };


	static const char *publicStr{ "public" };
	static unsigned int publicStrLen{ strlen(publicStr) };


	static const char *httpStar{ "*" };
	static unsigned int httpStarLen{ strlen(httpStar) };


	static const char *SetCookie{ "Set-Cookie" };
	static unsigned int SetCookieLen{ strlen(SetCookie) };


	static const char *Connection{ "Connection" };
	static unsigned int ConnectionLen{ strlen(Connection) };

	static const char* keepAlive{ "keep-alive" };
	static unsigned int keepAliveLen{ strlen(keepAlive) };

	static const char* httpClose{ "close" };
	static unsigned int httpCloseLen{ strlen(httpClose) };


	static const char* booleanTrue{ "true" };
	static unsigned int booleanTrueLen{ strlen(booleanTrue) };

	static const char* booleanFalse{ "false" };
	static unsigned int booleanFalseLen{ strlen(booleanFalse) };

	static const char* jsonNull{ "null" };
	static unsigned int jsonNullLen{ strlen(jsonNull) };

	
};


struct TRANSFORMTYPE
{

};


struct CUSTOMTAG
{

};



struct STLtreeFast
{
	//需要完善，因为分配可能不成功
	STLtreeFast(MEMORYPOOL<> *memoryPool):m_memoryPool(memoryPool)
	{
		if (!resize())
			std::cout << "init fail\n";
	}


	STLtreeFast& operator=(const STLtreeFast& other)
	{
		m_ptr = other.m_ptr;
		m_pos = other.m_pos;
		m_maxSize = other.m_maxSize;
		m_empty = other.m_empty;


		m_transformPtr = other.m_transformPtr;
		m_transformPos = other.m_transformPos;
		m_maxTransformSize = other.m_maxTransformSize;
		m_transformEmpty = other.m_transformEmpty;


		m_strSize = other.m_strSize;
		m_resize= other.m_strSize;

		index = other.index;

		m_memoryPool = other.m_memoryPool;

		return *this;
	}


	//
	//清空内部指向状态
	bool clear()
	{
		if (!m_ptr)
		{
			m_ptr = m_memoryPool->getMemory<const char**>(m_resize);
			if (!m_ptr)
				return false;
			m_maxSize = m_resize;
		}
		
		if (!m_transformPtr)
		{
			m_transformPtr = m_memoryPool->getMemory<const char**>(m_resize);
			if (!m_transformPtr)
				return false;
			m_maxTransformSize = m_resize;
		}

		m_strSize = 0;
		m_pos = m_transformPos = 0;
		index = -1;
		m_empty = m_transformEmpty = true;

		return true;
	}


	bool resize()
	{
		if (!m_ptr)
		{
			m_ptr = m_memoryPool->getMemory<const char**>(m_resize);
			if (!m_ptr)
				return false;
			m_maxSize = m_resize;
		}

		if (!m_transformPtr)
		{
			m_transformPtr = m_memoryPool->getMemory<const char**>(m_resize);
			if (!m_transformPtr)
				return false;
			m_maxTransformSize = m_resize;
		}

		m_strSize = 0;
		m_pos = 0;
		m_transformPos = 0;
		index = -1;
		m_empty = m_transformEmpty = true;

		return true;
	}


	void reset()
	{
		m_ptr = nullptr;
		m_pos = m_maxSize = 0;
		m_empty = true;

		m_transformPtr = nullptr;
		m_transformPos = m_maxTransformSize = 0;
		m_transformEmpty = true;


		m_strSize = 0;
		index = -1;
	}



	template<typename T = void>
	bool put(const char *begin, const char *end)
	{
		int len{ ((begin && end) ? end - begin : 0) };
		if (!len)
			return false;
		const int thisSize{ 8 };
		if (m_maxSize - m_pos < thisSize)
		{
			const char **ch{};
			if (m_maxSize * 2 - m_pos > thisSize)
			{
				ch = m_memoryPool->getMemory<const char**>(m_maxSize * 2);
				if (!ch)
					return false;
				m_maxSize *= 2;
			}
			else
			{
				ch = m_memoryPool->getMemory<const char**>(m_maxSize + thisSize * 2);
				if (!ch)
					return false;
				m_maxSize += thisSize * 2;
			}
			std::copy(m_ptr, m_ptr + m_pos, ch);
			m_ptr = ch;
		}

		if constexpr (std::is_same<T, TRANSFORMTYPE>::value)
		{
			if (m_maxTransformSize == m_transformPos)
			{
				const char **ch{};
				ch = m_memoryPool->getMemory<const char**>(m_maxTransformSize * 2);
				if (!ch)
					return false;
				m_maxTransformSize *= 2;

				std::copy(m_transformPtr, m_transformPtr + m_transformPos, ch);
				m_transformPtr = ch;
			}

			*(m_transformPtr + m_transformPos++) = begin;
			m_transformEmpty = false;
			len *= 6;
		}

		const char **ptr{ m_ptr + m_pos };
		const char **ptrBegin{ ptr };
		if (++index)
		{
			*ptr++ = MAKEJSON::comma;
			*ptr++ = MAKEJSON::comma + MAKEJSON::commaLen;

			++m_strSize;
		}
		*ptr++ = MAKEJSON::doubleQuotation;
		*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;

		*ptr++ = begin;
		*ptr++ = end;

		*ptr++ = MAKEJSON::doubleQuotation;
		*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;

		m_strSize += 2 + len;
		m_pos += std::distance(ptrBegin, ptr);
		m_empty = false;
		return true;
	}



	template<typename T = void>
	bool putraw(const char *begin, const char *end)
	{
		if (begin && end)
		{
			int len{ ((begin && end) ? end - begin : 0) };
			if (!len)
				return false;
			const int thisSize{ 4 };
			if (m_maxSize - m_pos < thisSize)
			{
				const char **ch{};
				if (m_maxSize * 2 - m_pos > thisSize)
				{
					ch = m_memoryPool->getMemory<const char**>(m_maxSize * 2);
					if (!ch)
						return false;
					m_maxSize *= 2;
				}
				else
				{
					ch = m_memoryPool->getMemory<const char**>(m_maxSize + thisSize * 2);
					if (!ch)
						return false;
					m_maxSize += thisSize * 2;
				}
				std::copy(m_ptr, m_ptr + m_pos, ch);
				m_ptr = ch;
			}

			if constexpr (std::is_same<T, TRANSFORMTYPE>::value)
			{
				if (m_maxTransformSize == m_transformPos)
				{
					const char **ch{};
					ch = m_memoryPool->getMemory<const char**>(m_maxTransformSize * 2);
					if (!ch)
						return false;
					m_maxTransformSize *= 2;

					std::copy(m_transformPtr, m_transformPtr + m_transformPos, ch);
					m_transformPtr = ch;
				}

				*(m_transformPtr + m_transformPos++) = begin;
				m_transformEmpty = false;
				len *= 6;
			}

			const char **ptr{ m_ptr + m_pos };
			const char **ptrBegin{ ptr };

			if (++index)
			{
				*ptr++ = MAKEJSON::comma;
				*ptr++ = MAKEJSON::comma + MAKEJSON::commaLen;
				++m_strSize;
			}
			*ptr++ = begin;
			*ptr++ = end;
			m_strSize += len;
			m_pos += std::distance(ptrBegin, ptr);
			m_empty = false;
		}
		return true;
	}



	template<typename T = void>
	bool putrawStr(const char *begin, const char *end)
	{
		int len{ ((begin && end) ? end - begin : 0) };
		if (!len)
			return false;
		const int thisSize{ 8 };
		if (m_maxSize - m_pos < thisSize)
		{
			const char **ch{};
			if (m_maxSize * 2 - m_pos > thisSize)
			{
				ch = m_memoryPool->getMemory<const char**>(m_maxSize * 2);
				if (!ch)
					return false;
				m_maxSize *= 2;
			}
			else
			{
				ch = m_memoryPool->getMemory<const char**>(m_maxSize + thisSize * 2);
				if (!ch)
					return false;
				m_maxSize += thisSize * 2;
			}
			std::copy(m_ptr, m_ptr + m_pos, ch);
			m_ptr = ch;
		}

		if constexpr (std::is_same<T, TRANSFORMTYPE>::value)
		{
			if (m_maxTransformSize == m_transformPos)
			{
				const char **ch{};
				ch = m_memoryPool->getMemory<const char**>(m_maxTransformSize * 2);
				if (!ch)
					return false;
				m_maxTransformSize *= 2;

				std::copy(m_transformPtr, m_transformPtr + m_transformPos, ch);
				m_transformPtr = ch;
			}

			*(m_transformPtr + m_transformPos++) = begin;
			m_transformEmpty = false;
			len *= 6;
		}

		const char **ptr{ m_ptr + m_pos };
		const char **ptrBegin{ ptr };

		if (++index)
		{
			*ptr++ = MAKEJSON::comma;
			*ptr++ = MAKEJSON::comma + MAKEJSON::commaLen;
			++m_strSize;
		}
		*ptr++ = MAKEJSON::doubleQuotation;
		*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;

		*ptr++ = begin;
		*ptr++ = end;

		*ptr++ = MAKEJSON::doubleQuotation;
		*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;
		m_strSize += 2 + len;

		m_pos += std::distance(ptrBegin, ptr);
		m_empty = false;
		return true;
	}



	//确定不存在转换字符的情况下不需要管默认参数T，否则填入TRANSFORMTYPE
	//默认STRTYPE用于value为字符的情况，对于数字，布尔，null可以设置STRTYPE为非void
	//"left":"right"
	//put函数拆开处理几种数据类型比较好
	//当插入值类型的时候，仅当len1 或len2 不为空时才有必要检查是否需要转换
	template<typename T = void,typename STRTYPE =void>
	bool put(const char * str1Begin, const char * str1End, const char * str2Begin, const char * str2End)
	{
		int len1{ ((str1End && str1Begin) ? str1End - str1Begin : 0) }, len2{ ((str2End && str2Begin) ? str2End - str2Begin : 0) };
		int thisSize{ 6 };   //  12 " 1 ":" 2 ",
		if (len1)
			thisSize += 4;
		if(len2)
			thisSize += 2;

		if (m_maxSize - m_pos < thisSize)
		{
			const char **ch{};
			if (m_maxSize * 2 - m_pos > thisSize)
			{
				ch = m_memoryPool->getMemory<const char**>(m_maxSize * 2);
				if (!ch)
					return false;
				m_maxSize *= 2;
			}
			else
			{
				ch = m_memoryPool->getMemory<const char**>(m_maxSize + thisSize * 2);
				if (!ch)
					return false;
				m_maxSize += thisSize * 2;
			}

			std::copy(m_ptr, m_ptr + m_pos, ch);
			m_ptr = ch;
		}


		if constexpr (std::is_same<T, TRANSFORMTYPE>::value && std::is_same<STRTYPE, void>::value)
		{
			//仅当len1 或len2 不为空时才有必要检查是否需要转换
			if (len1 || len2)
			{
				m_transformEmpty = false;

				if (m_maxTransformSize < (m_transformPos + 2))
				{
					const char **ch{};
					ch = m_memoryPool->getMemory<const char**>(m_maxTransformSize * 2);
					if (!ch)
						return false;
					m_maxTransformSize *= 2;

					std::copy(m_transformPtr, m_transformPtr + m_transformPos, ch);
					m_transformPtr = ch;
				}

				if (len1)
				{
					*(m_transformPtr + m_transformPos++) = str1Begin;
					len1 *= 6;
				}
				if (len2)
				{
					*(m_transformPtr + m_transformPos++) = str2Begin;
					len2 *= 6;
				}
			}
		}

		const char **ptr{ m_ptr + m_pos };
		const char **ptrBegin{ ptr };
		if (++index)
		{
			*ptr++ = MAKEJSON::comma;
			*ptr++ = MAKEJSON::comma + MAKEJSON::commaLen;

			++m_strSize;
		}

		if (len1)
		{
			*ptr++ = MAKEJSON::doubleQuotation;
			*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;

			*ptr++ = str1Begin;
			*ptr++ = str1End;
		}

		if (std::is_same<STRTYPE, void>::value)
		{
			if (len1)
			{
				*ptr++ = MAKEJSON::putStrMiddle;
				*ptr++ = MAKEJSON::putStrMiddle + MAKEJSON::putStrMiddleLen;
			}
			else
			{
				*ptr++ = MAKEJSON::doubleQuotation;
				*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;
			}
		}
		else
		{
			if (len1)
			{
				*ptr++ = MAKEJSON::pushBackMiddle1;
				*ptr++ = MAKEJSON::pushBackMiddle1 + MAKEJSON::pushBackMiddle1Len;
			}
		}


		if (len2)
		{
			*ptr++ = str2Begin;
			*ptr++ = str2End;
		}

		if (std::is_same<STRTYPE, void>::value)
		{
			*ptr++ = MAKEJSON::doubleQuotation;
			*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;
		}

		if (std::is_same<STRTYPE, void>::value)
		{
			m_strSize += 2 + len1 + len2;
		}
		else
		{
			m_strSize += len1 + len2;
		}
		if (len1)
			m_strSize += 3;
		
		m_pos += std::distance(ptrBegin, ptr);
		m_empty = false;
		return true;
	}



	//插入字符串
	//T为TRANSFORMTYPE说明需要进行转码处理
	//当插入值类型的时候，仅当len1 或len2 不为空时才有必要检查是否需要转换
	template<typename T = void>
	bool putString(const char* str1Begin, const char* str1End, const char* str2Begin, const char* str2End)
	{
		int len1{ ((str1End && str1Begin) ? str1End - str1Begin : 0) }, len2{ ((str2End && str2Begin) ? str2End - str2Begin : 0) };
		// 6的意思是右侧value的\" \" ,三处的指针存储空间
		int thisSize{ 6 };   //  12 " 1 ":" 2 ",
		// 若len1非空，则需存储 \"  左侧key值 \":\"  三处的指针存储空间
		if (len1)
			thisSize += 4;
		// 若len2非空,则需存储右侧value值的指针存储空间
		if (len2)
			thisSize += 2;

		//如果存储指针串最大长度 减去 目前已存储长度小于本次所需要存储指针的长度则进行扩容处理
		if (m_maxSize - m_pos < thisSize)
		{
			const char** ch{};
			if (m_maxSize * 2 - m_pos > thisSize)
			{
				ch = m_memoryPool->getMemory<const char**>(m_maxSize * 2);
				if (!ch)
					return false;
				m_maxSize *= 2;
			}
			else
			{
				ch = m_memoryPool->getMemory<const char**>(m_maxSize + thisSize * 2);
				if (!ch)
					return false;
				m_maxSize += thisSize * 2;
			}

			std::copy(m_ptr, m_ptr + m_pos, ch);
			m_ptr = ch;
		}

		//如果需要转换，则检测len1或len2是否为非空进行处理
		if constexpr (std::is_same<T, TRANSFORMTYPE>::value)
		{
			if (len1 || len2)
			{
				//设置转换标志
				m_transformEmpty = false;

				//2为len1 和len2首字符地址
				if (m_maxTransformSize < (m_transformPos + 2))
				{
					const char** ch{};
					ch = m_memoryPool->getMemory<const char**>(m_maxTransformSize * 2);
					if (!ch)
						return false;
					m_maxTransformSize *= 2;

					std::copy(m_transformPtr, m_transformPtr + m_transformPos, ch);
					m_transformPtr = ch;
				}

				//json最长的转换处理单字符转换后会变成6倍，故此处直接*6，用空间换时间快速处理
				if (len1)
				{
					*(m_transformPtr + m_transformPos++) = str1Begin;
					len1 *= 6;
				}
				if (len2)
				{
					*(m_transformPtr + m_transformPos++) = str2Begin;
					len2 *= 6;
				}
			}
		}

		//储存json指针操作
		const char** ptr{ m_ptr + m_pos };
		const char** ptrBegin{ ptr };
		if (++index)
		{
			*ptr++ = MAKEJSON::comma;
			*ptr++ = MAKEJSON::comma + MAKEJSON::commaLen;

			++m_strSize;
		}

		if (len1)
		{
			*ptr++ = MAKEJSON::doubleQuotation;
			*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;

			*ptr++ = str1Begin;
			*ptr++ = str1End;
		}

		if (len1)
		{
			*ptr++ = MAKEJSON::putStrMiddle;
			*ptr++ = MAKEJSON::putStrMiddle + MAKEJSON::putStrMiddleLen;
		}
		else
		{
			*ptr++ = MAKEJSON::doubleQuotation;
			*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;
		}


		if (len2)
		{
			*ptr++ = str2Begin;
			*ptr++ = str2End;
		}

		*ptr++ = MAKEJSON::doubleQuotation;
		*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;

		//右侧\" \"长度为2  
		m_strSize += 2 + len1 + len2;
		//如果len1非空，则需要加上左侧key值的 \"  \":三个字符长度
		if (len1)
			m_strSize += 3;

		//累加指针存储长度
		m_pos += std::distance(ptrBegin, ptr);
		m_empty = false;
		return true;
	}




	//插入布尔类型
	//T为TRANSFORMTYPE说明需要进行转码处理
	//当插入值类型的时候，仅当len1 不为空时才有必要检查是否需要转换
	template<typename T = void>
	bool putBoolean(const char* str1Begin, const char* str1End, const bool booleanValue)
	{
		int len1{ ((str1End && str1Begin) ? str1End - str1Begin : 0) }, len2{ booleanValue? MAKEJSON::booleanTrueLen: MAKEJSON::booleanFalseLen };
		int thisSize{ 4 };   //  12 " 1 ": 2 ,
		if (len1)
			thisSize += 6;

		if (m_maxSize - m_pos < thisSize)
		{
			const char** ch{};
			if (m_maxSize * 2 - m_pos > thisSize)
			{
				ch = m_memoryPool->getMemory<const char**>(m_maxSize * 2);
				if (!ch)
					return false;
				m_maxSize *= 2;
			}
			else
			{
				ch = m_memoryPool->getMemory<const char**>(m_maxSize + thisSize * 2);
				if (!ch)
					return false;
				m_maxSize += thisSize * 2;
			}

			std::copy(m_ptr, m_ptr + m_pos, ch);
			m_ptr = ch;
		}

		if constexpr (std::is_same<T, TRANSFORMTYPE>::value)
		{
			if (len1)
			{
				m_transformEmpty = false;
				if (m_maxTransformSize < (m_transformPos + 1))
				{
					const char** ch{};
					ch = m_memoryPool->getMemory<const char**>(m_maxTransformSize * 2);
					if (!ch)
						return false;
					m_maxTransformSize *= 2;

					std::copy(m_transformPtr, m_transformPtr + m_transformPos, ch);
					m_transformPtr = ch;
				}

				*(m_transformPtr + m_transformPos++) = str1Begin;
				len1 *= 6;
			}
		}

		const char** ptr{ m_ptr + m_pos };
		const char** ptrBegin{ ptr };
		if (++index)
		{
			*ptr++ = MAKEJSON::comma;
			*ptr++ = MAKEJSON::comma + MAKEJSON::commaLen;

			++m_strSize;
		}

		if (len1)
		{
			*ptr++ = MAKEJSON::doubleQuotation;
			*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;

			*ptr++ = str1Begin;
			*ptr++ = str1End;

			*ptr++ = MAKEJSON::pushBackMiddle1;
			*ptr++ = MAKEJSON::pushBackMiddle1 + MAKEJSON::pushBackMiddle1Len;
		}


		if (booleanValue)
		{
			*ptr++ = MAKEJSON::booleanTrue;
			*ptr++ = MAKEJSON::booleanTrue + MAKEJSON::booleanTrueLen;
		}
		else
		{
			*ptr++ = MAKEJSON::booleanFalse;
			*ptr++ = MAKEJSON::booleanFalse + MAKEJSON::booleanFalseLen;
		}

		

		m_strSize += len1 + len2;
		if (len1)
			m_strSize += 3;

		m_pos += std::distance(ptrBegin, ptr);
		m_empty = false;
		return true;
	}



	//插入NULL
	//T为TRANSFORMTYPE说明需要进行转码处理
	//当插入值类型的时候，仅当len1 不为空时才有必要检查是否需要转换
	template<typename T = void>
	bool putNull(const char* str1Begin, const char* str1End)
	{
		int len1{ ((str1End && str1Begin) ? str1End - str1Begin : 0) }, len2{ MAKEJSON::jsonNullLen };
		int thisSize{ 4 };   //  12 " 1 ": 2 ,
		if (len1)
			thisSize += 6;

		if (m_maxSize - m_pos < thisSize)
		{
			const char** ch{};
			if (m_maxSize * 2 - m_pos > thisSize)
			{
				ch = m_memoryPool->getMemory<const char**>(m_maxSize * 2);
				if (!ch)
					return false;
				m_maxSize *= 2;
			}
			else
			{
				ch = m_memoryPool->getMemory<const char**>(m_maxSize + thisSize * 2);
				if (!ch)
					return false;
				m_maxSize += thisSize * 2;
			}

			std::copy(m_ptr, m_ptr + m_pos, ch);
			m_ptr = ch;
		}

		if constexpr (std::is_same<T, TRANSFORMTYPE>::value)
		{
			if (len1)
			{
				m_transformEmpty = false;

				if (m_maxTransformSize < (m_transformPos + 1))
				{
					const char** ch{};
					ch = m_memoryPool->getMemory<const char**>(m_maxTransformSize * 2);
					if (!ch)
						return false;
					m_maxTransformSize *= 2;

					std::copy(m_transformPtr, m_transformPtr + m_transformPos, ch);
					m_transformPtr = ch;
				}

				*(m_transformPtr + m_transformPos++) = str1Begin;
				len1 *= 6;
			}
		}

		const char** ptr{ m_ptr + m_pos };
		const char** ptrBegin{ ptr };
		if (++index)
		{
			*ptr++ = MAKEJSON::comma;
			*ptr++ = MAKEJSON::comma + MAKEJSON::commaLen;

			++m_strSize;
		}

		if (len1)
		{
			*ptr++ = MAKEJSON::doubleQuotation;
			*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;

			*ptr++ = str1Begin;
			*ptr++ = str1End;

			*ptr++ = MAKEJSON::pushBackMiddle1;
			*ptr++ = MAKEJSON::pushBackMiddle1 + MAKEJSON::pushBackMiddle1Len;
		}


		*ptr++ = MAKEJSON::jsonNull;
		*ptr++ = MAKEJSON::jsonNull + MAKEJSON::jsonNullLen;


		m_strSize += len1 + len2;
		if (len1)
			m_strSize += 3;

		m_pos += std::distance(ptrBegin, ptr);
		m_empty = false;
		return true;
	}


	//插入数字类型，参数为整形或浮点型则实时转换
	//T为TRANSFORMTYPE说明需要进行转码处理
	//当插入值类型的时候，仅当len1 不为空时才有必要检查是否需要转换
	template<typename T = void,typename NUMBERTYPE ,
		typename = std::enable_if_t<std::is_integral<NUMBERTYPE>::value || std::is_floating_point<NUMBERTYPE>::value>>
		bool putNumber(const char* str1Begin, const char* str1End, NUMBERTYPE&& number)
	{
		static constexpr int numberMaxLen{ 25 };
		char *numberBuffer{ m_memoryPool->getMemory<char*>(numberMaxLen) };
		if (!numberBuffer)
			return false;

		
		auto [numPtr, ec] { std::to_chars(numberBuffer, numberBuffer + numberMaxLen, std::forward<NUMBERTYPE>(number)) };
		if (ec != std::errc())
			return false;
		int len1{ (str1End && str1Begin ? str1End - str1Begin : 0) }, len2{ std::distance(numberBuffer,numPtr) };
		int thisSize{ 4 };
		if (len1)
			thisSize += 6;

		if (m_maxSize - m_pos < thisSize)
		{
			const char** ch{};
			if (m_maxSize * 2 - m_pos > thisSize)
			{
				ch = m_memoryPool->getMemory<const char**>(m_maxSize * 2);
				if (!ch)
					return false;
				m_maxSize *= 2;
			}
			else
			{
				ch = m_memoryPool->getMemory<const char**>(m_maxSize + thisSize * 2);
				if (!ch)
					return false;
				m_maxSize += thisSize * 2;
			}

			std::copy(m_ptr, m_ptr + m_pos, ch);
			m_ptr = ch;
		}

		if constexpr (std::is_same<T, TRANSFORMTYPE>::value)
		{
			if (len1)
			{
				m_transformEmpty = false;

				if (m_maxTransformSize < (m_transformPos + 1))
				{
					const char** ch{};
					ch = m_memoryPool->getMemory<const char**>(m_maxTransformSize * 2);
					if (!ch)
						return false;
					m_maxTransformSize *= 2;

					std::copy(m_transformPtr, m_transformPtr + m_transformPos, ch);
					m_transformPtr = ch;
				}

				*(m_transformPtr + m_transformPos++) = str1Begin;
				len1 *= 6;
			}
		}

		const char** ptr{ m_ptr + m_pos };
		const char** ptrBegin{ ptr };
		if (++index)
		{
			*ptr++ = MAKEJSON::comma;
			*ptr++ = MAKEJSON::comma + MAKEJSON::commaLen;

			++m_strSize;
		}

		if (len1)
		{
			*ptr++ = MAKEJSON::doubleQuotation;
			*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;

			*ptr++ = str1Begin;
			*ptr++ = str1End;

			*ptr++ = MAKEJSON::pushBackMiddle1;
			*ptr++ = MAKEJSON::pushBackMiddle1 + MAKEJSON::pushBackMiddle1Len;
		}

		*ptr++ = numberBuffer;
		*ptr++ = numberBuffer + len2;

		m_strSize += len1 + len2;
		if (len1)
			m_strSize += 3;

		m_pos += std::distance(ptrBegin, ptr);
		m_empty = false;
		return true;
		
	}




	//插入数字类型，参数为字符串类型（用户需自己保证为数字字符串）
	//T为TRANSFORMTYPE说明需要进行转码处理
	//当插入值类型的时候，仅当len1 不为空时才有必要检查是否需要转换
	template<typename T = void>
		bool putNumber(const char* str1Begin, const char* str1End, const char* str2Begin, const char* str2End)
	{
		int len1{ (str1End && str1Begin ? str1End - str1Begin : 0) }, len2{ (str2End && str2Begin ? str2End - str2Begin : 0) };
		int thisSize{ 4 };
		if (len1)
			thisSize += 6;

		if (m_maxSize - m_pos < thisSize)
		{
			const char** ch{};
			if (m_maxSize * 2 - m_pos > thisSize)
			{
				ch = m_memoryPool->getMemory<const char**>(m_maxSize * 2);
				if (!ch)
					return false;
				m_maxSize *= 2;
			}
			else
			{
				ch = m_memoryPool->getMemory<const char**>(m_maxSize + thisSize * 2);
				if (!ch)
					return false;
				m_maxSize += thisSize * 2;
			}

			std::copy(m_ptr, m_ptr + m_pos, ch);
			m_ptr = ch;
		}

		if constexpr (std::is_same<T, TRANSFORMTYPE>::value)
		{
			if (len1)
			{
				m_transformEmpty = false;

				if (m_maxTransformSize < (m_transformPos + 1))
				{
					const char** ch{};
					ch = m_memoryPool->getMemory<const char**>(m_maxTransformSize * 2);
					if (!ch)
						return false;
					m_maxTransformSize *= 2;

					std::copy(m_transformPtr, m_transformPtr + m_transformPos, ch);
					m_transformPtr = ch;
				}

				*(m_transformPtr + m_transformPos++) = str1Begin;
				len1 *= 6;
			}
		}

		const char** ptr{ m_ptr + m_pos };
		const char** ptrBegin{ ptr };
		if (++index)
		{
			*ptr++ = MAKEJSON::comma;
			*ptr++ = MAKEJSON::comma + MAKEJSON::commaLen;

			++m_strSize;
		}

		if (len1)
		{
			*ptr++ = MAKEJSON::doubleQuotation;
			*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;

			*ptr++ = str1Begin;
			*ptr++ = str1End;

			*ptr++ = MAKEJSON::pushBackMiddle1;
			*ptr++ = MAKEJSON::pushBackMiddle1 + MAKEJSON::pushBackMiddle1Len;
		}

		*ptr++ = str2Begin;
		*ptr++ = str2End;

		m_strSize += len1 + len2;
		if (len1)
			m_strSize += 3;

		m_pos += std::distance(ptrBegin, ptr);
		m_empty = false;
		return true;
	}




	//插入对象
	//T为TRANSFORMTYPE说明需要进行转码处理
	template<typename T = void>
	bool putObject(const char * begin, const char * end, const STLtreeFast &other)
	{
		int len1{ (end && begin ? end - begin : 0) }, len2{ static_cast<int>(other.m_strSize) };
		int thisSize{ 6 + static_cast<int>(other.m_pos) };   //  12 " 1 ":{ 2 },
		if (len1)
			thisSize += 6;

		if (m_maxSize - m_pos < thisSize)
		{
			const char **ch{};
			if (m_maxSize * 2 - m_pos > thisSize)
			{
				ch = m_memoryPool->getMemory<const char**>(m_maxSize * 2);
				if (!ch)
					return false;
				m_maxSize *= 2;
			}
			else
			{
				ch = m_memoryPool->getMemory<const char**>(m_maxSize + thisSize * 2);
				if (!ch)
					return false;
				m_maxSize += thisSize * 2;
			}
			std::copy(m_ptr, m_ptr + m_pos, ch);
			m_ptr = ch;
		}

		if constexpr (std::is_same<T, TRANSFORMTYPE>::value)
		{
			if (!other.isTransformEmpty() || len1)
			{
				m_transformEmpty = false;
				if (!other.isTransformEmpty())
				{
					if (m_maxTransformSize < (m_transformPos + other.m_transformPos + 1))
					{
						const char **ch{};
						ch = m_memoryPool->getMemory<const char**>(m_maxTransformSize * 2 + other.m_transformPos);
						if (!ch)
							return false;
						m_maxTransformSize *= 2;
						m_maxTransformSize += other.m_transformPos;


						std::copy(m_transformPtr, m_transformPtr + m_transformPos, ch);
						m_transformPtr = ch;
					}

					if (len1)
					{
						*(m_transformPtr + m_transformPos++) = begin;
						len1 *= 6;
					}
					std::copy(other.m_transformPtr, other.m_transformPtr + other.m_transformPos, m_transformPtr + m_transformPos);
					m_transformPos += other.m_transformPos;
				}
				else
				{
					if (m_maxTransformSize == m_transformPos)
					{
						const char **ch{};
						ch = m_memoryPool->getMemory<const char**>(m_maxTransformSize * 2);
						if (!ch)
							return false;
						m_maxTransformSize *= 2;

						std::copy(m_transformPtr, m_transformPtr + m_transformPos, ch);
						m_transformPtr = ch;
					}

					*(m_transformPtr + m_transformPos++) = begin;
					len1 *= 6;
				}
			}
		}

		const char **ptr{ m_ptr + m_pos };
		const char **ptrBegin{ ptr };

		if (++index)
		{
			*ptr++ = MAKEJSON::comma;
			*ptr++ = MAKEJSON::comma + MAKEJSON::commaLen;
			++m_strSize;
		}

		if (len1)
		{
			*ptr++ = MAKEJSON::doubleQuotation;
			*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;

			*ptr++ = begin;
			*ptr++ = end;
		}

		if (len1)
		{
			*ptr++ = MAKEJSON::pushBackObject;
			*ptr++ = MAKEJSON::pushBackObject + MAKEJSON::pushBackObjectLen;
		}
		else
		{
			*ptr++ = MAKEJSON::leftBracket;
			*ptr++ = MAKEJSON::leftBracket + MAKEJSON::leftBracketLen;
		}

		if (!other.isEmpty())
		{
			std::copy(other.m_ptr, other.m_ptr + other.m_pos, ptr);
			ptr += other.m_pos;
		}

		*ptr++ = MAKEJSON::rightBracket;
		*ptr++ = MAKEJSON::rightBracket + MAKEJSON::rightBracketLen;

		m_strSize += 2 + len1 + len2;
		if (len1)
			m_strSize += 3;

		m_pos += std::distance(ptrBegin, ptr);
		m_empty = false;
		return true;
	}



	//插入数组
	//T为TRANSFORMTYPE说明需要进行转码处理
	template<typename T = void>
	bool putArray(const char * begin, const char * end, const STLtreeFast &other)
	{
		int len1{ (end && begin ? end - begin : 0) }, len2{ static_cast<int>(other.m_strSize) };
		int thisSize{ 6 + static_cast<int>(other.m_pos) };   //  12 " 1 ":[ 2 ],
		if (len1)
			thisSize += 4;
	
		if (m_maxSize - m_pos < thisSize)
		{
			const char **ch{};
			if (m_maxSize * 2 - m_pos > thisSize)
			{
				ch = m_memoryPool->getMemory<const char**>(m_maxSize * 2);
				if (!ch)
					return false;
				m_maxSize *= 2;
			}
			else
			{
				ch = m_memoryPool->getMemory<const char**>(m_maxSize + thisSize * 2);
				if (!ch)
					return false;
				m_maxSize += thisSize * 2;
			}
			std::copy(m_ptr, m_ptr + m_pos, ch);
			m_ptr = ch;
		}

		if constexpr (std::is_same<T, TRANSFORMTYPE>::value)
		{
			if (!other.isTransformEmpty() || len1)
			{
				m_transformEmpty = false;
				if (!other.isTransformEmpty())
				{
					if (m_maxTransformSize < (m_transformPos + other.m_transformPos + 1))
					{
						const char **ch{};
						ch = m_memoryPool->getMemory<const char**>(m_maxTransformSize * 2 + other.m_transformPos);
						if (!ch)
							return false;
						m_maxTransformSize *= 2;
						m_maxTransformSize += other.m_transformPos;


						std::copy(m_transformPtr, m_transformPtr + m_transformPos, ch);
						m_transformPtr = ch;
					}

					if (len1)
					{
						*(m_transformPtr + m_transformPos++) = begin;
						len1 *= 6;
					}
					std::copy(other.m_transformPtr, other.m_transformPtr + other.m_transformPos, m_transformPtr + m_transformPos);
					m_transformPos += other.m_transformPos;
				}
				else
				{
					if (m_maxTransformSize == m_transformPos)
					{
						const char **ch{};
						ch = m_memoryPool->getMemory<const char**>(m_maxTransformSize * 2);
						if (!ch)
							return false;
						m_maxTransformSize *= 2;

						std::copy(m_transformPtr, m_transformPtr + m_transformPos, ch);
						m_transformPtr = ch;
					}

					*(m_transformPtr + m_transformPos++) = begin;
					len1 *= 6;
				}
			}
		}

		const char **ptr{ m_ptr + m_pos };
		const char **ptrBegin{ ptr };

		if (++index)
		{
			*ptr++ = MAKEJSON::comma;
			*ptr++ = MAKEJSON::comma + MAKEJSON::commaLen;
			++m_strSize;
		}

		if (len1)
		{
			*ptr++ = MAKEJSON::doubleQuotation;
			*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;

			*ptr++ = begin;
			*ptr++ = end;
		}

		if (len1)
		{
			*ptr++ = MAKEJSON::pushBackArray;
			*ptr++ = MAKEJSON::pushBackArray + MAKEJSON::pushBackArrayLen;
		}
		else
		{
			*ptr++ = MAKEJSON::leftAngle;
			*ptr++ = MAKEJSON::leftAngle + MAKEJSON::leftAngleLen;
		}

		if (!other.isEmpty())
		{
			std::copy(other.m_ptr, other.m_ptr + other.m_pos, ptr);
			ptr += other.m_pos;
		}

		
		*ptr++ = MAKEJSON::rightAngle;
		*ptr++ = MAKEJSON::rightAngle + MAKEJSON::rightAngleLen;

		m_strSize += 2 + len1 + len2;
		if (len1)
			m_strSize += 3;
		m_pos += std::distance(ptrBegin, ptr);
		m_empty = false;
		return true;
	}


	template<typename T = void>
	bool push_back(const STLtreeFast &other)
	{
		unsigned int len{ other.m_strSize };
		if (!len)
			return false;
		unsigned int thisSize{ 6 + other.m_pos };
		if (m_maxSize - m_pos < thisSize)
		{
			const char **ch{};
			if (m_maxSize * 2 - m_pos > thisSize)
			{
				ch = m_memoryPool->getMemory<const char**>(m_maxSize * 2);
				if (!ch)
					return false;
				m_maxSize *= 2;
			}
			else
			{
				ch = m_memoryPool->getMemory<const char**>(m_maxSize + thisSize * 2);
				if (!ch)
					return false;
				m_maxSize += thisSize * 2;
			}
			std::copy(m_ptr, m_ptr + m_pos, ch);
			m_ptr = ch;
		}

		if constexpr (std::is_same<T, TRANSFORMTYPE>::value)
		{
			if (!other.isTransformEmpty())
			{
				if (m_maxTransformSize < other.m_transformPos)
				{
					const char **ch{};
					ch = m_memoryPool->getMemory<const char**>(m_maxTransformSize * 2 + other.m_transformPos);
					if (!ch)
						return false;
					m_maxTransformSize *= 2;
					m_maxTransformSize += other.m_transformPos;


					std::copy(m_transformPtr, m_transformPtr + m_transformPos, ch);
					m_transformPtr = ch;
				}

				std::copy(other.m_transformPtr, other.m_transformPtr + other.m_transformPos, m_transformPtr + m_transformPos);
				m_transformPos += other.m_transformPos;
			}

			m_transformEmpty = false;
		}

		const char **ptr{ m_ptr + m_pos };
		const char **ptrBegin{ ptr };

		if (++index)
		{
			*ptr++ = MAKEJSON::comma;
			*ptr++ = MAKEJSON::comma + MAKEJSON::commaLen;
			++m_strSize;
		}

		if (other.isEmpty())
		{
			*ptr++ = MAKEJSON::doubleQuotation;
			*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;

			*ptr++ = MAKEJSON::doubleQuotation;
			*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;

		}
		else
		{
			*ptr++ = MAKEJSON::leftBracket;
			*ptr++ = MAKEJSON::leftBracket + MAKEJSON::leftBracketLen;


			std::copy(other.m_ptr, other.m_ptr + other.m_pos, ptr);
			ptr += other.m_pos;


			*ptr++ = MAKEJSON::rightBracket;
			*ptr++ = MAKEJSON::rightBracket + MAKEJSON::rightBracketLen;
		}

		m_strSize += 2 + len;
		m_pos += std::distance(ptrBegin, ptr);

		m_empty = false;
		return true;
	}



	template<typename T = void>
	bool put_child(const char * begin, const char * end, const STLtreeFast &other)
	{
		int len1{ ((begin && end) ? end - begin : 0) }, len2{ other.isEmpty() ? 0 : static_cast<int>(other.m_strSize) };
		if (!len1)
			return false;
		int thisSize{ 12 + static_cast<int>(other.m_pos) };
		if (m_maxSize - m_pos < thisSize)
		{
			const char** ch{};
			if (m_maxSize * 2 - m_pos > thisSize)
			{
				ch = m_memoryPool->getMemory<const char**>(m_maxSize * 2);
				if (!ch)
					return false;
				m_maxSize *= 2;
			}
			else
			{
				ch = m_memoryPool->getMemory<const char**>(m_maxSize + thisSize * 2);
				if (!ch)
					return false;
				m_maxSize += thisSize * 2;
			}
			std::copy(m_ptr, m_ptr + m_pos, ch);
			m_ptr = ch;
		}

		if constexpr (std::is_same<T, TRANSFORMTYPE>::value)
		{
			if (!other.isTransformEmpty())
			{
				if (m_maxTransformSize < (m_transformPos + other.m_transformPos))
				{
					const char** ch{};
					ch = m_memoryPool->getMemory<const char**>(m_maxTransformSize * 2 + other.m_transformPos);
					if (!ch)
						return false;
					m_maxTransformSize *= 2;
					m_maxTransformSize += other.m_transformPos;


					std::copy(m_transformPtr, m_transformPtr + m_transformPos, ch);
					m_transformPtr = ch;
				}


				*(m_transformPtr + m_transformPos++) = begin;
				std::copy(other.m_transformPtr, other.m_transformPtr + other.m_transformPos, m_transformPtr + m_transformPos);
				m_transformPos += other.m_transformPos;
			}
			else
			{
				if (m_maxTransformSize == m_transformPos)
				{
					const char** ch{};
					ch = m_memoryPool->getMemory<const char**>(m_maxTransformSize * 2);
					if (!ch)
						return false;
					m_maxTransformSize *= 2;

					std::copy(m_transformPtr, m_transformPtr + m_transformPos, ch);
					m_transformPtr = ch;
				}

				*(m_transformPtr + m_transformPos++) = begin;
			}

			m_transformEmpty = false;
			len1 *= 6;
		}

		const char** ptr{ m_ptr + m_pos };
		const char** ptrBegin{ ptr };

		if (++index)
		{
			*ptr++ = MAKEJSON::comma;
			*ptr++ = MAKEJSON::comma + MAKEJSON::commaLen;
			++m_strSize;
		}

		*ptr++ = MAKEJSON::doubleQuotation;
		*ptr++ = MAKEJSON::doubleQuotation + MAKEJSON::doubleQuotationLen;

		*ptr++ = begin;
		*ptr++ = end;

		*ptr++ = MAKEJSON::pushBackMiddle1;
		*ptr++ = MAKEJSON::pushBackMiddle1 + MAKEJSON::pushBackMiddle1Len;

		//m_strSize += MAKEJSON::doubleQuotationLen + MAKEJSON::pushBackMiddle1Len;   3

		if (other.isEmpty())
		{
			*ptr++ = MAKEJSON::leftRight;
			*ptr++ = MAKEJSON::leftRight + MAKEJSON::leftRightLen;
			//m_strSize+= MAKEJSON::leftRightLen;   2
		}
		else
		{
			*ptr++ = MAKEJSON::leftAngle;
			*ptr++ = MAKEJSON::leftAngle + MAKEJSON::leftAngleLen;

			std::copy(other.m_ptr, other.m_ptr + other.m_pos, ptr);
			ptr += other.m_pos;

			*ptr++ = MAKEJSON::rightAngle;
			*ptr++ = MAKEJSON::rightAngle + MAKEJSON::rightAngleLen;

			//m_strSize += MAKEJSON::leftAngleLen + MAKEJSON::rightAngleLen;  2
		}

		m_strSize += 5 + len1 + len2;
		m_pos += std::distance(ptrBegin, ptr);
		m_empty = false;
		return true;
	}



	// 模板参数解释
	// 是否需要考虑json转换
	// 是否存在自定义http header头部
	// 自定义http header头部操作函数类型

	//一条http消息回复 比如  HTTP/1.0 200 OK\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:长度\r\n\r\njson结构字符串
	//拆成三块看，第一块为HTTP到Content-Length:    第二部分为长度和\r\n\r\n   第三部分为json结构字符串
	//STLTreeFast在插入字符串时仅保存json指针串，并且将指定转码的指针存入m_transformPtr中，并存入预判所需的空间m_strSize
	//生成时，利用m_strSize计算出长度所需空间大小
	//那么整条HTTP回复所需预判空间为HTTP到Content-Length:所需空间  长度所需空间+\r\n\r\n所需空间  +json所需空间
	//一次性获取该大小的空间，跳到json空间起始位置根据指针串生成json，根据模板参数T的标志判断是否需要处理json转义
	//生成json字符串后，根据实际生产json串首尾位置获取真正所需的长度，计算长度所需空间大小
	//计算整条HTTP回复所需预判空间为HTTP到Content-Length:所需空间  长度所需空间+\r\n\r\n所需空间，从json起始位置往前跳生成整条http消息
	//返回http消息起始位置和长度用于发送
	//如果某些http header需要自定义办法写入，则设置HTTPFLAG，在httpWrite传入写入lambda ，类型为(char*&)   httpLen为自定义http header加内容长度
	//则会在检测HTTPFLAG类型后再写入长度之后，返回指针位置进行自定义写入
	//利用此算法一次性生成http json回复，对于常规的生成算法具有绝对性能优势
template<typename T = void, typename HTTPFLAG = void, typename HTTPFUNCTION = void*,  typename ...ARG>
	bool make_json(char *&resultPtr, unsigned int &resultLen, HTTPFUNCTION httpWrite, unsigned int httpLen , const char *httpVersionBegin, const char *httpVersionEnd,
		const char *httpCodeBegin, const char *httpCodeEnd, const char *httpResultBegin, const char *httpResultEnd, ARG&&...args)
	{
		if constexpr (!std::is_same<T, TRANSFORMTYPE>::value)
		{

			if (!m_pos)
				return false;

			int parSize{ sizeof...(args) }, httpHeadLen{};
			if (!httpVersionBegin || !httpVersionEnd || !httpCodeBegin || !httpCodeEnd || !httpResultBegin || !httpResultEnd || !calLength(httpHeadLen, args...))
				return false;

			int needLength{ MAKEJSON::httpFrontLen + std::distance(httpVersionBegin,httpVersionEnd) + MAKEJSON::spaceLen + std::distance(httpCodeBegin,httpCodeEnd)
				+ MAKEJSON::spaceLen + std::distance(httpResultBegin,httpResultEnd) + MAKEJSON::halfNewLineLen + httpHeadLen + MAKEJSON::ContentLengthLen + MAKEJSON::colonLen };

			if constexpr (std::is_same<HTTPFLAG, CUSTOMTAG>::value)
			{
				needLength += httpLen + MAKEJSON::halfNewLineLen;
			}


			resultPtr = nullptr, resultLen = 0;

			int thisJsonLen{ jsonLen() };


			unsigned int needFrontLen{ needLength + stringLen(thisJsonLen) + MAKEJSON::newLineLen };

			unsigned int needLen{ needFrontLen + thisJsonLen };     //93

			/////////////////////////////

			//////////////////////////////////////////////////


			char* newResultPtr{ m_memoryPool->getMemory<char*>(needLen) };

			if (!newResultPtr)
				return false;


			unsigned int i{}, pos{ m_pos };
			char* iterEnd{ newResultPtr + needFrontLen }, * iterBegin{ newResultPtr + needFrontLen };

			const char** begin{ m_ptr }, ** end{ m_ptr + pos };

			*iterEnd++ = '{';
			do
			{

				std::copy(*begin, *(begin + 1), iterEnd);
				iterEnd += std::distance(*begin, *(begin + 1));
				begin += 2;
			} while (begin != end);


			*iterEnd++ = '}';

			unsigned int jsonResultLen = std::distance(iterBegin, iterEnd), finalStringLen = stringLen(jsonResultLen);


			//iterBegin - finalStringLen -MAKEJSON::newLineLen -needLength
			resultPtr = newResultPtr = iterBegin - MAKEJSON::newLineLen - finalStringLen - needLength;
			resultLen = std::distance(newResultPtr, iterEnd);

			////////////////////////////////////////////////////////////////////////////////////////////////

			std::copy(MAKEJSON::httpFront, MAKEJSON::httpFront + MAKEJSON::httpFrontLen, newResultPtr);
			newResultPtr += MAKEJSON::httpFrontLen;


			std::copy(httpVersionBegin, httpVersionEnd, newResultPtr);
			newResultPtr += std::distance(httpVersionBegin, httpVersionEnd);


			std::copy(MAKEJSON::space, MAKEJSON::space + MAKEJSON::spaceLen, newResultPtr);
			newResultPtr += MAKEJSON::spaceLen;


			std::copy(httpCodeBegin, httpCodeEnd, newResultPtr);
			newResultPtr += std::distance(httpCodeBegin, httpCodeEnd);


			std::copy(MAKEJSON::space, MAKEJSON::space + MAKEJSON::spaceLen, newResultPtr);
			newResultPtr += MAKEJSON::spaceLen;


			std::copy(httpResultBegin, httpResultEnd, newResultPtr);
			newResultPtr += std::distance(httpResultBegin, httpResultEnd);


			std::copy(MAKEJSON::halfNewLine, MAKEJSON::halfNewLine + MAKEJSON::halfNewLineLen, newResultPtr);
			newResultPtr += MAKEJSON::halfNewLineLen;

			packAgeHttpHeader(newResultPtr, args...);

			///////////////////////////////////////////////////////////////////////////////////////////////////



			std::copy(MAKEJSON::ContentLength, MAKEJSON::ContentLength + MAKEJSON::ContentLengthLen, newResultPtr);
			newResultPtr += MAKEJSON::ContentLengthLen;


			std::copy(MAKEJSON::colon, MAKEJSON::colon + MAKEJSON::colonLen, newResultPtr);
			newResultPtr += MAKEJSON::colonLen;


			if (jsonResultLen > 99999999)
				*newResultPtr++ = jsonResultLen / 100000000 + '0';
			if (jsonResultLen > 9999999)
				*newResultPtr++ = jsonResultLen / 10000000 % 10 + '0';
			if (jsonResultLen > 999999)
				*newResultPtr++ = jsonResultLen / 1000000 % 10 + '0';
			if (jsonResultLen > 99999)
				*newResultPtr++ = jsonResultLen / 100000 % 10 + '0';
			if (jsonResultLen > 9999)
				*newResultPtr++ = jsonResultLen / 10000 % 10 + '0';
			if (jsonResultLen > 999)
				*newResultPtr++ = jsonResultLen / 1000 % 10 + '0';
			if (jsonResultLen > 99)
				*newResultPtr++ = jsonResultLen / 100 % 10 + '0';
			if (jsonResultLen > 9)
				*newResultPtr++ = jsonResultLen / 10 % 10 + '0';
			*newResultPtr++ = jsonResultLen % 10 + '0';


			if constexpr (std::is_same<HTTPFLAG, CUSTOMTAG>::value)
			{
				std::copy(MAKEJSON::halfNewLine, MAKEJSON::halfNewLine + MAKEJSON::halfNewLineLen, newResultPtr);
				newResultPtr += MAKEJSON::halfNewLineLen;
				httpWrite(newResultPtr);
			}


			std::copy(MAKEJSON::newLine, MAKEJSON::newLine + MAKEJSON::newLineLen, newResultPtr);

			return true;
		}

		else
		{


			if (!m_pos)
				return false;

			int parSize{ sizeof...(args) }, httpHeadLen{};
			if (!httpVersionBegin || !httpVersionEnd || !httpCodeBegin || !httpCodeEnd || !httpResultBegin || !httpResultEnd || !calLength(httpHeadLen, args...))
				return false;

			int needLength{ MAKEJSON::httpFrontLen + std::distance(httpVersionBegin,httpVersionEnd) + MAKEJSON::spaceLen + std::distance(httpCodeBegin,httpCodeEnd)
				+ MAKEJSON::spaceLen + std::distance(httpResultBegin,httpResultEnd) + MAKEJSON::halfNewLineLen + httpHeadLen + MAKEJSON::ContentLengthLen + MAKEJSON::colonLen };

			if constexpr (std::is_same<HTTPFLAG, CUSTOMTAG>::value)
			{
				needLength += httpLen + MAKEJSON::halfNewLineLen;
			}


			resultPtr = nullptr, resultLen = 0;

			int thisJsonLen{ jsonLen() };

			unsigned int needFrontLen{ needLength + stringLen(thisJsonLen) + MAKEJSON::newLineLen };

			unsigned int needLen{ needFrontLen + thisJsonLen };

			/////////////////////////////////////////////////////////////


			char* newResultPtr{ m_memoryPool->getMemory<char*>(needLen) };

			if (!newResultPtr)
				return false;


			unsigned int i{}, pos{ m_pos };
			unsigned char uchar{};
			char* iterEnd{ newResultPtr + needFrontLen }, * iterBegin{ newResultPtr + needFrontLen };

			const char** begin{ m_ptr }, ** end{ m_ptr + pos }, ** transformBegin{ m_transformPtr }, ** transformEnd{ m_transformPtr + m_transformPos };
			char* iterTempBegin{}, * iterTempEnd{};
			*iterEnd++ = '{';
			do
			{
				if (*begin != *transformBegin)
				{
					std::copy(*begin, *(begin + 1), iterEnd);
					iterEnd += std::distance(*begin, *(begin + 1));
				}
				else
				{
					iterTempBegin = const_cast<char*>(*begin), iterTempEnd = const_cast<char*>(*(begin + 1));
					while (iterTempBegin != iterTempEnd)
					{
						switch (*iterTempBegin)
						{
						case '\"':
							*iterEnd++ = '\\';
							*iterEnd++ = '\"';
							break;
						case '\\':
							*iterEnd++ = '\\';
							*iterEnd++ = '\\';
							break;
						case '/':
							*iterEnd++ = '\\';
							*iterEnd++ = '/';
							break;
						case '\b':
							*iterEnd++ = '\\';
							*iterEnd++ = 'b';
							break;
						case '\f':
							*iterEnd++ = '\\';
							*iterEnd++ = 'f';
							break;
						case '\n':
							*iterEnd++ = '\\';
							*iterEnd++ = 'n';
							break;
						case '\r':
							*iterEnd++ = '\\';
							*iterEnd++ = 'r';
							break;
						case '\t':
							*iterEnd++ = '\\';
							*iterEnd++ = 't';
							break;
						default:
							uchar = static_cast<unsigned char>(*iterTempBegin);
							if (uchar < 0x20)
							{
								*iterEnd++ = '\\';
								*iterEnd++ = 'u';
								*iterEnd++ = '0';
								*iterEnd++ = '0';
								*iterEnd++ = uchar / 16 + '0';
								*iterEnd++ = (uchar % 16) < 10 ? (uchar % 16 + '0') : (uchar % 16 - 10 + 'a');
							}
							else
							{
								*iterEnd++ = *iterTempBegin;
							}
							break;
						}
						++iterTempBegin;
					}
					if (++transformBegin == transformEnd)
					{
						begin += 2;
						break;
					}
				}
				begin += 2;
			} while (begin != end);

			while (begin != end)
			{
				std::copy(*begin, *(begin + 1), iterEnd);
				iterEnd += std::distance(*begin, *(begin + 1));
				begin += 2;
			}
			*iterEnd++ = '}';


			unsigned int jsonResultLen{ std::distance(iterBegin, iterEnd) };
			unsigned int finalStringLen{ stringLen(jsonResultLen) };


			//iterBegin - finalStringLen -MAKEJSON::newLineLen -needLength
			resultPtr = newResultPtr = iterBegin - MAKEJSON::newLineLen - finalStringLen - needLength;
			resultLen = std::distance(newResultPtr, iterEnd);


			std::copy(MAKEJSON::httpFront, MAKEJSON::httpFront + MAKEJSON::httpFrontLen, newResultPtr);
			newResultPtr += MAKEJSON::httpFrontLen;

			std::copy(httpVersionBegin, httpVersionEnd, newResultPtr);
			newResultPtr += std::distance(httpVersionBegin, httpVersionEnd);

			std::copy(MAKEJSON::space, MAKEJSON::space + MAKEJSON::spaceLen, newResultPtr);
			newResultPtr += MAKEJSON::spaceLen;

			std::copy(httpCodeBegin, httpCodeEnd, newResultPtr);
			newResultPtr += std::distance(httpCodeBegin, httpCodeEnd);

			std::copy(MAKEJSON::space, MAKEJSON::space + MAKEJSON::spaceLen, newResultPtr);
			newResultPtr += MAKEJSON::spaceLen;

			std::copy(httpResultBegin, httpResultEnd, newResultPtr);
			newResultPtr += std::distance(httpResultBegin, httpResultEnd);

			std::copy(MAKEJSON::halfNewLine, MAKEJSON::halfNewLine + MAKEJSON::halfNewLineLen, newResultPtr);
			newResultPtr += MAKEJSON::halfNewLineLen;

			packAgeHttpHeader(newResultPtr, args...);


			std::copy(MAKEJSON::ContentLength, MAKEJSON::ContentLength + MAKEJSON::ContentLengthLen, newResultPtr);
			newResultPtr += MAKEJSON::ContentLengthLen;

			std::copy(MAKEJSON::colon, MAKEJSON::colon + MAKEJSON::colonLen, newResultPtr);
			newResultPtr += MAKEJSON::colonLen;


			if (jsonResultLen > 99999999)
				*newResultPtr++ = jsonResultLen / 100000000 + '0';
			if (jsonResultLen > 9999999)
				*newResultPtr++ = jsonResultLen / 10000000 % 10 + '0';
			if (jsonResultLen > 999999)
				*newResultPtr++ = jsonResultLen / 1000000 % 10 + '0';
			if (jsonResultLen > 99999)
				*newResultPtr++ = jsonResultLen / 100000 % 10 + '0';
			if (jsonResultLen > 9999)
				*newResultPtr++ = jsonResultLen / 10000 % 10 + '0';
			if (jsonResultLen > 999)
				*newResultPtr++ = jsonResultLen / 1000 % 10 + '0';
			if (jsonResultLen > 99)
				*newResultPtr++ = jsonResultLen / 100 % 10 + '0';
			if (jsonResultLen > 9)
				*newResultPtr++ = jsonResultLen / 10 % 10 + '0';
			*newResultPtr++ = jsonResultLen % 10 + '0';

			if constexpr (std::is_same<HTTPFLAG, CUSTOMTAG>::value)
			{
				std::copy(MAKEJSON::halfNewLine, MAKEJSON::halfNewLine + MAKEJSON::halfNewLineLen, newResultPtr);
				newResultPtr += MAKEJSON::halfNewLineLen;
				httpWrite(newResultPtr);
			}

			std::copy(MAKEJSON::newLine, MAKEJSON::newLine + MAKEJSON::newLineLen, newResultPtr);

			return true;
		}
	}



	//一次性生成普通body http回复的函数
	template<typename HTTPFLAG = void, typename HTTPFUNCTION = void*, typename ...ARG>
	bool make_pingPongResppnse(char *&resultPtr, unsigned int &resultLen, HTTPFUNCTION httpWrite, unsigned int httpLen, const char *httpVersionBegin, const char *httpVersionEnd,
		const char *httpCodeBegin, const char *httpCodeEnd, const char *httpResultBegin, const char *httpResultEnd, const char *httpBodyBegin, const char *httpBodyEnd, ARG&&...args)
	{
			int parSize{ sizeof...(args) }, httpHeadLen{};
			if (!httpVersionBegin || !httpVersionEnd || !httpCodeBegin || !httpCodeEnd || !httpResultBegin || !httpResultEnd || !httpBodyBegin || !httpBodyEnd || !calLength(httpHeadLen, args...))
				return false;

			int needLength{ MAKEJSON::httpFrontLen + std::distance(httpVersionBegin,httpVersionEnd) + MAKEJSON::spaceLen + std::distance(httpCodeBegin,httpCodeEnd)
				+ MAKEJSON::spaceLen + std::distance(httpResultBegin,httpResultEnd) + MAKEJSON::halfNewLineLen + httpHeadLen + MAKEJSON::ContentLengthLen + MAKEJSON::colonLen };

			if constexpr (std::is_same<HTTPFLAG, CUSTOMTAG>::value)
			{
				needLength += httpLen + MAKEJSON::halfNewLineLen;
			}

			resultPtr = nullptr, resultLen = 0;

			int thisbodyLen{ std::distance(httpBodyBegin,httpBodyEnd) };

			unsigned int needFrontLen{ needLength + stringLen(thisbodyLen) + MAKEJSON::newLineLen };

			unsigned int needLen{ needFrontLen + thisbodyLen };     //93

			/////////////////////////////

			//////////////////////////////////////////////////


			char *newResultPtr{ m_memoryPool->getMemory<char*>(needLen) };

			if (!newResultPtr)
				return false;

			resultPtr = newResultPtr, resultLen = needLen;

			std::copy(MAKEJSON::httpFront, MAKEJSON::httpFront + MAKEJSON::httpFrontLen, newResultPtr);
			newResultPtr += MAKEJSON::httpFrontLen;


			std::copy(httpVersionBegin, httpVersionEnd, newResultPtr);
			newResultPtr += std::distance(httpVersionBegin, httpVersionEnd);


			std::copy(MAKEJSON::space, MAKEJSON::space + MAKEJSON::spaceLen, newResultPtr);
			newResultPtr += MAKEJSON::spaceLen;


			std::copy(httpCodeBegin, httpCodeEnd, newResultPtr);
			newResultPtr += std::distance(httpCodeBegin, httpCodeEnd);


			std::copy(MAKEJSON::space, MAKEJSON::space + MAKEJSON::spaceLen, newResultPtr);
			newResultPtr += MAKEJSON::spaceLen;


			std::copy(httpResultBegin, httpResultEnd, newResultPtr);
			newResultPtr += std::distance(httpResultBegin, httpResultEnd);


			std::copy(MAKEJSON::halfNewLine, MAKEJSON::halfNewLine + MAKEJSON::halfNewLineLen, newResultPtr);
			newResultPtr += MAKEJSON::halfNewLineLen;

			packAgeHttpHeader(newResultPtr, args...);

			std::copy(MAKEJSON::ContentLength, MAKEJSON::ContentLength + MAKEJSON::ContentLengthLen, newResultPtr);
			newResultPtr += MAKEJSON::ContentLengthLen;


			std::copy(MAKEJSON::colon, MAKEJSON::colon + MAKEJSON::colonLen, newResultPtr);
			newResultPtr += MAKEJSON::colonLen;


			if (thisbodyLen > 99999999)
				*newResultPtr++ = thisbodyLen / 100000000 + '0';
			if (thisbodyLen > 9999999)
				*newResultPtr++ = thisbodyLen / 10000000 % 10 + '0';
			if (thisbodyLen > 999999)
				*newResultPtr++ = thisbodyLen / 1000000 % 10 + '0';
			if (thisbodyLen > 99999)
				*newResultPtr++ = thisbodyLen / 100000 % 10 + '0';
			if (thisbodyLen > 9999)
				*newResultPtr++ = thisbodyLen / 10000 % 10 + '0';
			if (thisbodyLen > 999)
				*newResultPtr++ = thisbodyLen / 1000 % 10 + '0';
			if (thisbodyLen > 99)
				*newResultPtr++ = thisbodyLen / 100 % 10 + '0';
			if (thisbodyLen > 9)
				*newResultPtr++ = thisbodyLen / 10 % 10 + '0';
			*newResultPtr++ = thisbodyLen % 10 + '0';


			if constexpr (std::is_same<HTTPFLAG, CUSTOMTAG>::value)
			{
				std::copy(MAKEJSON::halfNewLine, MAKEJSON::halfNewLine + MAKEJSON::halfNewLineLen, newResultPtr);
				newResultPtr += MAKEJSON::halfNewLineLen;
				httpWrite(newResultPtr);
			}

			std::copy(MAKEJSON::newLine, MAKEJSON::newLine + MAKEJSON::newLineLen, newResultPtr);
			newResultPtr += MAKEJSON::newLineLen;

			std::copy(httpBodyBegin, httpBodyEnd, newResultPtr);
			newResultPtr += thisbodyLen;

			return true;
	}


	const unsigned int jsonLen()
	{
		return m_strSize + 2;
	}



	const bool isEmpty()const
	{
		return m_empty;
	}


	const bool isTransformEmpty()const
	{
		return m_transformEmpty;
	}


	private:
		template<typename HEADERTYPE>
		bool calLength(int &httpLength, HEADERTYPE headerBegin, HEADERTYPE headerEnd, HEADERTYPE wordBegin, HEADERTYPE wordEnd)
		{
			if constexpr (std::is_same<HEADERTYPE,const char*>::value)
			{
				if (!headerBegin || !headerEnd || !wordBegin || !wordEnd)
					return false; 
				httpLength += std::distance(headerBegin, headerEnd) + 3 + std::distance(wordBegin, wordEnd);
				return true;
			}
			return false;
		}



		template<typename HEADERTYPE, typename ...ARG>
		bool calLength(int &httpLength, HEADERTYPE headerBegin, HEADERTYPE headerEnd, HEADERTYPE wordBegin, HEADERTYPE wordEnd, ARG&& ... args)
		{
			if constexpr (std::is_same<HEADERTYPE, const char*>::value)
			{
				if (!headerBegin || !headerEnd || !wordBegin || !wordEnd)
					return false;
				httpLength += std::distance(headerBegin, headerEnd) + 3 + std::distance(wordBegin, wordEnd);
				return calLength(httpLength, args...);
			}
			return false;
		}



		template<typename HEADERTYPE>
		void packAgeHttpHeader(char *&httpPointer, HEADERTYPE headerBegin, HEADERTYPE headerEnd, HEADERTYPE wordBegin, HEADERTYPE wordEnd)
		{
			if constexpr (std::is_same<HEADERTYPE, const char*>::value)
			{

				std::copy(headerBegin, headerEnd, httpPointer);
				httpPointer += std::distance(headerBegin, headerEnd);


				std::copy(MAKEJSON::colon, MAKEJSON::colon + MAKEJSON::colonLen, httpPointer);
				httpPointer += MAKEJSON::colonLen;


				std::copy(wordBegin, wordEnd, httpPointer);
				httpPointer += std::distance(wordBegin, wordEnd);

				std::copy(MAKEJSON::halfNewLine, MAKEJSON::halfNewLine + MAKEJSON::halfNewLineLen, httpPointer);
				httpPointer += MAKEJSON::halfNewLineLen;
			}
		}



		template<typename HEADERTYPE, typename ...ARG>
		void packAgeHttpHeader(char *&httpPointer, HEADERTYPE headerBegin, HEADERTYPE headerEnd, HEADERTYPE wordBegin, HEADERTYPE wordEnd, ARG&& ... args)
		{
			if constexpr (std::is_same<HEADERTYPE, const char*>::value)
			{

				std::copy(headerBegin, headerEnd, httpPointer);
				httpPointer += std::distance(headerBegin, headerEnd);


				std::copy(MAKEJSON::colon, MAKEJSON::colon + MAKEJSON::colonLen, httpPointer);
				httpPointer += MAKEJSON::colonLen;

				std::copy(wordBegin, wordEnd, httpPointer);
				httpPointer += std::distance(wordBegin, wordEnd);

				std::copy(MAKEJSON::halfNewLine, MAKEJSON::halfNewLine + MAKEJSON::halfNewLineLen, httpPointer);
				httpPointer += MAKEJSON::halfNewLineLen;

				packAgeHttpHeader(httpPointer, args...);
			}
		}




private:
			template<typename T>
			static T stringLen(const T num)
			{
				if (num <= static_cast<T>(0))
					return static_cast<T>(1);
				T testNum{}, result{};
				static T ten{ 10 };
				testNum = num, result = 1;
				while (testNum /= ten)
					++result;
				return result;
			}






private:
	//m_ptr与m_transformPtr存储内容皆为指针，其中m_transformPtr为存储需要json转义处理的指针首地址


	const char **m_ptr{};    //存储任意组合的指针串
	unsigned int m_pos{};    //上面的指针串的具体长度
	unsigned int m_maxSize{ };   //上面的指针串的最大可容纳空间
	bool m_empty{ true };        //是否为空


	const char **m_transformPtr{};     //存储需要进行json转义转换的指针串
	unsigned int m_transformPos{};     //上面的指针串的具体长度
	unsigned int m_maxTransformSize{ };  //上面的指针串的最大可容纳空间
	bool m_transformEmpty{ true };       //是否为空




	unsigned int m_resize{ 256 };   //指针串重置的空间大小
	unsigned int m_strSize{};        //预判转换后的目标字符串大小

	int index{ -1 };        //记录已插入元素数量

	MEMORYPOOL<> *m_memoryPool{ nullptr };                    //内存池指针


};