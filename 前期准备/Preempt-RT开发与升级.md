# Preempt-RT开发与升级

社区维护了两个跟随Linux mainline升级的Preempt-RT patch的git repositroies：

* develop：http://git.kernel.org/cgit/linux/kernel/git/rt/linux-rt-devel.git

    ```bash
    git://git.kernel.org/pub/scm/linux/kernel/git/rt/linux-rt-devel.git
    ```

* stable：http://git.kernel.org/cgit/linux/kernel/git/rt/linux-stable-rt.git

The first one hosts the current development PREEMPT_RT patches with the corresponding Linux mainline source. The development of a particular version usually stops when the focus switches to the next mainline version. This happens once a new stable candidate is released. After this, the development versions are moved to the second repository and are maintained by Steven Rostedt. The maintainers of the first git repository are Sebastian Siewior and Thomas Gleixner.

所有的历史RT patch存放在这里：

* https://cdn.kernel.org/pub/linux/kernel/projects/rt/

Preempt-RT目前更新到4.16，Linux mainline更新到4.17，同时4.18已经到

