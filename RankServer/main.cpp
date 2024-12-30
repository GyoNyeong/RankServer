#ifdef _DEBUG
#pragma comment(lib, "../mysql-connector/lib64/debug/vs14/mysqlcppconn.lib")
#else
#pragma comment(lib, "../mysql-connector/lib64/mysqlcppconn.lib")
#endif

#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <WinSock2.h>
#include <iostream>
#include "jdbc/mysql_Driver.h"   
#include "jdbc/mysql_Connection.h"
#include "jdbc/cppconn/statement.h"
#include "jdbc/cppconn/ResultSet.h"
#include "jdbc/cppconn/exception.h"
#include "jdbc/cppconn/prepared_statement.h"
#include "rapidjson/writer.h"
#include "rapidjson/document.h"

using namespace std;
using namespace rapidjson;

#pragma comment(lib,"ws2_32")

unsigned short Length;

int main()
{
	//WinSoc Initiailze
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	// MySQL Initialize
	sql::Driver* Driver = nullptr;
	sql::Connection* Connection = nullptr;
	sql::ResultSet* ResultSet = nullptr;
	sql::PreparedStatement* PreparedStatement = nullptr;

	Driver = get_driver_instance();

	Connection = Driver->connect("tcp://127.0.0.1", "root", "sky465");

	Connection->setSchema("rankdb"); //use

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
	ServerAddr.sin_port = htons(2000);
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

	//Wait for recieve Data
	while (true)
	{

		//Create and Intialize ClientSocket
		SOCKADDR_IN ClientSockAddr;
		memset(&ClientSockAddr, 0, sizeof(ClientSockAddr));
		int ClientSockSize = sizeof(ClientSockAddr);
		SOCKET ClientSocket = accept(ServerSock, (SOCKADDR*)&ClientSockAddr, &ClientSockSize);

		//u_short Length = 0;
	
		while (true)
		{
			int RecvByte = recv(ClientSocket, (char*)&Length, sizeof(Length), 0);
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

			char Buffer[1024] = { 0, };
			RecvByte = recv(ClientSocket, Buffer, ntohs(Length), 0);

			// json data
			Document JsonData; //rapidjosn
			JsonData.Parse(Buffer); // data parse

			if (JsonData.HasParseError())
			{
				cout << "Error: JSON parse error" << endl;
				closesocket(ClientSocket);
				continue;
			}

			// Check JsonMassage from recvData
			if (JsonData.HasMember("playerName") && JsonData.HasMember("point"))
			{
				string playerName = JsonData["playerName"].GetString();
				int score = JsonData["point"].GetInt();

				// transfer Data to MySQL
				try
				{
					// Prepared Statement Create Qurry
					PreparedStatement = Connection->prepareStatement("INSERT INTO ranking (PlayerName, Score) VALUES (?, ?)");
					PreparedStatement->setString(1, playerName);
					PreparedStatement->setInt(2, score);
					PreparedStatement->executeUpdate();  // Qurry
					cout << "Data inserted into MySQL successfully." << endl;
				}
				catch (sql::SQLException& e)
				{
					cout << "Error: " << e.what() << endl;
				}

				delete PreparedStatement;
			}

			else if (JsonData.HasMember("action"))
			{
				// 1. SQL 쿼리 실행 (상위 10명의 랭킹)
				PreparedStatement = Connection->prepareStatement("SELECT PlayerName, Score FROM ranking ORDER BY Score DESC LIMIT 10");
				ResultSet = PreparedStatement->executeQuery();

				// 2. JSON 응답 준비
				Document RankData;
				RankData.SetObject(); //json형식으로 설정
				Document::AllocatorType& allocator = RankData.GetAllocator();
				Value rankingArray(kArrayType);//json배열로 설정

				// 3. ResultSet에서 데이터를 읽어와서 JSON 객체에 추가
				while (ResultSet->next())
				{
					// 결과셋에서 PlayerName과 Score 가져오기
					string playerName = ResultSet->getString("PlayerName");
					int score = ResultSet->getInt("Score");

					// PlayerName과 Score 값을 JSON 객체에 추가
					Value playerObject(kObjectType);
					playerObject.AddMember("playerName", Value(playerName.c_str(), allocator), allocator);
					playerObject.AddMember("score", score, allocator);

					// rankingArray에 추가
					rankingArray.PushBack(playerObject, allocator);
				}

				// 4. JSON에 status와 ranking 데이터 추가
				RankData.AddMember("ranking", rankingArray, allocator);

				// 5. JSON을 문자열로 변환
				StringBuffer buffer;  // 문자열을 저장할 버퍼
				Writer<StringBuffer> writer(buffer);  // StringBuffer에 JSON을 작성하는 Writer 객체
				RankData.Accept(writer);  // JSON 객체를 문자열로 변환하여 buffer에 저장

				cout << "Sending JSON data to client: " << buffer.GetString() << endl;

				Length = htons(buffer.GetSize());

				// 6. 결과를 클라이언트로 전송
				int SendSize = send(ClientSocket, (char*)&Length, sizeof(Length), 0);
				SendSize = send(ClientSocket, buffer.GetString(), buffer.GetSize(), 0);
				//send(ClientSocket, "\n", 1, 0);
				cout << "Send Complete" << endl;

				// 7. 정리
				delete PreparedStatement;
				delete ResultSet;
				allocator.Clear();
				//allocator.Free(&RankData); 수동으로 해제하는 방식은 올바르나 rapidjson은 자동으로 이 과정이 이루어지기떄문에 현재의 줄은 불필요
			}
		}
		closesocket(ClientSocket);
	}

	// CloseSocket
	closesocket(ServerSock);
	WSACleanup();
	delete Connection;
	return 0;
}