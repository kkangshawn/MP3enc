# MP3enc

WAV to MP3 Encoder
---------------------------------------------

- using lame library (installation required - http://lame.sourceforge.net/)
- supports encoding multiple files using pthread by typing directory path
- works on Linux Ubuntu 15.10/Fedora 24(x86_64/armv7l), Windows 7/10(x86/64), MinGW system

##Build
- Linux, MinGW: make
- Windows: build by means of Microsoft Visual Studio 2015

##Note for Linux system
- Some system may require glibc-static library.
```sh
dnf install glibc-static
```
- If arch does not match,  
 . build lame static library and install or copy the file to ./lib

##Note for Windows system
- Microsoft Visual Studio 2015 solution/project files in **.\\msvc_solution**.
- When you build libmp3lame library,  
 . as recommended in **README**, use nmake instead of Visual Studio project.  
 . this needs to eliminate /opt:NOWIN98 from $(LN_OPTS)/$(LN_DLL) in **Makefile.MSVC** since it is deprecated.  
 . for x64 build, eliminate /GL from $(CC_OPTS).  

- In case that you encounter a link error on building pthread-w32 library,  
 . to build a static library of pthread, you need to modify **Makefile** - '/MD' in $(XCFLAGS) to '/MT'.  
 . if timespec definition duplicated, add '&& !defined(_INC_TIME)' to **pthread.h**  
 

```c
#if !defined(HAVE_STRUCT_TIMESPEC) && !defined(_INC_TIME)
#define HAVE_STRUCT_TIMESPEC
#if !defined(_TIMESPEC_DEFINED
#define _TIMESPEC_DEFINED
struct timespec {
        time_t tv_sec;
        long tv_nsec;
};
#endif /* _TIMESPEC_DEFINED */
#endif /* HAVE_STRUCT_TIMESPEC */
```
 

##Usage
```sh
MP3enc <input_filename [-o <output_filename>] | input_directory> [OPTIONS]
```
##Todo
