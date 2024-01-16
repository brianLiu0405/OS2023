cmd_hello/built-in.a := rm -f hello/built-in.a; echo hello.o | sed -E 's:([^ ]+):hello/\1:g' | xargs ar cDPrST hello/built-in.a
