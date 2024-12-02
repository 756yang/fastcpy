
# 实现最快的memcpy函数

## GLIBC的memmove实现

实现最快的memcpy函数：

1.  使用重叠加载和存储来避免分支
2.  将所有源加载到寄存器中并将它们存储在一起，以避免源和目标之间可能存在的地址重叠
3.  如果大小为`8 * VEC_SIZE`或更小，则将所有源加载到寄存器中并将它们存储在一起
4.  若目标地址大于源地址，则以未对齐的加载和对齐的存储一次向後复制`4 * VEC_SIZE`，
    在循环前加载第一个`4 * VEC`和最後一个`VEC`，并在循环後存储以支持重叠地址
5.  否则，以未对齐的加载和对齐的存储一次向前复制`4 * VEC_SIZE`，在循环前加载
    最後一个`4 * VEC`和第一个`VEC`，并在循环後存储它们以支持重叠地址
6.  在具有 ERMS 功能的机器上，如果大小大于等于`__x86_rep_movsb_threshold`且小于
    `__x86_rep_movsb_stop_threshold`，则将使用`REP MOVSB`
7.  如果`size >= __x86_shared_non_temporal_threshold`且目标和源之间没有重叠，
    则使用非临时存储替代对齐存储，一次从2个或4个页面复制
8.  对于@7，如果`size < 16 * __x86_shared_non_temporal_threshold`且源和目标没有
    `page alias`，则使用非临时存储一次从2个页面复制。如果
    `目标的页面对齐 - 源的页面对齐 < 8 * VEC_SIZE`，则在这种情况下`page alias`
    被视为可能
9.  如果` size >= 16 * __x86_shared_non_temporal_threshold`或源和目标存在
    `page alias`则使用非临时存储一次从4个页面复制

伪代码如下：

``` c
__x86_rep_movsb_threshold = 2048 // 这个值很合适！
__x86_shared_non_temporal_threshold = sharedcache / thread *3/4;

if size <= mutable then jumptable
elif size <= n x vecsize then expand loop
elif size <= cachesize / cores then
	mov src + size - vecsize to reg_vec
	for loop mov n x reg_vec
	expand loop and mov final reg_vec
else
	use movnt for m x reg_vec
end

if sz < vecsize jump less_vec

mov (src), vmm0
if sz > 2 vecsize jump more_2x_vec

mov (src + sz - vecsize), vmm1
mov vmm0, (dst)
mov vmm1, (dst + sz - vecsize)
ret


more_2x_vec:{

if sz > 8 vecsize jump more_8x_vec

mov (src + vecsize), vmm1
if sz <= 4 vecsize jump last_4x_vec

mov (src + 2 vecsize), vmm2
mov (src + 3 vecsize), vmm3
mov (src + sz - vecsize), vmm4
mov (src + sz - 2 vecsize), vmm5
mov (src + sz - 3 vecsize), vmm6
mov (src + sz - 4 vecsize), vmm7
mov vmm0, (dst)
mov vmm1, (dst + vecsize)
mov vmm2, (dst + 2 vecsize)
mov vmm3, (dst + 3 vecsize)
mov vmm4, (dst + sz - vecsize)
mov vmm5, (dst + sz - 2 vecsize)
mov vmm6, (dst + sz - 3 vecsize)
mov vmm7, (dst + sz - 4 vecsize)
ret
}

last_4x_vec:{

mov (src + sz - vecsize), vmm2
mov (src + sz - 2 vecsize), vmm3
mov vmm0, (dst)
mov vmm1, (dst + vecsize)
mov vmm2, (dst + sz - vecsize)
mov vmm3, (dst + sz - 2 vecsize)
ret
}

more_8x_vec:{

diff = dst - src
if diff < sz jump more_8x_vec_backward_check_nop
// if dst >= src && dst < src + sz jump ...
if sz > __x86_shared_non_temporal_threshold jump large_memcpy_2x

more_8x_vec_check:

r8 = (diff + sz ^ diff) >> 63
//r8 = dst + sz >= src && dst < src
if (diff & 0xf00) + r8 ==0 jump more_8x_vec_backward
// dst and src lower 12 bits maybe same and result to 4k False Alaising

more_8x_vec_forward:

// already loaded vmm0
mov (src + sz - vecsize), vmm5
mov (src + sz - 2 vecsize), vmm6
dsa = (dst | vecsize - 1) + 1
mov (src + sz - 3 vecsize), vmm7
mov (src + sz - 4 vecsize), vmm8
sra = src - dst + dsa
dsz = dst + sz - 4 vecsize

loop_4x_vec_forward:
	mov (sra), vmm1
	mov (sra + vecsize), vmm2
	mov (sra + 2 vecsize), vmm3
	mov (sra + 3 vecsize), vmm4
	sra = sra + 4 vecsize
	mov vmm1, (dsa)
	mov vmm2, (dsa + vecsize)
	mov vmm3, (dsa + 2 vecsize)
	mov vmm4, (dsa + 3 vecsize)
	dsa = dsa + 4 vecsize
if dsz > dsa jump loop_4x_vec_forward

mov vmm5, (dsz + 3 vecsize)
mov vmm6, (dsz + 2 vecsize)
mov vmm7, (dsz + vecsize)
mov vmm8, (dsz)
mov vmm0, (dst)
ret
}

more_8x_vec_backward_check_nop:{

if diff == 0 then ret

more_8x_vec_backward:

// already loaded vmm0
mov (src + vecsize), vmm5
mov (src + 2 vecsize), vmm6
dsa = dst + sz - 4 vecsize - 1
mov (src + 3 vecsize), vmm7
mov (src + sz - vecsize), vmm8
dsa = dsa & -vecsize
sra = src - dst + dsa

loop_4x_vec_backward:
	mov (sra + 3 vecsize), vmm1
	mov (sra + 2 vecsize), vmm2
	mov (sra + vecsize), vmm3
	mov (sra), vmm4
	sra = sra - 4 vecsize
	mov vmm1, (dsa + 3 vecsize)
	mov vmm2, (dsa + 2 vecsize)
	mov vmm3, (dsa + vecsize)
	mov vmm4, (dsa)
	dsa = dsa - 4 vecsize
if dst < dsa jump loop_4x_vec_backward

mov vmm0, (dst)
mov vmm5, (dst + vecsize)
mov vmm6, (dst + 2 vecsize)
mov vmm7, (dst + 3 vecsize)
mov vmm8, (dst + sz - vecsize)
ret
}

large_memcpy_2x:{

if sz > src - dst jump more_8x_vec_forward
// cacheline align
if vecsize < 64
	mov (src + vecsize), vmm1
if vecsize < 32
	mov (src + 2 vecsize), vmm2
	mov (src + 3 vecsize), vmm3
mov vmm0, (dst)
if vecsize < 64
	mov vmm1, (dst + vecsize)
if vecsize < 32
	mov vmm2, (src + 2 vecsize)
	mov vmm3, (src + 3 vecsize)
// adjust alignments
r8 = 64 - (dst & 63)
sra = src + r8
dsa = dst + r8
sza = sz - r8

// larger pipeline help relieve 4k False Alaising
if (~(-diff) & (4096 - 8 vecsize)) ==0 jump large_memcpy_4x
if sz >= 16 __x86_shared_non_temporal_threshold jump large_memcpy_4x

szq = sza & 8191
szp = sza >> 13

loop_large_memcpy_2x_outer:
	count = 4096 / (4 vecsize)
	loop_large_memcpy_2x_inner:
		prefetch (sra), 4 vecsize
		//prefetch (sra), 8 vecsize
		prefetch (sra), 4 vecsize + 4096
		//prefetch (sra), 8 vecsize + 4096
		load_set (sra), 0, vmm0, vmm1, vmm2, vmm3
		load_set (sra), 4096, vmm4, vmm5, vmm6, vmm7
		sra = sra + 4 vecsize
		store_set (dsa), 0, vmm0, vmm1, vmm2, vmm3
		store_set (dsa), 4096, vmm4, vmm5, vmm6, vmm7
		dsa = dsa + 4 vecsize
	if --count != 0 jump loop_large_memcpy_2x_inner
	dsa = dsa + 4096
	sra = sra + 4096
if --szp != 0 jump loop_large_memcpy_2x_outer
sfence

if szq <= 4 vecsize jump large_memcpy_2x_end

loop_large_memcpy_2x_tail:
	prefetch (sra), 4 vecsize
	prefetch (dsa), 4 vecsize
	mov (sra), vmm0
	mov (sra + vecsize), vmm1
	mov (sra + 2 vecsize), vmm2
	mov (sra + 3 vecsize), vmm3
	sra = sra + 4 vecsize
	szq = szq - 4 vecsize
	mova vmm0, (dsa)
	mova vmm1, (dsa + vecsize)
	mova vmm2, (dsa + 2 vecsize)
	mova vmm3, (dsa + 3 vecsize)
	dsa = dsa + 4 vecsize
if szq > 4 vecsize jump loop_large_memcpy_2x_tail

large_memcpy_2x_end:

mov (sra + szq - 4 vecsize), vmm0
mov (sra + szq - 3 vecsize), vmm1
mov (sra + szq - 2 vecsize), vmm2
mov (sra + szq - vecsize), vmm3
mov vmm0, (dsa + szq - 4 vecsize)
mov vmm1, (dsa + szq - 3 vecsize)
mov vmm2, (dsa + szq - 2 vecsize)
mov vmm3, (dsa + szq - vecsize)
ret
}

large_memcpy_4x:{

szq = sza & 16383
szp = sza >> 14

loop_large_memcpy_4x_outer:
	count = 4096 / (4 vecsize)
	loop_large_memcpy_4x_inner:
		prefetch (sra), 4 vecsize
		prefetch (sra), 4 vecsize + 4096
		prefetch (sra), 4 vecsize + 8192
		prefetch (sra), 4 vecsize + 12288
		load_set (sra), 0, vmm0, vmm1, vmm2, vmm3
		load_set (sra), 4096, vmm4, vmm5, vmm6, vmm7
		load_set (sra), 8192, vmm8, vmm9, vmm10, vmm11
		load_set (sra), 12288, vmm12, vmm13, vmm14, vmm15
		sra = sra + 4 vecsize
		store_set (dsa), 0, vmm0, vmm1, vmm2, vmm3
		store_set (dsa), 4096, vmm4, vmm5, vmm6, vmm7
		store_set (dsa), 8192, vmm8, vmm9, vmm10, vmm11
		store_set (dsa), 12288, vmm12, vmm13, vmm14, vmm15
		dsa = dsa + 4 vecsize
	if --count != 0 jump loop_large_memcpy_4x_inner
	dsa = dsa + 12288
	sra = sra + 12288
if --szp != 0 jump loop_large_memcpy_4x_outer
sfence

if szq <= 4 vecsize jump large_memcpy_4x_end

loop_large_memcpy_4x_tail:
	jump loop_large_memcpy_2x_tail
large_memcpy_4x_end:
	jump large_memcpy_2x_end
ret
}
```

