# 30dayMakeOS-efi
将《30天自制操作系统》书中内容用基于64位的UEFI架构改写



## 开发工具说明

极简开发环境，没有任何第三方库。基于64位Windows操作系统：

- NASM汇编器；
- MinGW-GCC 64位（无第三方库）；
- fatimg.exe（自制）：用于制作一个UEFI可识别的磁盘映像文件。

> 注：使用纯C语言编写的命令行工具，可将宿主机中一个普通文件夹（包含Bootloader和内核文件），打包成一个具有MBR 分区表和FAT16文件系统的img格式磁盘映像文件。
> 仅支持8.3短文件名。

- VirtualBox：将img文件转为可挂在的vdi虚拟磁盘文件，并运行虚拟机。

>  注：开发过程中会反复编译并修改新的.vdi虚拟磁盘文件；每次使用 `VBoxManage convertfromraw` 生成新的VDI文件时，VirtualBox都会为其生成一个新的UUID。但VirtualBox的全局配置依然记忆着旧VDI文件的UUID，产生冲突时VirtualBox会出于安全保护机制拒绝加载该磁盘。在Makefile中，需要加入自动重置UUID的步骤。



目录文件说明：

- fatimg：磁盘映像工具（见上文）；

- libc：笔者修改后不依赖任何操作系统功能的基本函数（如sprintf）的实现，在make的过程中生成可以直接链接的obj文件链接到我们的操作系统中去；



##具体实现

**Day_01**

纯汇编写一个最小的UEFI版Hello, world程序。	

**Day_02**

纯汇编编写bootloader，读盘并加载kernel.bin。
\*过程中没有使用EDK II或其它UEFI的第三方库，而是仅用汇编语言编写（在Windows上，配置相关的库会很麻烦，尤其是对初学者而言；笔者觉得还是汇编语言简单:)）。

**Day_03**

实现画面模式的切换；模仿书中进行条纹图案的显示。

**Day_04**

开始导入C语言，描绘操作系统界面，引入字体（hankaku.bin来源《30天自制操作系统》）。
这一节中，仅仅是将C语言生成的kernel.exe的二进制部分使用objcopy拷贝出来生成kernel.bin，因此对数据段的地址引用不正确。对于不涉及数据段的处理，如描画基本的界面可以成功，但涉及全局变量、字符串字面量的地方不能正常显示。
对于这个问题，我们会在下一节进行修正。

**Day_05**

内核文件拆分为asmhead.bin（由asmhead.asm生成）和bootpack.exe（由bootpack.c生成）两部分拼接而成。由bootloader跳转到asmhead.asm后，先解析bootpack.exe的PE文件结构，以正确处理各个段的地址，然后跳转到bootpack.exe执行。这样可以确保数据段的地址引用正确。

**Day_06**

初始化IDT、PIC；制作最简单的键盘中断处理程序。

**Day_07**

引入FIFO缓冲区，优化键盘中断的处理；完成鼠标控制电路的初始化，并制作用于接受数据的中断处理程序。







