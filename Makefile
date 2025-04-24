all: xcheck create-tests

create-tests: create-tests.c
	gcc -o create-tests create-tests.c -Wall -Werror -O

xcheck: xcheck.c
	gcc -o xcheck xcheck.c -Wall -Werror -O

clean:
	rm xcheck create-tests tester/tests/*.img tester/tests-out/* && rmdir tests/tests-out

test:
	make && ./create-tests && ./test-xcheck.sh
