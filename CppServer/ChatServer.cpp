#include "Buffer.h"
#include "google/protobuf/message.h"
#include "protocol.pb.h"

#include "Poco/Net/SocketReactor.h"
#include "Poco/Net/SocketAcceptor.h"
#include "Poco/Net/SocketNotification.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/NetException.h"
#include "Poco/NObserver.h"
#include "Poco/Exception.h"
#include "Poco/Thread.h"
#include "Poco/LocalDateTime.h"
#include "Poco/DateTimeFormat.h"
#include "Poco/DateTimeFormatter.h"
#include <iostream>
#include <map>
#include <vector>



using Poco::Net::SocketReactor;
using Poco::Net::SocketAcceptor;

using Poco::Net::SocketNotification;
using Poco::Net::ReadableNotification;
using Poco::Net::ShutdownNotification;
using Poco::Net::ServerSocket;
using Poco::Net::StreamSocket;
using Poco::NObserver;
using Poco::AutoPtr;
using Poco::Thread;

using Poco::LocalDateTime;
using Poco::DateTimeFormat;
using Poco::DateTimeFormatter;

using namespace Protobuf_net;
using namespace std;

static int g_sessionId = 0;

const int kIntLen = sizeof(int);
const unsigned long sendToAllUserSequence= 0xffffffff;


struct User
{
	User()
		:login(false),sessionId(-1),userName("")
	{
	}

	Buffer			inputBuffer;
	Buffer			outputBuffer;
	StreamSocket	socket;
	std::string		userName;
	int				sessionId;
	bool			login;
};

static vector<User*>  s_users;

class ChatServiceHandler
{
public:
	ChatServiceHandler(StreamSocket& socket, SocketReactor& reactor)
		:_reactor(reactor)
	{
		_user.socket = socket;
		_user.login = false;
		_user.sessionId = ++g_sessionId;

		_reactor.addEventHandler(_user.socket, NObserver<ChatServiceHandler, ReadableNotification>(*this, &ChatServiceHandler::onReadable));
		_reactor.addEventHandler(_user.socket, NObserver<ChatServiceHandler, ShutdownNotification>(*this, &ChatServiceHandler::onShutdown));

		s_users.push_back(&_user);

	}

	~ChatServiceHandler()
	{
		vector<User*>::iterator it = find(s_users.begin(), s_users.end(), &_user);
		if (it != s_users.end())
			s_users.erase(it);


		_reactor.removeEventHandler(_user.socket, NObserver<ChatServiceHandler, ReadableNotification>(*this, &ChatServiceHandler::onReadable));
		_reactor.removeEventHandler(_user.socket, NObserver<ChatServiceHandler, ShutdownNotification>(*this, &ChatServiceHandler::onShutdown));
	}

	void onReadable(const AutoPtr<ReadableNotification>& pNf)
	{
		try
		{
			int n = _user.inputBuffer.receiveBytes(_user.socket);
			if (n <= 0)
			{
				cout << "delete this" << endl;
				delete this;
			}
		}
		catch (Poco::Exception& exc)
		{
			cerr << "[ChatServiceHandler][onReadable]: " << exc.displayText() << endl;

			// notify to other users 
			chat::Message* friendNotification =  BuildFriendNotification(false);
			sendMessageToOthers(*friendNotification);
			delete friendNotification;

			// delete the disconnected-user connection
			delete this;
		}

		onBufferMessage();
	}

	void sendMessage(chat::Message& msg)
	{
		std::string result;
		result.resize(kIntLen);	//Reserved 4 bytes for head
		int len;
		bool succeed = msg.AppendToString(&result);
		if (succeed)
		{
			len = result.length() - kIntLen;
			char buf[kIntLen];
			buf[0] = (len >> 24) & 0x000000ff;
			buf[1] = (len >> 16) & 0x000000ff;
			buf[2] = (len >> 8) & 0x000000ff;
			buf[3] = (len) & 0x000000ff;

			std::copy(buf, buf + kIntLen, result.begin());	

			_user.outputBuffer.append(result);
			internalSendMessage();
		}	 
	}

	void sendMessageToSpecial(chat::Message& msg, User* user)
	{
		std::string result;
		result.resize(kIntLen);	//Reserved 4 bytes for head
		int len;
		bool succeed = msg.AppendToString(&result);
		if (succeed)
		{
			len = result.length() - kIntLen;
			char buf[kIntLen];
			buf[0] = (len >> 24) & 0x000000ff;
			buf[1] = (len >> 16) & 0x000000ff;
			buf[2] = (len >> 8) & 0x000000ff;
			buf[3] = (len) & 0x000000ff;

			std::copy(buf, buf + kIntLen, result.begin());	

			user->outputBuffer.append(result);

			int sended = 0;
			while (user->outputBuffer.readableBytes() > 0)
			{
				sended = user->socket.sendBytes(user->outputBuffer.peek(), user->outputBuffer.readableBytes());
				user->outputBuffer.retrieve(sended);
			} 
		}
	}

