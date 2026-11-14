CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

KIT   = csce2303-m0-iss-student/csce2303-m0-iss-student-v1.1
TESTS = python3 $(KIT)/public-tests/run_tests.py --iss "./iss"

SRCS = $(wildcard *.cpp)
HDRS = $(wildcard *.hpp)

iss: $(SRCS) $(HDRS)
	$(CXX) $(CXXFLAGS) -o iss $(SRCS)

test-cp1: iss
	$(TESTS) cp1

test-cp2: iss
	$(TESTS) cp2

# our own test programs in tests/
test-ours: iss
	$(TESTS) $(abspath tests)

clean:
	rm -f iss iss.exe *.o

.PHONY: test-cp1 test-cp2 test-ours clean
