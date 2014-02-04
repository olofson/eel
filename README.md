EEL - Extensible Embeddable Language
====================================

EEL is a scripting and programming language, designed specifically for hard real time applications. The primary target areas of application are control engineering and audio synthesis, but EEL should also be suitable for game scripting and for adding scripting capabilities to real time multimedia applications.

The syntax is C-like, but the language is higher level than C, and "safe" in the sense that EEL programs should not be able to crash the virtual machine or the host application. EEL has dynamic typing, automatic memory management, exception handling and built-in high level data types such as vectors, arrays and tables. New such data types can be added at run time by host applications.

EEL compiles into byte-code that runs on a virtual machine, which means that no explicit support for specific achitectures is needed for portability. The implementation is deliberately very self contained and has few dependencies, so that it can be easily integrated into "strange" environments, such as embedded devices running real time operating systems.
