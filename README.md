Eigth - Re-imagining Forth in the era of load-store architectures
=================================================================

Eigth is a simple programming environment that takes inspiration from
the Forth. In particular we take from Forth the concatenative
programming concept, where new opcodes can be defined and become
indistinguishable from the core words that are initially defined.

However even though we are inspired by Forth we still willingly discard
the most distinctive element of the language. Eigth replaces the Forth
data stack with registers. That means programming in Eigth feels very
different to Forth because Eigth feels much more like writing for
load/store architectures, including all modern RISC machines.

To finalize the cross-pollination of ideas we also take from Forth
the idea that control flow can be embedded as part of the word stream.
The end result is a merge of the control flow from classic structured
programming languages, with an assembly-like programming feel.

Spelling?
---------

4 is spelled four. The next element of the sequence after third is
spelled fourth. Forth is spelled Forth.

8 is spelt eight. The next element of the sequence after seventh is
spelt eighth. Eigth is spelled Eigth.

Sure forth is a real English word and eigth isn't... but Eigth is still
spelled Eigth. It's a whimsical name for a whimsical project.

Eigth is just an executable thought experiment. Eigth is fun. Eigth has
eight registers (which contributes to the name) Eigth is **not**
intending to suggest that it is twice as good as Forth.

License
-------

This program is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation, either version 3 of the License, or (at your
option) any later version.

This program is distributed in the hope that it will be useful, but
**without any warranty**; without even the implied warranty of
**merchantability** or **fitness for a particular purpose**. See the
[GNU General Public License](LICENSE.md) for more details.
