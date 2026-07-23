
克隆仓库：git clone --recursive https://github.com/cnjn/linux-mainline-zte-zxslc-sr1010.git -b build

编译源码：sh port/mainline/build-zxdbg.sh

编译产物：out/sr1010-zxdbg.itb

启动方式：1. 进uboot，密码 5cE080@fyBD 2. 主机接wan口，设置静态ip 192.168.1.100，启动tftpd 3. uboot shell中 tftpboot out/sr1010-zxdbg.itb 4. bootm


