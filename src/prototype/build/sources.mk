PROTOTYPE_ROOT := src/prototype

PROTOTYPE_KERNEL_SOURCES := \
	$(PROTOTYPE_ROOT)/ast.c \
	$(PROTOTYPE_ROOT)/context.c \
	$(PROTOTYPE_ROOT)/term.c \
	$(PROTOTYPE_ROOT)/type_declaration.c \
	$(PROTOTYPE_ROOT)/typing.c \
	$(PROTOTYPE_ROOT)/universe.c \
	$(PROTOTYPE_ROOT)/symbol.c

PROTOTYPE_GRAPH_SOURCES := \
	$(PROTOTYPE_ROOT)/ast.c \
	$(PROTOTYPE_ROOT)/context.c \
	$(PROTOTYPE_ROOT)/reader.c \
	$(PROTOTYPE_ROOT)/term.c \
	$(PROTOTYPE_ROOT)/type_declaration.c \
	$(PROTOTYPE_ROOT)/typing.c \
	$(PROTOTYPE_ROOT)/universe.c \
	$(PROTOTYPE_ROOT)/symbol.c

PROTOTYPE_COMPILER_SOURCES := \
	$(PROTOTYPE_ROOT)/ast.c \
	$(PROTOTYPE_ROOT)/context.c \
	$(PROTOTYPE_ROOT)/ast_inspect.c \
	$(PROTOTYPE_ROOT)/reader.c \
	$(PROTOTYPE_ROOT)/term.c \
	$(PROTOTYPE_ROOT)/type_declaration.c \
	$(PROTOTYPE_ROOT)/typing.c \
	$(PROTOTYPE_ROOT)/universe.c \
	$(PROTOTYPE_ROOT)/symbol.c

PROTOTYPE_HOTT_SOURCES := \
	$(PROTOTYPE_ROOT)/hott.c \
	$(PROTOTYPE_ROOT)/ast.c \
	$(PROTOTYPE_ROOT)/cwf_certificate.c \
	$(PROTOTYPE_ROOT)/kernel_view.c \
	$(PROTOTYPE_ROOT)/context.c \
	$(PROTOTYPE_ROOT)/term.c \
	$(PROTOTYPE_ROOT)/type_declaration.c \
	$(PROTOTYPE_ROOT)/typing.c \
	$(PROTOTYPE_ROOT)/universe.c \
	$(PROTOTYPE_ROOT)/symbol.c

PROTOTYPE_REPL_SOURCES := \
	$(PROTOTYPE_ROOT)/repl.c \
	$(PROTOTYPE_COMPILER_SOURCES)

PROTOTYPE_READER_SOURCES := \
	$(PROTOTYPE_ROOT)/read_file.c \
	$(PROTOTYPE_COMPILER_SOURCES)
