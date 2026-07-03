== Use common sense ==

These are conventions which we try to follow when writing code for ScummVM. Sticking to a common set of formatting / indention rules makes it easier to read through our source base (which now exceed half a million lines of code by far, making this quite important). If you want to submit patches, please try to adhere to these rules.

We don't always follow these rules slavishly, in certain cases it is OK (and in fact might be favorable) to stray from them. Applying common sense, as usual, is a good idea.

In the following examples tabs are replaced by spaces for visual consistency with the Code Formatting Conventions. But in actual code, use tabs for indentations (our tabs are assumed to be 4 spaces wide).

== Hugging braces ==

Braces in your code should look like the following example:

<syntaxhighlight lang="cpp">
for (int i = 0; i < t; i++) {
    [...]
}

if (j < k) {
    [...]
} else if (j > k) {
    [...]
} else {
    [...]
}

class Dummy {
    [...]
};
</syntaxhighlight>

Did you see the {}'s on that?

For a single statement expression bodies the braces are optional, you are free to write:

<syntaxhighlight lang="cpp">
for (int i = 0; i < t; i++)
    statement();

if (a)
   do_b();
else
   do_c();
</syntaxhighlight>

However, if you have if/else block pairs, it is advisable to add braces to avoid confusion and for consistency, e.g.:
<syntaxhighlight lang="cpp">
// NOT GOOD
if (a) {
  do_b();
  do_c();
} else
  do_e();

// EVEN WORSE
if (a)
   do_b();
else {
   do_c();
   do_e();
}

// RATHER WRITE
if (a) {
  do_b();
  do_c();
} else {
  do_e();
}

</syntaxhighlight>

And if you use nested expressions, this is a must, and modern compilers even issue a warning or sometimes error for you:

<syntaxhighlight lang="cpp">
// VERY BAD
if (a)
   if (b)
      do_foo();
   else
      do_bar();
else
   do_baz();


// RATHER WRITE
if (a) {
   if (b)
      do_foo();
   else
      do_bar();
} else {
   do_baz();
}

</syntaxhighlight>

== Tab indents, with tabstop at four spaces ==

Says it all, really.

== Whitespaces ==

'''Conventional operators surrounded by a space character'''

<syntaxhighlight lang="cpp">
a = (b + c) * d;
</syntaxhighlight>

'''C++ reserved words separated from opening parentheses by a white space'''

