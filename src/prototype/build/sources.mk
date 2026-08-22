PROTOTYPE_ROOT := src/prototype

PROTOTYPE_AST_SOURCES := \
	$(PROTOTYPE_ROOT)/src/graph/typed_occurrence_graph.c \
	$(PROTOTYPE_ROOT)/src/graph/occurrence_usage.c \
	$(PROTOTYPE_ROOT)/src/graph/compile_metadata.c \
	$(PROTOTYPE_ROOT)/src/artifact/interface.c \
	$(PROTOTYPE_ROOT)/src/artifact/publication.c \
	$(PROTOTYPE_ROOT)/src/artifact/wire_v83.c \
	$(PROTOTYPE_ROOT)/src/artifact/relocation.c \
	$(PROTOTYPE_ROOT)/src/artifact/link.c \
	$(PROTOTYPE_ROOT)/src/frontend/ast.c \
	$(PROTOTYPE_ROOT)/src/frontend/function_graph.c \
	$(PROTOTYPE_ROOT)/src/frontend/lowering.c \
	$(PROTOTYPE_ROOT)/src/frontend/universe_collection.c

PROTOTYPE_TYPING_SOURCES := \
	$(PROTOTYPE_ROOT)/src/kernel/judgement.c

PROTOTYPE_TYPE_DECLARATION_SOURCES := \
	$(PROTOTYPE_ROOT)/src/kernel/type_declaration.c \
	$(PROTOTYPE_ROOT)/src/kernel/type_schema_view.c

PROTOTYPE_CWF_SOURCES := \
	$(PROTOTYPE_ROOT)/src/kernel/cwf_certificate.c

PROTOTYPE_DIMENSION_SOURCES := \
	$(PROTOTYPE_ROOT)/src/dimension/operator.c \
	$(PROTOTYPE_ROOT)/src/dimension/face.c \
	$(PROTOTYPE_ROOT)/src/dimension/action.c

PROTOTYPE_KERNEL_SOURCES := \
	$(PROTOTYPE_AST_SOURCES) \
	$(PROTOTYPE_DIMENSION_SOURCES) \
	$(PROTOTYPE_ROOT)/src/kernel/context.c \
	$(PROTOTYPE_CWF_SOURCES) \
	$(PROTOTYPE_ROOT)/src/kernel/resource_usage.c \
	$(PROTOTYPE_ROOT)/src/core/term.c \
	$(PROTOTYPE_TYPE_DECLARATION_SOURCES) \
	$(PROTOTYPE_TYPING_SOURCES) \
	$(PROTOTYPE_ROOT)/src/kernel/universe.c \
	$(PROTOTYPE_ROOT)/src/support/storage.c \
	$(PROTOTYPE_ROOT)/src/support/symbol.c

PROTOTYPE_COMPILER_SESSION_SOURCES := \
	$(PROTOTYPE_ROOT)/src/driver/compiler_session.c \
	$(PROTOTYPE_ROOT)/src/driver/program_storage.c

PROTOTYPE_GRAPH_SOURCES := \
	$(PROTOTYPE_AST_SOURCES) \
	$(PROTOTYPE_DIMENSION_SOURCES) \
	$(PROTOTYPE_ROOT)/src/kernel/context.c \
	$(PROTOTYPE_CWF_SOURCES) \
	$(PROTOTYPE_ROOT)/src/kernel/resource_usage.c \
	$(PROTOTYPE_ROOT)/src/frontend/reader.c \
	$(PROTOTYPE_COMPILER_SESSION_SOURCES) \
	$(PROTOTYPE_ROOT)/src/core/term.c \
	$(PROTOTYPE_TYPE_DECLARATION_SOURCES) \
	$(PROTOTYPE_TYPING_SOURCES) \
	$(PROTOTYPE_ROOT)/src/kernel/universe.c \
	$(PROTOTYPE_ROOT)/src/support/storage.c \
	$(PROTOTYPE_ROOT)/src/support/symbol.c
PROTOTYPE_COMPILER_SOURCES := \
	$(PROTOTYPE_AST_SOURCES) \
	$(PROTOTYPE_DIMENSION_SOURCES) \
	$(PROTOTYPE_ROOT)/src/kernel/context.c \
	$(PROTOTYPE_CWF_SOURCES) \
	$(PROTOTYPE_ROOT)/src/kernel/resource_usage.c \
	$(PROTOTYPE_ROOT)/src/frontend/ast_inspect.c \
	$(PROTOTYPE_ROOT)/src/frontend/reader.c \
	$(PROTOTYPE_COMPILER_SESSION_SOURCES) \
	$(PROTOTYPE_ROOT)/src/core/term.c \
	$(PROTOTYPE_TYPE_DECLARATION_SOURCES) \
	$(PROTOTYPE_TYPING_SOURCES) \
	$(PROTOTYPE_ROOT)/src/kernel/universe.c \
	$(PROTOTYPE_ROOT)/src/support/storage.c \
	$(PROTOTYPE_ROOT)/src/support/symbol.c

PROTOTYPE_HOTT_SOURCES := \
	$(PROTOTYPE_ROOT)/src/identity/hott.c \
	$(PROTOTYPE_AST_SOURCES) \
	$(PROTOTYPE_DIMENSION_SOURCES) \
	$(PROTOTYPE_CWF_SOURCES) \
	$(PROTOTYPE_ROOT)/src/kernel/kernel_view.c \
	$(PROTOTYPE_ROOT)/src/kernel/context.c \
	$(PROTOTYPE_ROOT)/src/kernel/resource_usage.c \
	$(PROTOTYPE_ROOT)/src/core/term.c \
	$(PROTOTYPE_TYPE_DECLARATION_SOURCES) \
	$(PROTOTYPE_TYPING_SOURCES) \
	$(PROTOTYPE_ROOT)/src/kernel/universe.c \
	$(PROTOTYPE_ROOT)/src/support/storage.c \
	$(PROTOTYPE_ROOT)/src/support/symbol.c

PROTOTYPE_DRIVER_DIAGNOSTIC_SOURCES := \
	$(PROTOTYPE_ROOT)/src/driver/diagnostics.c

PROTOTYPE_REPL_SOURCES := \
	$(PROTOTYPE_ROOT)/src/driver/repl.c \
	$(PROTOTYPE_DRIVER_DIAGNOSTIC_SOURCES) \
	$(PROTOTYPE_COMPILER_SOURCES)

PROTOTYPE_READER_SOURCES := \
	$(PROTOTYPE_ROOT)/src/driver/read_file.c \
	$(PROTOTYPE_DRIVER_DIAGNOSTIC_SOURCES) \
	$(PROTOTYPE_COMPILER_SOURCES)
