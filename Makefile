CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

KIT = csce2303-m0-iss-student/csce2303-m0-iss-student-v1.1

iss: iss.cpp decoder.cpp decoder.hpp
	$(CXX) $(CXXFLAGS) -o iss iss.cpp decoder.cpp

test-cp1: iss
	python3 $(KIT)/public-tests/run_tests.py --iss "./iss" cp1

clean:
	rm -f iss iss.exe

.PHONY: test-cp1 clean
