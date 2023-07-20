## Lab0

遇到的问题/要点：  
1 现在按照课程链接能下载到的虚拟机是Ubuntu22.04，直接cmake源码会报错，需要自己下一个libpcap-dev。  
2 cmake没问题后，make源码也报错，在address.hh中include上\<array>后成功。  
3 webget照着上面手动的写即可。直接运行webget可以得到正确结果，但是测试过不了。网上只找到<https://zhuanlan.zhihu.com/p/494364984?utm_id=0>描述了同样的问题，但照这个方法做了还是没解决。  
update：成功通过，错误原因：打印读数据时使用了cout << sock.read() << "\n"，而实际上并不需要手动换行。直接改成cout << sock.read()就通过了。  
4 byte_stream的所有操作都在首尾进行，因此选择deque来实现。  
5 一开始没弄明白eof怎么表示，根据测试用例，writer调用end_input()后，且流的内容为空（buffer_empty()返回true）时，认为eof()返回true。

现在lab0都通过了：  
![success](https://github.com/QianLiii/sponge/assets/91267727/20173ebb-4d04-4681-b4a3-e991924b6917)

## Lab1

遇到的问题/要点：  
1 花了比较久的时间想清楚指导书到底要求reassembler在接收一个substring时做出怎样的行为；后来发现其实很直接：  
不能接收的字节（包括已组装好的、还不能进入窗口的、以及还未组装的里一切重复的）就直接丢弃；  
能接收的字节一律接收。  
2 由于index是不连续的，考虑用map存储收到的子串。  
3 这里参考了区间合并的算法，每次收到一个新的substring，都把能接收的部分和已有的合并，做完合并后再提交。  
先要判断首字节是否重合（因为map不重，所以要特殊判断）；如果首字节不重，就把新的substring暂存进map，然后就和已经存在的substring完全一样处理。  
和常规区间合并不同的是，这里有一个前提：已经存在的substring一定都是互不重叠的，所以只需要从新加入的substring（或它的前一个）开始检查。  
而当一系列连续的合并操作结束后，就不用再检查后面的，因为和新加入的substring相关的合并操作一定都是连续的，后面不会再有了。  
4 关于eof，一开始考虑用_eof_index记录设置了eof的子串的最后一个字节索引，当_eof_index之前的字节全部被提交给stream了（通过_first_unassembled判断）就可以关闭；  
但根据测试用例发现如果直接传一个index=0, eof=false的空串就错了，因为_eof_index默认初始化为0，不管有没有设置eof流都会直接被关闭。
因此又增加了标志位_wait_eof，当一次push_substring设置了eof后进入等待eof状态，然后才能根据索引判断。  

通过测试：  
![success](https://github.com/QianLiii/sponge/assets/91267727/96929e37-51fc-495b-8d0f-e459ee4f83af)

## Lab2

遇到的问题/要点：  
1 wrapping_integers的实现思路：  
wrap方法很直接不说了；unwrap方法的思路是，先求n和isn的差，这是一个32位带符号数并且可能是负的，但没有关系，直接强转成无符号（正值不会变，负值加上2^32）作为最终结果的初值ballpark。然后比较ballpark和checkpoint的大小，小了就+2^32，大了就-2^32。在ballpark即将“越过”checkpoint的时候，要比较一下越过前后的差值大小，保留小的。另外在改变ballpark时还需要注意可能溢出,比如checkpoint < ballpark < 2^32，那ballpark也不能再减小了，否则就溢出了。感觉这里的代码写的很直接（丑陋），或许可以优化一下。  
*小插曲：一开始打印log语句注释掉之后忘记重新make了，导致roundtrip这个测试运行了几分钟都没反应。。下次注意。  
2 receiver中，一开始想不到这里怎么获取到first_unassembled的数据，仔细翻了一遍前面的代码后在byte_stream里找到了bytes_written()这个函数，写入流的总字节数，其实就是first_unassembled。  
3 指导书里说checkpoint是"the index of the last reassembled byte"，但感觉这样的话首个字符串就不能统一表示？还没有任何字符提交到流时应该是多少呢？  
所以用的仍然是bytes_written()，目前看没问题。  
4 搞清楚seqno, absolute seqno, index之间的关系很重要。在unwrap了seqno之后得到的是absolute seqno，而push_substring需要的是index，所以传参时还需要-1。  
syn和fin标识都具有**seqno**，但没有**index**，因此是不需要向流里传入什么东西的，但在有syn标识的段里，要将它的负载的**seqno**增加1。  
5 不是设置了fin标识后，就能马上得到对fin的确认，还是要等到流真的关闭了（用流的input_ended()检查）才能确认。

通过测试：  
![success](https://github.com/QianLiii/sponge/assets/91267727/7c05edf0-5d10-42fc-9e75-42562e6ee7c1)

## Lab3

遇到的问题/要点：  
1 这个Lab的debug时间比较久，一些细节上指导书说的并不清楚（或者是我理解得不到位），要看着测试报的错推断是哪里不符合要求。 
2 一开始遇到send_retx测试过不了，看了好多遍测试结果之后观察到，rto超过60000时好像就会出现问题。然后发现我的timer接收的rto类型是uint16（被给的rto初始值的类型影响了），应该换成uint32。  
3 指导书里只说了当收到确认时，如果重传队列里还有segment，就要重启计时器；那么别忘了，如果没有就要关闭计时器。（我的实现中，计时器分 开启、超时、关闭 三个状态）。  
4 ack_received函数总体按照指导书给的步骤就可以，但根据测试，也有一些别的需要注意的：  
注意**有意义的**确认号的条件：比_newest_ackno大，但不大于_next_ackno；  
收到确认时**总是**会更新窗口大小，不管确认号是否有意义；  
如果重传队列不空，就要重启计时器，那么别忘了，如果为空就要关闭计时器。  
5 这里说一下我的timer实现思路：  
用start方法开启一个timer，接收两个参数，当前时间和RTO。
在tick方法更新时间后，会用expired方法（接收当前时间）判断之前开启的计时器是否到期。  
还有一个stop方法关闭计时器，一个is_start方法判断计时器是否被开启（过）。  
也就是说timer有三个状态：开启（未到期），开启（到期）和关闭。  
6 fill_window函数是花时间最久的地方，整体思路改了又改，这里理解并使用指导书3.3节的状态图很重要。  
我的代码总体思路如下：  
首先判断是否要发SYN，发出SYN的唯一条件是_next_seqno == 0。  
然后根据可发送数据的条件判断是否进入发送数据的循环：  
可以发送数据的条件有  
1）SYN被确认（_newest_ackno > 0）；  
2）窗口有空余（_newest_ackno + max(_window_size, static_cast<uint16_t>(1u)) > _next_seqno）画图就明白，还处理了通告窗口为0的情况；  
3）未发送过FIN（_next_seqno != _stream.bytes_written() + 2）；  
进入循环后还要检测是否**有数据可发**，如果流未结束但buffer为空，应该直接退出。一开始各种陷入死循环就是这里理解错了，lab实现的是**单线程**的，如果没有要发的数据就应该直接退出，而不是在while里等待。  
之后就是装配上尽可能大的负载，再判断是否要加上FIN。  
加上FIN标记的条件：  
1）流结束（_stream.eof()）；  
2）还没发送过FIN（_next_seqno < _stream.bytes_written() + 2）；  
3）可用窗口中放得下（length < _newest_ackno + max(_window_size, static_cast<uint16_t>(1u)) - _next_seqno），也就是说FIN不可以无脑加在最后一个segment上，如果窗口正好只能放下最后的负载，那就必须等窗口再向后滑动后，单独发一个只包含FIN的空segment。

通过测试：  
![success](https://github.com/QianLiii/sponge/assets/91267727/a11cf363-994b-4de1-8a78-da505e091f57)

## Lab4

遇到的问题/要点：  
1 这个lab看完指导书没有什么思路，感觉无从下手，看了代码给出的公共接口，也不太清楚哪个功能该放在哪个函数里实现。所以按照指导书“how to start”的建议，先完成了几个最简单的函数，其他功能大致按照对指导书的理解简单写了下。  
2 然后就开始面向测试用例编程：单独运行build/tests目录下的fsm_打头的文件，一个一个来，照着模拟测试给出的信息去完善代码，全都通过了代码大致上的实现就搞定了。  
3 这时候遇到了一个问题：模拟测试都能过，但真实测试全都过不了，按照testing一节的测试方法，在关闭连接时，输入ctrl+D后也会直接出现段错误并结束进程。为了确认问题是出在connection还是之前的代码中，拷贝了<https://github.com/Kiprey/sponge/>的tcp_connection代码来进行测试，通过了161/162，可以确定之前写的sender、receiver包括wrapping_integers这些基本没有大问题，问题还是出在connection上。  
4 然后经过漫长的寻找，找到了错误原因：  
在active函数里，判断sender是否处在FIN_ACKED状态，即满足prereq#3时，我用的条件是：

_sender.stream_in().eof() and _newest_received_ackno == wrap(_sender.stream_in().bytes_written() + 2, _cfg.fixed_isn.value())

这么写的原因是tcp_sender.hh中，注释里写了next_seqno_absolute函数是用于测试(usde for testing)，就想着实现中不能用这个函数。那么既然无法获取sender的next_seqno_absolute，在connection层面能获得的就只有wrappingint类型的_newest_received_ackno，要将它和uint64类型的_sender.stream_in().bytes_written() + 2比较，就必须得将其中之一wrap或另一个unwrap，而无论哪一种操作都需要获得sender的isn。但是，isn是sender的私有变量，connection层面没法获取，就草草写下了用_cfg.fixed_isn.value()来获取的操作，这一步就酿成了大错。_cfg.fixed_isn是一个std::optional类型的成员，它很有可能是nullopt（在它为nullopt时，实际上sender的isn由调用它的valueor方法用随机数初始化）！！所以这样判断在逻辑上根本就是错误的。  
所以，最后还是使用了next_seqno_absolute方法，判断条件改为：

_sender.stream_in().eof() and (_sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2) and (_sender.bytes_in_flight() == 0)
    
成功通过了测试。  
![success](https://github.com/QianLiii/sponge/assets/91267727/5b191e94-6518-47a8-aa95-6c9240de5b8c)

指导书最后的手动测试，三次握手：
![shakehand](https://github.com/QianLiii/sponge/assets/91267727/e23a8dde-41e1-4ee7-b77e-320be6fbe292)  
以及四次挥手：  
![byebye](https://github.com/QianLiii/sponge/assets/91267727/99b916b9-0d45-46b5-98e1-8ed3d6a1025e)

零窗口测试，也可以正常发送：  
![zerowin](https://github.com/QianLiii/sponge/assets/91267727/82bc3539-8d93-48dc-84f3-95eda685a13b)

5 在将socket替换成cs144socket时发现一个问题：在结束时会抛出异常bad file descriptor，查到可能是在socket关闭后又去调用，然后发现get_URL函数只需要在最后调用wait_until_closed，而**不再需要**显式调用close函数，因为wait_until_closed也完成了关闭socket的工作（根据注释）。  
![webget](https://github.com/QianLiii/sponge/assets/91267727/1de3ce75-1bf0-4e60-bb25-f933e83ec62d)

6 最后处理完成的问题是通过benchmark。尝试使用Kiprey的connection和wrapingintegers，可以运行benchmark。然后去看benchmark源码，尝试把测试的字符串长度改小，改到16 * 1024 * 1024时我的代码可以运行；24 * 1024 * 1024时会出现unclean shutdown警告（说明运行了connection的析构函数）。然后参考Kiprey的代码，在我的析构函数中不发送rst包而只是设流为error，这次运行出现了std::bad_alloc，然后思考了一下哪里涉及到自己操作内存了，就去把_pop_and_send函数里的右值引用和std:move换成了普通的拷贝，然后就通过了。  
![benchmark](https://github.com/QianLiii/sponge/assets/91267727/dacacffe-a5e5-4d2d-a3ed-060abd022b22)

## Lab5

遇到的问题/要点：  
1 这个lab基本跟着指导书写就可以，简化了很多细节。  
2 映射表我采用的是map<uint32_t, std::pair<EthernetAddress, size_t>>，就是ip地址映射到（以太网地址，存活时间）这样一个二元组。  
3 记得在收到arp报文后尝试发送等待队列里的ip报文，由于是简化版本的，不需要考虑一个ip报文一直得不到目标以太网地址而“积压”在等待队列里的情况，只要一直发出队首，直到队空或有一个还要等待为止。  
4 还有使用map记得注意map的迭代器失效情况。

![success](https://github.com/QianLiii/sponge/assets/91267727/429f94d7-9499-4145-8e7e-81475b59d726)

## Lab6

遇到的问题/要点：  
1 路由表使用的数据结构，是std::vector<std::map<uint32_t, std::pair<std::optional<Address>, size_t>>>，vector有33个元素对应了掩码长度0~32，map的键是网络号，值是下一跳地址和端口地址。  
2 第一次完成的时候，在遇到默认路由时会失败；检查发现uint32_t类型在左移32位时不会变成全0，而是会变成全1。解决方法是改用bitset<32>。  

![success](https://github.com/QianLiii/sponge/assets/91267727/5dcfba73-0b70-4b53-b022-82df96c38982)

## Lab7

自己简单测试：  
![success](https://github.com/QianLiii/sponge/assets/91267727/7e187026-e706-416c-84ab-61bde3f81fe2)