	void sendMessageToOthers(chat::Message& msg)
	{
		std::string result;
		result.resize(kIntLen);	//Reserved 4 bytes for head
		int len;
		bool succeed = msg.AppendToString(&result);
		if (succeed)
		{
			len = result.length() - kIntLen;
			char buf[kIntLen];
			buf[0] = (len >> 24) & 0x000000ff;
			buf[1] = (len >> 16) & 0x000000ff;
			buf[2] = (len >> 8) & 0x000000ff;
			buf[3] = (len) & 0x000000ff;

			std::copy(buf, buf + kIntLen, result.begin());	

			for (vector<User*>::const_iterator it = s_users.begin();
				it != s_users.end();
				++it)
			{
				if ((*it)->sessionId  == this->_user.sessionId || !this->_user.login)
					continue;

				(*it)->outputBuffer.append(result);

				int sended = 0;
				while ((*it)->outputBuffer.readableBytes() > 0)
				{
					sended = (*it)->socket.sendBytes((*it)->outputBuffer.peek(), (*it)->outputBuffer.readableBytes());
					(*it)->outputBuffer.retrieve(sended);
				} 
			}
		}
	}


	void internalSendMessage()
	{
		int sended = 0;
		while (_user.outputBuffer.readableBytes() > 0)
		{
			sended = _user.socket.sendBytes(_user.outputBuffer.peek(), _user.outputBuffer.readableBytes());
			_user.outputBuffer.retrieve(sended);
		} 
	}

	chat::Message* BuildFriendNotification(bool online)
	{
		chat::FriendNotification* friendNotification = chat::FriendNotification::default_instance().New();
		friendNotification->set_name(_user.userName);
		friendNotification->set_online(online);

		chat::Notification* notification = chat::Notification::default_instance().New();
		notification->set_allocated_friend_(friendNotification);

		chat::Message* friend_notification_msg = chat::Message::default_instance().New();
		friend_notification_msg->set_msg_type(chat::Friend_Notification);

		friend_notification_msg->set_sequence(sendToAllUserSequence);
		friend_notification_msg->set_allocated_notification(notification);

		return friend_notification_msg;
	}

	void onLogin_Request(chat::Message& recv_msg)
	{
		_user.userName = recv_msg.request().login().username();
		chat::LoginResponse* loginResponse = chat::LoginResponse::default_instance().New();
		loginResponse->set_ttl(10);

		chat::Response* resopnse = chat::Response::default_instance().New();
		resopnse->set_result(true);
		resopnse->set_last_response(true);			
		resopnse->set_allocated_login(loginResponse);

		chat::Message* login_rsp = chat::Message::default_instance().New();
		login_rsp->set_msg_type(chat::Login_Response);
		login_rsp->set_sequence(recv_msg.sequence());
		login_rsp->set_session_id(this->_user.sessionId);		
		login_rsp->set_allocated_response(resopnse);

		_user.login = true;
		sendMessage(*login_rsp);
		delete login_rsp;

		chat::Message* friendNotification =  BuildFriendNotification(true);

		sendMessageToOthers(*friendNotification);
		delete friendNotification;
	}

	void onLogout_Request(chat::Message& recv_msg)
	{
		chat::Response* resopnse = chat::Response::default_instance().New();
		resopnse->set_result(true);
		resopnse->set_last_response(true);			

		chat::Message* logout_rsp = chat::Message::default_instance().New();
		logout_rsp->set_msg_type(chat::Logout_Response);
		logout_rsp->set_sequence(recv_msg.sequence());
		logout_rsp->set_session_id(this->_user.sessionId);		
		logout_rsp->set_allocated_response(resopnse);

		_user.login = true;
		sendMessage(*logout_rsp);
		delete logout_rsp;

		chat::Message* friendNotification =  BuildFriendNotification(false);

		sendMessageToOthers(*friendNotification);
		delete friendNotification;

		delete this;
	}

	void onKeepalive_Request(chat::Message& recv_msg)
	{
		chat::Response* resopnse = chat::Response::default_instance().New();
		resopnse->set_result(true);
		resopnse->set_last_response(true);			

		chat::Message* rsp_msg = chat::Message::default_instance().New();
		rsp_msg->set_msg_type(chat::Keepalive_Response);
		rsp_msg->set_sequence(recv_msg.sequence());
		rsp_msg->set_session_id(this->_user.sessionId);		
		rsp_msg->set_allocated_response(resopnse);

		sendMessage(*rsp_msg);
		delete rsp_msg;
	}

	void onGet_Friends_Request(chat::Message& recv_msg)
	{
		chat::GetFriendsResponse* friends = chat::GetFriendsResponse::default_instance().New();

		for (vector<User*>::const_iterator it = s_users.begin();
			it != s_users.end();
			++it)
		{
			chat::Friend* onefriend = friends->add_friends();
			onefriend->set_name((*it)->userName);
			onefriend->set_online((*it)->login);
		}

		chat::Response* resopnse = chat::Response::default_instance().New();
		resopnse->set_result(true);
		resopnse->set_last_response(true);	
		resopnse->set_allocated_get_friends(friends);

		chat::Message* rsp_msg = chat::Message::default_instance().New();
		rsp_msg->set_msg_type(chat::Get_Friends_Response);
		rsp_msg->set_sequence(recv_msg.sequence());
		rsp_msg->set_session_id(this->_user.sessionId);		
		rsp_msg->set_allocated_response(resopnse);

		sendMessage(*rsp_msg);
		delete rsp_msg;
	}

