.PHONY: clean install

test: test.c
	cc $< -o $@ -O0 -ggdb3 -lm && ./test

install:
	install -m 0644 ${DESTDIR}/usr/local/include

clean:
	rm -f test
