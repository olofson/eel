EEL - Extensible Embeddable Language
====================================

EEL is a dynamic scripting language, designed to meet the requirements of real time applications. Intended fields of use include control engineering, music applications, audio synthesis, and video games.

Features
--------

EEL has a C-like syntax, but the language is higher level than C, and "safe" in the sense that EEL programs should not be able to crash the virtual machine, or the host application. EEL has dynamic typing, automatic memory management, exception handling, and built-in high level data types such as vectors, arrays and tables. New data types can be added at run time by host applications.

Design
------

EEL compiles into byte-code that runs on a custom virtual machine, which means that no explicit support for specific architectures is needed for portability. The implementation is deliberately very self contained, with few dependencies, to make it easy to integrate into minimal environments, such as embedded devices running real time operating systems.
