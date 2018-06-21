## How to Run

### 1. Prepare in Ubuntu 16.04

#### 1.1 CMake

```bash
$ sudo apt install cmake
```

### 2. Setup LLVM

#### 2.1 Version Choice
	
	This Project developed following the API Guild in LLVM-6.0 Doc. So LLVM-6.0 is most recommend.

	LLVM newer than LLVM-3.5 should be OK, but I haven't check them.

#### 2.2 Build and install

	See https://clang.llvm.org/get_started.html

### 3. Build RSC and run a demo

```bash
$ git clone git@47.93.254.86:RSC/RSC.git
git@47.93.254.86's password: litong
$ cd RSC/src
RSC/src$ mkdir build && cd build
RSC/src/build$ cmake ..
RSC/src/build$ make
RSC/src/build$ cd ../test/demo-test
RSC/src/test/demo-test$ bash demo-test.sh
RSC/src/test/demo-test$ cd ../..
RSC/src$ rm -rf build
```

## The Project is organized as follow:

/src: The source code of the llvm pass. Our main algorithms' implementation are here.
	/include
	/lib
	/test
	/tools

/scripts: Some functional scripts, such as control the tools work in some order, etc.