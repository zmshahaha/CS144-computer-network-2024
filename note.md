## lab0

queue不支持容器遍历(不支持begin，end)，所以使用deque

string_view Reader::peek() const返回值为string_view则返回的必须是全局的变量，因为string_view不支持拷贝，所以不能返回栈上数据。

因为peek函数返回string_view,所以基本只能用类里现有string变量。string可以删除开头字符，可以作queue,所以直接string就行。

string_view Reader::peek() const 中的const表示对类的每一个成员都是const，根据编译时的提示

## lab1

auto &iter = buffer_.begin()语法是错误的，不能加&，因为buffer_.begin()是右值，不能赋给引用

```c
  for (auto iter = buffer_.begin(); iter != buffer_.end(); ) {
    uint64_t index = iter->first;//正确
    uint64_t index = iter.first();//错误
```

中间会删的map遍历需要这样写

```c
for (auto it = myMap.begin(); it != myMap.end(); ) {
    if (it->first % 2 == 0) { // 删除键为偶数的元素
        it = myMap.erase(it); // erase 返回下一个有效的迭代器
    } else {
        ++it; // 移动到下一个元素
    }
}
```

算法思路: 每次新加一个字符串，对map中最多只会增加一个string slice，所以最多只要insert一次。insert的这次就可以基于新加的字符串修改(最多就是左右两边扩充)，将所有与新串有重叠或者紧邻的全部删除。即只会增加新的data或者减少旧的slice。

如果有一个slice完全包含新增的，则不做任何修改

减少复杂度就是要看什么情况下才会作修改

注意以下在reassembler中的错误调法

尝试如下

```c
Reader _reader = output_.reader();
Reader _reader2 = output_.reader();
cout<<&_reader<<" "<<&_reader2<<endl;
```

会发现这两个地址不同

正确的应该是

```c
Reader &_reader = output_.reader();
cout<<&_reader<<" "<<&_reader2<<endl;
```

此时发现reader地址正常

## lab2

1 << 31 UL错，1UL << 31对

unwrap的当前seqno是调用者自己