## fastcpy实现与探索

`rep movsb`在支持的平台可以使用，在AMD平台通常不支持，则不要使用！

`R7 3700X`使用`rep movsb`不如以循环SIMD指令来复制内存，`I7 7500U`使用`rep movsb`
在`size >= 2048`时效果都很好(与循环SIMD指令相差不大)，因而在`fastcpy`库裡不使用
`rep movsb`，根据`size`采取不同策略，从而应对所有情况并达到最优。

### 性能测试方案

综合性能采用`text_cat`算法测试，此测试将零碎字符串拼接为一个很长字符串，为避免
干扰，使用简单内存池进行`malloc/free`操作，数据以高效的`xorshift64`随机数算法
生成，保证耗时操作主要是内存复制。

对于各种尺寸的内存复制，采用`copy_block`算法测试，首先申请两个内存块用以放置
源内存数据和目标内存数据，内存块足够大以模拟随机内存访问，内存复制的源地址和
目标地址都以随机数生成，内存块大小也以随机数生成，还考虑`4K伪混叠`测试，为能测出
超小内存块复制的性能差异，优化了测试循环代码(以位运算代替除法，将`xorshift64`
生成的一个随机数以`rorx`利用13次进行不同`size`的内存复制)。

性能测试采用谷歌测试的方案，只需编写测试代码并多次调用宏命令即可。

#### 初步测试结果

综合性能 text_cat:

1.  memcpy_glibc
2.  memcpy_my2
3.  rte_memcpy_avx
4.  memcpy_erms(on intel)

小尺寸 1/256:

1.  memcpy_glibc 分支实现
2.  memcpy_my2 分支实现
3.  memcpy_jart1 跳表实现
4.  rte_memcpy_avx 分支实现

中尺寸 256/4096:

1.  memcpy_my2 八向量循环
2.  rte_memcpy_avx 八向量循环
1.  memcpy_glibc 四向量循环
3.  memcpy_erms(on intel)

中尺寸 4k/1M

1.  memcpy_glibc
1.  memcpy_erms(on intel)
2.  memcpy_my2
3.  rte_memcpy_avx

大尺寸 16M/128M 仅 movnt 版本性能忧

#### 初步测试结论

GLIBC的实现综合最优，可参考GLIBC实现更快的内存复制算法。

