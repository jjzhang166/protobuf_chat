#protobuf_chat

参考[我的Protobuf消息设计原则](http://my.oschina.net/cxh3905/blog/159122)，以及提供的[demo](http://my.oschina.net/cxh3905/blog/293000)

### 1. 功能介绍 
Protobuf_chat是一个简单的聊天程序，客户端使用C#，服务端有C++/C#两个版本，使用protobuf协议。

C++部分引用了[poco](http://pocoproject.org/)的Net库。

支持登录/注销，发送消息，心跳等基本功能。

### 2. protocol.proto解释
使用 protobuf 的enum定于消息的编号，也就是消息的类型。
```
enum MSG
{
	Login_Request		=	10001;
	Login_Response		=	10002;
	Logout_Request		=	10003;
	Logout_Response		=	10004;
	Keepalive_Request	=	10005;
	Keepalive_Response	=	10006;

	Get_Friends_Request	=	10007;
	Get_Friends_Response 	=	10008;
	Send_Message_Request	= 	10009;
	Send_Message_Response	= 	10010;

	Friend_Notification	= 	20001;
	Message_Notification	= 	20002;
	Welcome_Notification	= 	20003;
}
``` 

会为每个具有消息体的消息定义一个对应的protobuf message。例如Login_Request会有一个对应LoginRequest消息。

会为每个消息大类定义一个消息，例如请求消息全部包含在Request消息中，应答消息全部包含在Response消息中，通知消息全部包含在Notification消息中。

对于应答消息，并非总是成功的，因此在应答消息中还会包含另外2个字段。一个用于描述应答是否成功，一个用于描述失败时的字符串信息。 对于有多个应答的消息来说，可能会包含是否为最后一个应答消息的标识。应答的序号（类似与网络数据包被分包以后，协议要合并时，需要知道分片在包中的具体位置）。
```
message Response
{
	required bool	result	=	1;
	required bool	last_response	=	2;
	optional bytes	error_describe	=	3;

	optional LoginResponse	login	= 4;
	optional GetFriendsResponse	get_friends	= 5;
}
```

最后我定义一个大消息，把Request、Response、Notification全部封装在一起，让后在通信的时候都动大消息开始编解码。
```
message Message 
{
	required MSG		msg_type	=	1;
	required fixed32	sequence	=	2;
	optional fixed32	session_id	=	3;

	optional Request	request		=	4;
	optional Response	response	=	5;
	optional Notification notification	=	6;
}

```