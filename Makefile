CC      = cc
CFLAGS  = -std=c11 -Wall -Wextra -O2
SRCS    = main.c git.c display.c worktree.c init.c
TARGET  = gw

PREFIX  ?= /usr/local

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	@echo ""
	@echo "Add to your shell config:"
	@echo "  fish:  echo 'gw init fish | source' >> ~/.config/fish/config.fish"
	@echo "  bash:  echo 'eval \"\$$(gw init bash)\"' >> ~/.bashrc"
	@echo "  zsh:   echo 'eval \"\$$(gw init zsh)\"'  >> ~/.zshrc"

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)

clean:
	rm -f $(TARGET)
