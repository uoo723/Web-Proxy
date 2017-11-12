LIB = $(LIB_DIR)/libthpool.a
SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

$(LIB): $(OBJS)
	@$(AR) cr $@ $^
	@echo "Archive $(notdir $@)"

$(OBJS): $(BUILD_DIR)/%.o: %.c %.h
	@$(CC) $(CFLAGS) -c -o $@ $<
	@echo "CC $(notdir $@)"

.PHONY: clean

clean:
	@$(RM) -f $(LIB) $(OBJS)
	@echo "Remove Objects: $(notdir $(OBJS))"
	@echo "Remove Libraries: $(notdir $(LIB))"
