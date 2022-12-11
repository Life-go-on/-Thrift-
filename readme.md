

具体的视频教程参考ACwing，[Linux基础课第六章——thrif](https://www.acwing.com/video/3479/)

# 1. Thrift 简介

[thrift官网](https://thrift.apache.org/) **官网教程:进入官网->Tutorial->tutorial.thrift**

**Apache Thrift**软件框架用于可伸缩的跨语言服务开发，它将**软件栈**和**代码生成引擎**结合在一起，以构建在C++、Java、Python、PHP、Ruby、Erlang、Perl、Haskell、C#、Cocoa、JavaScript、Node.js、Smalltalk、OCaml和Delphi等语言之间高效、无缝地工作的服务。

**Thrift使用C++进行编写**，在安装使用的时候需要安装依赖，windows安装方式见官网即可。安装方式：[thrift官网介绍安装方式](http://thrift.apache.org/docs/install/) 

---

Thrift 采用IDL（Interface Definition Language）来定义通用的服务接口，然后通过Thrift提供的编译器，可以将服务接口编译成不同语言编写的代码，通过这个方式来实现跨语言的功能。

这里的 **接口**，可以简单理解为通信双方传递的信息，例如某些变量，处理变量的函数等。

- 通过命令 `thrift -r --gen <language> <Thrift filename>` 调用Thrift提供的编译器将服务接口编译成不同语言编写的代码。
- 这些代码又分为服务端和客户端，将所在不同进程(或服务器)的功能连接起来。
---

创建一个Thrif的简要过程为：

1. 定义服务接口(存放接口的文件夹就是thrift文件)
2. 作为服务端的服务，需要生成server。
3. 作为请求端的服务，需要生成client。

# 2. 简单的应用——游戏匹配服务

## 2.1 分析业务逻辑

![无标题.png](https://cdn.acwing.com/media/article/image/2021/10/05/97206_32d5aa8525-无标题.png) 

---

**分析图示内容**

一个游戏匹配服务的功能可能运行在一个或多个服务器(或进程)上，而 `thrift` 就是将不同服务器不同语言的功能连接起来。

图中的三个节点(功能)是完全独立的，既可以在同一个服务器上，也可以在不同服务器上。

每一个节点就是一个进程，每个进程可以使用不同的语言来实现。

1. 在GAME节点上实现 `match_client`，通过调用 `match_server` 中实现的两个服务接口函数获取功能，实现跨语言跨服务的工作。
2. 在匹配系统节点上实现 `match_server`和 `save_clinet`
   - `match_server`主要实现两个服务接口函数，供 `match_clinet`调用。主要是提供一个匹配池，从而可以不断的接收玩家和删除玩家，并同时根据一定的规则给每个玩家安排一局游戏。
   - `save_client`主要实现调用 `save_server`提供的函数接口 （注`save_server`提供的函数接口在本例中不用我们自己实现）来记录匹配的情况。
3. 每个节点(功能)之间通过 $thrift$ 定义的服务接口作为有向边进行连接。弧尾所在的节点创建客户端，弧头所在的节点创建服务端。

## 2.2  接口文件的编写

这里为了方便我们需要创建两个文件夹来分别表示game节点(game)和匹配服务节点(match_system)，简单起见，我们将这二者放在同一个服务器上，而数据存储节点的服务端已经实现好了，只要调用服务接口实现的函数即可。

接下来创建一个 `thrift` 文件夹存储 `.thrift` 文件，`.thrift`文件定义服务接口。其中用两个`.thrift`文件分别表示两条有向边，一条有向边可以包含多个服务接口。

这里我们只要定义 `match_client`和 `match_server`之间的接口文件，命名为 `match.thrift`，在这个文件中主要涉及3个信息：

1. 用户的信息（这里主要包括id,name,score)
2. 添加用户的函数
3. 删除用户的函数

参考官方文档，进行接口的书写

### 2.2.1  名字空间namespace

Thrift中的命名空间类似于C++中的namespace，提供了一种组织（隔离）代码的简便方式（即解决类型定义中的名字冲突问题）。

由于每种语言均有自己的命名空间定义方式（如:python中有module）, 因此 thrift 提供了统一的接口来让开发者根据特点的语言来定义自己想要的namespace。

**教程中的介绍:**

```
/**
 * Thrift files can namespace, package, or prefix their output in various
 * target languages.
 */

namespace cl tutorial
namespace cpp tutorial  
namespace d tutorial
namespace dart tutorial
namespace java tutorial
namespace php tutorial
namespace perl tutorial
namespace haxe tutorial
namespace netstd tutorial

//总而言之，语法格式为:namespace 使用的语言 空间名称
```

由于匹配系统采用 `c++`实现，因此这里可以定义命名空间为：`namespace cpp match_service`

### 2.2.2  定义交流信息

客户端与服务端之间交流的信息通常在**结构体** `struct` 中定义，有以下一些约束：

1. struct不能继承，但是可以嵌套，不能嵌套自己。(0.12.0版本可以支持嵌套自己本身)
2. 其成员都是有明确类型
3. 成员是被正整数编号过的，其中的编号使不能重复的，这个是为了在传输过程中编码使用。
4. 成员分割符可以是逗号（,）或是分号（;），而且可以混用
6. 每个字段可以设置默认值
7. 同一文件可以定义多个struct，也可以定义在不同的文件，通过include引入。

**教程中介绍:**
```
/**
 * Structs are the basic complex data structures. They are comprised of fields
 * which each have an integer identifier, a type, a symbolic name, and an
 * optional default value.
 *
 * Fields can be declared "optional", which ensures they will not be included
 * in the serialized output if they aren't set.  Note that this requires some
 * manual management in some languages.
 */
 
//注意，每个结构体中的变量都有`optional`和`required`之分，如果不指定则默认为`optional`类型
//optional表示不填充则不序列化
//required表示必须填充也必须序列化。
//这里我们暂时用不到这两个信息，因此可以先不管。

struct Work {
  1: i32 num1 = 0, //默认值
  2: i32 num2, //默认字段类型是optional
  3: Operation op,
  4: optional string comment,
  5: required string name, //本字段必须填充
}
```

这里我们定义结构体用来存储用户信息。其中i32表示int，string表示字符串。

```
struct User {
    1: i32 id,
    2: string name,
    3: i32 score,
}
```

### 2.2.3  定义服务接口

$Thrift$ 将服务接口定义在 `service`中，定义的方法和 `struct`类似，只不过每个函数内的参数也要进行编号

**教程中介绍:**

```
/**
 * Ahh, now onto the cool part, defining a service. Services just need a name
 * and can optionally inherit from another service using the extends keyword.
 */
service Calculator extends shared.SharedService {

  /**
   * A method definition looks like C code. It has a return type, arguments,
   * and optionally a list of exceptions that it may throw. Note that argument
   * lists and exception lists are specified using the exact same syntax as
   * field lists in struct or exception definitions.
   */

   void ping(),

   i32 add(1:i32 num1, 2:i32 num2),

   i32 calculate(1:i32 logid, 2:Work w) throws (1:InvalidOperation ouch),

   /**
    * This method has a oneway modifier. That means the client only makes
    * a request and does not listen for any response at all. Oneway methods
    * must be void.
    */
   oneway void zip()

}
```

`match_clint`要用到的主要是两个接口：

1. `add_user()`，用于向匹配池中添加玩家
2. `remove_user()`用于向匹配池中删除玩家

为了方便期间，我们定义接口的参数时，可以额外定义一个 `string info`用于表示额外信息，这样当以后想要更改接口时，可以直接将想传的信息传入到info中即可（可以直接认为info是一个序列化json文件）
```
service Match {
    i32 add_user(1: User user, 2: string info),

    i32 remove_user(1: User user, 2: string info),
}
```

---
## 2.3   match_server的初步实现

### 2.3.1  源文件的生成 (match_server 1.0)

$Thrift$可以通过我们定义好的接口，自动生成对应客户端与服务端的源文件，这里我们先实现 $cpp$ 形式的 `match_server`的源文件

```
thrift -r --gen xxx xxxx
//xxx表示我们要生成的源文件所使用的语言
//xxxx表示从当前目录到定义接口的路径
```
因此这里我们可以输入：

```
thrift -r --gen cpp tutorial.thrift 
//具体实现时，要根据接口文件的存放位置来调整路径
```

具体操作如图所示：
![2021-10-02_213443.png](https://cdn.acwing.com/media/article/image/2021/10/03/97206_24b6e98123-2021-10-02_213443.png) 

定义好接口之后，通过命令就可以直接生成C++版本的服务端相关的代码，但是具体业务我们还是需要具体书写。

---
### 2.3.2  编译生成的 .cpp 文件

1. 编译：

![2021-10-03_092958.png](https://cdn.acwing.com/media/article/image/2021/10/03/97206_13c8927a23-2021-10-03_092958.png) 

2. 链接：

   ![2021-10-03_095345.png](https://cdn.acwing.com/media/article/image/2021/10/03/97206_26c2852823-2021-10-03_095345.png)

   ![2021-10-03_100004.png](https://cdn.acwing.com/media/article/image/2021/10/03/97206_5ee3260f23-2021-10-03_100004.png) 
   ![2021-10-03_103957.png](https://cdn.acwing.com/media/article/image/2021/10/03/97206_d47a471223-2021-10-03_103957.png) 

- 好习惯：可执行文件和编译好的文件最好不要加进去，只加.cpp和.h文件。

C++编译很慢，链接很快。所以每次修改项目，重新编译时，**只需要编译修改过的.cpp文件即可**，防止编译时间过长。即修改哪个文件就编译哪个文件。

---
## 2.4  match_client的初步实现

### 2.4.1  源文件的生成 （match_client 1.0)

由于我们使用 `python`来实现 `match_client`，因此这里还要再次调用 `thrift -r --gen` 命令

![2021-10-03_152548.png](https://cdn.acwing.com/media/article/image/2021/10/03/97206_48b9a10724-2021-10-03_152548.png) 
![2021-10-03_131330.png](https://cdn.acwing.com/media/article/image/2021/10/04/97206_0dfd243725-2021-10-03_131330.png) 
![2021-10-03_154417.png](https://cdn.acwing.com/media/article/image/2021/10/03/97206_90e9ef8424-2021-10-03_154417.png) 
![2021-10-03_155550.png](https://cdn.acwing.com/media/article/image/2021/10/03/97206_e47583d524-2021-10-03_155550.png) 

### 2.4.2 match_client的优化 (match_client 2.0)

初始的客户端还不是很方便，现在我们希望客户端可以**直接从标准输入中获取信息**，按照下图进行代码的优化

![2021-10-03_161948.png](https://cdn.acwing.com/media/article/image/2021/10/03/97206_c14535c824-2021-10-03_161948.png) 

## 2.5  save_client的实现

`save_client`主要是实现将匹配结果传给 `save_server`进行保存。因为一个节点(功能)只能有一个main.cpp作为程序的入口，所以我们把 `match_server`和 `save_client`写在同一个main.cpp中。

1. 复制教程中的`client`端的代码，如下：

   ```c++
   int main() {
     std::shared_ptr<TTransport> socket(new TSocket("localhost", 9090));
     std::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
     std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
     CalculatorClient client(protocol);
   
     try {
       transport->open();
   
       client.ping();
       cout << "ping()" << endl;
   
       cout << "1 + 1 = " << client.add(1, 1) << endl;
   
       Work work;
       work.op = Operation::DIVIDE;
       work.num1 = 1;
       work.num2 = 0;
   
       try {
         client.calculate(1, work);
         cout << "Whoa? We can divide by zero!" << endl;
       } catch (InvalidOperation& io) {
         cout << "InvalidOperation: " << io.why << endl;
         // or using generated operator<<: cout << io << endl;
         // or by using std::exception native method what(): cout << io.what() << endl;
       }
   
       work.op = Operation::SUBTRACT;
       work.num1 = 15;
       work.num2 = 10;
       int32_t diff = client.calculate(1, work);
       cout << "15 - 10 = " << diff << endl;
   
       // Note that C++ uses return by reference for complex types to avoid
       // costly copy construction
       SharedStruct ss;
       client.getStruct(ss, 1);
       cout << "Received log: " << ss << endl;
   
       transport->close();
     } catch (TException& tx) {
       cout << "ERROR: " << tx.what() << endl;
     }
   }
   ```

   删去其中用不到的信息，并更改相关细节（例如带Calculator的字段都需要更改），即可得到我们需要的 `save_result()`函数

   ```c++
   void save_result(int id1, int id2) {
       cout << "Match Result:" << id1 << ' ' << id2 << endl;
   
       std::shared_ptr<TTransport> socket(new TSocket("123.57.47.211", 9090));
       std::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
       std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
       SaveClient client(protocol);
   
       try {
           transport->open();
   
      		client.save_data("acs_7699","a179c9f0",id1,id2);
           //user_name,password,player1_id,player2_id
   
           transport->close();
       } catch (TException &tx) {
           cout << "ERROR: " << tx.what() << endl;
       }
   }
   
   ```

## 2.6  match_server的完善

我们在前面已经 让$Thrift$ 根据接口自动生成了 `match_server` 的源文件，但是具体的细节还需要我们自己实现

分析 `match_server`的任务，不难发现我们需要：

1. 响应 `match_client`的请求，实现增加用户和删除用户的功能
2. 实现 `match_server` 的功能，即监控当前用户池内的用户，并不断的进行匹配
3. 实现 `save_client`的请求，并将匹配的结果传输给 `save_client`进行记录

第三个要求我们已经在2.4中实现了，接下来考虑实现1和2，为此我们要定义两个数据结构：

1. 消息队列：用于存放 `match_clinet`的请求（`Task`）
2. 玩家池：用于存放当前等待匹配的玩家

具体的作用为：

1. 每有一个客户端的请求到来，我们就向消息队列中增加一个请求
2. 服务端从消息队列中取出请求，然后按照要求向玩家池添加/删除玩家
3. 服务端根据玩家池内的玩家情况进行匹配

为了同时实现上述的几个功能，我们考虑利用多线程来实现，c++实现多线程需要用到消息队列，互斥锁，信号量等知识，具体的实现原理可以参考代码中的注释

同时这里提一个点：编译C++时，如果用到了线程，需要加上线程的动态链接库的参数`-pthread`，因此完整的链接命令为：`g++ *.o -o main -lthrift -pthread`

$tip$：此版本相比 `match_server 1.0`增加了大量代码，因此建议仔细参考源文件中的注释加以理解原理。

## 2.7  match_server的其他补充

### 2.7.1   多线程服务器模式

原本的服务器是单线程模式，即服务器一次只能响应一个客户端的请求，这显然也是不科学的，因此我们可以参考官方文档，将服务求修改为多线程模式，即服务器可以同时响应多个客户端的请求。

![2021-10-05_092449.png](https://cdn.acwing.com/media/article/image/2021/10/05/97206_d6beb85525-2021-10-05_092449.png) 

---
### 2.7.2  完善匹配规则 (match_server 3.0)

我们一开始定义的匹配机制为：两名玩家的分差在50以内就进行匹配。但这显然是不合理的，因此我们修改匹配规则为：等待时间越长，阈值越大。即匹配的范围随时间的推移而变大。故需要记录当前玩家在匹配池中等待的秒数。

根据这个逻辑，修改 `main.cpp`中的 `Pool`部分的代码实现这个逻辑即可，具体的实现方式可以参考源码及注释。
