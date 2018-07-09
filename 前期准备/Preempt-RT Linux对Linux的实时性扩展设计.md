# Preempt-RT Linux对Linux的实时性扩展设计

[TOC]

## 1. 基本原理

### 1.1 Linux specific preemption models

Linux针对不同领域（比如服务器、桌面PC等等）的应用实现了三种不同的抢占模型。PREEMPT-RT补丁提供了另外两种抢占模型，其中，“完全可抢占Kernel（Fully Preemptible Kernel）”模型将Linux操作系统变成了实时操作系统。

根据内核的不同，抢占模型也会不同。原则上来说，用户空间的程序总是可抢占的。

抢占一个正在运行的任务是由调度器执行的。这个操作可以在与内核交互时被触发，例如系统调用，或者异步事件（比如中断）等。调度器会保存被抢占的任务的运行环境，载入新任务的运行环境。

Linux内核实现了几个抢占模型。在配置内核编译选项的时候可以选择需要的模型。如果要将Linux变成实时操作系统（Real-time operating system, RTOS），必须选择“完全可抢占内核”（“Fully Preemptible Kernel”）抢占模型。下面是Linux抢占模型完整的选项列表，以及简短的解释。最后面两个选项只有打了PREEMPT_RT patch的内核才有。

* 非强制抢占模型（适用于服务器）【No Forced Preemption (server)】：传统的Linux抢占模型，适合吞吐量优先的机器。抢占仅发生在系统调用返回和中断。

* 自愿抢占Kernel（适用于桌面端）【Voluntary Kernel Preemption (Desktop)】：这个选项通过在内核代码中增加一些 ”明确的抢占发生时间点“（“explicit preemption points”）来降低内核的延迟，**代价是稍微降低了吞吐量**。除了这些明确的时间点，系统调用返回和中断返回是隐含的抢占时间点（preemption point）。

* 可抢占内核（适用于低延迟桌面端）【Preemptible Kernel (Low-Latency Desktop)】：这个选项将除了临界区外所有的内核代码设置为可抢占的，以此降低内核的延迟。每个禁止抢占的区域后面，都会有一个隐含的抢占点（preemption point）。

* 可抢占内核（基本的实时系统）：这个抢占模型类似于“可抢占内核（适用于低延迟桌面端）”，除此之外，线程化中断处理程序是必须开启的。这个模型主要用于测试和调试PREEMPT_RT patch实现的，用来替换原有机制的新机制。

* 完全可抢占内核（实时系统）【Fully Preemptible Kernel（RT）】：除了一小部分临界区代码外，所有的内核代码都是可抢占的。线程化中断处理程序是必须开启的。除此之外，替换了一些原有的机制（比如sleeping spinlocks，rt mutex等）以减少禁止抢占的区域。另外，大的禁止抢占区域用若干个锁替代。如果需要获得实时操作系统，必须选择这个模型。

### 1.2 Scheduling - Policy and priority

Linux kernel实现了几种实时和非实时的调度策略。根据调度策略，调度器决定哪个任务被换出，以及接下来要执行哪个任务。Linux实现了几种调度策略，它们被分为实时策略与非实时策略。这些调度策略已经包含在在Linux mainline中了。

#### （1）非实时调度策略（Non real-time policies）：

* SCHED_OTHER：每个任务有一个”nice值“。这是一个位于[-20,19]区间的整数值，-20是最高的nice值，19是最低的nice值。任务的平均运行时间与nice值有关。
* SCHED_BATCH：这个策略源于`SCHED_OTHER`，**针对吞吐量做了一些优化**。
* SCHED_IDLE：这个策略也源于`SCHED_OTHER`，但是有比19更弱的nice值（也就是大于19的nice值）。

#### （2）实时调度策略（Real-time policies）：

* SCHED_FIFO：先到先服务。每个任务有一个优先级，用一个[1,99]之间的整数表示，数值越大，优先级越高。一个任务一旦占用cpu，会一直运行到自己完成或者有更高优先级任务到达。
* SCHED_RR：时间片轮转 。这个策略源于`SCHED_FIFO`。不同的是，一个任务会在一个时间片内一直运行（如果没有被更高优先级的任务抢占）。当时间片用完后，该任务可以被具有相同优先级的任务抢占。时间片的定义在`procfs (/proc/sys/kernel/sched_rr_timeslice_ms)`中。
* SCHED_DEADLINE：这个策略实现了全局最早截止时间优先算法【Global Earliest Deadline First (GEDF) algorithm】。在这种策略下，一个任务可以抢占任何由`SCHED_FIFO`或者`SCHED_RR`策略调度的任务。

