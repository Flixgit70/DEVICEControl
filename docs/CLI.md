# DEVICEControl CLI

`DEVICEControlCLI` is a read-only command-line view of the DEVICEControl GUI.
Start DEVICEControl first; the CLI then reads the providers, Ember+ tree, live
parameter values and configured Pages/Panels already available in that running
application. Communication uses `127.0.0.1:37913` and is restricted to the local
computer.

## Interactive use

Run `DEVICEControlCLI` from a terminal. The prompt supports switch-style
navigation without `cd` or `ls` commands:

```text
DEVICEControl> show ?
  panel       configured Pages/Panels
  status      provider connection overview
  Lawo        Ember+ provider

DEVICEControl> show Lawo ?
  System      parameter group
  Routing     parameter group

DEVICEControl> show Lawo Sys<Tab>
DEVICEControl> show Lawo System <Tab>
```

Tab completes a unique entry and advances to the next tree level. Repeated Tab
presses therefore walk through a path as long as every level has one unique
choice. If several choices exist, Tab lists them. `?` lists the valid choices at
the current position without becoming part of the command.

Up and Down browse the last 100 commands. Recalled commands are fully editable:
Left and Right move within the line, Home and End jump to either edge, Backspace
and Delete remove characters, and newly typed text is inserted at the cursor.
Append `| watch` to any read command to refresh it every five seconds. `Ctrl+C`
stops only the watch display and returns to the CLI prompt:

```text
DEVICEControl> show Lawo System Inputs Gain | watch
DEVICEControl> show panel Routing | watch
```

Add `changes` to mark lines which differ from the preceding refresh. Changed
lines receive a `>>` marker and are highlighted in magenta for one interval.
`-d` and `--differences` are supported as shorter aliases:

```text
DEVICEControl> show panel Routing 1-8 | watch changes
DEVICEControl> show panel Routing 1-8 | watch -d
```

The CLI requests an Ember+ directory when navigation reaches a group which the
GUI has not loaded yet. On a slow provider, press `?` or Tab again after the
loading message.

## Commands

```text
show ?
show status
show version
show <provider> ?
show <provider> <group> <group> <parameter>
show panel ?
show panel <name|number|current>
show panel <name|number|current> <rows|label>
help
exit
```

`show panel <name>` prints every configured control in that panel with its
current label, value, status and provider-qualified path. `show` of a parameter
prints its current value, type, writable state and limits. The CLI does not
change parameters or panel configuration. `show status` includes the connection
summary and the individual status text of every configured provider.
`show version` prints the versions of the running `DEVICEControlCLI` client and
the connected DEVICEControl GUI. `DEVICEControlCLI --version` prints the local
CLI version without requiring the GUI to run.

Panel output includes a stable `ROW` column. A selector after the panel name
limits the output to one row, a range or a comma-separated combination. A unique
control label can also be used directly. The `LABEL` column adjusts to the
selected controls and preserves labels up to 256 characters:

```text
show panel Routing ?
show panel Routing 4
show panel Routing 3-8
show panel Routing 1-4,8-10,14,17
show panel Routing "Program Gain"
show panel Routing 1-4,8,12 | watch
```

## One-shot use

A command can also be passed directly, which is useful for shell scripts:

```bash
DEVICEControlCLI show status
DEVICEControlCLI show panel Routing
DEVICEControlCLI show Lawo System Inputs Gain
```

The process exits with a non-zero status if DEVICEControl is not running or the
command cannot be resolved.