小尺寸复制，不宜采用较大的跳表，二分法分支和超小跳表性能接近。

中尺寸复制，四向量循环、八向量循环的差距不明显，两向量循环较慢。

大尺寸复制，使用了`movnt`系列指令则有明显优势。


### 小内存复制探优

各种实现详见`fastcpy/test_copy_tiny.c`，其中，`switch_copy`是最优跳表实现，
`branch_copy`是最优分支实现，`tiny_copy`是小尺寸内存复制的完整实现。

经测试，`branch_copy`(8.43GB/s)优于`switch_copy`(7.38GB/s)，因而`tiny_copy`是
基于分支实现的，实测其优于GLIBC实现(GLIBC考虑内存重叠而不能再优化了)。

关键点：

1.  `size < 32 && size >= 16`分支应是`likely`，後续分支(除最後一个)应都是
    `unlikely`，这样分支预测失败的损失最小(这即是分支表的形式)
2.  `cmp $0x80, %[size]; jbe 0f`应替换为`add $-0x80, %[size]; jle 0f`，这是因为
    立即数`0x80`编码为四字节，导致`likely`分支处于缓存行边缘从而指令停滞

fastcpy实现代码：(`size <= 8*VEC_SIZE`)

``` asm
#if VEC_SIZE == 64
fastcpy_32_to_64:\n\t
		VEX_PRE(movups) (%[src]), %t[v1]\n\t
		VEX_PRE(movups) -32(%[src],%[size]), %t[v0]\n\t
		VEX_PRE(movups) %t[v1], (%[dst])\n\t
		VEX_PRE(movups) %t[v0], -32(%[dst],%[size])\n\t
		VZEROUPPER\n\t
		ret\n\t
#endif

fastcpy_tiny:\n\t
		mov 	%[dst], %%rax\n\t
		cmp 	$(VEC_SIZE), %[size]\n\t
		ja  	.F_more_vec\n\t
		.p2align 4\n\t
.F_less_vec:\n\t
#if VEC_SIZE == 64
		cmpl	$32, %k[size]\n\t
		jae 	fastcpy_32_to_64\n\t
#endif
#if VEC_SIZE > 16
		cmpl	$16, %k[size]\n\t
		jb  	.L_less_16\n\t
		VEX_PRE(movups) (%[src]), %x[v1]\n\t
		VEX_PRE(movups) -16(%[src],%[size]), %x[v0]\n\t
		VEX_PRE(movups) %x[v1], (%[dst])\n\t
		VEX_PRE(movups) %x[v0], -16(%[dst],%[size])\n\t
		ret\n\t
		.p2align 2\n\t
#endif
.L_less_16:\n\t
		cmpl	$8, %k[size]\n\t
		jae 	.L_in_8_15\n\t
		sub 	$4, %[size]\n\t
		jae 	.L_in_4_7\n\t
		cmpl	$-3, %k[size]\n\t
		jl  	.L_in_0\n\t
		movzbl	(%[src]), %k[dst]\n\t
		je  	.L_in_1\n\t
		movzwl	2(%[src],%[size]), %k[src]\n\t
		movw	%w[src], 2(%%rax,%[size])\n\t
.L_in_1:\n\t
		movb	%b[dst], (%%rax)\n\t
.L_in_0:\n\t
		ret\n\t
		.p2align 2\n\t
.L_in_4_7:\n\t
		movl	(%[src]), %k[dst]\n\t
		movl	(%[src],%[size]), %k[src]\n\t
		movl	%k[dst], (%%rax)\n\t
		movl	%k[src], (%%rax,%[size])\n\t
		ret\n\t
		.p2align 2\n\t
.L_in_8_15:\n\t
		movq	(%[src]), %[dst]\n\t
		movq	-8(%[src],%[size]), %[src]\n\t
		movq	%[dst], (%%rax)\n\t
		movq	%[src], -8(%%rax,%[size])\n\t
		ret\n\t

fastcpy:\n\t
		mov 	%[dst], %%rax\n\t
		cmp 	$(VEC_SIZE), %[size]\n\t
		jb  	.F_less_vec\n\t
.F_more_vec:\n\t
		VEX_PRE(movups) (%[src]), %[v0]\n\t
		cmp 	$(8*VEC_SIZE), %[size]\n\t
		ja  	_F_more_8x_vec\n\t
		VEX_PRE(movups) -VEC_SIZE(%[src],%[size]), %[v1]\n\t
		VEX_PRE(movups) %[v0], (%[dst])\n\t
		VEX_PRE(movups) %[v1], -VEC_SIZE(%[dst],%[size])\n\t
		cmpl	$(2*VEC_SIZE), %k[size]\n\t
		jbe 	0f\n\t
		VEX_PRE(movups) VEC_SIZE(%[src]), %[v0]\n\t
		VEX_PRE(movups) -2*VEC_SIZE(%[src],%[size]), %[v1]\n\t
		VEX_PRE(movups) %[v0], VEC_SIZE(%[dst])\n\t
		VEX_PRE(movups) %[v1], -2*VEC_SIZE(%[dst],%[size])\n\t
		add 	$(-4*VEC_SIZE), %[size]\n\t
		jle 	0f\n\t
		VEX_PRE(movups) 2*VEC_SIZE(%[src]), %[v3]\n\t
		VEX_PRE(movups) 3*VEC_SIZE(%[src]), %[v2]\n\t
		VEX_PRE(movups) (%[src],%[size]), %[v1]\n\t
		VEX_PRE(movups) VEC_SIZE(%[src],%[size]), %[v0]\n\t
		VEX_PRE(movups) %[v3], 2*VEC_SIZE(%[dst])\n\t
		VEX_PRE(movups) %[v2], 3*VEC_SIZE(%[dst])\n\t
		VEX_PRE(movups) %[v1], (%[dst],%[size])\n\t
		VEX_PRE(movups) %[v0], VEC_SIZE(%[dst],%[size])\n\t
		0:\n\t
		VZEROUPPER\n\t
		ret\n\t
		/* 以上 jcc 指令字节数必须为 2 否则跨缓存行导致效率降低 */
		.p2align 4\n\t
_F_more_8x_vec:\n\t
		subq	%[src], %%rax\n\t
		cmp 	%[size], fastcpy_non_temporal_threshold(%%rip)\n\t
		/* 分支初始执行, 条件跳转预取指令地址更低的位置 , 间接跳转预取当前位置 */
		jb  	memcpy_large_memcpy\n\t
		jl  	get_non_temporal_threshold\n\t
		testb	$((PAGE_SIZE-16*VEC_SIZE)>>8), %%ah\n\t
		je  	.F_more_8x_vec_backward\n\t
```

