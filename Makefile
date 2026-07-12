
all:
	cc -std=c11 -Wall -Wextra -I src/prototype \
		src/prototype/repl.c \
		src/prototype/ast.c \
		src/prototype/ast_inspect.c \
		src/prototype/reader.c \
		src/prototype/term.c \
		src/prototype/type_declaration.c \
		src/prototype/typing.c \
		src/prototype/universe.c \
		src/prototype/symbol.c \
		-o a.out

reader:
	cc -std=c11 -Wall -Wextra -I src/prototype \
		src/prototype/read_file.c \
		src/prototype/ast.c \
		src/prototype/ast_inspect.c \
		src/prototype/reader.c \
		src/prototype/term.c \
		src/prototype/type_declaration.c \
		src/prototype/typing.c \
		src/prototype/universe.c \
		src/prototype/symbol.c \
		-o read_file.out

clean:
	rm -f a.out read_file.out
