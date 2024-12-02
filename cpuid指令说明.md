

cpuid 参考: https://www.sandpile.org/x86/cpuid.htm

libcpuid 参考: https://libcpuid.sourceforge.net/documentation.html



standard level 0000_0004h(Intel) & extended level 8000_001Dh(AMD):

输入：`eax = 4` 和 `ecx = 0,1,...`

| 输出位数                  | 描述                                           |
|---------------------------|------------------------------------------------|
| `(eax >> 26) & 0x3f`      | (AMD此字段是保留的)每个封装的核心数量减1       |
| `(eax >> 14) & 0xfff`     | 共享此缓存的线程数(Intel)或核心数(AMD)减1      |
| `(eax >> 10) & 0xf`       | 保留字段                                       |
| `(eax >> 9) & 0x1`        | 全关联?                                        |
| `(eax >> 8) & 0x1`        | 自初始化?                                      |
| `(eax >> 5) & 0x7`        | 缓存级别, 一级缓存此值1, 二级则值2, 三级则值3  |
| `eax & 0x1f`      | 缓存类型, 值0则无, 值1则数据, 值2则指令, 值3则统一缓存 |
| `(ebx >> 22) & 0x3ff`     | 关联路数减1                                    |
| `(ebx >> 12) & 0x3ff`     | 物理行分区数减1                                |
| `ebx & 0xfff`             | 系统一致性缓存行大小减1                        |
| `ecx`                     | 缓存集合数减1                                  |
| `(edx >> 3) & 0x1fffffff` | 保留字段                                       |
| `(edx >> 2) & 0x1`        | 复杂索引?                                      |
| `(edx >> 1) & 0x1`        | 包括较低级别?                                  |
| `edx & 0x1`               | 回写无效?                                      |

缓存总大小不能以此单独获取，上述仅能获取缓存拓扑结构，还需结构处理器拓扑结构
以确定总缓存大小。以子叶`0`开始自增依次调用`cpuid leaf 4`或`cpuid leaf 0x8000001d`
直至`eax & 0x1f`值0，即可遍历所有缓存结构。




``` bash
# 使用示例: dcpuid eax 31 26
# 输出结果: (eax >> 26) & 0x3f
function dcpuid ()
{
	printf "($1 >> %d) & 0x%x" $(($3)) $(( (2<<($2-$3)) - 1))
}
```








































