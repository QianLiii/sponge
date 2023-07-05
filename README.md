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
