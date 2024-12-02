# fastcpy
最快的memcpy实现(best speed of memcpy impl)

经测试，综合性能超过了其他各版本实现，略优于GLIBC的实现

详细参考：[实现最快的memcpy函数.md](./实现最快的memcpy函数.md)

实现了以下函数

1.  `fastcpy` 内存复制(更推荐)，等效于`memcpy`
2.  `fastcpy_tiny` 内存复制(可能超小尺寸内存复制会快些)，等效于`memcpy`
3.  `fast_stpcpy` 字符串复制，等效于`stpcpy`，返回目标字符串的末尾
4.  `fast_wstpcpy` 字符串复制，等效于`u16_stpcpy`，返回目标字符串的末尾
5.  `fast_dstpcpy` 字符串复制，等效于`u32_stpcpy`，返回目标字符串的末尾
6.  `fast_qstpcpy` 字符串复制，等效于`u64_stpcpy`，返回目标字符串的末尾
7.  `fast_strcpy` 字符串复制，等效于`strcpy`，调用`fast_stpcpy`的`inline`函数
8.  `fast_wstrcpy` 字符串复制，等效于`u16_strcpy`，调用`fast_wstpcpy`的`inline`函数
9.  `fast_dstrcpy` 字符串复制，等效于`u32_strcpy`，调用`fast_dstpcpy`的`inline`函数
10. `fast_qstrcpy` 字符串复制，等效于`U64_strcpy`，调用`fast_qstpcpy`的`inline`函数

所有函数实现均已详细测试，其中，字符串复制函数允许传入参数地址不对齐自然边界。

[`libfastcpy.tar.gz`](./libfastcpy.tar.gz) 是 Windows 平台下编译的库归档，其中：

1.  `lib` 此目录存放静态库，子目录 `SSE2`,`AVX2`,`AVX512` 分别存放三种相应版本
2.  `dylib` 此目录存放动态库，子目录 `SSE2`,`AVX2`,`AVX512` 分别存放三种相应版本
3.  `lib`,`dylib` 目录下的 `libtfastcpy.*` 打包了三种版本，运行时自动选择最合适版本

