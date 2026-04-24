FOLDER      := $(notdir $(CURDIR))
PARENT      ?= ..
PLUGINS_DIR := plugins

ULIB    := $(PARENT)/ulib.o $(PARENT)/usys.o $(PARENT)/printf.o $(PARENT)/umalloc.o
USER_LD := $(PARENT)/user.ld

PLUGIN_SRCS := $(wildcard $(PLUGINS_DIR)/*.c)
PLUGIN_BINS := $(patsubst $(PLUGINS_DIR)/%.c,$(PARENT)/_$(FOLDER)-%,$(PLUGIN_SRCS))
MAIN_BIN    := $(PARENT)/_$(FOLDER)

.PRECIOUS: %.o $(PLUGINS_DIR)/%.o

.PHONY: all clean

all: $(MAIN_BIN) $(PLUGIN_BINS)

main.o: main.c
	$(CC) $(CFLAGS) -I$(PARENT) -c -o $@ $<

$(PLUGINS_DIR)/%.o: $(PLUGINS_DIR)/%.c
	$(CC) $(CFLAGS) -I$(PARENT) -c -o $@ $<

$(MAIN_BIN): main.o $(ULIB) $(USER_LD)
	$(LD) $(LDFLAGS) -n -N -T $(USER_LD) -e main -Ttext 0x1000 -o $@ main.o $(ULIB)

$(PARENT)/_$(FOLDER)-%: $(PLUGINS_DIR)/%.o $(ULIB) $(USER_LD)
	$(LD) $(LDFLAGS) -n -N -T $(USER_LD) -e main -Ttext 0x1000 -o $@ $< $(ULIB)

clean:
	rm -f main.o *.d $(PLUGINS_DIR)/*.o $(PLUGINS_DIR)/*.d \
	$(MAIN_BIN) $(PARENT)/_$(FOLDER)-*
