#!/bin/bash
chcp.com 65001
# 获取当前脚本所在目录
SCRIPT_DIR=$(dirname "$(realpath "$0")")

# 从当前目录开始，向上查找直到找到 CMakeLists.txt 文件，确定项目根目录
PROJECT_DIR=$SCRIPT_DIR
while [ ! -f "$PROJECT_DIR/CMakeLists.txt" ]; do
    PROJECT_DIR=$(dirname "$PROJECT_DIR")
done

# 获取项目根目录名
PROJECT_NAME=$(basename "$PROJECT_DIR")

echo "Project root directory name: $PROJECT_NAME"

# 创建并进入构建目录
BUILD_DIR="$PROJECT_DIR/build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# 如果之前用的是其他生成器，清理缓存
if [ -f "CMakeCache.txt" ]; then
    echo "Detected previous CMake cache, cleaning..."
    rm -rf CMakeCache.txt CMakeFiles
fi

# 运行 CMake 配置，指定工具链文件
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/gcc-arm-none-eabi.cmake -G "Unix Makefiles"
make -j 12

# 根据项目名称生成 ELF、BIN 和 HEX 文件路径
ELF_FILE="$BUILD_DIR/${PROJECT_NAME}.elf"
BIN_FILE="$BUILD_DIR/${PROJECT_NAME}.bin"
HEX_FILE="$BUILD_DIR/${PROJECT_NAME}.hex"

# 检查 ELF 文件是否生成成功
if [ -f "$ELF_FILE" ]; then
    # 将 ELF 文件转换为 BIN 文件和 HEX 文件
    arm-none-eabi-objcopy -O binary "$ELF_FILE" "$BIN_FILE"
    arm-none-eabi-objcopy -O ihex "$ELF_FILE" "$HEX_FILE"
    echo "Conversion to BIN and HEX completed."
else
    echo "Error: ELF file not found. Compilation might have failed."
    exit 1
fi

# ==============================================
# 烧录部分：支持 jlink / stm32cubeprogrammer / openocd / keil
# ==============================================

FLASHER=${1:-"jlink"}
DEVICE="STM32H723VG"

echo "----------------------------------------"
echo "准备烧录: $PROJECT_NAME"
echo "使用工具: $FLASHER"
echo "----------------------------------------"

# ---------- 通用函数：查找可执行文件 ----------
find_tool() {
    local tool_name=$1
    shift
    # 先尝试从 PATH 中查找
    if command -v "$tool_name" >/dev/null 2>&1; then
        command -v "$tool_name"
        return 0
    fi
    # 否则在常见安装路径中搜索
    for path in "$@"; do
        # 将 Windows 路径转为 Unix 风格以便在 Git Bash 中检查
        local unix_path=$(echo "$path" | sed 's|\\|/|g' | sed 's|^\([A-Z]\):|/\1|')
        if [ -f "$unix_path" ]; then
            echo "$unix_path"
            return 0
        fi
    done
    return 1
}

# ---------- 路径转换：Unix -> Windows ----------
to_win_path() {
    echo "$1" | sed 's|^/\([a-zA-Z]\)/|\1:/|' | sed 's|/|\\|g'
}

# ========== 1. J-Link 烧录 ==========
if [ "$FLASHER" = "jlink" ]; then
    # 查找 JLink.exe
    JLINK_CMD=$(find_tool "JLink.exe" \
        "/c/Program Files/SEGGER/JLink_V934/JLink.exe" \
        "/c/Program Files (x86)/SEGGER/JLink/JLink.exe" \
        "/c/Program Files/SEGGER/JLink/JLink.exe" \
        "/d/Program Files/SEGGER/JLink/JLink.exe"
    )

    if [ -z "$JLINK_CMD" ]; then
        echo "错误：未找到 JLink.exe，请安装 SEGGER J-Link 软件或将路径加入 PATH。"
        exit 1
    fi

    echo "找到 JLink: $JLINK_CMD"
    echo "生成 J-Link 命令脚本..."

    WIN_BIN_FILE=$(to_win_path "$BIN_FILE")

    cat > "$PROJECT_DIR/flash.jlink" << EOF
