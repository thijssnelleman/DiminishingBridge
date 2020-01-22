CC = g++ -std=c++11
CFLAGS = -g -Wall

TARGETS = \
	 main.cc

all:	$(TARGETS)

%:	%.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)