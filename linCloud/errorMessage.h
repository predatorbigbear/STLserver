#pragma once


enum class ERRORMESSAGE
{
	OK,

	SQL_QUERY_ERROR,

	SQL_NET_ASYNC_ERROR,

	SQL_MYSQL_RES_NULL,

	SQL_QUERY_ROW_ZERO,

	SQL_QUERY_FIELD_ZERO,

	SQL_RESULT_TOO_LAGGE,

	REDIS_ASYNC_WRITE_ERROR,

	REDIS_ASYNC_READ_ERROR,

	CHECK_REDIS_MESSAGE_ERROR,

	REDIS_READY_QUERY_ERROR,

	STD_BADALLOC,

	NO_KEY,

	ERROR,

	/////////////////////////redis  //////////////

	SIMPLE_STRING,

	REDIS_ERROR,

	INTERGER,

	LOT_SIZE_STRING,

	ARRAY,

	REDIS_DEFAULT,


	///////////////////////////////////////

	REDIS_MULTI_OK,

	REDIS_SINGLE_OK





};




enum class REDISPARSETYPE:int
{
	SIMPLE_STRING = '+',

	ERROR = '-',

	INTERGER = ':',

	LOT_SIZE_STRING = '$',

	ARRAY = '*',

	UNKNOWN_TYPE

};