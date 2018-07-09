# linux源码的打patch、编译、安装、运行

[TOC]

## 1. 在Linux源码上打Preempt-RT patch

首先，需要在[这里](https://www.kernel.org/pub/linux/kernel/)选择内核的版本并下载。之后，从[这里](https://www.kernel.org/pub/linux/kernel/projects/rt)找到并下载对应版本的PREEMPT_RT patch。

下载好后，解压patch，并打到Linux的代码上（下面Linux-4.4.12及其对应的Preempt-RT patch为例，在具体使用时需要更换为实际的版本号）：

```bash
$ xz -cd linux-4.4.12.tar.xz | tar xvf -
$ cd linux-4.4.12
$ xzcat ../patch-4.4.12-rt19.patch.xz | patch -p1
```

## 2. 配置内核

内核代码在编译前需要配置，一般使用比较多的是`make menuconfig`命令。唯一需要为Preempt-RT Linux配置的选项是`“Fully Preemptible Kernel”`，它在preemption model选项中。所有其它的选项根据系统的实际需要进行配置就可以了。关于如何配置kernel的更多详细信息可以参见[Linux kernel documentation](https://www.kernel.org/doc/Documentation/kbuild/kconfig.txt)。

## 3. 编译、安装、运行内核

后面的步骤与正常的Linux完全一样，可以参考[这个博客](https://www.cnblogs.com/Caden-liu8888/p/7752549.html)。


