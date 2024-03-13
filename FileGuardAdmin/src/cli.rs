use clap::{App, AppSettings, Arg, ArgAction, SubCommand};

const APP_NAME: &str = "FileGuardAdmin";
const APP_ABOUT: &str = "FileGuard admin terminate tool";

fn get_version_info() -> String {
    let build_version = env!("BUILD_VERSION");
    "".to_string()
}

pub fn parse_and_run() {
    let matches = App::new(APP_NAME)
        .setting(AppSettings::SubcommandRequiredElseHelp)
        .disable_colored_help(true)
        .version("s")
        .about(APP_ABOUT)
        .arg(Arg::new("version")
            .short('v')
            .long("version")
            .action(ArgAction::Version)
            .help("Show version information"))
        .subcommand(SubCommand::with_name("add-rule")
            .about("Add a rule")
            .arg(Arg::new("path")
                .help("Rule path name")
                .long("path")
                .short('p')
                .required(true)
                .takes_value(true))
            .arg(Arg::new("type")
                .help("Rule type")
                .long("type")
                .short('t')
                .required(true)
                .takes_value(true)))
        .subcommand(SubCommand::with_name("remove-rule")
            .about("Remove a rule")
            .arg(Arg::new("path")
                .help("Rule path name")
                .long("path")
                .short('p')
                .required(true)
                .takes_value(true))
            .arg(Arg::new("type")
                .help("Rule type")
                .long("type")
                .short('t')
                .required(true)
                .takes_value(true)))
        .subcommand(SubCommand::with_name("cleanup-rules")
            .about("Remove all rules from volume")
            .arg(Arg::new("volume")
                .help("Volume name")
                .long("volume")
                .short('v')
                .required(true)
                .takes_value(true))
        ).get_matches();

    match matches.subcommand() {

    }
}