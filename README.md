

## Pager

A simple pager program written in GNU C using only
the c std library and posix API completely dependency free.

### Dependencies / System Requirements

Pager depends on the POSIX.1 low-level terminal API `termios`
so will likely never support any Windows platform. This, however,
means that it should work on any POSIX compliant or SUS system.

It is recommended to use an XTerm capable terminal emulator,
but should work on any terminal that implements
(https://en.wikipedia.org/wiki/ANSI_escape_code) [ANSI escape sequences]

### Building

building and running can be done with my in-house build system
[remake](https://github.com/Krayfighter/remake)```remake build``` or GNU make with
the default target ```make```

pager can read from a file with invocations like
```./pager example.txt```
or can capture the output of a shell command like
```./pager --spawn "ls -R /home"```




