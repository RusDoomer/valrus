# This program comes with no guarantee that it is safe to run on your system

only works on linux, must have make and gcc installed: clone the repository, cd into it, and run make.

run with : `./ruslyzer <arguments>`

Can be run in 4 modes: analyze, rank, generate, and multithreaded defined by the arguments `-a`, `-r`, `-g`, `-m` respectively

universal arguments: work for all modes
- `-l` - selects the base layout by name, default is hiyou
- `-c` - selects the corpus by name, default is shai
- `-w` - selects the weights by name, default is default
- `-q` - enables quiet mode when used

arguments for generate and multithreaded modes
- a number after `-g` or `-m` - number of layouts to test when generating, must be greater than 50 per thread
- `-i` - enables improve mode, uses the layout selected by `-l` and improves upon it, keeping keys pinned in the weight file selected by `-w` stationary

arguments exclusive to multithreaded mode
- `-t` - select an specific number of threads to run instead of the default (cores * 4)

some example commands might be:
- `./ruslyzer` - the default
- `./ruslyzer -l hiyou -c shai -w default` - this is the same as the default
- `./ruslyzer -l mir -c monkeytype -q` - quiet mode with the layout mir and the monkeytype corpus
- `./ruslyzer -g 10000` - generate mode testing 10000 layouts
- `./ruslyzer -g 10000 -i -l colemak` - generate mode improving colemak
- `./ruslyzer -m 80000 -i -l xenia -c monkeytype -w sfb` - multithreaded mode improving xenia on the monkeytype corpus but only optimizing for sfb
- `./ruslyzer -m 100000 -t 100` - multithreaded mode with 100 threads, this means each thread will run the equivalent of `-g 1000`

the default weight file is empty and should be filled by the user to suit their preferences
