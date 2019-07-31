exists( daemon/keydefs.h ) {
    TEMPLATE = subdirs
    SUBDIRS = cli/trader-cli.pro daemon/traderd.pro
} else {
    error( "keydefs.h doesn't exist. You must either: 1) Generate the file with 'python generate_keys.py', or 2) Copy the example file with 'cp daemon/keydefs.h.example daemon/keydefs.h' and manually fill in your keys, or if you don't want hardcoded keys: 3) Copy the example file, leave your keys blank, and use the cli command 'setkeyandsecret' at runtime." )
}
