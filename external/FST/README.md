FST - Free Studio Technology: header files for building audio plugins
=====================================================================

this is a simple header file
to allow the compilation of audio plugins and audio plugin hosts following
the industrial standard of Steinberg's VST(2).

Steinberg provides a VST2 SDK, but:
- it has a proprietary license
- is no longer available from Steinberg (as they removed the VST2-SDK to push VST3)

FST is the attempt to create a bona-fide reverse-engineered header file,
that is created without reference to the official VST SDK and without its
developer(s) having agreed to the VST SDK license agreement.

All reverse-engineering steps are documented in the [REVERSE_ENGINEERING](docs/REVERSE_ENGINEERING.md) document.

## USING
You are free to use FST under our [LICENSE terms](#licensing).

However, Steinberg has trademarked the `VST` name, and explicitely forbids
anyone from using this name without agreeing to *their* LICENSE terms.

If you plan to release audio plugins based on the FST Interface, you are
not allowed to call them `VST` or even say that they are `VST Compatible`.

## LICENSING
FST is released under the [`GNU General Public License (version 3 or later)`](https://www.gnu.org/licenses/gpl-3.0.en.html).

It is our understanding that if you use the FST-header in your project,
you have to use a compatible (Open Source) license for your project as well.

There are no plans to release FST under a more permissive license.
If you want to build closed-source plugins (or plugin hosts), you have to
use the official Steinberg VST2 SDK, and comply with their licensing terms.


## Miss a feature? File a bug!
So far, FST is not feature complete.
Opcodes that have not been reversed engineered yet are marked as `deprecated`,
so you get a compiler warning when using them.

If you find you need a specific opcode that is not supported yet, please file a bug
at https://git.iem.at/zmoelnig/FST/issues

The more information you can provide, the more likely we will be able to implement missing
or fix broken things.
