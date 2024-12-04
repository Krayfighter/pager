

## Pager

A simple pager program written in GNU C using only
the c std library and posix API completely dependency free.


### Building

building and running can be done with [remake](https://github.com/Krayfighter/remake)
or simply with
```
  gcc src/main.c -o pager
```

pager can read from a file with invocations like
```pager example.txt```
or can capture the output of a shell command like
```pager --spawn "ls -a ../Downloads/"```




