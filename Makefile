# GPL2 Kai 2006@


CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -Iinclude
LIBS = `pkg-config --cflags --libs gtk+-3.0 libsoup-2.4 json-c` -lm
LDFLAGS = -Wl,--disable-new-dtags -Wl,-rpath,/usr/lib/x86_64-linux-gnu

# Directories
SRC_DIR = src
CORE_DIR = $(SRC_DIR)/core
UI_DIR = $(SRC_DIR)/ui
UI_GTK_DIR = $(UI_DIR)/gtk
BUILD_DIR = build
BIN_DIR = bin

# Target executable
TARGET = $(BIN_DIR)/gticker_portfolio

# Core library
CORE_LIB = $(BUILD_DIR)/libportfolio.a

# Source files
CORE_SOURCES = $(CORE_DIR)/portfolio_core.c \
               $(CORE_DIR)/analytics.c \
               $(CORE_DIR)/network.c \
               $(CORE_DIR)/enhanced_ta.c \
               $(CORE_DIR)/scalping_bot.c

UI_SOURCES = $(UI_DIR)/ui_factory.c \
             $(UI_GTK_DIR)/gtk_ui_main.c

MAIN_SOURCE = $(SRC_DIR)/main.c

# Object files
CORE_OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(CORE_SOURCES))
UI_OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(UI_SOURCES))
MAIN_OBJECT = $(BUILD_DIR)/main.o

# Default target
all: directories $(TARGET)

# Create directories
directories:
	@mkdir -p $(BUILD_DIR)/core
	@mkdir -p $(BUILD_DIR)/ui/gtk
	@mkdir -p $(BIN_DIR)

# Build core library
$(CORE_LIB): $(CORE_OBJECTS)
	ar rcs $@ $^
	@echo "Built core library: $@"

# Build executable
$(TARGET): $(MAIN_OBJECT) $(UI_OBJECTS) $(CORE_LIB)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS) $(LDFLAGS)
	@echo "Built executable: $@"

# Compile core source files
$(BUILD_DIR)/core/%.o: $(CORE_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@ $(LIBS)

# Compile UI source files
$(BUILD_DIR)/ui/%.o: $(UI_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@ $(LIBS)

$(BUILD_DIR)/ui/gtk/%.o: $(UI_GTK_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@ $(LIBS)

# Compile main
$(BUILD_DIR)/main.o: $(MAIN_SOURCE)
	$(CC) $(CFLAGS) -c $< -o $@ $(LIBS)

# Clean build files
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# Run the application
run: $(TARGET)
	./$(TARGET)

# Install dependencies (Ubuntu/Debian)
install-deps:
	sudo apt-get update
	sudo apt-get install -y \
		build-essential \
		pkg-config \
		libgtk-3-dev \
		libsoup2.4-dev \
		libjson-c-dev

# Install dependencies (Fedora)
install-deps-fedora:
	sudo dnf install -y \
		gcc \
		pkg-config \
		gtk3-devel \
		libsoup-devel \
		json-c-devel

# Install dependencies (Arch)
install-deps-arch:
	sudo pacman -S --noconfirm \
		gcc \
		pkg-config \
		gtk3 \
		libsoup \
		json-c

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: clean all

# Help
help:
	@echo "Available targets:"
	@echo "  all               - Build the application (default)"
	@echo "  clean             - Clean build files"
	@echo "  run               - Build and run the application"
	@echo "  debug             - Build with debug symbols"
	@echo "  install-deps      - Install dependencies (Ubuntu/Debian)"
	@echo "  install-deps-fedora - Install dependencies (Fedora)"
	@echo "  install-deps-arch   - Install dependencies (Arch)"
	@echo "  deb               - Build .deb package for Ubuntu 22.04 (requires Docker)"
	@echo "  deb-all           - Build .deb packages for all Ubuntu versions (requires Docker)"
	@echo "  help              - Show this help"

# Build deb package for Ubuntu 22.04
deb:
	@echo "Building Debian package for Ubuntu 22.04..."
	@chmod +x build-deb.sh
	./build-deb.sh 22.04

# Build deb packages for all Ubuntu versions
deb-all:
	@echo "Building Debian packages for all Ubuntu versions..."
	@chmod +x build-all-versions.sh
	./build-all-versions.sh --all

.PHONY: all clean run install-deps install-deps-fedora install-deps-arch debug help directories deb deb-all
