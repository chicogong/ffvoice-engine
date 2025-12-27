#!/bin/bash
# 代码静态分析脚本 / Code Static Analysis Script

set -e

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}🔍 Running static analysis with clang-tidy...${NC}"

# 检查 build 目录是否存在
if [ ! -d "build" ]; then
    echo -e "${RED}❌ Build directory not found!${NC}"
    echo -e "${YELLOW}💡 Run 'cmake -B build' first${NC}"
    exit 1
fi

# 检查 compile_commands.json 是否存在
if [ ! -f "build/compile_commands.json" ]; then
    echo -e "${RED}❌ compile_commands.json not found!${NC}"
    echo -e "${YELLOW}💡 Run 'cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON'${NC}"
    exit 1
fi

# 运行 clang-tidy
find src apps \( -name '*.cpp' \) | \
    xargs clang-tidy -p build --warnings-as-errors='*'

echo -e "${GREEN}✅ Static analysis complete!${NC}"
