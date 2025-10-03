#pragma once


#include<string_view>


namespace SQLCOMMAND
{
	/*
	CREATE TABLE `test` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(45) DEFAULT NULL,
  `age` int DEFAULT NULL,
  `province` varchar(45) DEFAULT NULL,
  `city` varchar(45) DEFAULT NULL,
  `country` varchar(45) DEFAULT NULL,
  `phone` varchar(20) DEFAULT NULL,
  `flag` int DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `index_flag` (`flag`)
) ENGINE=InnoDB AUTO_INCREMENT=9 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci |

	
	*/
	static const char *testSqlRead{ "select id,name,age,province,city,country,phone from test where flag=0 and id<4;select id,name,age,province,city,country,phone from test where flag=0 and id>3" };


	/*
	 CREATE TABLE `memoryTable` (
  `id` int NOT NULL AUTO_INCREMENT,
  `name` varchar(45) DEFAULT NULL,
  `age` int DEFAULT NULL,
  `province` varchar(45) DEFAULT NULL,
  `city` varchar(45) DEFAULT NULL,
  `country` varchar(45) DEFAULT NULL,
  `phone` varchar(20) DEFAULT NULL,
  `flag` int DEFAULT '0',
  PRIMARY KEY (`id`),
  KEY `index_flag` (`flag`)
) ENGINE=MEMORY DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci |

	*/

	static const char *testSqlReadMemory{ "select name,age,book from table1" };

	static const char *testTestLoginSQL{ "select password from user where username='" };

	static const char* testMysql{ "select account,password,phone,email,name,height,age,province,city,examine from user limit 2" };

	static const char* testMysqlDuplicate{ "insert into user(account,password,phone) value('ddj','123456Ab','13525889613')" };


	/*
	CREATE TABLE table1 (
    id1 TINYINT,
    id2 TINYINT UNSIGNED
    );
	*/
	static const char* testMysqlTINYINT{ "select id1,id2 from table1" };






	static const int testSqlReadLen{ strlen(testSqlRead) };

	static const int testSqlReadMemoryLen{ strlen(testSqlReadMemory) };

	static const int testTestLoginSQLLen{ strlen(testTestLoginSQL) };

	static const int testMysqlLen{ strlen(testMysql) };

	static const int testMysqlDuplicateLen{ strlen(testMysqlDuplicate) };

	static const int testMysqlTINYINTLen{ strlen(testMysqlTINYINT) };


	
	static const std::string_view insertUser1{ "insert into user(account,password,phone) value('" };
	static const std::string_view insertUser2{ "','" };
	static const std::string_view insertUser3{ "','" };
	static const std::string_view insertUser4{ "')" };


	static const std::string_view updateUser1{ "update user set name='" };
	static const std::string_view updateUser2{ "',height=" };
	static const std::string_view updateUser3{ ",age=" };
	static const std::string_view updateUser4{ ",province=" };
	static const std::string_view updateUser5{ ",city=" };

	//待审核状态
	static const std::string_view updateUser6{ ",examine=1 where account='" };
	static const std::string_view updateUser7{ "'" };


	static const std::string_view queryUserExamine{ "select account,name from user where examine=1 limit 10" };














}