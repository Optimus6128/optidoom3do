3DODEV	= C:/3DODev/
ARMDEV	= C:/ARM251/
BURGERLIB = ../lib/

# Project specific settings
NAME	= LaunchMe
STACKSIZE = 10000

CC		= $(ARMDEV)bin/armcc
AS 		= $(ARMDEV)bin/armasm
LD		= $(ARMDEV)bin/armlink
RM		= $(3DODEV)bin/rm
MODBIN	= $(3DODEV)bin/modbin
CCFLAGS = -O2 -bi -zps1 -za1 -wn -ff -fa -d DEBUG=0 -cpu ARM6 
ASFLAGS = -BI -i $(3DODEV)includes/
INCPATH	= -I$(3DODEV)includes -I$(BURGERLIB)burger -I$(BURGERLIB)intmath
LDFLAGS = -aif -r -b 0x00 -sym $(NAME).sym 
LIBPATH	= $(3DODEV)libs/
STARTUP = $(LIBPATH)cstartup.o
LIBS 	=	$(LIBPATH)Burger.lib \
			$(LIBPATH)Lib3DO.lib \
			$(LIBPATH)operamath.lib \
			$(LIBPATH)graphics.lib \
			$(LIBPATH)audio.lib \
			$(LIBPATH)music.lib \
			$(LIBPATH)filesystem.lib \
			$(LIBPATH)input.lib \
			$(LIBPATH)swi.lib \
			$(LIBPATH)clib.lib \
			$(LIBPATH)String.lib \
			$(LIBPATH)IntMath.lib

ARMLIB	= $(ARMDEV)lib
ARMINC	= $(ARMDEV)inc

SRC_S		= $(wildcard *.s)
SRC_C		= $(wildcard *.c)

OBJ	+= $(SRC_S:.s=.o)
OBJ	+= $(SRC_C:.c=.o)

all: $(NAME)
	
$(NAME): $(OBJ)
	$(LD) -dupok -o $(NAME). $(LDFLAGS) $(STARTUP) $(LIBS) $(OBJ)
	$(MODBIN) $(STACKSIZE) $(NAME)

%.o: %.c
	$(CC) $(INCPATH) $(CCFLAGS) -c $< -o $@

%.o: %.s
	$(AS) $(INCPATH) $(ASFLAGS) $< -o $@
	
clean:
	RM -f $(OBJ)
	RM -f $(NAME)
