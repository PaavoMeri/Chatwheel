CC = gcc
CFLAGS = -Wall -Wextra -I src/ $(shell pkg-config --cflags libpulse)
LDFLAGS = $(shell pkg-config --libs libpulse)
SRCS = src/main.c src/headset/headset.c src/mixer/mixer.c src/config.c
OBJS = $(SRCS:.c=.o)
TARGET = chatwheel
TEST = test

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(TEST): src/test.c $(filter-out src/main.o,$(OBJS))
	$(CC) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET) $(TEST)

.PHONY: test
test: $(TEST)

.PHONY: dirs
dirs:
	mkdir -p config systemd scripts

.PHONY: install
install: $(TARGET) dirs
	chmod +x scripts/install.sh
	./scripts/install.sh

.PHONY: uninstall
uninstall:
	systemctl --user stop chatwheel
	systemctl --user disable chatwheel
	sudo rm -f /usr/local/bin/chatwheel
	sudo rm -f /etc/systemd/user/chatwheel.service
	sudo rm -rf /etc/chatwheel
	rm -rf ~/.config/chatwheel