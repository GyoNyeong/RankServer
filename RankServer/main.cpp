#ifdef _DEBUG
#pragma comment(lib, "C:/Work/RankServer/mysql-connector/lib64/debug/vs14/mysqlcppconn.lib")
#else
#pragma comment(lib, "C:/Work/RankServer/mysql-connector/lib64/mysqlcppconn.lib")
#endif

#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <WinSock2.h>
#include <iostream>
#include "jdbc/mysql_driver.h"   
#include "jdbc/mysql_connection.h"
#include "jdbc/cppconn/statement.h"
#include "jdbc/cppconn/resultset.h"
#include "jdbc/cppconn/exception.h"
#include "jdbc/cppconn/prepared_statement.h"
#include "rapidjson/writer.h"
#include "rapidjson/document.h"

using namespace std;
using namespace rapidjson;

#pragma comment(lib,"ws2_32")

int main()
{
	//WinSoc Initiailze
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	// MySQL Initialize
	sql::Driver* driver = nullptr;
	sql::Connection* connection = nullptr;
	sql::ResultSet* resultSet = nullptr;

	sql::PreparedStatement* preparedStatement = nullptr;

	driver = get_driver_instance();

	connection = driver->connect("tcp://127.0.0.1", "root", "sky465");

	connection->setSchema("rankdb"); //use

	//Socket carete
	SOCKET ServerSock = socket(AF_INET, SOCK_STREAM, 0);
	if (ServerSock == SOCKET_ERROR)
	{
		cout << " error : Create Socket failed";
		cout << " Socket error : " << GetLastError() << endl;

		return -1;
	}

	//ServerSocket Initalize
	SOCKADDR_IN	ServerAddr;
	memset(&ServerAddr, 0, sizeof(ServerAddr));
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port = htons(3000);
	ServerAddr.sin_addr.s_addr = INADDR_ANY;

	//bind
	int Result = ::bind(ServerSock, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr));
	{
		if (Result == SOCKET_ERROR)
		{
			cout << "Bind error : " << GetLastError() << endl;

			return -1;
		}
	}

	//Listen
	listen(ServerSock, 5);
	
	//Create and Intialize ClientSocket
	SOCKADDR_IN ClientSockAddr;
	memset(&ClientSockAddr, 0, sizeof(ClientSockAddr));
	int ClientSockSize = sizeof(ClientSockAddr);
	SOCKET ClientSocket = accept(ServerSock, (SOCKADDR*)&ClientSockAddr, &ClientSockSize);

	//Wait for recieve Data
	while (true)
	{
		char Buffer[1024] = { 0, };
		int RecvByte = recv(ClientSocket, Buffer, sizeof(Buffer), 0);
		if (RecvByte <= 0)
		{
			if (RecvByte == 0)
			{
				cout << "Client disconnected." << endl;
			}
			else
			{
				cout << "Recv Error: " << WSAGetLastError() << endl;
			}
			break; 
		}

		// json data
		Document document;
		document.Parse(Buffer); // data parse

		if (document.HasParseError())
		{
			cout << "Error: JSON parse error" << endl;
			closesocket(ClientSocket);
			continue;
		}

		// Check JsonMassage from recvData
		if (document.HasMember("playerName") && document.HasMember("point"))
		{
			string playerName = document["playerName"].GetString();
			int score = document["point"].GetInt();

			// transfer Data to MySQL
			try
			{
				// Prepared Statement Create Qurry
				sql::PreparedStatement* preparedStatement = connection->prepareStatement("INSERT INTO ranking (PlayerName, Score) VALUES (?, ?)");
				preparedStatement->setString(1, playerName);
				preparedStatement->setInt(2, score);
				preparedStatement->executeUpdate();  // Qurry
				cout << "Data inserted into MySQL successfully." << endl;
			}
			catch (sql::SQLException& e)
			{
				cout << "Error: " << e.what() << endl;
			}

			delete preparedStatement;
		}
		
		else if (document.HasMember("action"))
		{
			cout << "Request Rank";
			// 1. SQL 쿼리 실행 (상위 10명의 랭킹)
			sql::PreparedStatement* preparedStatement = connection->prepareStatement("SELECT PlayerName, Score FROM ranking ORDER BY Score DESC LIMIT 10");
			sql::ResultSet* resultSet = preparedStatement->executeQuery();

			// 2. JSON 응답 준비
			Document responseDoc;
			responseDoc.SetObject();
			Document::AllocatorType& allocator = responseDoc.GetAllocator();
			Value rankingArray(kArrayType);

			// 3. resultSet에서 데이터를 읽어와서 JSON 객체에 추가
			while (resultSet->next())
			{
				// 결과셋에서 PlayerName과 Score 가져오기
				std::string playerName = resultSet->getString("PlayerName");
				int score = resultSet->getInt("Score");

				// PlayerName과 Score 값을 JSON 객체에 추가
				Value playerObject(kObjectType);
				playerObject.AddMember("playerName", Value(playerName.c_str(), allocator), allocator);
				playerObject.AddMember("score", score, allocator);

				// rankingArray에 추가
				rankingArray.PushBack(playerObject, allocator);
			}

			// 4. JSON에 status와 ranking 데이터 추가
			//responseDoc.AddMember("status", "success", allocator);
			responseDoc.AddMember("ranking", rankingArray, allocator);

			// 5. JSON을 문자열로 변환
			StringBuffer buffer;  // 문자열을 저장할 버퍼
			Writer<StringBuffer> writer(buffer);  // StringBuffer에 JSON을 작성하는 Writer 객체
			responseDoc.Accept(writer);  // JSON 객체를 문자열로 변환하여 buffer에 저장

			cout << "Sending JSON data to client: " << buffer.GetString() << endl;

			// 6. 결과를 클라이언트로 전송
			send(ClientSocket, buffer.GetString(), buffer.GetSize(), 0);
			send(ClientSocket, "\n", 1, 0);
			cout << "Send Complete" << endl;
			// 7. 정리
			delete preparedStatement;
			delete resultSet;
		}

	}

	// CloseSocket
	closesocket(ClientSocket);
	closesocket(ServerSock);
	WSACleanup();
	delete connection;
	return 0;
}