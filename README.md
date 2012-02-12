The Middleman Project
=====================

http://github.com/cklin/mdm

The Middleman Project (mdm) aims to create utility programs that unleash
the power of multi-processor and multi-core computer systems.  It does
so by helping you parallelize your shell scripts, Makefiles, or any
other program that invoke external programs.


Software requirements
---------------------

To run mdm, you need a modern (2.6+) Linux system, GNU [screen][screen]
and [ncurses library][ncurses].  It should be easy to port to other Unix
systems by writing new `/proc` parsers and fixing any library
incompatibilities.


Building and installing mdm
---------------------------

To build mdm, simply run `make` at the toplevel.  This project is simple
enough so that there is no need for autoconf and automake.  To install,
use `make install` as follows:

    $ make install PREFIX=/install/directory/prefix

Without the `PREFIX` override, `make install` installs mdm to `/usr/local`.


How does it work?
-----------------

The philosophy behind mdm is that users should benefit from their
multi-core systems without making drastic changes to their shell
scripts.  With mdm, you annotate your scripts to specify which commands
might benefit from parallelization, and then you run it under the
supervision of the mdm system.  At runtime, the mdm system dynamically
discovers parallelization opportunities and run the annotated commands
in parallel as appropriate.


mdm in 3 easy steps
-------------------

Suppose you use the following shell script (encode.sh) for encoding your
music library.  It works, but it leaves your quad-core computer mostly
idle because it processes only one file at a time.

    #!/bin/bash
    for i in */*.wav
    do echo $i
       ffmpeg -i "$i" "${i%%.wav}.mp3"
    done

You can parallelize this shell script in three easy steps.

 1. Find commands that you think are suitable for parallel execution,
    and annotate them with mdm-run.  Here is the modified encode.sh:

        #!/bin/bash
        for i in *.wav
        do echo $i
           mdm-run ffmpeg -i "$i" "${i%%.wav}.mp3"
        done

 2. Specify the I/O behavior of your parallel commands in an iospec
    file.  You know ffmpeg reads from its -i option argument and writes
    to its command argument (w/o option), so this is what you write in
    your iospec file:

        ffmpeg R-i W

    You can skip this step if you are certain the parallel command
    cannot interfere with any other command in the script.

 3. Run the script under mdm.screen as follows:

        $ mdm.screen -c iospec encode.sh

    You should see a monitoring program (mdm-top) displaying the
    execution status of your parallel commands, and the encoding process
    should (hopefully) complete a lot sooner because you are giving all
    processing cores a good workout!


When not to annotate with mdm-run
---------------------------------

The mdm-run command runs executable programs asynchronously.  Therefore,
there are a few cases where you should not annotate a command with mdm-run:

 1. The command is a shell built-in,

 2. You need to know the exit status of the command, or

 3. You perform I/O redirection on the command.


The I/O specification file
--------------------------

The I/O specification file (iospec) specifies the I/O behavior of
programs.  The mdm system use these specifications to decide whether it
is okay to run two annotated commands at the same time.  Each line of
the file describes a program.  Here are a few examples:

    ffmpeg R-i W
    rm     W
    cc     W-o 0-c Rbusy R
    date   Wbusy

In plain English:

  * `ffmpeg` reads from the option argument of `-i` and writes to all its
    non-option arguments,

  * `rm` writes to all its non-option arguments,

  * `cc` writes to its `-o` argument, `-c` takes no arguments, reads from the
    (abstract) file "busy" and from its non-option arguments, and

  * `date` writes to the (abstract) file "busy".

Adding the abstract file "busy" to the iospec ensures that mdm will
never schedule the date command to run when any "cc" command is still
running (and vice versa).

Beware that the iospec format is subject to change in the future.


When to use mdm-sync
--------------------

The mdm-sync command is just like mdm-run, except that it does not
submit the command for parallel execution.  Use mdm-sync to annotate a
command when you don't want it to run in parallel, but you think it
might interfere with a command annotated by mdm-run.


Found a bug?
------------

Please report bugs through the [GitHub mdm issue tracker][tracker].

[screen]: http://www.gnu.org/software/screen/
[ncurses]: http://www.gnu.org/software/ncurses/
[tracker]: http://github.com/cklin/mdm/issues
