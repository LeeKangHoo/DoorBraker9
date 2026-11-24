BUILD_DIR := build
TARGET := $(BUILD_DIR)/log_server
SOURCES := $(wildcard src/*.c)
OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

CFLAGS ?= -O2 -Wall -Wextra

.PHONY: all clean

ifneq ($(SOURCES),)
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -pthread -o $@ $^ $(LDLIBS)
else
all:
	@printf '%s\n' 'No C source files in src/.'
endif

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -Iinclude $(CFLAGS) -pthread -MMD -MP -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	$(RM) $(TARGET) $(BUILD_DIR)/*.o $(BUILD_DIR)/*.d

-include $(OBJECTS:.o=.d)
