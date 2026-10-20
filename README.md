# 

> Write commands in plain English. TalkTerm figures out the rest.

I got tired of forgetting flags and syntax for basic stuff like listing hidden files or checking disk space. So I built this — a terminal that understands English and converts it to the actual Linux command under the hood.

It's not a replacement for bash. Think of it as a wrapper that makes common tasks easier. And if you already know the real command, just type it — it passes through.

## Getting started

You need GCC and readline:

```
sudo apt install build-essential libreadline-dev
```

Then:

```
make
./talkterm
```

That's it. Type `help` once you're in.

To install it globally so you can run `talkterm` from anywhere:

```
sudo make install
```

## How it works

You type something like:

```
talk:~/projects ❯ show hidden files
talk:~/projects ❯ create folder src
talk:~/projects ❯ go to src
talk:~/projects/src ❯ delete file old.txt
```

And it runs `ls -la`, `mkdir -p src`, `cd src`, `rm old.txt` respectively.

The parser breaks your input into a **verb** (what to do), a **target type** (file/folder/package), **modifiers** (hidden/force/detailed), and a **name**. Then the executor maps that to the right shell command.

## Some commands I use a lot

```
show files                    →  ls
show hidden files             →  ls -la
show files detailed           →  ls -lh
go to /some/path              →  cd /some/path
go back                       →  cd ..
create folder myproject       →  mkdir -p myproject
create file notes.txt         →  touch notes.txt
delete file junk.log          →  rm junk.log (asks confirmation)
rename old.txt to new.txt     →  mv old.txt new.txt
find file config              →  find . -iname '*config*'
search TODO                   →  grep -rn 'TODO' .
show memory                   →  free -h
show disk                     →  df -h
check python version          →  python --version
```

Git stuff works too:

```
save changes fixed login      →  git add -A && git commit -m 'fixed login'
push changes                  →  git push
pull changes                  →  git pull
show branches                 →  git branch -a
check git status              →  git status
```

Package management:

```
install requests              →  pip install requests
install system curl           →  sudo apt install -y curl
```

And you can always just type the real command — `git log`, `sudo apt update`, `docker ps` — it detects those and runs them directly.

## Project structure

```
src/
  main.c       - the REPL loop, prompt, help menu
  parser.c     - tokenizer + verb/target/modifier matching
  parser.h     - all the enums and the ParsedCommand struct
  executor.c   - takes a ParsedCommand and runs the right shell command
  executor.h
  history.c    - saves command history to ~/.talkterm_history
  history.h
  colors.h     - ANSI color codes
```

## Building

```
make              # build
make run          # build + run
make clean        # delete build files
sudo make install # copy to /usr/local/bin
```

Compiles with `-Wall -Wextra -Wpedantic -std=c17`. Zero warnings.

## What's next

- Tab completion for TalkTerm commands (not just readline defaults)
- Config file for custom aliases
- Plugin system for adding new command mappings
- Better error messages with suggestions ("did you mean...?")

## License

MIT