### 1.3 Scheduling - RT throttling

RT throttling机制可以保证在实时的应用程序中出现程序错误时，系统不会卡死在那里。RT trhottling机制可以停止这种程序。RT throtting的设置在proc文件系统里。

实时应用程序的程序错误会导致整个系统挂起。这种程序错误类似于一个`while(true){}`的死循环。当实时应用程序有最高的优先级，并且调度策略是`SCHED_FIFO`时，其它所有的任务都不能抢占它。这就导致系统阻塞其它的所有任务，并且一直在100%的CPU占用率下执行这个死循环。Real-time throttling机制通过限制每个周期内实时任务的执行时间，来避免这种情况。这个机制的设置在proc文件系统内。默认设置为：

```
# cat /proc/sys/kernel/sched_rt_period_us
1000000
# cat /proc/sys/kernel/sched_rt_runtime_us
950000
```

为了让实时任务只占用50%的CPU，并且让一个周期的时间更长，可以用下面的命令设置这两个值：

```
# echo 2000000 > /proc/sys/kernel/sched_rt_period_us
# echo 1000000 > /proc/sys/kernel/sched_rt_runtime_us
```

如果实时任务的运行时间和周期时长一样的话，Real-time throttling机制就没用了。在`sched_rt_runtime_us`变量中写入`-1`有同样的效果，可以取消对实时任务的CPU时间限制：

```
# echo -1 > /proc/sys/kernel/sched_rt_runtime_us
```

这个机制已经在Linux mainline中实现了。


### 1.4 Priority inversion - Priority inheritance

当一个高优先级的任务因为一个低优先级的任务占有了互斥资源，而被这个低优先级的任务阻塞时，另外一个优先级在这两个任务之间的程序，在优先级最高的任务恢复运行前，可以先执行。这个现象就是优先级反转。它可以通过优先级继承来解决。优先级继承是解决优先级反转问题的一个方法。

#### 1.4.1 优先级反转

