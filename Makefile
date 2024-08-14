
# 编译选项
CFLAGS = -Wall -Wextra -lm -lavformat -lavcodec -lavutil -lswscale

# 源文件目录
SRCDIR = ./

# 头文件目录
INCDIR = include

# 生成目标文件的目录
OBJDIR = obj

# 最终可执行文件名
TARGET = app

# 源文件列表（自动查找SRCDIR目录下的.c文件）
SRCS = $(wildcard $(SRCDIR)/*.c)

# 目标文件列表
OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))

APP_PATH=/home/nfs/

# 默认目标
all: $(TARGET)
	cp -f $(TARGET) $(APP_PATH)

# 编译规则
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CXX) $(CFLAGS) -I$(INCDIR) -c $< -o $@

# 目标文件夹
$(OBJDIR):
	mkdir -p $(OBJDIR)

# 生成可执行文件
$(TARGET): $(OBJS)
	$(CXX) $(CFLAGS) -o $@ $^

# 清理
clean:
	rm -rf $(OBJDIR) $(TARGET)
down:
	cp $(TARGET) ~/nfs

# 伪目标，防止与同名文件冲突
.PHONY: all clean down
