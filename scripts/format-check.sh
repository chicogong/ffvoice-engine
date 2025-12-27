#!/bin/bash
# 代码格式检查脚本（不修改文件）/ Code Format Check Script (No Modifications)

set -e

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}🔍 Checking C++ code formatting...${NC}"

# 临时文件用于存储未格式化的文件列表
UNFORMATTED_FILES=$(mktemp)

# 查找所有 C++ 文件并检查格式
find src apps include tests \
    \( -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.cc' \) \
    | while read file; do
        if ! clang-format --dry-run --Werror "$file" 2>/dev/null; then
            echo "$file" >> "$UNFORMATTED_FILES"
        fi
    done

# 检查是否有未格式化的文件
if [ -s "$UNFORMATTED_FILES" ]; then
    echo -e "${RED}❌ The following files are not properly formatted:${NC}"
    cat "$UNFORMATTED_FILES"
    echo ""
    echo -e "${YELLOW}💡 Run './scripts/format.sh' to fix formatting${NC}"
    rm "$UNFORMATTED_FILES"
    exit 1
else
    echo -e "${GREEN}✅ All files are properly formatted!${NC}"
    rm "$UNFORMATTED_FILES"
    exit 0
fi
