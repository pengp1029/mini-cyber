# GTest 环境配置指南

> 沉淀日期: 2026-05-13

## 安装步骤

### 方法一：系统包管理器安装（不推荐，ABI 兼容性问题）

```bash
sudo apt install libgtest-dev
cd /usr/src/gtest && sudo cmake . && sudo make && sudo cp lib/*.a /usr/lib/
```

**问题：** 系统预装的 gtest 编译 ABI 与当前编译器不匹配，链接时报 `undefined reference`。

### 方法二：从源码编译（推荐）

```bash
cd /tmp
git clone https://github.com/google/googletest.git
cd googletest
cmake -B build -DCMAKE_CXX_STANDARD=17
cmake --build build
sudo cp build/lib/libgtest*.a /usr/lib/
sudo cp build/lib/libgmock*.a /usr/lib/
```

## 常见问题

### 问题 1：链接报 `undefined reference`

**错误信息：**
```
undefined reference to `testing::internal::MakeAndRegisterTestInfo(...)`
```

**原因：** ABI 不兼容。代码用新 ABI 编译（默认），但 gtest 库是旧 ABI 编译的。

**解决方法：** 编译时强制使用旧 ABI（临时方案）
```bash
g++ -D_GLIBCXX_USE_CXX11_ABI=0 test.cpp -lgtest -lgtest_main -pthread -o test
```

或者重新编译 gtest（长期方案，见方法二）。

---

### 问题 2：覆盖系统库后仍然链接失败

**现象：** 重新编译 gtest 并覆盖系统库后，`g++ -lgtest` 仍然报错。

**原因：** 链接器可能缓存或查找多个版本的库。

**解决方法：** 直接指定绝对路径
```bash
g++ -std=c++17 test.cpp \n    /usr/lib/libgtest.a \n    /usr/lib/libgtest_main.a \n    /usr/lib/libgmock.a \n    -lrt -pthread -o test
```

---

### 问题 3：gmock 依赖缺失

**错误信息：** undefined reference to gmock 相关符号

**解决方法：** 链接时显式包含 gmock
```bash
g++ test.cpp /tmp/googletest/build/lib/lib{gtest,gtest_main,gmock}.a -pthread -o test
```

---

## 推荐编译方式

### Makefile 方式

```makefile
GTEST_ROOT = /tmp/googletest
CXX = g++
CXXFLAGS = -std=c++17 -I$(GTEST_ROOT)/googletest/include
LDFLAGS = $(GTEST_ROOT)/build/lib/libgtest.a \n          $(GTEST_ROOT)/build/lib/libgtest_main.a \n          $(GTEST_ROOT)/build/lib/libgmock.a \n          -lrt -pthread

test: test.cpp
    $(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)
```

### Shell Alias 方式

```bash
alias gtest-compile='g++ -std=c++17 $1 \n    /tmp/googletest/build/lib/libgtest.a \n    /tmp/googletest/build/lib/libgtest_main.a \n    /tmp/googletest/build/lib/libgmock.a \n    -lrt -pthread -o ${2:-test} \n    -I/tmp/googletest/googletest/include'

# 使用：gtest-compile test.cpp test
```

## 调试技巧

### 检查链接器实际查找的库

```bash
g++ -v test.cpp -lgtest 2>&1 | grep gtest
```

### 检查库中的符号

```bash
nm /usr/lib/libgtest.a | grep MakeAndRegisterTestInfo
```

### 检查库版本和修改时间

```bash
ls -la /usr/lib/libgtest*
ldconfig -p | grep gtest
```

## 关键概念

### ABI 兼容性

GCC 5 引入了新的 C++ ABI（Dual ABI）：
- 旧 ABI：`std::string` 为旧版本
- 新 ABI（默认）：`std::__cxx11::string` 为新版本

代码和库必须用同一 ABI 编译，否则符号名不匹配，链接失败。

### 静态库链接优先级

链接器查找顺序：
1. 命令行绝对路径（如 `/usr/lib/libgtest.a`）
2. `-L` 指定的路径
3. 系统默认路径（`/usr/lib`、`/lib` 等）
4. `LIBRARY_PATH` 环境变量

**最佳实践：** 关键库用绝对路径，避免歧义。