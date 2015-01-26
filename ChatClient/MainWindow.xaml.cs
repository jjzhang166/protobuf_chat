using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using pb = Google.ProtocolBuffers;
using System.Collections.ObjectModel;
using System.Timers;
using System.Windows.Threading;

namespace ChatClient
{
    /// <summary>
    /// MainWindow.xaml 的交互逻辑
    /// </summary>
    public partial class MainWindow : Window
    {
        private Session session = null;
        private ObservableCollection<string> my_friends = new ObservableCollection<string>();
        private ObservableCollection<TextMessage> text_messages = new ObservableCollection<TextMessage>();
        private Timer timeoutTimer = new Timer();
        private bool reveive_KeepaliveResponse = false;
        private delegate void TimerDispatcherDelegate();

        public MainWindow()
        {
            InitializeComponent();
        }

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
            ipaddress.Text = "127.0.0.1";
            port.Text = "9977";
            username.Text = "";
            list_friends.ItemsSource = my_friends;
            list_messages.ItemsSource = text_messages;

            timeoutTimer.Interval = 5000;
            timeoutTimer.Elapsed += new ElapsedEventHandler(MsgTimeOutChecking);
            timeoutTimer.AutoReset = true;
            timeoutTimer.Enabled = false;
        }

        private void OnDisconnectedUpdate()
        {
            session.SessionId = 0;
            this.login.IsEnabled = false;
            this.logout.IsEnabled = false;
            this.send_message.IsEnabled = false;
            this.timeoutTimer.Enabled = false;
        }
        /// <summary>
        /// 主动触发超时消息回调
        /// </summary>
        /// <returns></returns>
        private void MsgTimeOutChecking(object sender, ElapsedEventArgs e)
        {
            if (!reveive_KeepaliveResponse)
            {
                this.Dispatcher.Invoke(DispatcherPriority.Normal, new TimerDispatcherDelegate(OnDisconnectedUpdate));
                MessageBox.Show("Disconnect server");                            
            }
            else
            {
                reveive_KeepaliveResponse = false;
                OnKeepalive_Request();
            }
        }

        void OnMessage(chat.Message msg)
        {
            if( msg.HasNotification )
            {
                if (msg.Notification.HasWelcome)
                {
                    this.Title = msg.Notification.Welcome.Text.ToStringUtf8();
                }
                else if( msg.Notification.HasFriend )
                {
                    var name = msg.Notification.Friend.Name.ToStringUtf8();
                    if( msg.Notification.Friend.Online )
                    {
                        my_friends.Add(name);
                    }
                    else
                    {
                        my_friends.Remove(name);
                    }
                }
                else if (msg.Notification.HasMsg)
                {
                    var txtMsg = new TextMessage(msg.Notification.Msg);
                    text_messages.Add(txtMsg);

                    list_messages.SelectedIndex = list_messages.Items.Count - 1;
                    list_messages.ScrollIntoView(list_messages.SelectedItem);
                }
            }
        }
        void OnConnection(IAsyncResult ar)
        {    
             Login();
        }

        void OnKeepalive_Request()
        {
            chat.Message KeepaliveRequest = new chat.Message.Builder()
            {
                MsgType = chat.MSG.Keepalive_Request,
                Sequence = session.Sequence
            }.Build();

            Transaction<chat.Message>.AddRequest(KeepaliveRequest.Sequence, (chat.Message rsp_msg) =>
            {
                if (rsp_msg.HasResponse && rsp_msg.Response.HasResult && rsp_msg.Response.Result)
                {
                    reveive_KeepaliveResponse = true;
                }
            });

            session.SendMessage(KeepaliveRequest);
        }

