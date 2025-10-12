
all: parser.tab.c lex.yy.c
	cc -I ./include src/main.c src/term.c src/symbol.c src/environment.c src/parse.c parser.tab.c lex.yy.c -o a.out

parser.tab.c parser.tab.h: src/parser.y
	bison -d src/parser.y

lex.yy.c: src/lexer.l parser.tab.h
	flex src/lexer.l

clean:
	rm -f parser.tab.c parser.tab.h lex.yy.c a.out
