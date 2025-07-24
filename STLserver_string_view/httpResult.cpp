#include "httpResult.h"

HTTPRESULT::HTTPRESULT()
{
	m_headerPtr.reset(new std::string_view*[all_len]);
	m_headerbegin = m_headerEnd = m_headerPtr.get();
	m_Set_Cookie.reserve(50);
}

void HTTPRESULT::resetheader()
{
	if (std::distance(m_headerbegin, m_headerEnd))
	{
		std::for_each(m_headerbegin, m_headerEnd, [](std::string_view *&ptr)
		{
			*ptr = std::string_view{};
		});
		m_headerEnd = m_headerbegin ;
	}
	m_Set_Cookie.clear();
}

const bool HTTPRESULT::isMethodEmpty()
{
	return m_Method.empty();
}

const bool HTTPRESULT::isTargetEmpty()
{
	return m_Target.empty();
}

const bool HTTPRESULT::isVersionEmpty()
{
	return m_Version.empty();
}

const bool HTTPRESULT::isAcceptEmpty()
{
	return m_Accept.empty();
}

const bool HTTPRESULT::isAccept_CharsetEmpty()
{
	return m_Accept_Charset.empty();
}

const bool HTTPRESULT::isAccept_EncodingEmpty()
{
	return m_Accept_Encoding.empty();
}

const bool HTTPRESULT::isAccept_LanguageEmpty()
{
	return m_Accept_Language.empty();
}

const bool HTTPRESULT::isAccept_RangesEmpty()
{
	return m_Accept_Ranges.empty();
}

const bool HTTPRESULT::isAuthorizationEmpty()
{
	return m_Authorization.empty();
}

const bool HTTPRESULT::isCache_ControlEmpty()
{
	return m_Cache_Control.empty();
}

const bool HTTPRESULT::isConnectionEmpty()
{
	return m_Connection.empty();
}

const bool HTTPRESULT::isCookieEmpty()
{
	return m_Cookie.empty();
}

const bool HTTPRESULT::isContent_LengthEmpty()
{
	return m_Content_Length.empty();
}

const bool HTTPRESULT::isContent_TypeEmpty()
{
	return m_Content_Type.empty();
}

const bool HTTPRESULT::isDateEmpty()
{
	return m_Date.empty();
}

const bool HTTPRESULT::isExpectEmpty()
{
	return m_Expect.empty();
}

const bool HTTPRESULT::isFromEmpty()
{
	return m_From.empty();
}

const bool HTTPRESULT::isHostEmpty()
{
	return m_Host.empty();
}

const bool HTTPRESULT::isIf_MatchEmpty()
{
	return m_If_Match.empty();
}

const bool HTTPRESULT::isIf_Modified_SinceEmpty()
{
	return m_If_Modified_Since.empty();
}

const bool HTTPRESULT::isIf_None_MatchEmpty()
{
	return m_If_None_Match.empty();
}

const bool HTTPRESULT::isIf_RangeEmpty()
{
	return m_If_Range.empty();
}

const bool HTTPRESULT::isIf_Unmodified_SinceEmpty()
{
	return m_If_Unmodified_Since.empty();
}

const bool HTTPRESULT::isMax_ForwardsEmpty()
{
	return m_Max_Forwards.empty();
}

const bool HTTPRESULT::isPragmaEmpty()
{
	return m_Pragma.empty();
}

const bool HTTPRESULT::isProxy_AuthorizationEmpty()
{
	return m_Proxy_Authorization.empty();
}

const bool HTTPRESULT::isRangeEmpty()
{
	return m_Range.empty();
}

const bool HTTPRESULT::isRefererEmpty()
{
	return m_Referer.empty();
}

const bool HTTPRESULT::isSetCookieEmpty()
{
	return m_Set_Cookie.empty();
}

const bool HTTPRESULT::isTEEmpty()
{
	return m_TE.empty();
}

const bool HTTPRESULT::isUpgradeEmpty()
{
	return m_Upgrade.empty();
}

const bool HTTPRESULT::isUser_AgentEmpty()
{
	return m_User_Agent.empty();
}

const bool HTTPRESULT::isViaEmpty()
{
	return m_Via.empty();
}

const bool HTTPRESULT::isWarningEmpty()
{
	return m_Warning.empty();
}

const bool HTTPRESULT::isBodyEmpty()
{
	return m_Body.empty();
}

