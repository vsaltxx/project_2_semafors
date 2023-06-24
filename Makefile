TARGET=proj2
CFLAGS=-std=gnu99 -Wall -Wextra -Werror -pedantic -pthread
COMPILER=gcc
RM=rm -f

$(TARGET) : $(TARGET).c
		$(COMPILER) $(CFLAGS) $(TARGET).c -o $(TARGET)

clean :
	$(RM) *.o $(PROJ)
