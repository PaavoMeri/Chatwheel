CC = gcc
CFLAGS = -Wall -Wextra -I src/ $(shell pkg-config --cflags libpulse)
LDFLAGS = $(shell pkg-config --libs libpulse) -lm
SRCS = src/main.c src/headset/headset.c src/mixer/mixer.c src/config.c \
	src/audio_stream_inventory.c src/application_identity.c \
	src/active_application_inventory.c \
	src/application_classifier.c \
	src/pattern_matcher.c \
	src/mixer/pulse_stream_lifecycle.c
OBJS = $(SRCS:.c=.o)
TARGET = chatwheel
TEST_TARGET = build/test_audio_stream_inventory
PULSE_LIFECYCLE_TEST_TARGET = build/test_pulse_stream_lifecycle
APPLICATION_IDENTITY_TEST_TARGET = build/test_application_identity
ACTIVE_APPLICATION_TEST_TARGET = build/test_active_application_inventory
PATTERN_MATCHER_TEST_TARGET = build/test_pattern_matcher
APPLICATION_CLASSIFIER_TEST_TARGET = build/test_application_classifier

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: test
test: $(TEST_TARGET) $(PULSE_LIFECYCLE_TEST_TARGET) \
		$(APPLICATION_IDENTITY_TEST_TARGET) $(ACTIVE_APPLICATION_TEST_TARGET) \
		$(PATTERN_MATCHER_TEST_TARGET) $(APPLICATION_CLASSIFIER_TEST_TARGET)
	./$(TEST_TARGET)
	./$(PULSE_LIFECYCLE_TEST_TARGET)
	./$(APPLICATION_IDENTITY_TEST_TARGET)
	./$(ACTIVE_APPLICATION_TEST_TARGET)
	./$(PATTERN_MATCHER_TEST_TARGET)
	./$(APPLICATION_CLASSIFIER_TEST_TARGET)

$(TEST_TARGET): tests/test_audio_stream_inventory.c src/audio_stream_inventory.c \
		src/audio_stream_inventory.h
	mkdir -p build
	$(CC) -Wall -Wextra -Werror -I src/ \
		tests/test_audio_stream_inventory.c src/audio_stream_inventory.c \
		-o $(TEST_TARGET)

$(PULSE_LIFECYCLE_TEST_TARGET): tests/test_pulse_stream_lifecycle.c \
		src/mixer/pulse_stream_lifecycle.c src/mixer/pulse_stream_lifecycle.h \
		src/audio_stream_inventory.c src/audio_stream_inventory.h
	mkdir -p build
	$(CC) $(CFLAGS) -Werror \
		tests/test_pulse_stream_lifecycle.c \
		src/mixer/pulse_stream_lifecycle.c src/audio_stream_inventory.c \
		-o $(PULSE_LIFECYCLE_TEST_TARGET) $(LDFLAGS)

$(APPLICATION_IDENTITY_TEST_TARGET): tests/test_application_identity.c \
		src/application_identity.c src/application_identity.h \
		src/audio_stream_inventory.h
	mkdir -p build
	$(CC) -Wall -Wextra -Werror -I src/ \
		tests/test_application_identity.c src/application_identity.c \
		-o $(APPLICATION_IDENTITY_TEST_TARGET)

$(ACTIVE_APPLICATION_TEST_TARGET): tests/test_active_application_inventory.c \
		src/active_application_inventory.c src/active_application_inventory.h \
		src/application_identity.c src/application_identity.h \
		src/audio_stream_inventory.c src/audio_stream_inventory.h
	mkdir -p build
	$(CC) -Wall -Wextra -Werror -I src/ \
		tests/test_active_application_inventory.c \
		src/active_application_inventory.c src/application_identity.c \
		src/audio_stream_inventory.c -o $(ACTIVE_APPLICATION_TEST_TARGET)

$(PATTERN_MATCHER_TEST_TARGET): tests/test_pattern_matcher.c \
		src/pattern_matcher.c src/pattern_matcher.h
	mkdir -p build
	$(CC) -Wall -Wextra -Werror -I src/ \
		tests/test_pattern_matcher.c src/pattern_matcher.c \
		-o $(PATTERN_MATCHER_TEST_TARGET)

$(APPLICATION_CLASSIFIER_TEST_TARGET): tests/test_application_classifier.c \
		src/application_classifier.c src/application_classifier.h \
		src/active_application_inventory.c src/active_application_inventory.h \
		src/application_identity.c src/application_identity.h \
		src/audio_stream_inventory.c src/audio_stream_inventory.h \
		src/pattern_matcher.c src/pattern_matcher.h src/config.h
	mkdir -p build
	$(CC) -Wall -Wextra -Werror -I src/ \
		tests/test_application_classifier.c src/application_classifier.c \
		src/active_application_inventory.c src/application_identity.c \
		src/audio_stream_inventory.c src/pattern_matcher.c \
		-o $(APPLICATION_CLASSIFIER_TEST_TARGET)

.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET) $(TEST_TARGET) $(PULSE_LIFECYCLE_TEST_TARGET) \
		$(APPLICATION_IDENTITY_TEST_TARGET) $(ACTIVE_APPLICATION_TEST_TARGET) \
		$(PATTERN_MATCHER_TEST_TARGET) $(APPLICATION_CLASSIFIER_TEST_TARGET)

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
