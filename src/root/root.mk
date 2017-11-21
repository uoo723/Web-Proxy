TARGET_DIR = $(MAKE_DIR)/target
TARGET = $(TARGET_DIR)/server

SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

$(TARGET): $(OBJS) $(LIB_DIR)/*.a
	@mkdir -p $(TARGET_DIR)
	@$(CC) $(CFLAGS) -o $@ $< $(LIBS)
	@echo "Generate target $(notdir $@)"

$(OBJS): $(SRCS)
	@$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean

clean:
	@$(RM) -f $(OBJS) $(TARGET)
	@echo "Remove Objects: $(notdir $(OBJS))"
	@echo "Remove Target: $(notdir $(TARGET))"