关键点：`fastcpy_tiny`位于`fastcpy`之前，这使`fastcpy`的主逻辑部分十分紧凑，
得以避免`32~256`尺寸内存复制的指令跨缓存行问题，并使小内存复制性能明显高于
GLIBC实现，这可是fastcpy的主要优化点。

### 中尺寸内存复制优化

各种实现详见`fastcpy/test_copy_medium.c`，其中，`copy_forward`是`fastcpy`最终
采用的算法，`copy_unroll4`是四向量循环，`copy_unroll6`是六向量循环，
`copy_unroll8`是八向量循环，`copy_unroll9`是优化分支的八向量循环(实测不如
`copy_unroll8`)，GLIBC的实现方案类似`copy_unroll4`但用更多xmm寄存器优化了分支。

实测`copy_unroll4`略慢于`copy_unroll6`，`copy_unroll8`则无论如何优化都不理想，
这可能是因为已就绪的读取仍然需等待後续读取才能写入，若复制的尺寸越大，则`copy_unroll4`相比于`copy_unroll6`更有优势，在尺寸达到`2MB`时，两者相当。

`copy_unroll6`算法优化，首先读写首尾各一向量，同时计算按目标地址对齐到向量大小，
`dst`对齐後还需加`3*VEC_SIZE`以使循环代码中的立即数可用一字节表示，循环收尾部分
有三种算法，跳转表仅需判断一次分支但最慢(小型分支不应使用跳转表)，分支表和循环
一样快，但分支表占用更多代码，因此使用简单的一向量循环即可。

注意：由于`4K伪混叠`的存在，必须同时实现前向和後向复制并根据情况选择合适的。

fastcpy的前向复制算法：

``` asm
.F_more_8x_vec_forward:\n\t
		/* VEX_PRE(movups) (%[sra]), %[v0]\n\t 已经读取了 v0 */
		VEX_PRE(movups) -VEC_SIZE(%[sra],%[dsz]), %[v1]\n\t
		lea 	-VEC_SIZE(%[dsa],%[dsz]), %[dsz]\n\t
		movq	%[dsa], %%rax\n\t
		VEX_PRE(movups) %[v0], (%[dsa])\n\t
		orq 	$(VEC_SIZE-1), %[dsa]\n\t
		lea 	(5*VEC_SIZE+1)(%[dsa]), %[dsa]\n\t
		VEX_PRE(movups) %[v1], (%[dsz])\n\t
		sub 	%%rax, %[sra]\n\t
		add 	%[dsa], %[sra]\n\t
		mov 	$(-6*VEC_SIZE), %[ldz]\n\t
		/* 循环复制 */
		.p2align 4,,12\n\t
		0:\n\t
		VEX_PRE(movups) -5*VEC_SIZE(%[sra]), %[v5]\n\t
		VEX_PRE(movups) -4*VEC_SIZE(%[sra]), %[v4]\n\t
		VEX_PRE(movups) -3*VEC_SIZE(%[sra]), %[v3]\n\t
		VEX_PRE(movups) -2*VEC_SIZE(%[sra]), %[v2]\n\t
		VEX_PRE(movups) -1*VEC_SIZE(%[sra]), %[v1]\n\t
		VEX_PRE(movups) 0*VEC_SIZE(%[sra]), %[v0]\n\t
		sub 	%[ldz], %[sra]\n\t
		VEX_PRE(movaps) %[v5], -5*VEC_SIZE(%[dsa])\n\t
		VEX_PRE(movaps) %[v4], -4*VEC_SIZE(%[dsa])\n\t
		VEX_PRE(movaps) %[v3], -3*VEC_SIZE(%[dsa])\n\t
		VEX_PRE(movaps) %[v2], -2*VEC_SIZE(%[dsa])\n\t
		VEX_PRE(movaps) %[v1], -1*VEC_SIZE(%[dsa])\n\t
		VEX_PRE(movaps) %[v0], 0*VEC_SIZE(%[dsa])\n\t
		sub 	%[ldz], %[dsa]\n\t
		cmp 	%[dsa], %[dsz]\n\t
		ja  	0b\n\t
		/* 循环收尾 */
		lea 	VEC_SIZE(%[ldz]), %[ldz]\n\t
		sub 	%[dsa], %[dsz]\n\t
		cmp 	%[dsz], %[ldz]\n\t
		jge 	2f\n\t
		.p2align 4,,8\n\t
		1:\n\t
		VEX_PRE(movups) (%[sra],%[ldz]), %[v0]\n\t
		VEX_PRE(movaps) %[v0], (%[dsa],%[ldz])\n\t
		add 	$(VEC_SIZE), %[ldz]\n\t
		cmp 	%[dsz], %[ldz]\n\t
		jl  	1b\n\t
		2:\n\t
		VZEROUPPER\n\t
		ret\n\t
```

fastcpy的後向复制算法：

``` asm
.F_more_8x_vec_backward:\n\t
		/* VEX_PRE(movups) (%[src]), %[v0]\n\t 已经读取了 v0 */
		VEX_PRE(movups) -VEC_SIZE(%[src],%[size]), %[v1]\n\t
		lea 	(-1-VEC_SIZE)(%[dst],%[size]), %[dsa]\n\t
		lea 	5*VEC_SIZE(%[dst]), %[dsz]\n\t
		movl	$(6*VEC_SIZE), %k[ldz]\n\t
		VEX_PRE(movups) %[v0], (%[dst])\n\t
		VEX_PRE(movups) %[v1], 1(%[dsa])\n\t
		and 	$(-VEC_SIZE), %[dsa]\n\t
		sub 	%[dst], %[sra]\n\t
		add 	%[dsa], %[sra]\n\t
#ifdef XMM_NEED_SAVE
		xchg	%%rcx, %%rax\n\t
#else
		mov 	%[dst], %%rax\n\t
#endif
		/* 循环复制 */
		.p2align 4,,10\n\t
		0:\n\t
		VEX_PRE(movups) 0*VEC_SIZE(%[sra]), %[v5]\n\t
		VEX_PRE(movups) -1*VEC_SIZE(%[sra]), %[v4]\n\t
		VEX_PRE(movups) -2*VEC_SIZE(%[sra]), %[v3]\n\t
		VEX_PRE(movups) -3*VEC_SIZE(%[sra]), %[v2]\n\t
		VEX_PRE(movups) -4*VEC_SIZE(%[sra]), %[v1]\n\t
		VEX_PRE(movups) -5*VEC_SIZE(%[sra]), %[v0]\n\t
		sub 	%[ldz], %[sra]\n\t
		VEX_PRE(movaps) %[v5], 0*VEC_SIZE(%%rcx)\n\t
		VEX_PRE(movaps) %[v4], -1*VEC_SIZE(%%rcx)\n\t
		VEX_PRE(movaps) %[v3], -2*VEC_SIZE(%%rcx)\n\t
		VEX_PRE(movaps) %[v2], -3*VEC_SIZE(%%rcx)\n\t
		VEX_PRE(movaps) %[v1], -4*VEC_SIZE(%%rcx)\n\t
		VEX_PRE(movaps) %[v0], -5*VEC_SIZE(%%rcx)\n\t
		sub 	%[ldz], %%rcx\n\t
		cmp 	%%rcx, %[dsz]\n\t
		jb  	0b\n\t
		/* 循环收尾 */
		cmp 	%%rcx, %%rax\n\t
		jnb 	2f\n\t
		.p2align 4,,7\n\t
		1:\n\t
		VEX_PRE(movups) (%[sra]), %[v0]\n\t
		VEX_PRE(movaps) %[v0], (%%rcx)\n\t
		sub 	$(VEC_SIZE), %%rcx\n\t
		sub 	$(VEC_SIZE), %[sra]\n\t
		cmp 	%%rcx, %%rax\n\t
		jb  	1b\n\t
		2:\n\t
		VZEROUPPER\n\t
		ret\n\t
```