<syntaxhighlight lang="cpp">
while (true) {
</syntaxhighlight>

'''Commas followed by a white space'''

<syntaxhighlight lang="cpp">
someFunction(a, b, c);
</syntaxhighlight>
<syntaxhighlight lang="cpp">
int d, e;
</syntaxhighlight>

'''Semicolons followed by a space character, if there is more on a line'''

<syntaxhighlight lang="cpp">
for (int a = 0; b < c; d++)
</syntaxhighlight>
<syntaxhighlight lang="cpp">
doSomething(e); doSomething(f);	// This is probably bad style anyway
</syntaxhighlight>

'''Comment operators followed by a space character'''

<syntaxhighlight lang="cpp">
// C++-style comment

/* C-style comment */

/*
 * Multiline C-style comments have vertical alignment of the star symbols
 */
</syntaxhighlight>


'''Mandatory ''{}'' for empty ''for''/''while'' loops'''

<syntaxhighlight lang="cpp">
while (i < length - 1 && array[++i] != item);   // bad
while (i < length - 1 && array[++i] != item) {} // good
</syntaxhighlight>

'''When declaring class inheritance and in a ? construct, colons should be surrounded by white space'''

<syntaxhighlight lang="cpp">
class BusWheel : public RubberInflatable {
</syntaxhighlight>
<syntaxhighlight lang="cpp">
(isNight) ? colorMeDark() : colorMeBright();
</syntaxhighlight>

'''Indentation level is not increased after namespace clause'''

<syntaxhighlight lang="cpp">
namespace Scumm {

byte Actor::kInvalidBox = 0;

void Actor::initActorClass(ScummEngine *scumm) {
    _vm = scumm;
}

} // End of namespace Scumm
</syntaxhighlight>

'''Array delete operator has no whitespace before []'''
<syntaxhighlight lang="cpp">
delete[] foo;
</syntaxhighlight>

'''Template definitions'''

No whitespace between template keyword and <
<syntaxhighlight lang="cpp">
template<typename foo>
void myFunc(foo arg) {
    // ...
}
</syntaxhighlight>

'''Operator overloading'''

Operator keyword is NOT separated from the name, except for type conversion operators where it is required.
<syntaxhighlight lang="cpp">
struct Foo {
    void operator()() {
        // ...
    }

    operator bool() {
        return true;
    }
};
</syntaxhighlight>

'''Pointers and casts'''

No whitespace after a cast; and in a pointer, we write a whitespace before the star but not after it.
<syntaxhighlight lang="cpp">
const char *ptr = (const char *)foobar;
</syntaxhighlight>

'''References'''

We use the same rule for references as we do for pointers: use a whitespace before the "&" but not after it.
<syntaxhighlight lang="cpp">
int i = 0;
int &ref = i;
</syntaxhighlight>

'''Arrow operator'''

We put no spaces around the arrow operator.
<syntaxhighlight lang="cpp">
int a = foo->bar;
</syntaxhighlight>

'''Vertical alignment'''

When it adds to readability, a vertical alignment by means of extra tabs or spaces is allowed. However, it is not advised to have the opening and closing brackets/braces to occupy a single line.
<syntaxhighlight lang="cpp">
int foo     = 2;
int morefoo = 3;

Common::Rect *r = new Common::Rect(x,
                                   y,
                                   x + w,
                                   y + h);
</syntaxhighlight>

== Switch/Case constructs ==

<syntaxhighlight lang="cpp">
switch (cmd) {
case kSomeCmd:
    a = 1;
    func1();
    b = a + 1;
    // fall through
case kSomeVerySimilarCmd:
    someMoreCmd();
    a = 3;
    break;
case kSaveCmd:
    save();
    break;
case kLoadCmd:
case kPlayCmd:
    close();
    break;
default:
    Dialog::handleCommand(sender, cmd, data);
}
</syntaxhighlight>
* Note the comment on whether fall through is intentional. Use exactly this and not some variation both for consistency and so that the compiler will see it and suppress potential warnings.

== Vertical space ==

'''Avoid composite one liners'''

<syntaxhighlight lang="cpp">
if (condition) do_foo();  // <-- bad example

// Proper way of writing it
if (condition)
    do_foo();
</syntaxhighlight>


'''Split out variable declarations'''
<syntaxhighlight lang="cpp">
int a = 1;
int b = 2;
int c;

c = a + b;
</syntaxhighlight>

'''Split out logical program blocks'''
<syntaxhighlight lang="cpp">
void MacWindowManager::setScreen(ManagedSurface *screen) {
    _screen = screen;
    delete _screenCopy;
    _screenCopy = nullptr;

    if (_desktop)
        _desktop->free();
    else
        _desktop = new ManagedSurface();

    _desktop->create(_screen->w, _screen->h, _pixelformat);
    drawDesktop();
}
</syntaxhighlight>

== Preprocessor pragmas ==
All preprocessor pragrmas must start from the first column:

<syntaxhighlight lang="cpp">
#include "common/system.h"

#pragma mark --- Start of example ---
void example() {
    int i = 0;

#ifdef USE_FEATURE
    Feature f = new Feature();
#endif

    for (int x = 0; x < 10; x++) {

#define FOO f
        warning("%d", (FOO).code());
#undef FOO
    }
}
</syntaxhighlight>

== Naming ==

'''Constants'''

Basically, you have two choices (this has historical reasons, and we probably should try to unify this one day):

# Camel case with prefix 'k': <pre> kSomeKludgyConstantName </pre >
# All upper case, with words separated by underscores (no leading/trailing underscores): <pre>SOME_KLUDGY_CONSTANT_NAME</pre>

(As a side remark, we recommend avoiding #define for creating constants, because these can lead to weird and difficult to track down conflicts. Instead use enums or the <code>const</code> keyword.)

'''Type names'''

Camel case starting with upper case. This includes template types.

<syntaxhighlight lang="cpp">
class MyClass { /* ... */ };
struct MyStruct { /* ... */ };
typedef int MyInt;

template typename<FancyPants>
void bestClothes(FancyPants pants) {/* ... */ }
</syntaxhighlight>

'''Class member variables'''

Prefixed with '_' and in camel case (Yo! no underscore separators), starting with lowercase.

<syntaxhighlight lang="cpp">
char *_someVariableName;
</syntaxhighlight>

'''Class methods'''

Camel case, starting with lowercase.

<syntaxhighlight lang="cpp">
void thisIsMyFancyMethod();
</syntaxhighlight>

'''Non-member functions'''

Camel case, starting with lowercase.

<syntaxhighlight lang="cpp">
void thisIsMyFancyGlobalFunction();
</syntaxhighlight>

'''Local variables'''

Use camel case (Yo! no underscore separators), starting with lowercase.

<syntaxhighlight lang="cpp">
char *someVariableName;
</syntaxhighlight>

Note that for POD structures it is fine to use this rule too.

'''Global variables'''

In general you should avoid global variables, but if it can't be avoided, use 'g_' as prefix, camel case, and start with lowercase

<syntaxhighlight lang="cpp">
int g_someGlobalVariable;
</syntaxhighlight>

== Special comments ==

=== Special Keywords ===

The following goes slightly beyond code formatting: We use certain keywords (together with an explanatory text) to mark certain sections of our code. In particular:
* '''FIXME''' marks code that contains hacks or bad temporary workarounds, things that really should be revised at a later point.
* '''TODO''' marks incomplete code, or things that could be done better but are left for the future.
* '''WORKAROUND''' marks code that works around bugs in the original game, like script bugs. Sometimes these bugs worked in the original due to bugs in the original engine, sometimes the bug was visible in the original, too. It's important that you explain here what exactly you work around, and if applicable, refer to relevant tracker items!
* '''I18N:''' Adds comment to translators for internationalization. See [[Supporting GUI Translation]].

=== Doxygen documentation comments ===

ScummVM uses the [http://www.doxygen.org/ Doxygen] software to generate HTML documentation for the codebase (available [http://doxygen.scummvm.org here]).

Doxygen supports ''documentation blocks''. These are specially-formatted comments that doxygen prints out in the generated documentation. They are similar in purpose to Java's JavaDoc or Python's docstrings.

There are many ways to mark such comments, but developers are encouraged to use the JavaDoc style:

<syntaxhighlight lang="cpp">
/**
 * Move ("warp") the mouse cursor to the specified position in virtual
 * screen coordinates.
 * @param x             the new x position of the mouse
 * @param y             the new y position of the mouse
 */
virtual void warpMouse(int x, int y) = 0;
</syntaxhighlight>
(See [http://doxygen.scummvm.org/d9/df4/classOSystem.html#ecab84670def917107d6c1b5ca3b82c3 here] for the docs generated from this.)

As shown in the example, documentation blocks also support several special commands such as '''param'''. Those are prefixed with either '''@''' or '''\''', but developers should always use '''@'''.

If you want to add a brief explanation of a variable or function ''after'' its declaration, this is the correct syntax:
<syntaxhighlight lang="cpp">
int16 x;	///< The horizontal part of the point
int16 y;	///< The vertical part of the point
</syntaxhighlight>
(See [http://doxygen.scummvm.org/d7/d66/structCommon_1_1Point.html#2d868735aeaaf391ce9b3df9232c031f here] for the docs generated from this.)

For more information, visit the official documentation:
* [http://www.stack.nl/~dimitri/doxygen/docblocks.html Documentation blocks].
* [http://www.stack.nl/~dimitri/doxygen/commands.html List of available commands].

== Automatically converting code to our conventions ==
The following settings for [http://astyle.sourceforge.net/ Artistic Style] (also known as astyle) approximate our code formatting conventions and can be used to quickly convert a big bunch of source files to our conventions. Note that you may still have to manually clean up some stuff afterwards.
<pre>
indent=tab=4
style=attach
pad-oper
pad-header
align-pointer=name
unpad-paren
indent-preproc-block
indent-preproc-define
indent-preproc-cond
convert-tabs
</pre>

=== Emacs style ===
Put the following in your .emacsrc file

<pre>
(setq-default default-tab-width 4)
(setq-default c-basic-offset 4)
(setq-default indent-tabs-mode t)
</pre>
