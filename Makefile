TARGET      := ttl_udf_hash          # 出力バイナリ名
SRCS        := ttl_udf_hash.c        # 追加ソースはここに列挙

SAI_INC_DIR ?= /home/inc
SAI_LIB_DIR ?= /lib/x86_64-linux-gnu

# リンクするライブラリ名
#   * vs イメージなら通常 libsaivs.so
#   * ASIC 実機 SDK なら libsai.so など
SAI_LIB     ?= saivs

CC      ?= gcc
CFLAGS  += -Wall -Werror -O2 -g -I$(SAI_INC_DIR)
LDFLAGS += -L$(SAI_LIB_DIR) -l$(SAI_LIB) -lpthread -lrt

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	sudo ./$(TARGET)

