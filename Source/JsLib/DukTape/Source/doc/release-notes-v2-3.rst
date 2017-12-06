=========================
Duktape 2.3 release notes
=========================

Release overview
================

Main changes in this release (see RELEASES.rst for full details):

* TBD.

* DUK_USE_ALIGN_BY now always defaults to 8 (natural alignment) to avoid any
  potentially unsafe assumptions about compiler behavior for unaligned memory
  accesses and pointers (which may be an issue even on x86).

* duk_xxx_literal() API call variants which take a plain C literal argument,
  for example duk_get_prop_literal(ctx, -2, "myProperty").  The calls are
  conceptually similar to the duk_xxx_string() variants, but can take advantage
  of the fact that e.g. string length for a C literal can be determined at
  compile time using sizeof("myProperty") - 1 (the -1 is for NUL termination).
  Literal strings used by application code are also automatically pinned
  between mark-and-sweep rounds, and a small lookup cache makes mapping a C
  literal to a heap string object quite fast (almost as fast as using a heapptr).
  For now the calls are experimental.

Upgrading from Duktape 2.2
==========================

No action (other than recompiling) should be needed for most users to upgrade
from Duktape v2.2.x.  Note the following:

* TBD.

* If performance matters, you might consider using duk_xxx_literal() variants
  in place of duk_xxx_string() variants when the argument is a C literal.
  With literal pinning and the literal lookup cache this improves property
  access performance around 20% with minimal application changes.  Make sure
  that the arguments are C literals, e.g. duk_get_prop_literal(ctx, "mykey")
  and not pointers to strings like duk_get_prop_literal(ctx, strptr).

* If you're using the duk_xxx_heapptr() API call variants, you might consider
  switching to the duk_xxx_literal() variants.  They are less error prone, and
  with literal pinning and the literal lookup cache almost as fast as using a
  borrowed heap pointer.

* If you're working with a low memory target or any other target where memory
  usage matters, you may want to ensure UDK_USE_LITCACHE_SIZE is undefined in
  configure.py configuration (this is included in low_memory.yaml base config).

* If you're working with a low memory target or any other target where memory
  usage matters, you may want to force DUK_USE_ALIGN_BY to a lower value
  (4 or 1) to reduce memory overhead.  For most targets the memory overhead
  is not important.

* Base-64 and hex encoding/decoding support is now configurable and disabled
  by default in the example low_memory.yaml configuration.  Enable them
  manually if necessary using DUK_USE_BASE64_SUPPORT and DUK_USE_HEX_SUPPORT.

* Several -fsanitize=undefined warnings have been fixed in the default
  configuration using explicit checks to avoid undefined behavior.  For
  example, floating point division by zero is avoided and behavior in that
  case is implemented explicitly, at some cost in footprint and performance.
  For many compilers the undefined behavior assumptions in Duktape source
  are fine, and you can remove the extra overhead by enabling the
  DUK_USE_ALLOW_UNDEFINED_BEHAVIOR option in configure.py (this option is
  enabled in the low_memory.yaml and performance_sensitive.yaml example
  configurations).  Note, however, that recent gcc/clang versions are
  optimizing around undefined behavior so while relying on undefined behavior
  may work in one version, it may break with newer compiler versions.
