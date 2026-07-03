On this page, we collect some coding conventions. Anybody who works on ScummVM, and anybody who would like to submit a patch, should read and adhere to these, in addition to the [[Code Formatting Conventions]].

Many of these are all about ''portability'' of the code to as many systems as possible. This is one of the primary design goals of ScummVM. It means that we strive to ensure our code compiles and runs on as many systems as possible. On this page we try to collect typical portability problems and pitfalls, and how to avoid them.

Beyond that, there are other caveats that we point out. For example things that make engines non-reentrant (which causes problems when using the "return to launcher" feature, and then resuming a game).

''TODO: This page is incomplete. We should probably also link on some good & relevant web pages on the net, and only focus on the most frequent or maybe unusual (ScummVM specific) issues.''


== Language features ==
ScummVM is written in a subset of C++11. Due to limitations of the C++ run-times on various platforms, the following features cannot currently be used:
* C++ exceptions (throw/catch): Not all C++ compilers support these correctly (esp. on embedded systems), and exception support incurs a noticeable overhead in binary size.
* Global C++ objects: Their constructors / destructors will not be called on certain targets, causing all kinds of bad problems and requiring ugly workarounds. (The GCC option -Wglobal-constructors helps finding code doing this.)
* We implement wrappers for a subset of the Standard Template Library (STL) in common/std/, which internally use ScummVM classes. You simply need to include the appropriate file, and use Std:: rather than std:: as the namespace. Note that these should only be used when porting existing codebases already using the STL. For developing entirely new engines from scratch, you need to use the classes in /common/ directly.

We are reviewing these decisions from time to time, but so far, in our estimation the drawbacks of using any of these outweigh the hypothetical advantages.

== Endianness issues ==
If you don't know what "Endianness issues" refers to, read up here: https://en.wikipedia.org/wiki/Endianness

ScummVM engine authors have to keep endianness issues in mind for two reasons:
* ScummVM runs on both little endian (Windows, x86 Mac OS X, x86 Linux...) and big endian hosts (PowerPC Mac OS X, PlayStation 3 Linux...). So when writing data (think savegames) to files and reading it back again, you need to compensate for this. This is easily done by using the READ_ and WRITE_ macros from common/endian.h (like READ_LE_UINT32 or WRITE_BE_UINT16.) respectively the corresponding Stream class methods (like readUint32LE or writeUint16BE).
* Furthermore, some games existed in multiple versions, e.g. one for Windows and one for Mac OS. In that case, you may have to detect and distinguish these versions and employ different reading calls, to compensate for endian differences in the game data files.

As most desktop machines are now little-endian i.e. x86, debugging any remaining endian issues without access to a big-endian desktop system is hard. [[HOWTO-Debug-Endian-Issues|HOWTO Debug Endian Issues]] details a solution to this by using a virtual machine software package to emulate a machine with a different endian CPU to the host system.

== Struct packing ==
Do *not* assume that structs are stored in a certain way in memory -- the layout of a struct in memory can vary a lot between platforms, and on most modern systems it will almost never be "packed". If you absolutely must use packed structs, do not just use some #pragma or __attribute__ (as that is not portable either). Instead, do the following:
* Before the struct(s) you need to be packed, insert
<syntaxhighlight lang="cpp">
  #include "common/pack-start.h"	// START STRUCT PACKING
</syntaxhighlight>
* After the struct(s) you need to be packed, insert
<syntaxhighlight lang="cpp">
  #include "common/pack-end.h"	// END STRUCT PACKING
</syntaxhighlight>
* At the "end" of each struct you need to be packed, insert the following between the closing } and the ; after it:
<syntaxhighlight lang="cpp">
  PACKED_STRUCT
</syntaxhighlight>
You may want to look at some files which already do that, e.g. look at <code>engines/scumm/boxes.cpp</code>

== File access ==
In a nutshell, you must not use fopen(), fread(), fwrite(), etc., nor open(), close(), etc. in your code (unless you are developing a backend, that is). Instead you must use the APIs for reading and writing files provided by ScummVM. Please consult the [[HOWTO-Open Files|HOWTO Open Files]] page for details.

== Use of global / static variables ==
There are two major issues with global variables (and that includes static variables hidden inside function bodies, too). In general, you should therefore avoid them whenever possible. These affect mainly frontend code, though (engines, GUI code, common code), backends authors have more liberties in this regard.

The first issue are global variables of a type that requires a constructor. The problem with that is multifold. E.g. the order in which such global constructors are invoked is not well-defined in general. So things may work fine on one system but bad on another. Next, engines can be compiled into dynamically loadable plugins. So when the plugin is loaded, the constructors of global objects in that plugin may or may not be called, depending on the code loader in use. Likewise, when the plugin is unloaded, destructors may or may not be called.

Therefore, '''use of object requiring global constructors is forbidden in ScummVM'''. Developers are encouraged to compile their code with the "-Wglobal-destructors" option (when using GCC or clang), combined with -Werror. Exceptions may be possible for backend code, but even there we advise them to be avoided.


The second issue concerns problems causing engines to be non-reentrant. Consider a code snippet like this:
<syntaxhighlight lang="cpp">
  bool foo(bool cond) {
    static bool bar = false;
    if (cond)
      bar = true;
    return bar;
  }
</syntaxhighlight>
Suppose this function is part of engine quux. When a new game using this engine is started, foo(false) will initially always return false. After foo(true) has been called the first time, foo will always return true. Maybe foo(true) is called when the user/player solves a certain puzzle in the game, and maybe the foo() function is meant to test whether this puzzle has already been solved.
Now imagine the player uses the "return to launcher" feature; once back in the launcher, they restart the game, again using the quux engine.
Now the static global variable inside foo() has not been reset! As a consequence, the engine will still think that puzzle has been solved, even though a completely new game was started.

This is another important reason why global variables keeping state are dangerous. At the very least, you ''must'' re-init any global variables whenever your engine starts. Relying on one-time initialization assignments to global variables, as in
<syntaxhighlight lang="cpp">
   static int myGlobal = 0;
</syntaxhighlight>
is ''not'' sufficient.

Therefore, '''use of non-const static globals inside function bodies is strictly forbidden''', as there is no way to reset them from code outside of that function.

In addition, global variables are strongly discouraged; if they are to be used, then they ''must'' be accompanied by a comment that explains why this static variable is necessary, and why it cannot be replaced by something else, like typically a member variable of a suitable class / object. The comment should also indicate where the variable is (re)set when the engine launches. If no such comment is present, a generic comment of the following form shall be added to the variable declaration:
<syntaxhighlight lang="cpp">
  // FIXME: non-const global var
</syntaxhighlight>

As a minor exception, if an existing engine uses many globals (due to not (yet) being objectified), it is permissible to use a single comment to document multiple globals at once.
