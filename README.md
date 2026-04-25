<div align="center">
<img src="./public/banner.svg">
</div>

---

## What is Oh-My-XV6

xv6 is a re-implementation of Dennis Ritchie's and Ken Thompson's Unix Version 6 (v6).  xv6 loosely follows the structure and style of v6, but is implemented for a modern x86-based multiprocessor using ANSI C.

xv6-64 is a 64-bit port of MIT's xv6, by Anthony Shelton and Jakob Eriksson, for use in UIC's Operating Systems curriculum.

Oh-My-XV6 was build on top of the XV6 kernel provided by UIC to add quality of life features 
that were inspired by [OhMyZsh](https://github.com/ohmyzsh/ohmyzsh) 

## Features

- shell command history
- poweroff command
- fseek system call
- move and touch implementations
- file jumping command
- filesystem crawler
- autocomplete commands

## Building and Running

```
git clone https://github.com/edmuri/Oh-My-XV6
cd Oh-My-XV6
```

Make sure you are running a Linux environment. 

If you have not done so, make sure you have the QEMU PC simulators. 

You can install via

```
sudo apt update -y
sudo apt install -y build-essential git qemu-system-x86
```

then run with 

```
cd src 
make qemu-nox
```

## Acknowledgements
The code in the files that constitute xv6 is Copyright 2006-2017
Frans Kaashoek, Robert Morris, Russ Cox, Anthony Shelton and Jakob Eriksson.

## Oh-My-XV6 Contributors
[![Kaito](https://img.shields.io/badge/Kaito_Sekiya-800000?style=for-the-badge&logo=github&logoColor=white)](https://github.com/Givikap)
[![Michael](https://img.shields.io/badge/Michael_Oltman-A6500A?style=for-the-badge&logo=github&logoColor=white)](https://github.com/moltm3)
[![Eduardo](https://img.shields.io/badge/Eduardo_Murillo-313030?style=for-the-badge&logo=github&logoColor=white)](https://github.com/edmuri)

