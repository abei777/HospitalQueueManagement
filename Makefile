# Makefile - Hospital Queue Management System
#
#   make          build the application      -> build/hospital_queue
#   make run      build and start it
#   make test     build and run the whole automated test suite
#   make clean    delete the build/ folder
#
# Run every command from the project root (the folder containing this file),
# because the program's default data file is the relative path data/patients.txt.

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -Werror -O2
INCLUDES := -I. -Itests

BUILD    := build
APP      := $(BUILD)/hospital_queue

LIB_SRCS := Patient.cpp HospitalQueue.cpp PatientStorage.cpp HospitalManagementSystem.cpp
LIB_OBJS := $(LIB_SRCS:%.cpp=$(BUILD)/%.o)

TEST_NAMES := test_patient test_priority_queue test_registration \
              test_serving_search_cancel test_statistics test_persistence \
              test_application
TEST_BINS  := $(TEST_NAMES:%=$(BUILD)/tests/%)

.PHONY: all run test clean
all: $(APP)

$(BUILD)/%.o: %.cpp $(wildcard *.h)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(APP): $(LIB_OBJS) $(BUILD)/main.o
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD)/tests/%: tests/%.cpp $(LIB_OBJS) $(wildcard tests/*.h)
	@mkdir -p $(BUILD)/tests
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(LIB_OBJS) -o $@

run: $(APP)
	$(abspath $(APP))

# Runs every suite, prints each suite's result line and a grand total.
# The exit status is non-zero if any suite fails to build or has a failing check.
test: $(TEST_BINS)
	@total_pass=0; total_fail=0; status=0; \
	for t in $(TEST_NAMES); do \
	  out=$$($(abspath $(BUILD))/tests/$$t); rc=$$?; \
	  line=$$(printf '%s\n' "$$out" | grep '^Result:'); \
	  printf '%-28s %s\n' "$$t" "$$line"; \
	  printf '%s\n' "$$out" | grep '^\[FAIL\]'; \
	  p=$$(printf '%s' "$$line" | sed -n 's/Result: \([0-9]*\) passed.*/\1/p'); \
	  f=$$(printf '%s' "$$line" | sed -n 's/.* \([0-9]*\) failed/\1/p'); \
	  total_pass=$$((total_pass + $${p:-0})); total_fail=$$((total_fail + $${f:-0})); \
	  if [ $$rc -ne 0 ]; then status=1; fi; \
	done; \
	echo "----------------------------------------------------------------"; \
	echo "TOTAL: $$total_pass passed, $$total_fail failed"; \
	exit $$status

clean:
	rm -rf $(BUILD)
