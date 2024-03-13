use windows::core::HRESULT;
use windows::core::imp::GetLastError;
use windows::Win32::Foundation::{CloseHandle, HANDLE};
use windows::Win32::Storage::InstallableFileSystems::FilterConnectCommunicationPort;

pub mod raw {
    use std::fmt;
    use std::fmt::Formatter;

    #[repr(C)]
    pub struct CoreVersion {
        pub major: u32,
        pub minor: u32,
        pub patch: u32,
        pub build: u32
    }

    impl fmt::Display for CoreVersion {
        fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
            todo!()
        }
    }
}

pub struct Admin {
    port: HANDLE
}

impl Admin {

    fn connect_core(&mut self) -> Option<HRESULT> {
        self.port = unsafe {
            if let Some(port_handle) = FilterConnectCommunicationPort() {

            } else {
                return GetLastError().to;
            }
        };
        None
    }

    fn disconnect_core(&mut self) {
        unsafe { CloseHandle(self.port).expect("TODO: panic message"); }
    }

    fn request_core_message(&self) {
        todo!()
    }

    pub fn new() -> Result<Admin, HRESULT> {
        todo!()
    }
}

impl Drop for Admin {
    fn drop(&mut self) {
        self.disconnect_core()
    }
}