注意：当`VEC_SIZE==64`时(使用AVX512指令)，改为采用四向量循环。由于六向量循环的
代码无法在一个缓存行内，故而某些时候性能略低于GLIBC实现。


### 大尺寸内存复制优化

各种实现详见`fastcpy/test_copy_large.c`，其中，`copy_large_NO`是不使用预取指令的
四向量循环，`copy_large_P0`和`copy_large_T0`是T0预取的四向量循环，`copy_large_2x`
是GLIBC的两页复制实现，`copy_large_4x`是GLIBC的四页复制实现，`copy_large_NTA`是
采用NTA预取的四向量循环，`copy_large_U4`,`copy_large_U6`,`copy_large_U8`是不同
数量的向量循环。

经过各种实现，大尺寸内存复制的瓶颈在内存带宽，循环的向量数量不太影响性能，因此，
使用四向量循环即可。由于目前硬件都自带预取，顺序读写则无需软件预取，实测采用任何
软件预取方案都会使性能下降，NTA预取相比于T0预取更影响性能，但复制的内存尺寸越大，
则NTA预取越有优势(但仍不如不采用软件预取，预取指令始终有开销的)。

由于某些情况下无预取可能有更糟糕的结果，T0预取有时候有性能提升，fastcpy采用T0预取
方案(和GLIBC相同)。并且，实测同时从两页复制确实有性能提升，在某些平台上(例如
`I7 7500U`)，同时从四页复制还有性能提升，因此最终采用GLIBC的方案即可。

但是GLIBC的代码并没有最优化，经优化後，fastcpy的此部分代码如下：

