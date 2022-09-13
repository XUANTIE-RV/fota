# env.sh

if [ -d /usr/local/oecore-x86_64/sysroots ]; then
    export SYSROOT_PATH=/usr/local/oecore-x86_64/sysroots
fi

if [ -d ~/.local/riscv64-toolchain/sysroots ]; then
    export SYSROOT_PATH=~/.local/riscv64-toolchain/sysroots
fi