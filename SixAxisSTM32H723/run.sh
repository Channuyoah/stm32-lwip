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

# # 运行 CMake 配置和编译项目
# cmake ..
# make -j 12
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

# # 1
# # # 执行 OpenOCD 进行烧录
# # openocd -f "$PROJECT_DIR/flash.cfg"
# # # uncomment this could support downloading code to flash

# 2
# 执行 OpenOCD 进行烧录 (使用 CubeIDE 自带的 OpenOCD)
# "D:/STM32CubeIDE_2.1.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.openocd.win32_2.4.400.202601091506/tools/bin/openocd.exe" \
#     -s "D:/STM32CubeIDE_2.1.1/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.debug.openocd_2.3.300.202602021527/resources/openocd/st_scripts" \
#     -f "$PROJECT_DIR/flash.cfg"

# # 3
# # 自动查找 CubeIDE 中的 OpenOCD
# OPENOCD_BIN=$(find "D:/STM32CubeIDE"* -type f -path "*/tools/bin/openocd.exe" 2>/dev/null | head -1)
# OPENOCD_SCRIPTS=$(find "D:/STM32CubeIDE"* -type d -path "*/resources/openocd/scripts" 2>/dev/null | head -1)

# if [ -n "$OPENOCD_BIN" ] && [ -n "$OPENOCD_SCRIPTS" ]; then
#     "$OPENOCD_BIN" -s "$OPENOCD_SCRIPTS" -f "$PROJECT_DIR/flash.cfg"
# else
#     echo "Error: OpenOCD not found in default CubeIDE paths."
#     exit 1
# fi

# ==============================================
# 通用烧录部分：自动查找工具 + 路径转换
# ==============================================

FLASHER=${1:-"jlink"}
DEVICE="STM32H723VG"
BIN_FILE="$BUILD_DIR/${PROJECT_NAME}.bin"
HEX_FILE="$BUILD_DIR/${PROJECT_NAME}.hex"

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

elif [ "$FLASHER" = "stlink" ]; then
    # 查找 ST-LINK_CLI.exe
    STLINK_CMD=$(find_tool "ST-LINK_CLI.exe" \
        "/c/Program Files (x86)/STMicroelectronics/STM32 ST-LINK Utility/ST-LINK Utility/ST-LINK_CLI.exe" \
        "/c/Program Files/STMicroelectronics/STM32 ST-LINK Utility/ST-LINK Utility/ST-LINK_CLI.exe"
    )

    if [ -z "$STLINK_CMD" ]; then
        echo "错误：未找到 ST-LINK_CLI.exe，请安装 STM32 ST-LINK Utility。"
        exit 1
    fi

    echo "找到 ST-LINK_CLI: $STLINK_CMD"
    echo "执行 ST-Link 烧录..."

    "$STLINK_CMD" -c SWD UR -P "$HEX_FILE" -Rst -Run

else
    echo "错误：未知的烧录器参数 '$FLASHER'"
    echo "用法: ./run.sh [jlink|stlink]"
    exit 1
fi

echo "----------------------------------------"
echo "烧录完成！"