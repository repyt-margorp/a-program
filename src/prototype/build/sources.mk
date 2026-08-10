PROTOTYPE_ROOT := src/prototype

PROTOTYPE_KERNEL_SOURCES := \
	$(PROTOTYPE_ROOT)/ast.c \
	$(PROTOTYPE_ROOT)/src/kernel/context.c \
	$(PROTOTYPE_ROOT)/src/core/term.c \
	$(PROTOTYPE_ROOT)/src/kernel/type_declaration.c \
	$(PROTOTYPE_ROOT)/typing.c \
	$(PROTOTYPE_ROOT)/src/kernel/universe.c \
	$(PROTOTYPE_ROOT)/src/support/symbol.c

PROTOTYPE_GRAPH_SOURCES := \
	$(PROTOTYPE_ROOT)/ast.c \
	$(PROTOTYPE_ROOT)/src/kernel/context.c \
	$(PROTOTYPE_ROOT)/src/frontend/reader.c \
	$(PROTOTYPE_ROOT)/src/core/term.c \
	$(PROTOTYPE_ROOT)/src/kernel/type_declaration.c \
	$(PROTOTYPE_ROOT)/typing.c \
	$(PROTOTYPE_ROOT)/src/kernel/universe.c \
	$(PROTOTYPE_ROOT)/src/support/symbol.c

PROTOTYPE_COMPILER_SOURCES := \
	$(PROTOTYPE_ROOT)/ast.c \
	$(PROTOTYPE_ROOT)/src/kernel/context.c \
	$(PROTOTYPE_ROOT)/src/frontend/ast_inspect.c \
	$(PROTOTYPE_ROOT)/src/frontend/reader.c \
	$(PROTOTYPE_ROOT)/src/core/term.c \
	$(PROTOTYPE_ROOT)/src/kernel/type_declaration.c \
	$(PROTOTYPE_ROOT)/typing.c \
	$(PROTOTYPE_ROOT)/src/kernel/universe.c \
	$(PROTOTYPE_ROOT)/src/support/symbol.c

PROTOTYPE_HOTT_SOURCES := \
	$(PROTOTYPE_ROOT)/hott.c \
	$(PROTOTYPE_ROOT)/ast.c \
	$(PROTOTYPE_ROOT)/src/kernel/cwf_certificate.c \
	$(PROTOTYPE_ROOT)/src/kernel/kernel_view.c \
	$(PROTOTYPE_ROOT)/src/kernel/context.c \
	$(PROTOTYPE_ROOT)/src/core/term.c \
	$(PROTOTYPE_ROOT)/src/kernel/type_declaration.c \
	$(PROTOTYPE_ROOT)/typing.c \
	$(PROTOTYPE_ROOT)/src/kernel/universe.c \
	$(PROTOTYPE_ROOT)/src/support/symbol.c

PROTOTYPE_REPL_SOURCES := \
	$(PROTOTYPE_ROOT)/src/driver/repl.c \
	$(PROTOTYPE_COMPILER_SOURCES)

PROTOTYPE_READER_SOURCES := \
	$(PROTOTYPE_ROOT)/src/driver/read_file.c \
	$(PROTOTYPE_COMPILER_SOURCES)
