# Makefile — 源程序格式处理工具构建脚本
# 用法：
#   make        编译生成 formatter.exe
#   make run    编译并运行测试
#   make clean  清理中间文件
# 注：如在 Visual Studio 中使用，可忽略本文件，按目录结构建工程即可。

CC      = gcc
CFLAGS  = -Wall -g -std=c11
TARGET  = formatter
SRCS    = main.c lexer.c ast.c parser.c printer.c error.c
OBJS    = $(SRCS:.c=.o)
TEST    = test/01_variables.c

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

run: $(TARGET)
	./$(TARGET) $(TEST)

clean:
	rm -f $(OBJS) $(TARGET)
