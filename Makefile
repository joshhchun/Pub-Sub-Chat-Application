# Configuration

CC		= gcc
LD		= gcc
AR		= ar
CFLAGS		= -g -std=gnu99 -Wall -Iinclude -fPIC
LDFLAGS		= -Llib -pthread
ARFLAGS		= rcs

# Variables

CLIENT_HEADERS  = $(wildcard include/mq/*.h include/chat/*.h)
CLIENT_SOURCES  = $(wildcard src/*.c)
CLIENT_OBJECTS  = $(CLIENT_SOURCES:.c=.o)
CLIENT_LIBRARY  = lib/libmq_client.a

# Rules

all:	$(CLIENT_LIBRARY)

chat: src/chat_app.o lib/libmq_client.a
	$(CC) $(LDFLAGS) -o bin/chat_app src/chat_app.o lib/libmq_client.a -lncurses

%.o:			%.c $(CLIENT_HEADERS)
	@echo "Compiling $@"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(CLIENT_LIBRARY):	$(CLIENT_OBJECTS)
	@echo "Linking   $@"
	@$(AR) $(ARFLAGS) $@ $^

bin/%:  		$(CLIENT_LIBRARY)
	@echo "Linking   $@"
	@$(LD) $(LDFLAGS) -o $@ $^

clean:
	@echo "Removing  objects"

	@echo "Removing  libraries"
	@rm -f $(CLIENT_LIBRARY)
	
.PRECIOUS: %.o
