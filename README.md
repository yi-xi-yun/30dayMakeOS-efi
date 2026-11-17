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


## 具体实现


**Day_01**

纯汇编写一个最小的UEFI版Hello, world程序。	

**Day_02**

纯汇编编写bootloader，读盘并加载kernel.bin。
\*过程中没有使用EDK II或其它UEFI的第三方库，而是仅用汇编语言编写（在Windows上，配置相关的库会很麻烦，尤其是对初学者而言；笔者觉得还是汇编语言简单:)）。

**Day_03**

实现画面模式的切换；模仿书中进行条纹图案的显示。