``` asm
memcpy_large_memcpy:
		xor 	%%r9, %%r9\n\t
#ifdef XMM_NEED_SAVE
		sub 	$0xB0, %%rsp\n\t
		mov 	%[cxo], (%%rsp)\n\t
		mov 	%[cxi], 8(%%rsp)\n\t
		VEX_PRE(movups) %%xmm6, 0x10(%%rsp)\n\t
		VEX_PRE(movups) %%xmm7, 0x20(%%rsp)\n\t
#else
		mov 	%[size], %%r8\n\t
#endif
		/* rax 仍保留 dst - src 的值 */
		dec 	%%rax\n\t
		lea 	(%[src],%%r8), %[cxi]\n\t
#if VEC_SIZE < 64
		VEX_PRE(movups) VEC_SIZE(%[src]), %[v1]\n\t
#endif
#if VEC_SIZE < 32
		VEX_PRE(movups) 2*VEC_SIZE(%[src]), %[v2]\n\t
		VEX_PRE(movups) 3*VEC_SIZE(%[src]), %[v3]\n\t
#endif
		testb	$((PAGE_SIZE-16*VEC_SIZE)>>8), %%ah\n\t
		cmovne	fastcpy_non_temporal_threshold(%%rip), %%r9\n\t
		mov 	%[dst], %[cxo]\n\t
		mov 	%[dst], %%rax\n\t
		VEX_PRE(movups) -VEC_SIZE(%[cxi]), %[v4]\n\t
		VEX_PRE(movups) -2*VEC_SIZE(%[cxi]), %[v5]\n\t
		andl	$63, %k[cxo]\n\t
		sub 	$64, %[cxo]\n\t
		VEX_PRE(movups) -3*VEC_SIZE(%[cxi]), %[v6]\n\t
		VEX_PRE(movups) -4*VEC_SIZE(%[cxi]), %[v7]\n\t
		lea 	(%[dst],%%r8), %[cxi]\n\t
		lea 	-1(%%r8,%[cxo]), %[cxz]\n\t
		VEX_PRE(movups) %[v0], (%[dst])\n\t
#if VEC_SIZE < 64
		VEX_PRE(movups) %[v1], VEC_SIZE(%[dst])\n\t
#endif
#if VEC_SIZE < 32
		VEX_PRE(movups) %[v2], 2*VEC_SIZE(%[dst])\n\t
		VEX_PRE(movups) %[v3], 3*VEC_SIZE(%[dst])\n\t
#endif
		sub 	%[cxo], %[src]\n\t
		sub 	%[cxo], %[dst]\n\t
		VEX_PRE(movups) %[v4], -VEC_SIZE(%[cxi])\n\t
		VEX_PRE(movups) %[v5], -2*VEC_SIZE(%[cxi])\n\t
		shl 	$4, %%r9\n\t
		mov 	%[cxz], %[cxo]\n\t
		VEX_PRE(movups) %[v6], -3*VEC_SIZE(%[cxi])\n\t
		VEX_PRE(movups) %[v7], -4*VEC_SIZE(%[cxi])\n\t
		shr 	$(%c[lgvs]+2), %k[cxz]\n\t
		movl	$(PAGE_SIZE/LARGE_LOAD_SIZE), %k[cxi]\n\t
		cmp 	%%r9, %[cxo]\n\t
		jae 	.L_large_memcpy_4x\n\t
.L_large_memcpy_2x:\n\t
		shr 	$(%c[lgpg]+1), %[cxo]\n\t
		/* Copy 4x VEC at a time from 2 pages.  */
		.p2align 4\n\t
		0:\n\t
		LOAD_ONE_SET((%[src]), 0, %[v0], %[v1], %[v2], %[v3])\n\t
		LOAD_ONE_SET((%[src]), PAGE_SIZE, %[v4], %[v5], %[v6], %[v7])\n\t
		/* 为下次循环而预取, 应置于加载之後 */
		sub 	$(-LARGE_LOAD_SIZE), %[src]\n\t
		PREFETCH_ONE_SET(1, (%[src]), 0)\n\t
		PREFETCH_ONE_SET(1, (%[src]), PAGE_SIZE)\n\t
		STORE_ONE_SET((%[dst]), 0, %[v0], %[v1], %[v2], %[v3])\n\t
		STORE_ONE_SET((%[dst]), PAGE_SIZE, %[v4], %[v5], %[v6], %[v7])\n\t
		sub 	$(-LARGE_LOAD_SIZE), %[dst]\n\t
#if FASTCPY_NOUSE_INC_DEC
		subl	$1, %k[cxi]\n\t
#else
		decl	%k[cxi]\n\t
#endif
		jne 	0b\n\t
		add 	$(PAGE_SIZE), %[src]\n\t
		lea 	PAGE_SIZE(%[dst]), %[dst]\n\t
		movl	$(PAGE_SIZE/LARGE_LOAD_SIZE), %k[cxi]\n\t
#if FASTCPY_NOUSE_INC_DEC
		sub 	$1, %[cxo]\n\t
#else
		dec 	%[cxo]\n\t
#endif
		jne 	0b\n\t
		/* 尾部处理 */
		andl	$((2*PAGE_SIZE-1)>>(%c[lgvs]+2)), %k[cxz]\n\t
.L_large_memcpy_tail:\n\t
#ifdef XMM_NEED_SAVE
		VEX_PRE(movups) 0x20(%%rsp),%%xmm7\n\t
		VEX_PRE(movups) 0x10(%%rsp),%%xmm6\n\t
		mov 	8(%%rsp), %[cxi]\n\t
		mov 	(%%rsp), %[cxo]\n\t
#endif
		je  	1f\n\t
		.p2align 4,,10\n\t
		0:\n\t
		VEX_PRE(movups) (%[src]), %[v0]\n\t
		VEX_PRE(movups) VEC_SIZE(%[src]), %[v1]\n\t
		VEX_PRE(movups) 2*VEC_SIZE(%[src]), %[v2]\n\t
		VEX_PRE(movups) 3*VEC_SIZE(%[src]), %[v3]\n\t
		sub 	$(-4*VEC_SIZE), %[src]\n\t
		PREFETCH_ONE_SET(1, (%[src]), 0)\n\t
		VEX_PRE(movntps) %[v0], (%[dst])\n\t
		VEX_PRE(movntps) %[v1], VEC_SIZE(%[dst])\n\t
		VEX_PRE(movntps) %[v2], 2*VEC_SIZE(%[dst])\n\t
		VEX_PRE(movntps) %[v3], 3*VEC_SIZE(%[dst])\n\t
		sub 	$(-4*VEC_SIZE), %[dst]\n\t
#if FASTCPY_NOUSE_INC_DEC
		subl	$1, %k[cxz]\n\t
#else
		decl	%k[cxz]\n\t
#endif
		jne 	0b\n\t
		1:\n\t
		sfence\n\t
#ifdef XMM_NEED_SAVE
		add 	$0xB0, %%rsp\n\t
#endif
		VZEROUPPER\n\t
		ret\n\t
#if defined(XMM_NEED_SAVE) && VEC_SIZE != 64
		.p2align 4\n\t
.L_large_memcpy_4x:\n\t
		VEX_PRE(movups) %%xmm8,0x30(%%rsp)\n\t
		VEX_PRE(movups) %%xmm9,0x40(%%rsp)\n\t
		VEX_PRE(movups) %%xmm10,0x50(%%rsp)\n\t
		VEX_PRE(movups) %%xmm11,0x60(%%rsp)\n\t
		VEX_PRE(movups) %%xmm12,0x70(%%rsp)\n\t
		VEX_PRE(movups) %%xmm13,0x80(%%rsp)\n\t
		VEX_PRE(movups) %%xmm14,0x90(%%rsp)\n\t
		VEX_PRE(movups) %%xmm15,0xA0(%%rsp)\n\t
#else
		.p2align 3,,3\n\t
		.p2align 2\n\t
.L_large_memcpy_4x:\n\t
#endif
		shr 	$(%c[lgpg]+2), %[cxo]\n\t
		/* Copy 4x VEC at a time from 4 pages.  */
		.p2align 4\n\t
		0:\n\t
		LOAD_ONE_SET((%[src]), 0, %[v0], %[v1], %[v8], %[v9])\n\t
		LOAD_ONE_SET((%[src]), PAGE_SIZE, %[v2], %[v3], %[v10], %[v11])\n\t
		LOAD_ONE_SET((%[src]), 2*PAGE_SIZE, %[v4], %[v5], %[v12], %[v13])\n\t
		LOAD_ONE_SET((%[src]), 3*PAGE_SIZE, %[v6], %[v7], %[v14], %[v15])\n\t
		sub 	$(-LARGE_LOAD_SIZE), %[src]\n\t
		/* 为下次循环而预取, 应置于加载之後 */
		PREFETCH_ONE_SET(1, (%[src]), 0)\n\t
		PREFETCH_ONE_SET(1, (%[src]), PAGE_SIZE)\n\t
		PREFETCH_ONE_SET(1, (%[src]), 2*PAGE_SIZE)\n\t
		PREFETCH_ONE_SET(1, (%[src]), 3*PAGE_SIZE)\n\t
		STORE_ONE_SET((%[dst]), 0, %[v0], %[v1], %[v8], %[v9])\n\t
		STORE_ONE_SET((%[dst]), PAGE_SIZE, %[v2], %[v3], %[v10], %[v11])\n\t
		STORE_ONE_SET((%[dst]), 2*PAGE_SIZE, %[v4], %[v5], %[v12], %[v13])\n\t
		STORE_ONE_SET((%[dst]), 3*PAGE_SIZE, %[v6], %[v7], %[v14], %[v15])\n\t
		sub 	$(-LARGE_LOAD_SIZE), %[dst]\n\t
#if FASTCPY_NOUSE_INC_DEC
		subl	$1, %k[cxi]\n\t
#else
		decl	%k[cxi]\n\t
#endif
		jne 	0b\n\t
		add 	$(3*PAGE_SIZE), %[src]\n\t
		lea 	3*PAGE_SIZE(%[dst]), %[dst]\n\t
		movl	$(PAGE_SIZE/LARGE_LOAD_SIZE), %k[cxi]\n\t
#if FASTCPY_NOUSE_INC_DEC
		sub 	$1, %[cxo]\n\t
#else
		dec 	%[cxo]\n\t
#endif
		jne 	0b\n\t
		andl	$((4*PAGE_SIZE-1)>>(%c[lgvs]+2)), %k[cxz]\n\t
#if defined(XMM_NEED_SAVE) && VEC_SIZE != 64
		VEX_PRE(movups) 0xA0(%%rsp),%%xmm15\n\t
		VEX_PRE(movups) 0x90(%%rsp),%%xmm14\n\t
		VEX_PRE(movups) 0x80(%%rsp),%%xmm13\n\t
		VEX_PRE(movups) 0x70(%%rsp),%%xmm12\n\t
		VEX_PRE(movups) 0x60(%%rsp),%%xmm11\n\t
		VEX_PRE(movups) 0x50(%%rsp),%%xmm10\n\t
		VEX_PRE(movups) 0x40(%%rsp),%%xmm9\n\t
		VEX_PRE(movups) 0x30(%%rsp),%%xmm8\n\t
#endif
		jmp 	.L_large_memcpy_tail\n\t
```