	void onSend_Message_Request(chat::Message& recv_msg)
	{
		chat::Response* resopnse = chat::Response::default_instance().New();
		resopnse->set_result(true);
		resopnse->set_last_response(true);	

		chat::Message* rsp_msg = chat::Message::default_instance().New();
		rsp_msg->set_msg_type(chat::Send_Message_Response);
		rsp_msg->set_sequence(recv_msg.sequence());
		rsp_msg->set_session_id(this->_user.sessionId);		
		rsp_msg->set_allocated_response(resopnse);

		sendMessage(*rsp_msg);
		delete rsp_msg;

		chat::MessageNotification* messageNotification = chat::MessageNotification::default_instance().New();
		messageNotification->set_sender(this->_user.userName);
		std::string text = recv_msg.request().send_message().text();
		messageNotification->set_text(text);
		LocalDateTime now;
		std::string str = DateTimeFormatter::format(now, DateTimeFormat::SORTABLE_FORMAT);
		str = "   " + str;
		messageNotification->set_timestamp(str);

		chat::Notification* notification = chat::Notification::default_instance().New();
		notification->set_allocated_msg(messageNotification);

		chat::Message* text_msg = chat::Message::default_instance().New();
		text_msg->set_msg_type(chat::Message_Notification);
		text_msg->set_sequence(sendToAllUserSequence);		
		text_msg->set_allocated_notification(notification);

		if (recv_msg.request().send_message().has_receiver())
		{
			string receiver = recv_msg.request().send_message().receiver();
			for (vector<User*>::const_iterator it = s_users.begin();
				it != s_users.end();
				++it)
			{
				if ((*it)->userName == receiver)
				{
					sendMessageToSpecial(*text_msg, *it);			
				}
				break;				
			}
		}
		else
		{
			sendMessageToOthers(*text_msg);
		}
		delete text_msg;
	}

	void onBufferMessage()
	{
		const int kIntLen = sizeof(int);
		while (_user.inputBuffer.readableBytes() >= kIntLen)
		{
			const void* data = _user.inputBuffer.peek();
			const char* tmp = static_cast<const char*>(data);
			int len = (*tmp & 0x000000ff) << 24
				| (*(tmp + 1) & 0x000000ff) << 16
				| (*(tmp + 2) & 0x000000ff) << 8
				| (*(tmp + 3) & 0x000000ff);
			if (len > 65536 || len < 0)
			{
				// "Invalid length "
				cout << "Invalid receive message length " << endl;
				_user.inputBuffer.retrieveAll();
				break;
			}
			else if (_user.inputBuffer.readableBytes() >= len + kIntLen)
			{
				_user.inputBuffer.retrieve(kIntLen);
				string message(_user.inputBuffer.peek(), len);
				_user.inputBuffer.retrieve(len);
				chat::Message recv_msg;
				recv_msg.ParseFromString(message);
				cout << "[ChatServiceHandler][onBufferMessage]recv_msg:" << endl
					<< recv_msg.Utf8DebugString() << endl;

				switch (recv_msg.msg_type())
				{
				case chat::Login_Request:
					{
						onLogin_Request(recv_msg);			
					}
					break;
				case chat::Logout_Request:
					{
						onLogout_Request(recv_msg);
					}
					break;
				case chat::Keepalive_Request:
					{
						onKeepalive_Request(recv_msg);
					}
					break;
				case chat::Get_Friends_Request:
					{
						onGet_Friends_Request(recv_msg);
					}
					break;
				case chat::Send_Message_Request:
					{
						onSend_Message_Request(recv_msg);
					}
					break;
				}
			}
			else
			{
				break;
			}
		}
	}

	void onShutdown(const AutoPtr<ShutdownNotification>& pNf)
	{
		cout << "[ChatServiceHandler][onShutdown]" << endl;
		delete this;
	}

private:
	SocketReactor& _reactor;
	User _user;
};



int main(int argc, char** argv)
{	
	unsigned short port = 9977;

	// set-up a server socket
	ServerSocket svs(port);
	// set-up a SocketReactor...
	SocketReactor reactor;
	// ... and a SocketAcceptor
	SocketAcceptor<ChatServiceHandler> acceptor(svs, reactor);
	// run the reactor in its own thread so that we can wait for 
	// a termination request
	Thread thread;
	thread.start(reactor);

	char c(' ');

	while (c != 'q' && c != 'Q')
	{
		cout << "Press q then enter to quit: \n";
		cin >> c;
	}
	// Stop the SocketReactor
	reactor.stop();
	thread.join();

	google::protobuf::ShutdownProtobufLibrary();
	return 0;
}