const bool HTTPRESULT::isTransfer_EncodingEmpty()
{
	return m_Transfer_Encoding.empty();
}

const bool HTTPRESULT::isparaEmpty()
{
	return m_para.empty();
}


void HTTPRESULT::setMethod(const char* begin, const char* end)
{
	m_Method = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_Method;
}

void HTTPRESULT::setTarget(const char* begin, const char* end)
{
	m_Target = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_Target;
}

void HTTPRESULT::setVersion(const char* begin, const char* end)
{
	m_Version = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_Version;
}

void HTTPRESULT::setAccept(const char* begin, const char* end)
{
	m_Accept = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_Accept;
}

void HTTPRESULT::setAccept_Charset(const char* begin, const char* end)
{
	m_Accept_Charset = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_Accept_Charset;
}

void HTTPRESULT::setAccept_Encoding(const char* begin, const char* end)
{
	m_Accept_Encoding = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_Accept_Encoding;
}

void HTTPRESULT::setAccept_Language(const char* begin, const char* end)
{
	m_Accept_Language = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_Accept_Language;
}

void HTTPRESULT::setAccept_Ranges(const char* begin, const char* end)
{
	m_Accept_Ranges = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_Accept_Ranges;
}

void HTTPRESULT::setAuthorization(const char* begin, const char* end)
{
	m_Authorization = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_Authorization;
}

void HTTPRESULT::setCache_Control(const char* begin, const char* end)
{
	m_Cache_Control = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_Cache_Control;
}

void HTTPRESULT::setConnection(const char* begin, const char* end)
{
	m_Connection = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_Connection;
}

void HTTPRESULT::setCookie(const char* begin, const char* end)
{
	m_Cookie = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_Cookie;
}

void HTTPRESULT::setContent_Length(const char* begin, const char* end)
{
	m_Content_Length = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_Content_Length;
}

void HTTPRESULT::setContent_Type(const char* begin, const char* end)
{
	m_Content_Type = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_Content_Type;
}

void HTTPRESULT::setDate(const char* begin, const char* end)
{
	m_Date = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_Date;
}

void HTTPRESULT::setExpect(const char* begin, const char* end)
{
	m_Expect = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_Expect;
}

void HTTPRESULT::setFrom(const char* begin, const char* end)
{
	m_From = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_From;
}

void HTTPRESULT::setHost(const char* begin, const char* end)
{
	m_Host = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_Host;
}

void HTTPRESULT::setIf_Match(const char* begin, const char* end)
{
	m_If_Match = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_If_Match;
}

void HTTPRESULT::setIf_Modified_Since(const char* begin, const char* end)
{
	m_If_Modified_Since = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_If_Modified_Since;
}

void HTTPRESULT::setIf_None_Match(const char* begin, const char* end)
{
	m_If_None_Match = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_If_None_Match;
}

void HTTPRESULT::setIf_Range(const char* begin, const char* end)
{
	m_If_Range = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_If_Range;
}

void HTTPRESULT::setIf_Unmodified_Since(const char* begin, const char* end)
{
	m_If_Unmodified_Since = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_If_Unmodified_Since;
}

void HTTPRESULT::setMax_Forwards(const char* begin, const char* end)
{
	m_Max_Forwards = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_Max_Forwards;
}

void HTTPRESULT::setPragma(const char* begin, const char* end)
{
	m_Pragma = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_Pragma;
}

void HTTPRESULT::setProxy_Authorization(const char* begin, const char* end)
{
	m_Proxy_Authorization = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_Proxy_Authorization;
}

void HTTPRESULT::setRange(const char* begin, const char* end)
{
	m_Range = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_Range;
}

void HTTPRESULT::setReferer(const char* begin, const char* end)
{
	m_Referer = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_Referer;
}

void HTTPRESULT::setSetCookie(const char* begin, const char* end)
{
	m_Set_Cookie.emplace_back(std::string_view(begin, std::distance(begin, end)));
}

void HTTPRESULT::setTE(const char* begin, const char* end)
{
	m_TE = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_TE;
}

void HTTPRESULT::setUpgrade(const char* begin, const char* end)
{
	m_Upgrade = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_Upgrade;
}

void HTTPRESULT::setUser_Agent(const char* begin, const char* end)
{
	m_User_Agent = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_User_Agent;
}

void HTTPRESULT::setVia(const char* begin, const char* end)
{
	m_Via = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_Via;
}

