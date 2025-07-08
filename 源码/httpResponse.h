#pragma once



namespace HTTPRESPONSE
{
	static const char* httpHead{ "HTTP/" };
	static int httpHeadLen{ strlen(httpHead) };

	static const char* OK200{ " 200 OK\r\n" };
	static int OK200Len{ strlen(OK200) };

	static const char* Access_Control_Allow_Origin{ "Access-Control-Allow-Origin:*\r\n" };
	static int Access_Control_Allow_OriginLen{ strlen(Access_Control_Allow_Origin) };

	static const char* Content_Length{ "Content-Length:" };
	static int Content_LengthLen{ strlen(Content_Length) };

	static const char* newline{ "\r\n\r\n" };
	static int newlineLen{ strlen(newline) };

	static const char* halfNewLine{ "\r\n" };
	static int halfNewLineLen{ strlen(halfNewLine) };

}
