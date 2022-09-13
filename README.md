## 安装 pkg-config
```
sudo apt install pkg-config
```

## 修改环境变量 

在 文件 env.sh 中 修改
```
export SYSROOT_PATH=~/.local/riscv64-toolchain/sysroots
```

路径到 D1_SDK 的安装 sysroot 路径

## 设置环境变量

```
source env.sh
```

## 编译

```
cd solutions/fota-service
mkdir build
cd build
cmake ..
make
```

## solution 增加组件

solutions/fota-service/CMakeLists.txt 文件中 增加 COMPONENT_LIST 即可
```
list(APPEND COMPONENT_LIST
            kv
            ulog)

```

## 如何增加一个三方库依赖

```
pkg_check_modules (LIBS REQUIRED IMPORTED_TARGET gio-2.0 dbus-1)
```

在该语句后面增加三方库名称
注意：名称会有版本号后缀，可以去 ~/.local/riscv64-toolchain/sysroots/riscv64-oe-linux/usr/lib/pkgconfig 目录搜索对应的具体名称