device $DEVICE
si SWD
speed 4000
loadbin $WIN_BIN_FILE 0x08000000
r 2
g
qc
EOF

    echo "执行 J-Link 烧录..."
    "$JLINK_CMD" -nogui 1 -autoconnect 1 -commandfile "$PROJECT_DIR/flash.jlink"

# ========== 2. STM32CubeProgrammer 烧录（替代旧版 ST-LINK Utility） ==========
elif [ "$FLASHER" = "stlink" ]; then
    # 查找 STM32_Programmer_CLI.exe
    CUBE_CMD=$(find_tool "STM32_Programmer_CLI.exe" \
        "/c/Program Files/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe" \
        "/c/Program Files (x86)/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe" \
        "/d/Program Files/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe"
    )

    if [ -z "$CUBE_CMD" ]; then
        echo "错误：未找到 STM32_Programmer_CLI.exe，请安装 STM32CubeProgrammer。"
        exit 1
    fi

    echo "找到 STM32CubeProgrammer: $CUBE_CMD"
    echo "执行烧录（使用 STM32CubeProgrammer）..."

    # 使用 HEX 文件，模式：SWD + Under Reset，烧录后校验、复位、运行
    "$CUBE_CMD" -c port=SWD mode=UR -w "$HEX_FILE" -v -hardRst -run
    if [ $? -eq 0 ]; then
        echo "烧录成功！"
    else
        echo "烧录失败，请检查连接或芯片状态。"
        exit 1
    fi

# ========== 3. OpenOCD 烧录（适合 VS Code + Cortex-Debug） ==========
elif [ "$FLASHER" = "openocd" ]; then
    # 查找 openocd.exe
    OPENOCD_CMD=$(find_tool "openocd.exe" \
        "/c/OpenOCD/bin/openocd.exe" \
        "/c/Program Files/OpenOCD/bin/openocd.exe" \
        "/d/OpenOCD/bin/openocd.exe"
    )

    if [ -z "$OPENOCD_CMD" ]; then
        echo "错误：未找到 openocd.exe，请安装 OpenOCD 并将路径加入 PATH。"
        exit 1
    fi

    echo "找到 OpenOCD: $OPENOCD_CMD"
    echo "执行 OpenOCD 烧录..."

    # 使用 BIN 文件，指定起始地址 0x08000000
    # 生成临时 OpenOCD 脚本
    cat > "$PROJECT_DIR/flash.openocd" << EOF
# 使用 ST-Link 接口
source [find interface/stlink.cfg]
# 目标芯片配置（适用于 STM32H7 系列）
source [find target/stm32h7x.cfg]

# 复位连接
init
reset halt

# 烧录程序
program $BIN_FILE 0x08000000 verify

# 复位并运行
reset run
exit
EOF

    # 执行 OpenOCD
    "$OPENOCD_CMD" -f "$PROJECT_DIR/flash.openocd"

    if [ $? -eq 0 ]; then
        echo "烧录成功！"
    else
        echo "烧录失败，请检查 OpenOCD 配置或硬件连接。"
        exit 1
    fi

