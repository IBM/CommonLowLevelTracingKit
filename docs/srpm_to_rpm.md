<!-- Copyright (c) 2024, International Business Machines -->
<!-- SPDX-License-Identifier: BSD-2-Clause-Patent -->

# SRPM to RPM
```bash
cmake --workflow --preset rpm  --fresh
mkdir -p rpm/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
rpm -ivh --define "_topdir $(pwd)/rpm" build/clltk-1.2.10-1.src.rpm
rpmbuild -bb --define "_topdir $(pwd)/rpm" rpm/SPECS/clltk.spec
```