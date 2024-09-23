## lab1

queue不支持容器遍历(不支持begin，end)，所以使用deque

string_view Reader::peek() const返回值为string_view则返回的必须是全局的变量，因为string_view不支持拷贝，所以不能返回栈上数据。

因为peek函数返回string_view,所以基本只能用类里现有string变量。string可以删除开头字符，可以作queue,所以直接string就行。

string_view Reader::peek() const 中的const表示对类的每一个成员都是const，根据编译时的提示