最後，还需确定`fastcpy_non_temporal_threshold`的值，以合理安排内存复制策略。


### 确定非临时复制的启动值

GLIBC的公式：`__x86_shared_non_temporal_threshold = shared / threads *3/4`

这个公式仅对内存重度负载适合，若系统仅一个线程进行内存复制测试，则共享缓存尚有
很多未得到利用，经多次测试和拟合计算，我建议采取平方根倒数算法(这可用SSE指令
加速计算)，公式为：
`fastcpy_non_temporal_threshold = l3cache / sqrt(threads + 2*cores)`

注：对于`threads==1`的特例，此阈值直接指定为共享缓存即可。若CPUID信息获取失败，
则采用默认值`0x200000`(2MB)即可，如此则适用于所有`x86-64`平台。

### 关于内联汇编的写法

最初，我以内在函数实现了GLIBC的memmove移植到C语言，但检查代码發现编译器生成了
许多低效的代码，很可能内在函数对优化而言并不透明，采用内联汇编後则可避免部分
问题。考虑到可控的要求，故将所有代码以汇编编写，为使可读性更强，采用了：

1.  `ASM_OPT`宏，简单将其参数转变为字符串，用于编写内联汇编裡的汇编指令
2.  以C变量名称访问寄存器，让编译器自行决定汇编中的寄存器使用
3.  考虑到指令码的长度，需为某些变量指定寄存器，以控制生成代码的大小

## fastcpy静态库和动态库的打包

fastcpy设计为独立操作系统的实现，无需依赖任何库，因此也无需动态库入口函数。目前，
fastcpy支持`SSE`,`AVX`,`AVX512`三种版本，为通用性和性能考虑，建议采用`AVX`版本。

使用gcc编译的动态库默认链接`msvcrt`并添加入口函数`DllMainCRTStartup`，这使小型
动态库的体积比静态库大得多(多出90KB)，其实，DLL动态库可以无入口函数，需满足以下：

1.  代码编译为地址无关
2.  全局资源静态初始化，无需构造函数
3.  不依赖于其他动态库，可静态链接C运行时

在Windows上使用时，建议提供`.lib`後缀的导入库或静态库文件，可使用`Visual Studio`
的`x64本机工具命令行`构建：(需gcc编译源文件)

``` Makefile
all: libfastcpy.lib libfastcpy.dll
	del fastcpy.obj

# 生成动态库, 无任何依赖项的静态初始化 DLL 无需入口函数
libfastcpy.dll: fastcpy.obj
	link /DLL /DEF:libfastcpy.def /MACHINE:X64 /SUBSYSTEM:WINDOWS /NOENTRY /NODEFAULTLIB $? /OUT:..\dylib\$@

# 生成静态库
libfastcpy.lib: fastcpy.obj
	lib $? /OUT:..\lib\$@

# 源文件编译
fastcpy.obj: fastcpy.c
	gcc -w -m64 -c -DFASTCPY_STATIC_LIBRARY -O3 -DNDEBUG -march=x86-64-v3 -fcall-used-xmm0 -fcall-used-xmm1 -fcall-used-xmm2 -fcall-used-xmm3 -fcall-used-xmm4 -fcall-used-xmm5 -fcall-used-xmm6 -fcall-used-xmm7 -fcall-used-xmm8 -fcall-used-xmm9 -fcall-used-xmm10 -fcall-used-xmm11 -fcall-used-xmm12 -fcall-used-xmm13 -fcall-used-xmm14 -fcall-used-xmm15 -mno-red-zone -mno-vzeroupper fastcpy.c -o fastcpy.obj
```

注意：`nmake`的`Makefile`文件不能保存为`UTF-8`编码，需保存为`ANSI`编码。

`link`不识别gcc编译的`.drectve`节，因此需`def`文件。要从动态库生成其`def`文件，
可用`Mingw-W64`的`gendef`工具，也可手动编写`def`文件。这样编译得到的`.lib`文件
可以在`Mingw-W64`环境下直接使用，体积也更小，适合发布。

### 关于编译选项

由于fastcpy要支持`x86-64`的两种调用约定`Microsoft x64`和`System V AMD64`，不能
完全使用汇编语言，又因需汇编优化性能并控制指令生成，也不能使用`C/C++`辅助编程，
因此最终采取裸函数的内联汇编，所有代码以汇编描述，并针对两种调用约定绑定不同
寄存器，如下语法可指定内联汇编裡变量或函数的名称：

	type c_name asm("asm_name");// c_name也可以是函数名和参数列表

可以为之指定寄存器名称(此需`register`修饰`type`)。为手动保存xmm寄存器，需指定
`-fcall-used-reg`选项，避免编译器自动生成保存xmm寄存器的代码，然後在内联汇编中
按需保存恢复相应的寄存器。寄存器保存恢复需要栈区，为安全使用，必须操作栈指针
`rsp`，同时还需禁止编译器生成代码使用SysV的`red-zone`(红区，栈指针下的128字节
区)。为通过ymm寄存器传参，还必须禁止编译器自动插入`vzeroupper`指令，gcc添加
`-mno-vzeroupper`编译选项即可。

关于`-march`选项：

1.  `-march=x86-64` 最基本的`x86-64`指令集，支持SSE2
2.  `-march=x86-64-v2` 支持所有SSE系列指令，不支持AVX指令
3.  `-march=x86-64-v3` 支持AVX、AVX2、FMA指令，不支持AVX512指令
4.  `-march=x86-64-v4` 支持AVX512F、AVX512BW、AVX512CD、AVX512DQ、AVX512VL指令


## 优化过程杂谈

1.  关于AVX512指令的测试

