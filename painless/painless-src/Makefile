SRCS = $(shell find . -name "*.cpp")

OBJS = $(addsuffix .o, $(basename $(SRCS)))

EXEC = painless

CXX = mpic++

LIBS = -Wl,-Bstatic -lmapleCOMSPS -L../mapleCOMSPS/build/release/lib/ \
       -lmapleChronoBT -L../mapleChronoBT/build/release/lib \
       -lm4ri -L../mapleCOMSPS/m4ri-20140914/.libs \
       -Wl,-Bdynamic -lpthread -lz -lm -lmpi

CXXFLAGS = -I../mapleCOMSPS -I../mapleCOMSPS/m4ri-20140914 -I../mapleChronoBT\
           -D __STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS \
           -std=c++11 -O3 -D NDEBUG

$(EXEC): $(OBJS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIBS)

%.o: %.cpp
	$(CXX) -c $< -o $@ $(CXXFLAGS) $(LIBS)

clean:
	rm -f $(OBJS) $(EXEC)
