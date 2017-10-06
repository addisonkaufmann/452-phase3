PREFIX = ${HOME}

TARGET = libphase3.a
ASSIGNMENT = 452phase3
CC = gcc
AR = ar
COBJS = phase3.o libuser.o
CSRCS = ${COBJS:.o=.c}

PHASE1LIB = patrickphase1
PHASE2LIB = patrickphase2
#PHASE1LIB = patrickphase1debug
#PHASE2LIB = patrickphase2debug

HDRS = sems.h

INCLUDE = ${PREFIX}/include

#CFLAGS = -Wall -g -I. -I/home/cs452/fall15/include 
#LDFLAGS += -L. -L/home/cs452/fall15/lib
CFLAGS = -Wall -g -std=gnu99 -I${INCLUDE} -I.  

UNAME := $(shell uname -s)

ifeq ($(UNAME), Darwin)
        CFLAGS += -D_XOPEN_SOURCE
endif

LDFLAGS += -L. -L${PREFIX}/lib

TESTDIR = testcases
TESTS = test00 test01 test02 test03 test04 test05 test06 test07 test08 \
        test09 test10 test11 test12 test13 test14 test15 test16 test17 \
        test18 test19 test20 

LIBS = -l$(PHASE2LIB) -l$(PHASE1LIB) -lusloss -l$(PHASE1LIB) -l$(PHASE2LIB) \
	-lphase3 

$(TARGET):	$(COBJS)
		$(AR) -r $@ $(COBJS) 

#$(TESTS):	$(TARGET) $(TESTDIR)/$$@.c p1.o
$(TESTS):	$(TARGET) p1.o
	$(CC) $(CFLAGS) -c $(TESTDIR)/$@.c
	$(CC) $(LDFLAGS) -o $@ $@.o $(LIBS) p1.o

clean:
	rm -f $(COBJS) $(TARGET) test*.o test*.txt term* $(TESTS) \
		libuser.o p1.o core

phase3.o:	sems.h

libuser.a:	libuser.c
	$(CC) $(CFLAGS) -c libuser.c
	ar -r libuser.a libuser.o

submit: $(CSRCS) $(HDRS) Makefile
	tar cvzf phase3.tgz $(CSRCS) $(HDRS) Makefile