在不支持AVX512的硬件上测试`AVX512`版本的fastcpy，需模拟AVX512指令，建议采用Intel
的[SDE工具](https://software.intel.com/en-us/articles/debugging-applications-with-intel-sde)

2.  以jmp指令代替ret指令的方法

在`C/C++`中，函数返回以`ret`指令完成，若最後一个`return`语句调用了仅寄存器传参
的函数，则(在`x86-64`调用约定下)可优化为`jmp`指令调用函数。这是`C/C++`中，唯一
合法的`jmp`跳转到其他标号的方法，内联汇编使用`jmp`指令会导致栈区破坏(除非你使用
裸函数)。在`C/C++`中，除了裸函数，其他函数都会生成`ret`指令，若需在函数末尾进行
操作，只能如此。若需手动控制寄存器的保存与恢复，只能以内联汇编，否则，`C/C++`
编译器都会在函数序言和尾声中进行栈区操作，编译器尚未提供任何选项控制栈区操作。

3.  gcc编译器的警告

gcc编译器的警告选项有[Warning-Options](https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html)

4.  关于gcc asm伪代码

[GNU Assembler伪代码](https://sourceware.org/binutils/docs/as/Pseudo-Ops.html)

5.  关于指令uops查询

[uops.info](https://uops.info/table.html)

6.  关于`inc/dec`与`add/sub`指令的优劣

参考：[stackoverflow: inc vs add](https://stackoverflow.com/questions/36510095/inc-instruction-vs-add-1-does-it-matter)

由于现代CPU部分标志寄存器重命名，两者性能一样。从代码体积来看，`inc/dec`通常优于
`add/sub`(除了某些架构，比如`Alder Lake-E`的`inc/dec`可能需两时钟周期)。

7.  gcc内联汇编的寄存器约束

参考：[机器相关约束](https://gcc.gnu.org/onlinedocs/gcc/Machine-Constraints.html)

| 约束 | 说明                                                                  |
|------|-----------------------------------------------------------------------|
| `R`  | 基础的八个寄存器`a`,`b`,`c`,`d`,`si`,`di`,`bp`,`sp`                   |
| `r`  | (非机器相关)所有可用的整数寄存器                                      |
| `q`  | 可访问低八位的寄存器，`x86`中仅`a`,`b`,`c`,`d`，`x64`中所有整数寄存器 |
| `Q`  | 可访问高八位的寄存器，仅`a`,`b`,`c`,`d`                               |
| `a`  | `a`表示`ax`或`eax`或`rax`                                             |
| `b`  | `b`表示`bx`或`ebx`或`rbx`                                             |
| `c`  | `c`表示`cx`或`ecx`或`rcx`                                             |
| `d`  | `d`表示`dx`或`edx`或`rdx`                                             |
| `S`  | `S`表示`si`,`esi`,`rsi`                                               |
| `D`  | `D`表示`di`,`edi`,`rdi`                                               |
| `A`  | `x86`中表示`eax`和`edx`的组合，`x86`中表示`rax`和`rdx`的组合          |
| `x`  | 所有SSE寄存器，`xmm0~xmm15`或`ymm0~ymm15`                             |
| `v`  | 所有EVEX编码的SSE寄存器，`xmm0~xmm31`或`ymm0~ymm31`或`zmm0~zmm31`     |

8.  cpuid参考详见{[cpuid指令说明]cpuid指令说明.md}

9.  [在线汇编器](https://shell-storm.org/online/Online-Assembler-and-Disassembler/)

10. 非临时读写和预取指令

参考：[stackoverflow: 非临时加载和硬件预取如何共同工作？](https://stackoverflow.com/questions/32103968/non-temporal-loads-and-the-hardware-prefetcher-do-they-work-together)

结论：现代硬件上通常不应使用软件预取，除非内存访问位置随机。

MOVNTDQA指令对普通内存(`WB`可回写缓存)上和正常加载一样，但需额外`ALU uop`，因此
应仅用于`WC`内存区域。`PREFETCHNTA`将数据加载到`L1`和`L3`并从`L2`中驱逐，其可
最大限度避免缓存污染(只会使用部分L3缓存)，由于预取指令需提前發出，难以确定需提前
预取的偏移量，很可能预取失效，因而，通常使用`PREFETCHT0`指令。

11. 关于`4K伪混叠`

参考：[L1缓存带宽下降以4K伪混叠](https://stackoverflow.com/questions/25774190/l1-memory-bandwidth-50-drop-in-efficiency-using-addresses-which-differ-by-4096)

[理解Intel CPU的4K伪混叠](https://stackoverflow.com/questions/54415140/understanding-4k-aliasing-on-intel-cpus)

这是`x86`架构的存储缓冲机制导致，写入的数据暂存于存储缓冲，读取数据时必须检测
存储缓冲中是否有需要的数据，以内存地址的末12位作为检测，只要存在重叠的可能，
读取都必须重新發出指令，这导致严重的性能下降。较新的CPU已经缓解了`4K伪混叠`的
问题，但仍然存在不少影响，因而建议编程时尝试避免`4K伪混叠`。

假定`rdi`,`rsi`对齐到4K，若采取前向复制，则`copy(rdi + 64, rsi, size)`会因
`4K伪混叠`而性能下降，若采取後向复制，则`copy(rdi, rsi + 64, size)`会因`4K伪混叠`
而性能下降，若同时实现前向复制和後向复制并采取合适者，则可完全避免`4K伪混叠`。

注：只要存储缓冲的数据已写入缓存，则相关读取不会發生`4K伪混叠`。

12. gcc扩展asm内联操作数

参考：[gcc扩展汇编寄存器捕获](https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html#Clobbers-and-Scratch-Registers)

13. [gcc优化选项](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html)

14. [gcc选项汇总](https://gcc.gnu.org/onlinedocs/gcc/Option-Summary.html)

15. [一个简单的几种memcpy实现的性能测试对比](https://dirtysalt.github.io/html/simple-memcpy-perf-comparison.html)

fastcpy_bench测试的各种memcpy是参考[此文提到的仓库代码](https://github.com/dirtysalt/codes/tree/master/cc/sr-test/memcpy-bench)而略加修改。

16. [谷歌性能测试的使用说明](https://github.com/google/benchmark/blob/main/docs/user_guide.md)

## 字符串复制的实现

参考 GLIBC [strcpy-avx2.S](./strcpy-avx2.S) 和 [strcpy-evex.S](./strcpy-evex.S) 修改
而实现，并考虑到非自然对齐问题(GLIBC 未曾考虑) 且对各种宽字符都有相应实现。

## 动态库符号重定位实现

[dylibreloc](https://github.com/756yang/dylibreloc.git) 是为实现通用的运行时符号
重定位，从而允许根据运行情况动态选择函数版本。目前，尚未加入显示加载动态库的
代码，因此打包後会包含所有函数版本的不同符号。

此实现是为了使 fastcpy 库可在所有 `x86-64` 平台适用而编写的。