void HTTPRESULT::setWarning(const char* begin, const char* end)
{
	m_Warning = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_Warning;
}

void HTTPRESULT::setBody(const char* begin, const char* end)
{
	m_Body = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_Body;
}

void HTTPRESULT::setTransfer_Encoding(const char* begin, const char* end)
{
	m_Transfer_Encoding = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_Transfer_Encoding;
}

void HTTPRESULT::setpara(const char* begin, const char* end)
{
	m_para = std::string_view(begin, std::distance(begin, end));
	*m_headerEnd++ = &m_para;
}

const std::string_view HTTPRESULT::getMethod()
{
	return m_Method;
}

const std::string_view HTTPRESULT::getTarget()
{
	return m_Target;
}

const std::string_view HTTPRESULT::getVersion()
{
	return m_Version;
}

const std::string_view HTTPRESULT::getAccept()
{
	return m_Accept;
}

const std::string_view HTTPRESULT::getAccept_Charset()
{
	return m_Accept_Charset;
}

const std::string_view HTTPRESULT::getAccept_Encoding()
{
	return m_Accept_Encoding;
}

const std::string_view HTTPRESULT::getAccept_Language()
{
	return m_Accept_Language;
}

const std::string_view HTTPRESULT::getAccept_Ranges()
{
	return m_Accept_Ranges;
}

const std::string_view HTTPRESULT::getAuthorization()
{
	return m_Authorization;
}

const std::string_view HTTPRESULT::getCache_Control()
{
	return m_Cache_Control;
}

const std::string_view HTTPRESULT::getConnection()
{
	return m_Connection;
}

const std::string_view HTTPRESULT::getCookie()
{
	return m_Cookie;
}

const std::string_view HTTPRESULT::getContent_Length()
{
	return m_Content_Length;
}

const std::string_view HTTPRESULT::getContent_Type()
{
	return m_Content_Type;
}

const std::string_view HTTPRESULT::getDate()
{
	return m_Date;
}

const std::string_view HTTPRESULT::getExpect()
{
	return m_Expect;
}

const std::string_view HTTPRESULT::getFrom()
{
	return m_From;
}

const std::string_view HTTPRESULT::getHost()
{
	return m_Host;
}

const std::string_view HTTPRESULT::getIf_Match()
{
	return m_If_Match;
}

const std::string_view HTTPRESULT::getIf_Modified_Since()
{
	return m_If_Modified_Since;
}

const std::string_view HTTPRESULT::getIf_None_Match()
{
	return m_If_None_Match;
}

const std::string_view HTTPRESULT::getIf_Range()
{
	return m_If_Range;
}

const std::string_view HTTPRESULT::getIf_Unmodified_Since()
{
	return m_If_Unmodified_Since;
}

const std::string_view HTTPRESULT::getMax_Forwards()
{
	return m_Max_Forwards;
}

const std::string_view HTTPRESULT::getPragma()
{
	return m_Pragma;
}

const std::string_view HTTPRESULT::getProxy_Authorization()
{
	return m_Proxy_Authorization;
}

const std::string_view HTTPRESULT::getRange()
{
	return m_Range;
}

const std::string_view HTTPRESULT::getReferer()
{
	return m_Referer;
}

const std::vector<std::string_view>& HTTPRESULT::getSetCookie()
{
	return m_Set_Cookie;
}

const std::string_view HTTPRESULT::getTE()
{
	return m_TE;
}

const std::string_view HTTPRESULT::getUpgrade()
{
	return m_Upgrade;
}

const std::string_view HTTPRESULT::getUser_Agent()
{
	return m_User_Agent;
}

const std::string_view HTTPRESULT::getVia()
{
	return m_Via;
}

const std::string_view HTTPRESULT::getWarning()
{
	return m_Warning;
}

const std::string_view HTTPRESULT::getBody()
{
	return m_Body;
}

const std::string_view HTTPRESULT::getTransfer_Encoding()
{
	return m_Transfer_Encoding;
}

const std::string_view HTTPRESULT::getpara()
{
	return m_para;
}

