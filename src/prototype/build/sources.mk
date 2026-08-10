PROTOTYPE_ROOT := src/prototype

PROTOTYPE_AST_SOURCES := \
	$(PROTOTYPE_ROOT)/src/graph/operation_graph.c \
	$(PROTOTYPE_ROOT)/src/graph/compile_metadata.c \
	$(PROTOTYPE_ROOT)/src/artifact/interface.c \
	$(PROTOTYPE_ROOT)/src/artifact/publication.c \
	$(PROTOTYPE_ROOT)/src/artifact/wire_v70.c \
	$(PROTOTYPE_ROOT)/src/artifact/relocation.c \
	$(PROTOTYPE_ROOT)/src/artifact/link.c \
	$(PROTOTYPE_ROOT)/src/frontend/ast.c \
	$(PROTOTYPE_ROOT)/src/frontend/lowering.c

PROTOTYPE_TYPING_SOURCES := \
	$(PROTOTYPE_ROOT)/src/kernel/judgement.c

PROTOTYPE_KERNEL_SOURCES := \
	$(PROTOTYPE_AST_SOURCES) \
	$(PROTOTYPE_ROOT)/src/kernel/context.c \
	$(PROTOTYPE_ROOT)/src/core/term.c \
	$(PROTOTYPE_ROOT)/src/kernel/type_declaration.c \
	$(PROTOTYPE_TYPING_SOURCES) \
	$(PROTOTYPE_ROOT)/src/kernel/universe.c \
	$(PROTOTYPE_ROOT)/src/support/symbol.c

PROTOTYPE_GRAPH_SOURCES := \
	$(PROTOTYPE_AST_SOURCES) \
	$(PROTOTYPE_ROOT)/src/kernel/context.c \
	$(PROTOTYPE_ROOT)/src/frontend/reader.c \
	$(PROTOTYPE_ROOT)/src/core/term.c \
	$(PROTOTYPE_ROOT)/src/kernel/type_declaration.c \
	$(PROTOTYPE_TYPING_SOURCES) \
	$(PROTOTYPE_ROOT)/src/kernel/universe.c \
	$(PROTOTYPE_ROOT)/src/support/symbol.c

PROTOTYPE_COMPILER_SOURCES := \
	$(PROTOTYPE_AST_SOURCES) \
	$(PROTOTYPE_ROOT)/src/kernel/context.c \
	$(PROTOTYPE_ROOT)/src/frontend/ast_inspect.c \
	$(PROTOTYPE_ROOT)/src/frontend/reader.c \
	$(PROTOTYPE_ROOT)/src/core/term.c \
	$(PROTOTYPE_ROOT)/src/kernel/type_declaration.c \
	$(PROTOTYPE_TYPING_SOURCES) \
	$(PROTOTYPE_ROOT)/src/kernel/universe.c \
	$(PROTOTYPE_ROOT)/src/support/symbol.c

PROTOTYPE_HOTT_SOURCES := \
	$(PROTOTYPE_ROOT)/src/identity/hott.c \
	$(PROTOTYPE_AST_SOURCES) \
	$(PROTOTYPE_ROOT)/src/kernel/cwf_certificate.c \
	$(PROTOTYPE_ROOT)/src/kernel/kernel_view.c \
	$(PROTOTYPE_ROOT)/src/kernel/context.c \
	$(PROTOTYPE_ROOT)/src/core/term.c \
	$(PROTOTYPE_ROOT)/src/kernel/type_declaration.c \
	$(PROTOTYPE_TYPING_SOURCES) \
	$(PROTOTYPE_ROOT)/src/kernel/universe.c \
	$(PROTOTYPE_ROOT)/src/support/symbol.c

PROTOTYPE_REPL_SOURCES := \
	$(PROTOTYPE_ROOT)/src/driver/repl.c \
	$(PROTOTYPE_COMPILER_SOURCES)

PROTOTYPE_READER_SOURCES := \
	$(PROTOTYPE_ROOT)/src/driver/read_file.c \
	$(PROTOTYPE_COMPILER_SOURCES)
