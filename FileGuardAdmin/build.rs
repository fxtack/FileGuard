use std::process::Command;

fn main() {
    let output = Command::new("git").args(["rev-parse", "HEAD"]).output().unwrap();
    let build_version = String::from_utf8(output.stdout).unwrap();
    println!("cargo:rustc-env=BUILD_VERSION={}", build_version[0..8].trim_end())
}