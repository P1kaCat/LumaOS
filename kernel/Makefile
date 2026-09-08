	$(CC) $(KERNEL_CFLAGS) -c audio.c -o $@

$(BUILD_DIR)/console.o: console.c console.h framebuffer.h pointer.h ../include/handoff.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(KERNEL_CFLAGS) -c console.c -o $@

$(BUILD_DIR)/framebuffer.o: framebuffer.c framebuffer.h ../include/handoff.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(KERNEL_CFLAGS) -c framebuffer.c -o $@

$(BUILD_DIR)/mouse.o: mouse.c mouse.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(KERNEL_CFLAGS) -c mouse.c -o $@

$(BUILD_DIR)/ps2_mouse.o: ps2_mouse.c mouse.h pointer.h cpu.h apic.h ../include/handoff.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(KERNEL_CFLAGS) -c ps2_mouse.c -o $@

$(BUILD_DIR)/pointer.o: pointer.c pointer.h framebuffer.h ../include/handoff.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(KERNEL_CFLAGS) -c pointer.c -o $@

$(BUILD_DIR)/graphics.o: graphics.c graphics.h framebuffer.h console.h pointer.h sched.h ../include/graphics_abi.h ../include/handoff.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(KERNEL_CFLAGS) -c graphics.c -o $@