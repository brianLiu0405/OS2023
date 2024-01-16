cmd_revstr/built-in.a := rm -f revstr/built-in.a; echo revstr.o | sed -E 's:([^ ]+):revstr/\1:g' | xargs ar cDPrST revstr/built-in.a
