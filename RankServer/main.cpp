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
#include "Windows.h"
#include "process.h"

using namespace std;
using namespace rapidjson;

#pragma comment(lib,"ws2_32")

unsigned short Length;

struct ThreadData
{
	SOCKET ClientSocket;
	sql::Connection* connection;
};

unsigned WINAPI WorkThread(void* param)
{
	ThreadData* ClientData = (ThreadData*)param;
	SOCKET ClientSocket = ClientData->ClientSocket;
	sql::Connection* Connection = ClientData->connection;
	
	if (Connection == nullptr)
	{
		cout << "Connection not initialized!" << endl;
		closesocket(ClientSocket);
		delete ClientData;
		return -1;
	}

	sql::PreparedStatement* PreparedStatement = nullptr;
	sql::ResultSet* ResultSet = nullptr;

	while (true)
	{
		int RecvByte = recv(ClientSocket, (char*)&Length, sizeof(Length), 0);
		if (RecvByte <= 0)
		{
			if (RecvByte == 0)
			{
				cout << "Client disconnected." << endl;
				closesocket(ClientSocket);
				delete ClientData;
				return -1;
			}
			else
			{
				cout << "Recv Error: " << WSAGetLastError() << endl;
			}
			break;

		}

		char Buffer[1024] = { 0, };
		RecvByte = recv(ClientSocket, Buffer, ntohs(Length), 0);

		Document JsonData;
		JsonData.Parse(Buffer);

		if (JsonData.HasParseError())
		{
			cout << "Error: JSON parse error" << endl;
			closesocket(ClientSocket);
			continue;
		}

		if (JsonData.HasMember("playerName") && JsonData.HasMember("point"))
		{
			string playerName = JsonData["playerName"].GetString();
			int score = JsonData["point"].GetInt();

			try
			{
				PreparedStatement = Connection->prepareStatement("INSERT INTO ranking (PlayerName, Score) VALUES (?, ?)");
				PreparedStatement->setString(1, playerName);
				PreparedStatement->setInt(2, score);
				PreparedStatement->executeUpdate();
				cout << "Data inserted into MySQL successfully." << endl;
			}
			catch (sql::SQLException& e)
			{
				cout << "Error: " << e.what() << endl;
			}

			if (PreparedStatement)
			{
				delete PreparedStatement;
			}
			PreparedStatement = nullptr;
		}
		else if (JsonData.HasMember("action"))
		{
			// 1. SQL query execution (top 10 rankings)
			PreparedStatement = Connection->prepareStatement("SELECT PlayerName, Score FROM ranking ORDER BY Score DESC LIMIT 10");
			ResultSet = PreparedStatement->executeQuery();

			// 2. Prepare JSON response
			Document RankData;
			RankData.SetObject(); // Set as JSON object
			Document::AllocatorType& allocator = RankData.GetAllocator();
			Value rankingArray(kArrayType); // Set as JSON array

			// 3. Read data from ResultSet and add to JSON object
			while (ResultSet->next())
			{
				// Get PlayerName and Score from ResultSet
				string playerName = ResultSet->getString("PlayerName");
				int score = ResultSet->getInt("Score");

				// Add PlayerName and Score to JSON object
				Value playerObject(kObjectType);
				playerObject.AddMember("playerName", Value(playerName.c_str(), allocator), allocator);
				playerObject.AddMember("score", score, allocator);

				// Add to rankingArray
				rankingArray.PushBack(playerObject, allocator);
			}

			// 4. Add ranking data to JSON object
			RankData.AddMember("ranking", rankingArray, allocator);

			// 5. Convert JSON to string
			StringBuffer buffer;  // Buffer to store the string
			Writer<StringBuffer> writer(buffer);  // Writer object to write JSON to buffer
			RankData.Accept(writer);  // Convert JSON object to string in buffer

			cout << "Sending JSON data to client: " << buffer.GetString() << endl;

			Length = htons(buffer.GetSize());

			// 6. Send result to client
			int SendSize = send(ClientSocket, (char*)&Length, sizeof(Length), 0);
			SendSize = send(ClientSocket, buffer.GetString(), buffer.GetSize(), 0);
			//send(ClientSocket, "\n", 1, 0);
			cout << "Send Complete" << endl;

			// 7. Cleanup
			delete PreparedStatement;
			delete ResultSet;
			allocator.Clear();
		}
	}
		// top 10 rankings query...


	// Cleanup
	if (PreparedStatement)
	{
		delete PreparedStatement;
	}
	if (ResultSet)
	{
		delete ResultSet;
	}
	closesocket(ClientSocket);
	delete ClientData;
	return 0;
}

int main()
{
	//WinSoc Initiailze
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	// MySQL Initialize
	sql::Driver* Driver = nullptr;
	sql::Connection* Connection = nullptr;	

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

		ThreadData* clientData = new ThreadData;
		clientData->ClientSocket = ClientSocket;
		clientData->connection = Connection;

		HANDLE ThreadHandle = (HANDLE)_beginthreadex(0, 0, WorkThread, (void*)clientData, 0, 0);
		if (ThreadHandle == NULL)
		{
			cout << "Error: Could not create thread" << endl;
			closesocket(ClientSocket);
			delete clientData;
		}
		else
		{
			CloseHandle(ThreadHandle); // Thread handle is no longer needed
		}
	}
	
	closesocket(ServerSock);
	WSACleanup();
	delete Connection;
	return 0;
}