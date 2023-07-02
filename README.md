## Lab0
遇到的问题/要点：  
1 现在按照课程链接能下载到的虚拟机是Ubuntu22.04，直接cmake源码会报错，需要自己下一个libpcap-dev。  
2 cmake没问题后，make源码也报错，在address.hh中include上<array>后成功。  
3 webget照着上面手动的写即可。直接运行webget可以得到正确结果，但是测试过不了。网上只找到 https://zhuanlan.zhihu.com/p/494364984?utm_id=0 描述了同样的问题，但照这个方法做了还是没解决。  
4 byte_stream的所有操作都在首尾进行，因此选择deque来实现。  
5 一开始没弄明白eof怎么表示，根据测试用例，writer调用end_input()后，且流的内容为空（buffer_empty()返回true）时，认为eof()返回true。