![priority-inversion](http://oy60g0sqx.bkt.clouddn.com/2018-02-26-priority-inversion.png)

* 时间点（1）：一个低优先级的任务`L`开始运行；
* 时间点（2）：`L`占用互斥资源；
* 时间点（3）：一个高优先级的任务`H`抢占了`L`，开始执行，但`L`仍占有互斥资源；
* 时间点（4）：第三个任务`M`，它的优先级在`H`和`L`之间，并且不需要`L`占有的资源。`M`已经准备就绪，但是它必须等待，因为更高优先级的`H`正在运行。
* 时间点（5）：`H`需要的资源仍然被`L`占用，所以`H`停止运行，直到资源可用。
* 时间点（6）：已准备就绪的任务`M`会阻止`L`执行，因为`M`的优先级更高。这就导致了优先级反转，因为**高优先级的`H`必须等待优先级较低的`M`执行完才能运行**。只有`M`执行完，`L`才能执行，`L`执行完，才能释放资源。
* 时间点（7）：`L`执行完，释放资源，`H`继续执行。

#### 1.4.2 优先级继承

优先级反转问题可以用优先级继承来解决：

![priority-inheritance](http://oy60g0sqx.bkt.clouddn.com/2018-02-26-priority-inheritance.png)

* 时间点（5）：`H`申请资源，但是资源被`L`占用，于是`L`继承`H`的优先级，继续执行。
* 时间点（6）：**`L`释放资源，优先级恢复成自己原来的优先级**，`H`获得资源继续执行。
* 时间点（7）：`H`执行完毕，`M`执行。
* 时间点（8）：`M`执行完毕，`L`执行。

也就是说，当`H`需要被`L`占用的资源时，`L`继承`H`的优先级，以此让资源更快的释放。这样，就不会出现`M`优先级低于`H`，但比`H`先完成的情况。

## 2. 技术细节

PREEMPT_RT patch的主要目标是让kernel中不可抢占的代码量最小化。因此需要实现一些不同的机制，这些机制有部分已经包含在Linux mainline中了。

## 2.1 高精度计时器

高精度计时器（High Resolution Timer）让精确的定时调度成为可能，并且移除了计时器对全局变量`jiffies`的依赖。[“high resolution timer design notes”](https://rt.wiki.kernel.org/index.php/High_resolution_timer_design_notes)里解释了高精度计时器的实现细节，现在已经是kernel documentation的一部分。

从Linux-2.6.24-rc1开始，高精度计时器patches就被完整的合并到mainline里，所以这个项目已经成为历史。之前都是包含在realtime preemption patch里的。

jiffies是Linux中记录从电脑开机到现在总共的时钟中断次数的变量。硬件给内核提供一个系统定时器用以计算和管理时间，内核通过编程预设系统定时器的频率，即节拍率（tick rate)，每一个周期称作一个节拍（tick）。Linux内核从2.5版内核开始把频率从100调高到1000。更多关于jiffies的资料可以参见[这个博客](http://blog.csdn.net/allen6268198/article/details/7270194)。

### 2.1 高精度计时器介绍

要了解高精度计时器，首先要对Linux的时间子系统有基本的了解。可以参考[Linux时间子系统系列博客](http://www.wowotech.net/timer_subsystem/time_subsystem_index.html)。

#### 2.1.1 如何使用高精度计时器

只有最近发布的glibc（GNU libc，即c运行库，是Linux系统中最底层的api库）需要用到高精度计时器。当Linux内核中的高精度计时器启用后，`nanosleep`，`itimers`，以及`posix timers`都可以提供高精度模式而不用修改源代码。动态优先级机制支持高精度计时器。

当realtime preemption启用后，`itimer`和`posix interval timers`的信号发送不能在硬中断的高分辨率时钟中断的context中完成。因为锁的限制，信号的发送必须在线程的context中。为了避免延迟过长，软中断线程已经被分割成几个preemption patch。这些分割可以显著提高系统的实时性，但是仍有一个问题没有解决。`hrtimers`软中断线程可以被高优先级的线程延迟任意长的时间。一个可能的解决方案是提高`hrtimer`软中断线程的优先级，但是这样的话，所有与signal相关的timer都会以高优先级发送消息，从而引起原本的高优先级任务的延迟。之前某个版本的realtime preemption patch已经为这个问题提供了一个解决方案：根据接收消息的任务的优先级，来动态调整软中断的优先级。

When realtime preemption is enabled, the delivery of signals at the expiry of itimer and posix interval timers can not be done in the hard interrupt context of the high resolution timer interrupt. The signal delivery must happen in thread context due to locking constraints. To avoid long latencies the softirq threads have been separated in the realtime preemption patch a while ago. While this separation enhanced the behavior significantly, there was still a problem remaining. The hrtimers softirq thread can be arbitrarily long delayed by higher priority tasks. A possible solution is to up the priority of the hrtimer softirq thread, but this has the effect that all timer related signals are delivered at high priority and therefore introduce latency impacts to high priority tasks. A prior version of the realtime preemption patch contained a solution for this problem already: dynamic adjustment of the softirq priority depending on the priority of the task for which the signal has to be delivered.

This functionality was removed with the rework of the high resolution timer patches and due to a subtle race condition with the Priority Inheritance code. The new design of RT-Mutexes and the core PI support in the scheduler removed this race condition and allowed to re-implement this feature. On a PentiumIII 400 MHz test machine this change reduced the maximum user space latency for a thread waiting on the delivery of a periodic signal significantly from ~400 to ~90 micro seconds under full system load.

Note that (clock_)nanosleep functions do not suffer from this problem as the wakeup function at timer expiry is executed in the context of the high resolution timer interrupt. If an application is not using asynchronous signal handlers, it is recommended to use the clock_nanosleep() function with the TIMER_ABSTIME flag set instead of waiting for the periodic timer signal delivery. The application has to maintain the absolute expiry time for the next interval itself, but this is a lightweight operation of adding and normalizing two struct timespec values. The benefit is significantly lower maximum latencies (~50us) and less OS overhead in general.

#### 2.1.2 High resolution timer design notes

更多信息请参见OLS 2006 talk的paper “hrtimers and beyond”。这篇paper是OLS 2006 Proceedings Volume 1的一部分，可以在[OLS website](https://www.kernel.org/doc/ols/2006/ols2006v1-pages-333-346.pdf)找到（“/Users/lt/Documents/thu_oslab/Preempt_RT/项目网站Doc翻译/补充资料/Hrtimers and Beyond: Transforming the Linux Time Subsystems.pdf”）。

这个talk的slides在[这里](http://www.cs.columbia.edu/~nahum/w6998/papers/ols2006-hrtimers-slides.pdf)（“/Users/lt/Documents/thu_oslab/Preempt_RT/项目网站Doc翻译/补充资料/hrtimers and beyond ­- transformation of the Linux time(r) system.pdf”）。

这个slides包含五张图（分别在第2、15、18、20、22页)，它们阐述了和时间相关的Linux子系统的演变过程。图#1（p. 2）展示了在hrtimers之前，Linux time® system的设计，以及其它已经合并到mainline的部分。

注意：paper和slides讨论的是“clock event source”，但我们暂时换为“clock event devices”这个名字。

设计包含几下几个基本的部分：

* hrtimer基础设施（hrtimer base infrastructure）
* timeofday和clock source管理（timeofday and clock source management）
* Clock event管理（Clock event management）
* 高精度计时器的设计（High resolution timer functionality）
* 动态节拍（Dynamic tick）

#### 2.1.3 hrtimer base infrastructure

hrtimer的基础设施在linux-2.6.16中合并到kernel中。实现的细节在`Documentation/timers/hrtimers.txt`中。在slides的图#2（OLS slides p. 15）中也能看到。

The main differences to the timer wheel, which holds the armed timer_list type timers are:

1. time ordered enqueueing into a rb-tree
2. independent of ticks (the processing is based on nanoseconds)

#### 2.1.4 timeofday and clock source management

正如slides的图#3（OLS slides p. 18）所示，John Stultz提出的Generic Time Of Day (GTOD)框架将很多代码从architecture-specific区域移入generic management framework。

John Stultz's Generic Time Of Day (GTOD) framework moves a large portion of code out of the architecture-specific areas into a generic management framework, as illustrated in figure #3 (OLS slides p. 18). The architecture specific portion is reduced to the low level hardware details of the clock sources, which are registered in the framework and selected on a quality based decision. The low level code provides hardware setup and readout routines and initializes data structures, which are used by the generic time keeping code to convert the clock ticks to nanosecond based time values. All other time keeping related functionality is moved into the generic code. The GTOD base patch got merged into the 2.6.18 kernel.

关于Generic Time Of Day框架的更多信息，请参见[OLS 2005 Proceedings Volume 1](https://www.kernel.org/doc/ols/2005/ols2005v1-pages-227-240.pdf)。

论文“We Are Not Getting Any Younger: A New Approach to Time and Timers”的作者是J. Stultz, D.V. Hart, & N. Aravamudan。

图#3（OLS slides p.18）阐述了这个变化
Figure #3 (OLS slides p.18) illustrates the transformation.

### 2.2 Sleeping spinlocks

在non-PREEMPT-RT模型中，spinlock直接映射到raw spinlock。任务B占有了一个spinlock后，后申请该spinlock的任务A会一直空转等待，直到任务B释放了该spinlock。在raw spinlock中，禁止抢占。在PREEMPT_RT模型中，spinlock被映射到sleeping spinlock，而raw spinlock保持原语义。一个等待sleeping spinlock的任务将会睡眠，当该spinlock被释放时，任务会被唤醒。在sleeping spinlock中，允许抢占。

自旋锁（spinlock）在kernel中用于保证在某个时刻最多只有一个线程可以访问临界区代码。相比于mutex，spinlock的优势在于使用简单、执行速度快。然而，如果锁保持的时间太长，或者与其它线程产生竞争的可能性增大时，spinlock可能会成为性能瓶颈，并且可能会造成延迟（latency）。延迟（latency）是实时系统中非常重要的一个问题。

为了降低延迟，PREEMPT_RT patch将kernel中大部分的spinlock换成了rt_mutex。为了支持这项修改的基础工作大部分已经合并到主线了，它们包括减少临界区代码量，以及增加一个新的类型，来区别真正的spinlock和修改后`spinlock_t`类型中使用的rt_mutex。为了最少地修改kernel代码，已有的`spinlock_t`数据类型和使用它们的函数保持不变，但是当PREEMPT_RT开启后，`spinlock_t`会映射为rt_mutex。不幸的是，这可能会让阅读代码的人误解，因为当PREEMPT_RT开启时，`spinlock_t`不是一个真正的spinlock。

因为sleeping spinlock允许抢占，所以它们不能被锁上，即使抢占和中断disabled。如果某处的spinlock必须上锁，必须使用原来的spinlock。

当rt_mutex由于某种原因不适合使用时，代码必须修改为使用`raw_spinlock_t`，`raw_spinlock_t`保持原来的spinlock的语义。

### 2.3 Threaded interrupt handler

### 2.4 rt_mutex

Mainline的Linux内核中所有的mutex都被替换为rt_mutex。rt_mutex实现了优先级继承机制，防止优先级反转。Sleeping spinlock和rwlock也实现了这个机制。然而，持有信号量的进程会被抢占，但是没有参与优先级继承。

### 2.5 RCU


