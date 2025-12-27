#!/bin/bash
# 代码格式化脚本 / Code Formatting Script

set -e

# 颜色定义
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}🎨 Formatting C++ code...${NC}"

# 查找所有 C++ 文件并格式化
find src apps include tests \
    \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.cc' \) \
    -exec clang-format -i {} \;

echo -e "${GREEN}✅ Code formatting complete!${NC}"
