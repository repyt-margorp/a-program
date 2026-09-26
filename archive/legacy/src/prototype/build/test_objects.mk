include src/prototype/build/sources.mk

CC ?= cc
TEST_STANDARD ?= c11
TEST_WARNING_FLAGS ?= -Wall -Wextra
TEST_CPPFLAGS ?=
TEST_OBJECT_ROOT ?= /tmp/a-program-prototype-test-objects
TEST_SOURCES ?=

TEST_OBJECTS := $(addprefix $(TEST_OBJECT_ROOT)/,$(TEST_SOURCES:.c=.o))
TEST_DEPENDENCIES := $(TEST_OBJECTS:.o=.d)

.PHONY: prototype-test-objects print-prototype-test-objects

prototype-test-objects: $(TEST_OBJECTS)

print-prototype-test-objects:
	@printf '%s\n' $(TEST_OBJECTS)

$(TEST_OBJECT_ROOT)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -std=$(TEST_STANDARD) $(TEST_WARNING_FLAGS) $(TEST_CPPFLAGS) \
		-I src/prototype/include -I src/prototype -MMD -MP -c $< -o $@

-include $(TEST_DEPENDENCIES)
