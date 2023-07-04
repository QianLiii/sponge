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