std::string_view HTTPRESULT::findView(const char* begin, const char* end)
{
	if (!begin || !end || std::distance(begin, end) < 1)
		return {};
	switch (std::distance(begin, end))
	{
	case 2:
		//"TE"
		return m_TE;
		break;

	case 3:
		//"Via"
		return m_Via;

		break;

	case 4:
		switch (*begin)
		{
		case 'd':
		case 'D':
			//"Date"
			return m_Date;

			break;

		case 'f':
		case 'F':
			//"From"
			return m_From;

			break;

		case 'h':
		case 'H':
			//"Host"
			return m_Host;

			break;
		case 'p':
		case 'P':
			//para
			return m_para;

			break;
		case 'b':
		case 'B':
			//Body
			return m_Body;


			break;
		default:

			break;
		}
		break;

	case 5:
		//"Range"
		return m_Range;

		break;

	case 6:
		switch (*begin)
		{
		case 'a':
		case 'A':
			//"Accept"
			return m_Accept;

			break;

		case 'c':
		case 'C':
			//"Cookie"
			return m_Cookie;

			break;

		case 'e':
		case 'E':
			//"Expect"
			return m_Expect;

			break;

		case 'p':
		case 'P':
			//"Pragma"
			return m_Pragma;

			break;

		default:

			break;
		}
		break;

	case 7:
		switch (*begin)
		{
		case 'r':
		case 'R':
			//"Referer"
			return m_Referer;

			break;

		case 'u':
		case 'U':
			//"Upgrade"
			return m_Upgrade;

			break;

		case 'w':
		case 'W':
			//"Warning"
			return m_Warning;

			break;

		default:

			break;
		}
		break;


	case 8:
		switch (*(begin + 3))
		{
		case 'm':
		case 'M':
			//"If-Match"
			return m_If_Match;

			break;

		case 'r':
		case 'R':
			//"If-Range"
			return m_If_Range;

			break;

		default:

			break;
		}
		break;

	case 10:
		switch (*begin)
		{
		case 'c':
		case 'C':
			// "Connection"
			return m_Connection;

			break;

		case 'U':
		case 'u':
			//"User-Agent"
			return m_User_Agent;

			break;

		default:

			break;
		}
		break;

	case 12:
		switch (*begin)
		{
		case 'c':
		case 'C':
			//"Content-Type"
			return m_Content_Type;

			break;

		case 'm':
		case 'M':
			//"Max-Forwards"
			return m_Max_Forwards;

			break;

		default:

			break;
		}
		break;

	case 13:
		switch (*(begin + 1))
		{
		case 'c':
		case 'C':
			//"Accept-Ranges"
			return m_Accept_Ranges;

			break;

		case 'u':
		case 'U':
			//"Authorization"
			
			return m_Authorization;

			break;

		case 'a':
		case 'A':
			// "Cache-Control"
			
			return m_Cache_Control;

			break;

		case 'f':
		case 'F':
			// "If-None-Match"
			
			return m_If_None_Match;

			break;

		default:

			break;
		}
		break;

	case 14:
		switch (*begin)
		{
		case 'a':
		case 'A':
			//"Accept-Charset"
			
			return m_Accept_Charset;

			break;

		case 'c':
		case 'C':
			//"Content-Length"
			
			return m_Content_Length;

			break;

		default:

			break;
		}

		break;

	case 15:
		switch (*(begin + 7))
		{
		case 'e':
		case 'E':
			//"Accept-Encoding"
			
			return m_Accept_Encoding;

			break;

		case 'l':
		case 'L':
			//"Accept-Language"
			
			return m_Accept_Language;

			break;

		default:

			break;
		}


		break;

	case 17:
		switch (*begin)
		{
		case 'i':
		case 'I':
			// "If-Modified-Since"
			
			return m_If_Modified_Since;

			break;

		case 't':
		case 'T':
			//Transfer-Encoding
			
			return m_Transfer_Encoding;

			break;

		default:

			break;
		}

	case 19:
		switch (*begin)
		{
		case 'i':
		case 'I':
			// "If-Unmodified-Since"
			
			return m_If_Unmodified_Since;

			break;

		case 'p':
		case 'P':
			// "Proxy-Authorization"
			
			return m_Proxy_Authorization;

			break;

		default:

			break;
		}
		break;


	default:

		break;
	}
	return {};
}

std::string_view HTTPRESULT::findView(const char* str)
{
	if (!str)
		return {};
	return findView(str, str + strlen(str));
}

std::string_view HTTPRESULT::findView(const std::string& str)
{
	if(str.empty())
		return {};
	return findView(str.data(),str.data()+str.size());
}

std::string_view HTTPRESULT::findView(const std::string_view str)
{
	if (str.empty())
		return {};
	return findView(str.data(), str.data() + str.size());
}
