#pragma once
#include "publicHeader.h"



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

	static const char *testSqlReadMemory{ "select id,name,age,province,city,country,phone from memoryTable where flag=0 and id<4;select id,name,age,province,city,country,phone from memoryTable where flag=0 and id>3" };

	static const char *testTestLoginSQL{ "select password from user where username='" };



	static const int testSqlReadLen{ strlen(testSqlRead) };

	static const int testSqlReadMemoryLen{ strlen(testSqlReadMemory) };

	static const int testTestLoginSQLLen{ strlen(testTestLoginSQL) };
























}