        void Login()
        {
            chat.Message login = new chat.Message.Builder()
            {
                MsgType = chat.MSG.Login_Request,
                Sequence = session.Sequence,
                Request = new chat.Request.Builder()
                {
                    Login = new chat.LoginRequest.Builder()
                    {
                        Username = pb.ByteString.CopyFromUtf8(username.Text)
                    }.Build()
                }.Build()
            }.Build();

            Transaction<chat.Message>.AddRequest(login.Sequence, (chat.Message rsp_msg) => {
                if (rsp_msg.HasResponse && rsp_msg.Response.HasResult && rsp_msg.Response.Result)
                {
                    session.SessionId = rsp_msg.SessionId;
                    this.login.IsEnabled = false;
                    this.logout.IsEnabled = true;
                    GetFriends();

                    OnKeepalive_Request();
                    this.timeoutTimer.Enabled = true;
                }
            });
            session.SendMessage(login);
        }
        void GetFriends()
        {
            chat.Message msg = new chat.Message.Builder()
            {
                MsgType = chat.MSG.Get_Friends_Request,
                Sequence = session.Sequence,
                SessionId = session.SessionId,
            
            }.Build();
            Transaction<chat.Message>.AddRequest(msg.Sequence, (chat.Message rsp_msg) =>
            {
                if( rsp_msg.HasResponse && rsp_msg.Response.HasGetFriends )
                {
                    var get_friends = rsp_msg.Response.GetFriends;
                    foreach( var friend in get_friends.FriendsList )
                    {
                        my_friends.Add(friend.Name.ToStringUtf8());
                    }
                }
            });
            session.SendMessage(msg);
        }
        void OnClose(IAsyncResult ar)
        {
            session = null;
        }

        private void login_Click(object sender, RoutedEventArgs e)
        {
            if (ipaddress.Text == "" || port.Text == "" || username.Text == "")
            {
                MessageBox.Show("Username/IPAddress/Port cannot be empty");
                return;
            }

            if( session == null )
            {
                session = new Session();
                session.Address = ipaddress.Text;
                session.Port = Int32.Parse(port.Text);
                session.MessageHandler = OnMessage;
                session.OnClose = OnClose;
                session.OnConnection = OnConnection;
                session.Dispatcher = this.Dispatcher;

                session.StartConnection();
            }
            else
            {
                Login();
            }
        }

        private void logout_Click(object sender, RoutedEventArgs e)
        {
            chat.Message msg = new chat.Message.Builder()
            {
                MsgType = chat.MSG.Logout_Request,
                Sequence = session.Sequence,
                SessionId = session.SessionId,

            }.Build();
            Transaction<chat.Message>.AddRequest(msg.Sequence, (chat.Message rsp_msg) =>
            {
                session.SessionId = 0;
                my_friends.Clear();
                text_messages.Clear();
                this.login.IsEnabled = true;
                this.logout.IsEnabled = false;
                this.timeoutTimer.Enabled = false;
            });
            session.SendMessage(msg);
        }

        private void send_message_Click(object sender, RoutedEventArgs e)
        {
            if (msg_text.Text.Length == 0)
            {
                return;
            };

            chat.SendMessageRequest sendMessageRequest;
            if (list_friends.SelectedIndex >= 0 
                && list_friends.SelectedItem as string != this.username.Text)
            {
                sendMessageRequest = new chat.SendMessageRequest.Builder()
                {
                    Receiver = pb.ByteString.CopyFromUtf8(list_friends.SelectedItem as string),
                    Text = msg_text.Text
                }.Build();
            }
            else
            {
                sendMessageRequest = new chat.SendMessageRequest.Builder()
                {
                    Text = msg_text.Text
                }.Build();

            }

            chat.Message msg = new chat.Message.Builder()
            {
                MsgType = chat.MSG.Send_Message_Request,
                Sequence = session.Sequence,
                SessionId = session.SessionId,
                Request = new chat.Request.Builder()
                {
                    SendMessage = sendMessageRequest
                }.Build()
            }.Build();

            text_messages.Add(new TextMessage("me!!!   ", msg_text.Text));
            Transaction<chat.Message>.AddRequest(msg.Sequence, (chat.Message rsp_msg) =>
            {
                if (rsp_msg.Response.Result)
                {
                    msg_text.Text = "";
                }
            });
            session.SendMessage(msg);
        }

        private void Window_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            if (session == null || session.SessionId == 0) // already logout
            {
                return;
            }

            session.SessionId = 0;
            chat.Message msg = new chat.Message.Builder()
            {
                MsgType = chat.MSG.Logout_Request,
                Sequence = session.Sequence,
                SessionId = session.SessionId,

            }.Build();
            session.SendMessage(msg);
            this.timeoutTimer.Enabled = false;
        }
    }

    public class TextMessage
    {
        public TextMessage( chat.MessageNotification msgNotify)
        {
            Sender = msgNotify.Sender;
            Timestamp = msgNotify.Timestamp;
            Text = msgNotify.Text;
        }
        public TextMessage( string sender, string text)
        {
            Sender = sender;
            Text = text;
            Timestamp = DateTime.Now.ToString();
        }
        public string Sender
        {
            get;
            set;
        }
        public string Timestamp
        {
            get;
            set;
        }
        public string Text
        {
            get;
            set;
        }
    }
}
