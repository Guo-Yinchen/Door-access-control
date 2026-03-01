# Third-Party Notices

This project uses the following third-party open-source component(s).
See each project for full license texts and details.

## libgpiod (Linux GPIO character device library)
- Project: libgpiod
- Upstream: https://git.kernel.org/pub/scm/libs/libgpiod/libgpiod.git
- Package (build system): libgpiod-dev 2.2.1-2+deb13u1 (arm64)
- License:
  - Core library: LGPL-2.1+ (Files: *)  
  - Tools: GPL-2+ (Files: tools/*)  
  (Verified from: /usr/share/doc/libgpiod-dev/copyright)
  
- How used:
  - Linked as `-lgpiod` 
  - Used via libgpiod v2 C API to request GPIO output lines and set values
- Where used:
  - include/GPIO/gpio-line.hpp 
  - src/GPIO/gpio-line.cpp 