# ========== 4. Keil MDK 烧录（通过固定下载工程） ==========
elif [ "$FLASHER" = "keil" ]; then
    # 固定的 Keil 下载工程路径（注意：使用正斜杠，Git Bash 可识别）
    KEIL_PROJECT_BASE="D:/VSCODESTM32H723/keil_for_download/SevenAxis/MDK-ARM"
    KEIL_PROJECT_FILE="$KEIL_PROJECT_BASE/SixAxisSTM32H723.uvprojx"
    KEIL_OUTPUT_DIR="$KEIL_PROJECT_BASE/SixAxisSTM32H723"

    # 转换为 Unix 风格路径供 Git Bash 操作（将盘符转为 /d/ 形式）
    UNIX_KEIL_PROJECT_FILE=$(echo "$KEIL_PROJECT_FILE" | sed 's|^\([A-Z]\):|/\1|' | sed 's|\\|/|g')
    UNIX_KEIL_OUTPUT_DIR=$(echo "$KEIL_OUTPUT_DIR" | sed 's|^\([A-Z]\):|/\1|' | sed 's|\\|/|g')

    # 检查 Keil 工程是否存在
    if [ ! -f "$UNIX_KEIL_PROJECT_FILE" ]; then
        echo "错误：Keil 工程文件不存在：$UNIX_KEIL_PROJECT_FILE"
        exit 1
    fi

    # 固定 UV4.exe 路径（用户指定）
    UV4_CMD="E:/keil5/UV4/UV4.exe"
    # 转换为 Unix 格式用于检查存在性
    UNIX_UV4_CMD=$(echo "$UV4_CMD" | sed 's|^\([A-Z]\):|/\1|' | sed 's|\\|/|g')
    if [ ! -f "$UNIX_UV4_CMD" ]; then
        echo "错误：未找到 UV4.exe，请检查路径：$UV4_CMD"
        exit 1
    fi

    echo "找到 Keil uVision: $UV4_CMD"
    echo "Keil 工程路径: $UNIX_KEIL_PROJECT_FILE"
    echo "输出目标目录: $UNIX_KEIL_OUTPUT_DIR"

    # 创建目标输出目录（如果不存在）
    mkdir -p "$UNIX_KEIL_OUTPUT_DIR"

    # 复制编译生成的 ELF、BIN、HEX 文件到 Keil 工程输出目录，并重命名为工程期望的文件名
    echo "正在复制输出文件..."

    if [ -f "$ELF_FILE" ]; then
        cp -f "$ELF_FILE" "$UNIX_KEIL_OUTPUT_DIR/SixAxisSTM32H723.elf"
        echo "  ELF 复制完成"
    else
        echo "  ELF 文件不存在，跳过"
    fi

    if [ -f "$BIN_FILE" ]; then
        cp -f "$BIN_FILE" "$UNIX_KEIL_OUTPUT_DIR/SixAxisSTM32H723.bin"
        echo "  BIN 复制完成"
    else
        echo "  BIN 文件不存在，跳过"
    fi

    if [ -f "$HEX_FILE" ]; then
        cp -f "$HEX_FILE" "$UNIX_KEIL_OUTPUT_DIR/SixAxisSTM32H723.hex"
        echo "  HEX 复制完成"
    else
        echo "  HEX 文件不存在，跳过"
    fi

    # 如果有 .axf 文件也复制（Keil 使用 .axf 作为调试文件）
    AXF_FILE="$BUILD_DIR/${PROJECT_NAME}.axf"
    if [ -f "$AXF_FILE" ]; then
        cp -f "$AXF_FILE" "$UNIX_KEIL_OUTPUT_DIR/SixAxisSTM32H723.axf"
        echo "  AXF 复制完成"
    fi

    echo "文件复制完毕。"

    # 转换为 Windows 路径供 UV4.exe 使用
    WIN_KEIL_PROJECT_FILE=$(to_win_path "$UNIX_KEIL_PROJECT_FILE")

    echo "执行 Keil 命令行烧录..."
    echo "（请确保 Keil 工程的 Flash Download 选项中已勾选 'Reset and Run'）"

    # 使用绝对路径调用 UV4.exe（需转换为 Windows 格式）
    "$UNIX_UV4_CMD" -f "$WIN_KEIL_PROJECT_FILE" -j0

    if [ $? -eq 0 ]; then
        echo "Keil 烧录成功！"
    else
        echo "Keil 烧录失败，请检查："
        echo "  1. 调试器连接是否正常"
        echo "  2. Keil 工程配置的芯片型号、调试器类型是否正确"
        echo "  3. 目标板是否已上电"
        exit 1
    fi

else
    echo "错误：未知的烧录器参数 '$FLASHER'"
    echo "用法: ./run.sh [jlink|stlink|openocd|keil]"
    exit 1
fi