use crate::arch::Syscall;
use cstr_core::{CStr,CString};
use crate::Stat;

pub fn sched_yield() {
    Syscall(0x00,0,0,0);
}

pub(crate) fn exit(status: u8) -> ! {
    Syscall(0x01,status as usize,0,0);
    unreachable!();
}

pub fn fork() -> i32 {
    Syscall(0x02,0,0,0) as i32
}

pub fn open(path: &str, mode: usize) -> isize {
    let cpath = CString::new(path).expect("owlOS Programmer API: String conversion failed");
    let ptr = cpath.into_raw();
    let ret = Syscall(0x03,ptr as usize,mode,0);
    let _ = unsafe {CString::from_raw(ptr)}; // This prevents memory leaking from occuring.
    ret
}

pub fn close(fd: isize) -> isize {
    Syscall(0x04,fd as usize,0,0)
}

pub fn read(fd: isize, buf: &mut [u8]) -> isize {
    loop {
        let status = Syscall(0x05,fd as usize,buf.as_mut_ptr() as usize,buf.len());
        if status != -11 {
            return status;
        }
        Syscall(0,0,0,0); // Yield
    }
}

pub fn write(fd: isize, buf: &[u8]) -> isize {
    loop {
        let status = Syscall(0x06,fd as usize,buf.as_ptr() as usize,buf.len());
        if status != -11 {
            return status;
        }
        Syscall(0,0,0,0); // Yield
    }
}

pub fn lseek(fd: isize, offset: isize, whence: usize) -> isize {
    Syscall(0x07,fd as usize,offset as usize,whence)
}

pub fn dup(fd: isize) -> isize {
    Syscall(0x08,fd as usize,-1isize as usize,0)
}

pub fn dup2(old_fd: isize, new_fd: isize) -> isize {
    Syscall(0x08,old_fd as usize,new_fd as usize,0)
}

pub fn unlink(path: &str) -> isize {
    let cpath = CString::new(path).expect("owlOS Programmer API: String conversion failed");
    let ptr = cpath.into_raw();
    let ret = Syscall(0x0a,ptr as usize,0,0);
    let _ = unsafe {CString::from_raw(ptr)}; // This prevents memory leaking from occuring.
    ret
}

pub fn stat(path: &str, buf: &mut Stat) -> isize {
    let cpath = CString::new(path).expect("owlOS Programmer API: String conversion failed");
    let ptr = cpath.into_raw();
    let ret = Syscall(0x0b,ptr as usize,buf as *mut _ as usize,0);
    let _ = unsafe {CString::from_raw(ptr)}; // This prevents memory leaking from occuring.
    ret
}

pub fn fstat(fd: isize, buf: &mut Stat) -> isize {
    Syscall(0x0c,fd as usize,buf as *mut _ as usize,0)
}

pub fn access(path: &str, mode: usize) -> isize {
    let cpath = CString::new(path).expect("owlOS Programmer API: String conversion failed");
    let ptr = cpath.into_raw();
    let ret = Syscall(0x0d,ptr as usize,mode,0);
    let _ = unsafe {CString::from_raw(ptr)}; // This prevents memory leaking from occuring.
    ret
}

pub fn chmod(path: &str, mode: usize) -> isize {
    let cpath = CString::new(path).expect("owlOS Programmer API: String conversion failed");
    let ptr = cpath.into_raw();
    let ret = Syscall(0x0e,ptr as usize,mode,0);
    let _ = unsafe {CString::from_raw(ptr)}; // This prevents memory leaking from occuring.
    ret
}

pub fn chown(path: &str, uid: i32, gid: i32) -> isize {
    let cpath = CString::new(path).expect("owlOS Programmer API: String conversion failed");
    let ptr = cpath.into_raw();
    let ret = Syscall(0x0f,ptr as usize,uid as usize,gid as usize);
    let _ = unsafe {CString::from_raw(ptr)}; // This prevents memory leaking from occuring.
    ret
}

pub fn umask(mode: usize) -> usize {
    Syscall(0x10,mode,0,0) as usize
}

pub fn ioctl(fd: isize, req: usize, arg: usize) -> isize {
    Syscall(0x11,fd as usize,req,arg)
}

pub fn execv(path: &str, argv: &[&CStr]) -> isize {
    let cpath = CString::new(path).expect("owlOS Programmer API: String conversion failed");
    let ptr = cpath.into_raw();
    let ret = Syscall(0x12,ptr as usize,argv.as_ptr() as usize,0);
    let _ = unsafe {CString::from_raw(ptr)}; // This prevents memory leaking from occuring.
    ret
}

pub fn wait(wstatus: *mut usize) -> isize {
    loop {
        let result = Syscall(0x13,-1isize as usize,wstatus as usize,0);
        if result != 0 {
            return result;
        }
        Syscall(0,0,0,0); // Yield
    }
}

pub fn waitpid(pid: i32, wstatus: *mut usize, opt: usize) -> isize {
    loop {
        let result = Syscall(0x13,pid as usize,wstatus as usize,0);
        if result != 0 || opt & 1 == 1 {
            return result;
        }
        Syscall(0,0,0,0); // Yield
    }
}

pub fn getuid() -> u32 {
    Syscall(0x14,0,0,0) as u32
}

pub fn geteuid() -> u32 {
    Syscall(0x15,0,0,0) as u32
}

pub fn getgid() -> u32 {
    Syscall(0x16,0,0,0) as u32
}

pub fn getegid() -> u32 {
    Syscall(0x17,0,0,0) as u32
}

pub fn getpid() -> i32 {
    Syscall(0x18,0,0,0) as i32
}

pub fn getppid() -> i32 {
    Syscall(0x19,0,0,0) as i32
}

pub fn setpgid(pid: i32, group: i32) -> isize {
    Syscall(0x1a,pid as usize,group as usize,0)
}

pub fn getpgrp() -> i32 {
    Syscall(0x1b,0,0,0) as i32
}

pub fn signal(sig: u8, handler: &fn(u8)) -> isize {
    Syscall(0x1c,sig as usize,handler as *const _ as usize,0)
}

pub fn kill(pid: i32, sig: u8) -> isize {
    Syscall(0x1d,pid as usize,sig as usize,0)
}

pub fn sigreturn() {
    Syscall(0x1e,0,0,0);
}

// nanosleep goes here

//pub fn 

pub fn sbrk(expand: isize) -> isize {
    Syscall(0x22,expand as usize,0,0)
}

pub fn foxkernel_powerctl(cmd: usize) -> isize {
    Syscall(0xf0,cmd,0,0)
}