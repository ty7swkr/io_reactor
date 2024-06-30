SYS	:=	$(shell gcc -dumpmachine)
CC	=	g++
#CC	=	clang++-6.0

TARGET	=	libhttps1_reactor.a

SOURCES	= \
	Https1Handler.cpp \
	Https1Reactor.cpp

INSTALL_DIR	=	./

# 소스에서 참조되는 헤더파일의 위치
# 절대경로는 사용 X, 상대경로로 사용토록 한다. 

# 디버깅정보 & thread 재진입을 포함시킨다.
CPPFLAGS	+=	-g
CPPFLAGS	+=	-D_REENTRANT

#	-O2							최적화
#	-std=c++03			c++03 문법 기준으로 검사 및 컴파일
#	-std=c++11			c++11 문법 기준으로 검사 및 컴파일
#	-Wall -Wextra		모든 경고 킴
#	-Wfloat-equal		C,C++ 소수점 비교연산시 경고 출력(C,C++ 소수점 연산은 정확하지 않음)
#	-m64						64비트로 컴파일
#	-m32						32비트로 컴파일,
#									32비트 컴파일시에는 32비트용 라이브러리가 시스템 내에 설치되어 있어야 한다.
#CPPFLAGS	+=	-O2 -std=c++11 -Wall -Wextra -Winline -Wfloat-equal -m64
CPPFLAGS	+=	-O2 -std=c++17 -Wall -Wextra -Wfloat-equal -m64

# 용량을 줄여주는 옵션(사용하지 않는 오브젝트 제거...)
#CPPFLAGS	+=	-ffunction-sections -fdata-sections -s

# 용량을 줄이기위한 옵션
ifneq (, $(findstring linux, $(SYS)))
	ifneq (, $(findstring g++, $(CC)))
#		CPPFLAGS	+= -Wl,--gc-sections -s
	endif
endif

# 참조 라이브러리 추가
# -L디렉토리위치 -l라이브러리명
######################################## default
INCLUDE 	+= -I../

######################################## default lib
LDFLAGS += -lpthread -lssl -lcrypto

#LDFLAGS += -ljemalloc
#LDFLAGS += -l:libjemalloc.so.2

OBJECTS	:=	$(SOURCES:.cpp=.o)
DEPENDS	:=	$(SOURCES:.cpp=.d)

#all: $(OBJECTS)
#	rm -rf core.*
#	@if ! test -f $(BUILD_NUMBER_FILE); then echo 0 > $(BUILD_NUMBER_FILE); fi
#	@echo $$(($$(cat $(BUILD_NUMBER_FILE)) + 1)) > $(BUILD_NUMBER_FILE)
#	@touch $(BUILD_NUMBER_SOURCE)
#	$(CC) -o $(TARGET) $(OBJECTS) $(CPPFLAGS) $(LDFLAGS)
#	@echo $$(($$(cat $(BUILD_NUMBER_FILE)) - 1)).`date +'%Y%m%d'`
all: $(OBJECTS)
	rm -rf core.*
	ar rcv $(TARGET) $(OBJECTS)
	cp $(TARGET) ../libs
#	$(CC) -o $(TARGET) $(OBJECTS) $(CPPFLAGS) $(LDFLAGS)

clean:
	rm -rf $(TARGET) $(OBJECTS)

install: all
#	cp $(TARGET) $(INSTALL_DIR)

.c.o: $(.cpp.o)
#.cpp.o:
#	$(CC) $(INCLUDE) $(BUILD_NUMBER_FLAGS) $(CPPFLAGS) -c $< -o $@
.cpp.o:
	$(CC) $(INCLUDE) $(CPPFLAGS) -c $< -o $@

%d:%cpp
	$(CC) $(INCLUDE) $(CPPFLAGS) -MM -MP -MT "$(@:.d=.o) $@" -MF $@ $<

.PHONY: all

-include $(DEPENDS)





