# GDB Program Tracer

A lightweight program tracer written in C. This tool programmatically wraps the GNU Debugger (`gdb` or `gdb-multiarch`) using pseudo-terminals (`forkpty`) to automate execution tracking, instruction stepping, and state logging.

## Features

* **Automated GDB Interaction:** Communicates with GDB programmatically via a pseudo-terminal (PTY) without manual user input.
* **Instruction-Level Tracing:** Automatically configures GDB to display assembly instructions and repeatedly steps through the program (`stepi`).
* **Multi-Architecture Support:** Natively supports standard `gdb` as well as `gdb-multiarch` for cross-architecture analysis.
* **State Machine Driven:** Uses a robust internal state machine (`GDB_INIT`, `GDB_SHELL`, `GDB_EXECUTING`) to reliably parse GDB's output and determine when it is ready for the next command.
* **Automated Logging:** Automatically enables GDB's logging feature (`set logging enabled on`) to save trace data to disk.

## Prerequisites

To build and run this project, you need:
* A C compiler (e.g., `gcc` or `clang`)
* `gdb` or `gdb-multiarch` installed on your system
* Standard POSIX headers and the `util` library (for `pty.h` and `forkpty`)

## Building

Compile the source code using `gcc`.


```bash
gcc -o tracer tracer.c -lutil
```

## Usage

Run the compiled tracer by specifying the GDB version and the target binary you want to analyze.

```bash
./tracer [-g | -m] -b <path_to_binary> [-v]

```

### Command-Line Arguments

* `-g`: Use the standard `gdb` binary. *(Required if `-m` is not used)*
* `-m`: Use `gdb-multiarch`. *(Required if `-g` is not used)*
* `-b <binary>`: Specify the path to the target binary you want to trace. *(Required)*
* `-v`: Enable verbose mode.

### Example

To trace a binary named `program` using standard GDB:

```bash
./tracer -g -b ./program

```

This will launch GDB, set a breakpoint at `main`, run the program, and begin executing `stepi` instructions while printing the program counter.

## How It Works

1. **PTY Allocation:** The program uses `forkpty()` to spawn a child process running GDB, effectively tricking GDB into thinking it's connected to a real terminal. This avoids buffering issues associated with standard pipes.
2. **State Management:** The parent process reads from the PTY descriptor, buffering the output until it detects the `(gdb) ` shell anchor or a program termination message.
3. **Command Execution:** Once the shell is ready, the tracer dispatches commands from the predefined `cmds` array.
4. **Tracing:** The default command sequence sets a breakpoint at `main`, begins execution, and loops through instruction-level steps (`stepi`) with the `$pc` (program counter) displayed.

## Customizing the Tracer

You can easily customize the automated GDB commands by modifying the `cmds[]` array in `main()`:

```c
cmd_t cmds[] = {
    {"set logging enabled on", ACTION_NEXT_CMD, OPTION_NONE},
    {"break main", ACTION_NEXT_CMD, OPTION_NONE},
    {"run", ACTION_NEXT_CMD, OPTION_NONE},
    {"info registers", ACTION_NEXT_CMD, OPTION_NONE}, // Custom command example
    {"stepi", ACTION_START_TRACE, OPTION_SHOW_INSTRUCTION},
    {"", ACTION_END}
};

```

* `ACTION_NEXT_CMD`: Fire and wait for the next `(gdb) ` prompt.
* `ACTION_START_TRACE`: Begin a repeating trace loop (often paired with options like `OPTION_SHOW_INSTRUCTION`).
* `ACTION_END`: Terminates the tracing session.

## Graceful Shutdown

You can stop the tracing process at any time by sending a `SIGINT` (pressing `Ctrl+C`). The tracer intercepts the signal, cleans up the GDB process via `SIGTERM`, and closes the file descriptors.
