#protobuf_chat

�ο�[�ҵ�Protobuf��Ϣ���ԭ��](http://my.oschina.net/cxh3905/blog/159122)���Լ��ṩ��[demo](http://my.oschina.net/cxh3905/blog/293000)

### 1. ���ܽ��� 
Protobuf_chat��һ���򵥵�������򣬿ͻ���ʹ��C#�������ʹ��C++��ʹ��protobufЭ�顣
֧�ֵ�¼/ע����������Ϣ�������Ȼ������ܡ�

### 2. protocol.proto����
ʹ�� protobuf ��enum������Ϣ�ı�ţ�Ҳ������Ϣ�����͡�
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

��Ϊÿ��������Ϣ�����Ϣ����һ����Ӧ��protobuf message������Login_Request����һ����ӦLoginRequest��Ϣ��

��Ϊÿ����Ϣ���ඨ��һ����Ϣ������������Ϣȫ��������Request��Ϣ�У�Ӧ����Ϣȫ��������Response��Ϣ�У�֪ͨ��Ϣȫ��������Notification��Ϣ�С�

����Ӧ����Ϣ���������ǳɹ��ģ������Ӧ����Ϣ�л����������2���ֶΡ�һ����������Ӧ���Ƿ�ɹ���һ����������ʧ��ʱ���ַ�����Ϣ�� �����ж��Ӧ�����Ϣ��˵�����ܻ�����Ƿ�Ϊ���һ��Ӧ����Ϣ�ı�ʶ��Ӧ�����ţ��������������ݰ����ְ��Ժ�Э��Ҫ�ϲ�ʱ����Ҫ֪����Ƭ�ڰ��еľ���λ�ã���

����Ҷ���һ������Ϣ����Request��Response��Notificationȫ����װ��һ���ú���ͨ�ŵ�ʱ�򶼶�����Ϣ��ʼ����롣



