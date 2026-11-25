// Native FreeBSD process provider for the pinned SteamOS Manager source.
use anyhow::{Result, ensure};
use std::{collections::HashSet, io, mem, ptr};

const MAX_ENV: usize = 8 * 1024 * 1024;
const MAX_ANCESTORS: usize = 128;

fn snapshot(pid: libc::pid_t) -> Result<libc::kinfo_proc> {
    ensure!(pid > 0, "invalid process ID");
    let mib = [libc::CTL_KERN, libc::KERN_PROC, libc::KERN_PROC_PID, pid];
    let mut value = mem::MaybeUninit::<libc::kinfo_proc>::zeroed();
    let mut len = mem::size_of::<libc::kinfo_proc>();
    // SAFETY: mib and value are valid, correctly sized buffers; read only.
    let rc = unsafe { libc::sysctl(mib.as_ptr(), mib.len() as _, value.as_mut_ptr().cast(), &mut len, ptr::null(), 0) };
    if rc != 0 { return Err(io::Error::last_os_error().into()); }
    ensure!(len == mem::size_of::<libc::kinfo_proc>(), "unexpected kinfo_proc size or process disappeared");
    // SAFETY: the kernel returned one full kinfo_proc into zeroed storage.
    let value = unsafe { value.assume_init() };
    ensure!(value.ki_structsize as usize == len && value.ki_pid == pid, "incompatible process ABI or PID");
    Ok(value)
}

fn environment(pid: libc::pid_t) -> Result<Vec<u8>> {
    let mib = [libc::CTL_KERN, libc::KERN_PROC, libc::KERN_PROC_ENV, pid];
    for _ in 0..3 {
        let mut len = 0;
        // SAFETY: size-only query; no kernel data is written to a null buffer.
        let rc = unsafe { libc::sysctl(mib.as_ptr(), mib.len() as _, ptr::null_mut(), &mut len, ptr::null(), 0) };
        if rc != 0 { return Err(io::Error::last_os_error().into()); }
        ensure!(len <= MAX_ENV, "process environment exceeds limit");
        if len == 0 { return Ok(Vec::new()); }
        let mut bytes = vec![0u8; len];
        // SAFETY: bytes owns a writable buffer of exactly len bytes.
        let rc = unsafe { libc::sysctl(mib.as_ptr(), mib.len() as _, bytes.as_mut_ptr().cast(), &mut len, ptr::null(), 0) };
        if rc == 0 {
            ensure!(len <= bytes.len(), "invalid environment size");
            bytes.truncate(len);
            ensure!(bytes.is_empty() || bytes.last() == Some(&0), "unterminated process environment");
            return Ok(bytes);
        }
        let err = io::Error::last_os_error();
        if err.raw_os_error() != Some(libc::ENOMEM) { return Err(err.into()); }
    }
    anyhow::bail!("process environment changed during read")
}

fn appid_from_environment(bytes: &[u8]) -> Option<u64> {
    for field in bytes.split(|&b| b == 0) {
        if let Some(value) = field.strip_prefix(b"SteamGameId=") {
            return std::str::from_utf8(value).ok()?.parse().ok();
        }
    }
    None
}

pub(crate) fn read_comm(pid: u32) -> Result<String> {
    let info = snapshot(pid.try_into()?)?;
    let name: Vec<u8> = info.ki_comm.iter().take_while(|&&c| c != 0).map(|&c| c as u8).collect();
    Ok(String::from_utf8(name)?.trim_end().to_owned())
}

pub(crate) fn get_appid(pid: u32) -> Result<Option<u64>> {
    let mut pid: libc::pid_t = pid.try_into()?;
    let mut seen = HashSet::new();
    for _ in 0..MAX_ANCESTORS {
        ensure!(seen.insert(pid), "cycle in process ancestry");
        let before = snapshot(pid)?;
        let bytes = environment(pid)?;
        let after = snapshot(pid)?;
        ensure!(before.ki_start.tv_sec == after.ki_start.tv_sec &&
                before.ki_start.tv_usec == after.ki_start.tv_usec &&
                before.ki_ppid == after.ki_ppid, "process identity or ancestry changed");
        if let Some(id) = appid_from_environment(&bytes) { return Ok(Some(id)); }
        if after.ki_ppid <= 1 { return Ok(None); }
        pid = after.ki_ppid;
    }
    anyhow::bail!("process ancestry exceeds limit")
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn parser() {
        assert_eq!(appid_from_environment(b"A=B\0SteamGameId=123\0C=D\0"), Some(123));
        assert_eq!(appid_from_environment(b"NotSteamGameId=123\0"), None);
        assert_eq!(appid_from_environment(b"SteamGameId=bad\0SteamGameId=456\0"), None);
        assert_eq!(appid_from_environment(b"SteamGameId=18446744073709551616\0"), None);
    }
    #[test]
    fn native_self() {
        assert!(!read_comm(std::process::id()).unwrap().is_empty());
        assert!(snapshot(0).is_err());
        assert!(environment(std::process::id().try_into().unwrap()).is_ok());
    }
}
