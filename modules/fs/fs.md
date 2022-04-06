Filesystem Module
============

@namespace lagrange::fs

@defgroup module-fs FS Module
@brief Filesystem utilities.

This module exists as an alias towards one of the following backend:
1. [gulrak/filesystem](https://github.com/gulrak/filesystem), for C++11/C++14 compatibility.
2. [Boost.Filesystem](https://github.com/boostorg/filesystem), for systems using Boost.
3. [std::filesystem](https://en.cppreference.com/w/cpp/filesystem), for C++17.
