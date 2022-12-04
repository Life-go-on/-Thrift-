//本代码在thrift生成的server的模板基础上进行完善得
//实现了match_server的功能
//主要包含两方面：1. 作为服务器接受game发来的add_user和remove_user请求
// 2.作为客户端向save服务器发送save_result的请求
//同时使用多线程，并行的执行以上两个功能

//以下为运行thrift所必须的库，具体可以参考thrift的官方文档
#include <thrift/TToString.h>
#include <thrift/concurrency/ThreadFactory.h>
#include <thrift/concurrency/ThreadManager.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>

//以下为实现客户端和服务器之前通信所必须的库
//可以通过 thrift -r --gen <语言类型> <接口文件> 来自动生成
#include "match_server/Match.h"
#include "save_client/Save.h"

//以下为实现具体功能所引入的C++库函数
#include <unistd.h>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

using namespace ::match_service;
using namespace ::save_service;

using namespace std;

struct Task {  //消息队列中存储的一个任务单元
	User user;
	string type;  //任务类型
};

struct MessageQueue {  //消息队列
	queue<Task> q;
	mutex m;                //互斥锁，实现互斥
	condition_variable cv;  //条件变量，实现同步
	//二者共同构成了OS中学习到的P(),V()操作
} mq;

class Pool {  //玩家池
	private:
		vector<User> users;
		vector<int> waits;  //等待时间
	public:
		void add(const User& user) {
			users.push_back(user);
			waits.push_back(0);
		}

		void remove(const User& user) {
			for (int i = 0; i < users.size(); i++) {
				if (users[i].id == user.id) {
					users.erase(users.begin() + i);
					waits.erase(waits.begin() + i);
					break;
				}
			}
		}

		void match() {
			for (int i = 0; i < waits.size(); i++)
				waits[i]++;  // 等待轮次 + 1
			//寻找可以匹配的玩家
			while (users.size() > 1) {
				bool exist = false;  //记录当前的玩家池中是否存在可以匹配的两个人
				for (int i = 0; i < users.size(); i++) {
					for (int j = i + 1; j < users.size(); j++) {
						if (isMatch(i, j)) {
							auto a = users[i], b = users[j];
							//由于j在i后，所以一定要先删j
							//如果先删i，就会导致迭代器无法正确找到j的位置
							users.erase(users.begin() + j);
							waits.erase(waits.begin() + j);

							users.erase(users.begin() + i);
							waits.erase(waits.begin() + i);

							save_result(a.id, b.id);
							exist = true;
							break;
						}
					}
					if (exist)
						break;
				}
				if (!exist)  //如果不存在可以匹配的两个人，则直接退出，避免死循环占用cpu
					break;
			}
		}

		bool isMatch(int i, int j) {
			//某个玩家容许的分差=等待轮次*50
			int deltA = waits[i] * 50;
			int deltB = waits[j] * 50;
			int diff = abs(users[i].score - users[j].score);
			//当diff<=deltA，说明A可以接受B
			//当diff<=deltB，说明B可以接受A
			//当二者同时成立时，双方才能够互相匹配上
			return diff <= deltA && diff <= deltB;
		}

		void save_result(int id1, int id2) {
			cout << "Match Result:" << id1 << ' ' << id2 << endl;

			std::shared_ptr<TTransport> socket(new TSocket(
						"123.57.47.211", 9090));  // 123.57.47.211为save服务器的地址
			std::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
			std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
			SaveClient client(protocol);

			try {
				transport->open();

				int32_t res = client.save_data("acs_7699", "a179c9f0", id1,
						id2);  // user_name,password,player1_id,player2_id
				if(res) cout<<"Fail!"<<endl;
				else cout<<"Success!"<<endl;
				transport->close();
			} catch (TException& tx) {
				cout << "ERROR: " << tx.what() << endl;
			}
		}
} pool;

class MatchHandler : virtual public MatchIf {
	public:
		MatchHandler() {}

		int32_t add_user(const User& user, const std::string& info) {
			cout<<"add user:"<<user.id<<' '<<user.name<<' '<<user.score<<endl;//添加调试信息
			unique_lock<mutex> lck(mq.m);  //上锁
			//由于unique_lock类的析构函数中有解锁的操作，因此在add_user()执行完后直接帮我们进行了解锁操作
			mq.q.push({user, "add"});
			//唤醒被阻塞的线程（这里主要是consume_task()会被阻塞）
			mq.cv.notify_all();

			return 0;
		}

		int32_t remove_user(const User& user, const std::string& info) {
			cout<<"remove user:"<<user.id<<' '<<user.name<<' '<<user.score<<endl;//添加调试信息
			unique_lock<mutex> lck(mq.m);
			mq.q.push({user, "remove"});
			mq.cv.notify_all();

			return 0;
		}
};

class MatchCloneFactory : virtual public MatchIfFactory {
	public:
		~MatchCloneFactory() override = default;
		MatchIf* getHandler(const ::apache::thrift::TConnectionInfo& connInfo) override {
			std::shared_ptr<TSocket> sock =
				std::dynamic_pointer_cast<TSocket>(connInfo.transport);
			return new MatchHandler;
		}
		void releaseHandler(MatchIf* handler) override { delete handler; }
};

void consume_task() {
	while (1) {
		unique_lock<mutex> lck(mq.m);
		if (mq.q.empty()) {
			// message_queue.cv.wait(lck);
			lck.unlock();
			pool.match();
			sleep(1);
		} else {  //如果消息队列非空
			Task t = mq.q.front();
			mq.q.pop();
			lck.unlock();  //解锁
			//这里要提前解锁是为了在处理任务的过程中使得add_user()和remove_user()还能继续向队列中添加任务
			if (t.type == "add")
				pool.add(t.user);
			else if (t.type == "remove")
				pool.remove(t.user);
		}
	}
}

int main(int argc, char** argv) {
	int port = 8080;  // thrift默认为9090，这里我们自定义为8080
	//注意match client端也要相应的进行修改
	TThreadedServer server(std::make_shared<MatchProcessorFactory>(
				std::make_shared<MatchCloneFactory>()),
			std::make_shared<TServerSocket>(port),
			std::make_shared<TBufferedTransportFactory>(),
			std::make_shared<TBinaryProtocolFactory>());

	cout << "Start Match Server" << endl;

	thread matching_thread(consume_task);
	server.serve();
	return 0;
}

